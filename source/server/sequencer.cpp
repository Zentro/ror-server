/*
This file is part of "Rigs of Rods Server" (Relay mode)
Copyright 2007 Pierre-Michel Ricordel
Contact: pricorde@rigsofrods.com
"Rigs of Rods Server" is distributed under the terms of the GNU General Public License.

"Rigs of Rods Server" is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.

"Rigs of Rods Server" is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "sequencer.h"

#include "messaging.h"
#include "sha1_util.h"
#include "listener.h"
#include "receiver.h"
#include "broadcaster.h"
#include "notifier.h"
#include "userauth.h"
#include "SocketW.h"
#include "logger.h"
#include "config.h"
#include "utils.h"
#include "ScriptEngine.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
//#define REFLECT_DEBUG
#define UID_NOT_FOUND 0xFFFF


#ifdef __GNUC__
#include <stdlib.h>
#endif


#include <cstdio>



void *s_klthreadstart(void* vid)
{
    STACKLOG;
	((Sequencer*)vid)->killerthreadstart();
	return NULL;
}

// init the singleton pointer
Sequencer* Sequencer::mInstance = NULL;

/// retreives the instance of the Sequencer
Sequencer* Sequencer::Instance() {
    STACKLOG;
	if(!mInstance)
		mInstance = new Sequencer;
	return mInstance;
}

unsigned int Sequencer::connCrash = 0;
unsigned int Sequencer::connCount = 0;


Sequencer::Sequencer() :  listener( NULL ), notifier( NULL ), authresolver(NULL),
fuid( 1 ), startTime ( Messaging::getTime() )
{
    STACKLOG;
}

Sequencer::~Sequencer()
{
	STACKLOG;
	//cleanUp();
}

/**
 * Inililize, needs to be called before the class is used
 */
void Sequencer::initilize()
{
    STACKLOG;

    Sequencer* instance  = Instance();
	instance->clients.reserve( Config::getMaxClients() );
	instance->listener = new Listener(Config::getListenPort());

	instance->script = 0;
#ifdef WITH_ANGELSCRIPT
	if(Config::getEnableScripting())
	{
		instance->script = new ScriptEngine(instance);
		instance->script->loadScript(Config::getScriptName());
	}
#endif //WITH_ANGELSCRIPT

	pthread_create(&instance->killerthread, NULL, s_klthreadstart, &instance);

	instance->authresolver = 0;
	if( Config::getServerMode() != SERVER_LAN )
	{
		instance->notifier = new Notifier(instance->authresolver);

		// only start userauth if we are registered with the master server and if we have trustworthyness > 1
		if(instance->notifier->getAdvertised() && instance->notifier->getTrustLevel()>1)
			instance->authresolver = new UserAuth(instance->notifier->getChallenge());

	}
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::cleanUp()
{
    STACKLOG;

	static bool cleanup = false;
	if(cleanup) return;
	cleanup=true;

    Sequencer* instance = Instance();
	Logger::log(LOG_INFO,"closing. disconnecting clients ...");
	const char *str = "server shutting down (try to reconnect later!)";
	for( unsigned int i = 0; i < instance->clients.size(); i++)
	{
		// HACK-ISH override all thread stuff and directly send it!
		Messaging::sendmessage(instance->clients[i]->sock, MSG2_DELETE, instance->clients[i]->uid, 0, strlen(str), str);
		//disconnect(instance->clients[i]->uid, );
	}
	Logger::log(LOG_INFO,"all clients disconnected. exiting.");

	if(instance->notifier)
		delete instance->notifier;

#ifdef WITH_ANGELSCRIPT
	if(instance->script)
		delete instance->script;
#endif //WITH_ANGELSCRIPT

	if(instance->authresolver)
		delete instance->authresolver;

#ifndef WIN32
	sleep(2);
#else
	Sleep(2000);
#endif

	delete instance->listener;
	delete instance->mInstance;
}

void Sequencer::notifyRoutine()
{
    STACKLOG;
	//we call the notify loop
    Sequencer* instance = Instance();
    instance->notifier->loop();
}

bool Sequencer::checkNickUnique(char *nick)
{
    STACKLOG;
	// WARNING: be sure that this is only called within a clients_mutex lock!

	// check for duplicate names
	Sequencer* instance = Instance();
	for (unsigned int i = 0; i < instance->clients.size(); i++)
	{
		if (!strcmp(nick, instance->clients[i]->nickname))
		{
			return true;
		}
	}
	return false;
}


int Sequencer::getFreePlayerColour()
{
    STACKLOG;
	// WARNING: be sure that this is only called within a clients_mutex lock!

	int col = 0;
	Sequencer* instance = Instance();
recheck_col:
	for (unsigned int i = 0; i < instance->clients.size(); i++)
	{
		if(instance->clients[i]->colournumber == col)
		{
			col++;
			goto recheck_col;
		}
	}
	return col;
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, user_credentials_t *user)
{
    STACKLOG;
    Sequencer* instance = Instance();
	//we have a confirmed client that wants to play
	//try to find a place for him
	Logger::log(LOG_DEBUG,"got instance in createClient()");

    MutexLocker scoped_lock(instance->clients_mutex);
	bool dupeNick = Sequencer::checkNickUnique(user->username);
	int playerColour = Sequencer::getFreePlayerColour();
	int dupecounter = 2;

	// check if server is full
	Logger::log(LOG_DEBUG,"searching free slot for new client...");
	if( instance->clients.size() >= Config::getMaxClients() )
	{
		Logger::log(LOG_WARN,"join request from '%s' on full server: rejecting!", user->username);
		// set a low time out because we don't want to cause a back up of
		// connecting clients
		sock->set_timeout( 10, 0 );
		Messaging::sendmessage(sock, MSG2_FULL, 0, 0, 0, 0);
		throw std::runtime_error("Server is full");
	}

	if(dupeNick)
	{
		char buf[20] = "";
		strncpy(buf, user->username, 20);
		Logger::log(LOG_WARN,"found duplicate nick, getting new one: %s", buf);
		if(strnlen(buf, 20) == 20)
			//shorten the string
			buf[18]=0;
		while(dupeNick)
		{
			sprintf(buf+strnlen(buf, 18), "%d", dupecounter++);
			Logger::log(LOG_DEBUG,"checked for duplicate nick (2): %s", buf);
			dupeNick = Sequencer::checkNickUnique(buf);
		}
		Logger::log(LOG_WARN,"chose alternate username: %s\n", buf);
		strncpy(user->username, buf, 20);

		// we should send him a message about the nickchange later...
	}


	client_t* to_add = new client_t;

	//okay, create the stuff
	to_add->flow=false;
	to_add->status=USED;
	to_add->initialized=false;
	to_add->vehicle_name[0]=0;
#ifdef WITH_ANGELSCRIPT
	to_add->position=Vector3(0,0,0);
#endif //WITH_ANGELSCRIPT
	to_add->beambuffersize=0;
	to_add->sbi=0;
	to_add->colournumber=playerColour;

	// auth stuff
	to_add->authstate = AUTH_NONE;
	if(instance->authresolver)
	{
		Logger::log(LOG_INFO, "getting user auth level");
		int auth_flags = instance->authresolver->getUserModeByUserToken(user->uniqueid);
		if(auth_flags != AUTH_NONE)
			to_add->authstate |= auth_flags;

		char authst[4] = "";
		if(auth_flags & AUTH_ADMIN) strcat(authst, "A");
		if(auth_flags & AUTH_MOD) strcat(authst, "M");
		if(auth_flags & AUTH_RANKED) strcat(authst, "R");
		if(auth_flags & AUTH_BOT) strcat(authst, "B");
		Logger::log(LOG_INFO, "user auth flags: " + std::string(authst));
	}

	memset(to_add->nickname, 0, 32); // for 20 character long nicknames :)
	strncpy(to_add->nickname, user->username, 20);
	strncpy(to_add->uniqueid, user->uniqueid, 60);
	to_add->receiver = new Receiver();
	to_add->broadcaster = new Broadcaster();

	// replace bad characters
	for (unsigned int i=0; i<20; i++)
	{
		if(to_add->nickname[i] == 0)
			break;
		// btw, the above code is disfunctional ...
	}

	to_add->uid=instance->fuid;
	instance->fuid++;
	to_add->sock = sock;//this won't interlock

	instance->clients.push_back( to_add );
	to_add->receiver->reset(to_add->uid, sock);
	to_add->broadcaster->reset(to_add->uid, sock, Sequencer::disconnect, Messaging::sendmessage, Messaging::addBandwidthDropOutgoing);

	// process slot infos
	int npos = instance->getPosfromUid(to_add->uid);
	instance->clients[npos]->slotnum = npos;

	// now inform the client of his data
	client_info_on_join info_own;
	memset(&info_own, 0, sizeof(client_info_on_join));
	info_own.version = 1;
	info_own.slotid = npos;
	info_own.colournum = playerColour;
	strncpy(info_own.nickname, instance->clients[npos]->nickname, 20);
	info_own.authstatus = instance->clients[npos]->authstate;


	SWBaseSocket::SWBaseError error;
	if(Sequencer::isbanned(sock->get_peerAddr(&error).c_str()))
	{
		Logger::log( LOG_DEBUG, "receiver thread %d owned by uid %d terminated (banned user)", ThreadID::getID(), instance->clients[npos]->uid);
		Logger::log(LOG_VERBOSE,"banned user rejected: uid %i", instance->clients[npos]->uid);
		Messaging::sendmessage(sock, MSG2_BANNED, instance->clients[npos]->uid, 0, 0, 0);
		Sequencer::disconnect(instance->clients[npos]->uid, "you are banned");
		return;
	}

	Logger::log(LOG_VERBOSE,"Sending welcome message to uid %i, slotpos: %i", instance->clients[npos]->uid, npos);
	if( Messaging::sendmessage(sock, MSG2_WELCOME, instance->clients[npos]->uid, 0, sizeof(playerColour), (char *)&playerColour) )
	{
		Sequencer::disconnect(instance->clients[npos]->uid, "error sending welcome message" );
		return;
	}

	// notify everyone of the new client
	for(unsigned int i = 0; i < instance->clients.size(); i++)
	{
		instance->clients[i]->broadcaster->queueMessage(MSG2_USER_JOIN, instance->clients[npos]->uid, 0, sizeof(client_info_on_join), (char*)&info_own);
	}

	// done!
	Logger::log(LOG_VERBOSE,"Sequencer: New client added");
}

//this is called from the hearbeat notifier thread
int Sequencer::getHeartbeatData(char *challenge, char *hearbeatdata)
{
    STACKLOG;

    Sequencer* instance = Instance();
	SWBaseSocket::SWBaseError error;
	int clientnum = getNumClients();
	// lock this mutex after getNumClients is called to avoid a deadlock
	MutexLocker scoped_lock(instance->clients_mutex);

	sprintf(hearbeatdata, "%s\n" \
	                      "version4\n" \
	                      "%i\n", challenge, clientnum);
	if(clientnum > 0)
	{
		for( unsigned int i = 0; i < instance->clients.size(); i++)
		{
			char authst[4] = "";
			if(instance->clients[i]->authstate & AUTH_ADMIN) strcat(authst, "A");
			if(instance->clients[i]->authstate & AUTH_MOD) strcat(authst, "M");
			if(instance->clients[i]->authstate & AUTH_RANKED) strcat(authst, "R");
			if(instance->clients[i]->authstate & AUTH_BOT) strcat(authst, "B");

			char playerdata[1024] = "";
			char positiondata[128] = "";
#ifdef WITH_ANGELSCRIPT
			sprintf(positiondata, "%0.2f,%0.2f,%0.2f", instance->clients[i]->position.x, instance->clients[i]->position.y, instance->clients[i]->position.z);
#endif //WITH_ANGELSCRIPT
			sprintf(playerdata, "%d;%s;%s;%s;%s;%s;%s\n", i,
					instance->clients[i]->vehicle_name,
					instance->clients[i]->nickname,
					positiondata,
					instance->clients[i]->sock->get_peerAddr(&error).c_str(),
					instance->clients[i]->uniqueid,
					authst);
			strcat(hearbeatdata, playerdata);
		}
	}
	return 0;
}

int Sequencer::getNumClients()
{
    STACKLOG;
    Sequencer* instance = Instance();
	MutexLocker scoped_lock(instance->clients_mutex);
	return (int)instance->clients.size();
}

int Sequencer::authNick(std::string token, std::string &nickname)
{
    STACKLOG;
    Sequencer* instance = Instance();
	MutexLocker scoped_lock(instance->clients_mutex);
	if(!instance->authresolver)
		return AUTH_NONE;
	return instance->authresolver->resolve(token, nickname);
}

ScriptEngine* Sequencer::getScriptEngine()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->script;
}

void Sequencer::killerthreadstart()
{
    STACKLOG;
    Sequencer* instance = Instance();
	Logger::log(LOG_DEBUG,"Killer thread ready");
	while (1)
	{
		SWBaseSocket::SWBaseError error;

		Logger::log(LOG_DEBUG,"Killer entering cycle");

		instance->killer_mutex.lock();
		while( instance->killqueue.empty() )
			instance->killer_mutex.wait(instance->killer_cv);

		//pop the kill queue
		client_t* to_del = instance->killqueue.front();
		instance->killqueue.pop();
		instance->killer_mutex.unlock();

		Logger::log(LOG_DEBUG,"Killer called to kill %s", to_del->nickname );
		// CRITICAL ORDER OF EVENTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// stop the broadcaster first, then disconnect the socket.
		// other wise there is a chance (being concurrent code) that the
		// socket will attempt to send a message between the disconnect
		// which makes the socket invalid) and the actual time of stoping
		// the bradcaster

		if(to_del->beambuffersize>0 && to_del->sbi)
		{
			Logger::log(LOG_DEBUG,"freeing beam memory of %s", to_del->nickname );
			// free beam memory
			free(to_del->sbi);
			to_del->beambuffersize=0;
			to_del->sbi=0;
		}

		to_del->broadcaster->stop();
		to_del->receiver->stop();
        to_del->sock->disconnect(&error);
		// END CRITICAL ORDER OF EVENTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		delete to_del->broadcaster;
		delete to_del->receiver;
		delete to_del->sock;
		to_del->broadcaster = NULL;
		to_del->receiver = NULL;
		to_del->sock = NULL;

		delete to_del;
		to_del = NULL;
	}
}

void Sequencer::disconnect(int uid, const char* errormsg, bool isError)
{
    STACKLOG;
    Sequencer* instance = Instance();

    MutexLocker scoped_lock(instance->killer_mutex);
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;

	// send an event if user is rankend and if we are a official server
	if(instance->authresolver && (instance->clients[pos]->authstate & AUTH_RANKED))
	{
		instance->authresolver->sendUserEvent(instance->clients[pos]->uniqueid, (isError?"crash":"leave"), instance->clients[pos]->nickname, "");
	}

#ifdef WITH_ANGELSCRIPT
	if(instance->script)
		instance->script->playerDeleted(instance->clients[pos]->uid, isError?1:0);
#endif //WITH_ANGELSCRIPT

	//this routine is a potential trouble maker as it can be called from many thread contexts
	//so we use a killer thread
	Logger::log(LOG_VERBOSE, "Disconnecting Slot %d: %s", pos, errormsg);

	client_t *c = instance->clients[pos];

	Logger::log(LOG_DEBUG, "adding client to kill queue, size: %d", instance->killqueue.size());
	instance->killqueue.push(c);

	//notify the others
	for( unsigned int i = 0; i < instance->clients.size(); i++)
	{
		// notify of delete always
		{
			if(isError)
				instance->clients[i]->broadcaster->queueMessage(MSG2_DELETE, instance->clients[pos]->uid, 0, (int)strlen(errormsg), errormsg);
			else
				instance->clients[i]->broadcaster->queueMessage(MSG2_USER_LEAVE, instance->clients[pos]->uid, 0, (int)strlen(errormsg), errormsg);

		}
	}
	instance->clients.erase( instance->clients.begin() + pos );

	instance->killer_cv.signal();


	instance->connCount++;
	if(isError)
		instance->connCrash++;
	Logger::log(LOG_INFO, "crash statistic: %d of %d deletes crashed", instance->connCrash, instance->connCount);

	printStats();
}

//this is called from the listener thread initial handshake
void Sequencer::enableFlow(int uid)
{
    STACKLOG;
    Sequencer* instance = Instance();

    MutexLocker scoped_lock(instance->clients_mutex);
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;

	instance->clients[pos]->flow=true;
	// now they are a bonified part of the server, show the new stats
    printStats();
}


//this is called from the listener thread initial handshake
int Sequencer::sendMOTD(int uid)
{
    STACKLOG;

	std::vector<std::string> lines;
	int res = readFile("motd.txt", lines);
	if(res)
		return res;

	std::vector<std::string>::iterator it;
	for(it=lines.begin(); it!=lines.end(); it++)
	{
		serverSay(*it, uid, 1);
	}
	return 0;
}

int Sequencer::readFile(std::string filename, std::vector<std::string> &lines)
{
	FILE *f = fopen(filename.c_str(), "r");
	if (!f)
		return -1;
	int linecounter=0;
	while(!feof(f))
	{
		char line[2048] = "";
		memset(line, 0, 2048);
		fgets (line, 2048, f);
		linecounter++;

		if(strnlen(line, 2048) <= 2)
			continue;

		// strip line (newline char)
		char *ptr = line;
		while(*ptr)
		{
			if(*ptr == '\n')
			{
				*ptr=0;
				break;
			}
			ptr++;
		}
		lines.push_back(std::string(line));
	}
	fclose (f);
	return 0;
}


UserAuth* Sequencer::getUserAuth()
{
	STACKLOG;
	Sequencer* instance = Instance();
	return instance->authresolver;
}

//this is called from the listener thread initial handshake
void Sequencer::notifyAllVehicles(int uid, bool lock)
{
    STACKLOG;
    Sequencer* instance = Instance();

	if(lock)
		MutexLocker scoped_lock(instance->clients_mutex);

    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;


	client_info_on_join info_own;
	memset(&info_own, 0, sizeof(client_info_on_join));
	info_own.version = 1;
	info_own.slotid = pos;
	info_own.colournum = instance->clients[pos]->colournumber;
	strncpy(info_own.nickname, instance->clients[pos]->nickname, 20);
	info_own.authstatus = instance->clients[pos]->authstate;

	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED)
		{
			// construct info packet
			client_info_on_join info;
			memset(&info, 0, sizeof(client_info_on_join));
			info.version = 1;
			strncpy(info.nickname, instance->clients[i]->nickname, 20);
			info.authstatus = instance->clients[i]->authstate;
			info.slotid = instance->clients[i]->slotnum;
			info.colournum = instance->clients[i]->colournumber;

			// send user infos
			// all others to new user
			instance->clients[pos]->broadcaster->queueMessage(MSG2_USER_INFO, instance->clients[i]->uid, 0, sizeof(client_info_on_join), (char*)&info );

			// new user to all others
			instance->clients[i]->broadcaster->queueMessage(MSG2_USER_INFO, instance->clients[pos]->uid, 0, sizeof(client_info_on_join), (char*)&info_own );

			Logger::log(LOG_VERBOSE, " * %d streams registered for user %d", instance->clients[i]->streams.size(), instance->clients[i]->uid);
			for(std::map<unsigned int, stream_register_t>::iterator it = instance->clients[i]->streams.begin(); it!=instance->clients[i]->streams.end(); it++)
			{
				Logger::log(LOG_VERBOSE, "sending stream registration %d:%d to user %d", instance->clients[i]->uid, it->first, uid);
				instance->clients[pos]->broadcaster->queueMessage(MSG2_STREAM_REGISTER, instance->clients[i]->uid, it->first, sizeof(stream_register_t), (char*)&it->second);
			}

		}
	}
}

int Sequencer::sendGameCommand(int uid, std::string cmd)
{
    STACKLOG;
    Sequencer* instance = Instance();
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return -1;
	// send
	const char *data = cmd.c_str();
	int size = cmd.size();
	// -1 = comes from the server
	instance->clients[pos]->broadcaster->queueMessage(MSG2_GAME_CMD, -1, 0, size, data);
	return 0;
}

// this does not lock the clients_mutex, make sure it is locked before hand
void Sequencer::serverSay(std::string msg, int uid, int type)
{
    STACKLOG;
    Sequencer* instance = Instance();
	if(type==0)
		msg = std::string("SERVER: ") + msg;

	for (int i = 0; i < (int)instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED &&
				instance->clients[i]->flow &&
				(uid==-1 || ((int)instance->clients[i]->uid) == uid))
			instance->clients[i]->broadcaster->queueMessage(MSG2_CHAT, -1, -1, (int)msg.size(), msg.c_str() );
	}
}

void Sequencer::serverSayThreadSave(std::string msg, int uid, int type)
{
    STACKLOG;
    Sequencer* instance = Instance();
    //MutexLocker scoped_lock(instance->clients_mutex);
	instance->serverSay(msg, uid, type);
}

bool Sequencer::kick(int kuid, int modUID, const char *msg)
{
    STACKLOG;
    Sequencer* instance = Instance();
    unsigned short pos = instance->getPosfromUid(kuid);
    if( UID_NOT_FOUND == pos ) return false;
    unsigned short posMod = instance->getPosfromUid(modUID);
    if( UID_NOT_FOUND == posMod ) return false;

	char kickmsg[1024] = "";
	strcat(kickmsg, "kicked by ");
	strcat(kickmsg, instance->clients[posMod]->nickname);
	if(msg)
	{
		strcat(kickmsg, ": ");
		strcat(kickmsg, msg);
	}
	Logger::log(LOG_VERBOSE, "player '%s' kicked by '%s'",instance->clients[pos]->nickname, instance->clients[posMod]->nickname);
	disconnect(instance->clients[pos]->uid, kickmsg);
	return true;
}

bool Sequencer::ban(int buid, int modUID, const char *msg)
{
    STACKLOG;
    Sequencer* instance = Instance();
    unsigned short pos = instance->getPosfromUid(buid);
    if( UID_NOT_FOUND == pos ) return false;
    unsigned short posMod = instance->getPosfromUid(modUID);
    if( UID_NOT_FOUND == posMod ) return false;
	SWBaseSocket::SWBaseError error;

	// construct ban data and add it to the list
	ban_t* b = new ban_t;
	memset(b, 0, sizeof(ban_t));

	b->uid = buid;
	if(msg) strncpy(b->banmsg, msg, 256);
	strncpy(b->bannedby_nick, instance->clients[posMod]->nickname, 20);
	strncpy(b->ip, instance->clients[pos]->sock->get_peerAddr(&error).c_str(), 16);
	strncpy(b->nickname, instance->clients[pos]->nickname, 20);
	Logger::log(LOG_DEBUG, "adding ban, size: %d", instance->bans.size());
	instance->bans.push_back(b);
	Logger::log(LOG_VERBOSE, "new ban added '%s' by '%s'", instance->clients[pos]->nickname, instance->clients[posMod]->nickname);

	char tmp[1024]="banned";
	if(msg)
	{
		strcat(tmp, ": ");
		strcat(tmp, msg);
	}
	return kick(buid, modUID, tmp);
}

bool Sequencer::unban(int buid)
{
    STACKLOG;
    Sequencer* instance = Instance();
	for (unsigned int i = 0; i < instance->bans.size(); i++)
	{
		if(((int)instance->bans[i]->uid) == buid)
		{
			instance->bans.erase(instance->bans.begin() + i);
			Logger::log(LOG_VERBOSE, "uid unbanned: %d", buid);
			return true;
		}
	}
	return false;
}

bool Sequencer::isbanned(const char *ip)
{
	if(!ip) return false;
    STACKLOG;
    Sequencer* instance = Instance();
	for (unsigned int i = 0; i < instance->bans.size(); i++)
	{
		if(!strcmp(instance->bans[i]->ip, ip))
			return true;
	}
	return false;
}

void Sequencer::streamDebug()
{
    STACKLOG;
    Sequencer* instance = Instance();

    //MutexLocker scoped_lock(instance->clients_mutex);

	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED)
		{
			Logger::log(LOG_VERBOSE, " * %d %s (slot %d):", instance->clients[i]->uid, instance->clients[i]->nickname, i);
			if(!instance->clients[i]->streams.size())
				Logger::log(LOG_VERBOSE, "  * no streams registered for user %d", instance->clients[i]->uid);
			else
				for(std::map<unsigned int, stream_register_t>::iterator it = instance->clients[i]->streams.begin(); it!=instance->clients[i]->streams.end(); it++)
				{
					char *types[] = {(char *)"truck", (char *)"character", (char *)"aitraffic", (char *)"chat"};
					char *typeStr = (char *)"unkown";
					if(it->second.type>=0 && it->second.type <= 3)
						typeStr = types[it->second.type];
					Logger::log(LOG_VERBOSE, "  * %d:%d, type:%s status:%d name:'%s'", instance->clients[i]->uid, it->first, typeStr, it->second.status, it->second.name);
				}
		}
	}
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int uid, int type, unsigned int streamid, char* data, unsigned int len)
{
	STACKLOG;
    Sequencer* instance = Instance();

    MutexLocker scoped_lock(instance->clients_mutex);
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;

	int publishMode=0;
	// publishMode = 0 no broadcast
	// publishMode = 1 broadcast to all clients except sender
	// publishMode = 2 broadcast to authed users (bots)
	// publishMode = 3 broadcast to all clients including sender

	if(type==MSG2_STREAM_DATA)
	{
		if(!instance->clients[pos]->initialized)
		{
			notifyAllVehicles(instance->clients[pos]->uid, false);
			instance->clients[pos]->initialized=true;
		}

		publishMode = 1;
	}
	else if (type==MSG2_STREAM_REGISTER)
	{
		if(instance->clients[pos]->streams.size() > 20)
		{
			Logger::log(LOG_VERBOSE, " * new stream registered dropped, too much streams for user: %d", instance->clients[pos]->uid);
			publishMode = 0; // drop
		}

		stream_register_t *reg = (stream_register_t *)data;
		Logger::log(LOG_VERBOSE, " * new stream registered: %d:%d, type: %d, name: '%s', status: %d", instance->clients[pos]->uid, streamid, reg->type, reg->name, reg->status);
		for(int i=0;i<128;i++) if(reg->name[i] == ' ') reg->name[i] = 0; // convert spaces to zero's
		reg->name[127] = 0;
		instance->clients[pos]->streams[streamid] = *reg;

		instance->streamDebug();

		// reset some stats
		// streams_traffic limited through streams map
		instance->clients[pos]->streams_traffic[streamid].bandwidthIncoming=0;
		instance->clients[pos]->streams_traffic[streamid].bandwidthIncomingLastMinute=0;
		instance->clients[pos]->streams_traffic[streamid].bandwidthIncomingRate=0;
		instance->clients[pos]->streams_traffic[streamid].bandwidthOutgoing=0;
		instance->clients[pos]->streams_traffic[streamid].bandwidthOutgoingLastMinute=0;
		instance->clients[pos]->streams_traffic[streamid].bandwidthOutgoingRate=0;

		publishMode = 1;
	}
	else if (type==MSG2_USE_VEHICLE)
	{
		Logger::log(LOG_VERBOSE,"MSG2_USE_VEHICLE is deprecated");
	}
#if 0
	else if (type==MSG2_VEHICLE_BEAMS)
	{
		// store beam info in memory
		if(len > sizeof(simple_beam_info_header) + sizeof(simple_beam_info) * 5000) // 5000 = MAX_BEAMS
		{
			// message too big!
			return;
		}
		instance->clients[pos]->beambuffersize = len;
		instance->clients[pos]->sbi = (simple_beam_info*)malloc(len);
		memcpy(instance->clients[pos]->sbi, data, len);

		Logger::log(LOG_VERBOSE,"Got beam data (%d bytes / %d kB) for slot %d: %s", instance->clients[pos]->beambuffersize, instance->clients[pos]->beambuffersize/1024, pos, instance->clients[pos]->vehicle_name);

		publishMode = 3;
	}
	else if (type==MSG2_REQUEST_VEHICLE_BEAMS)
	{
		// reply with beam info
		int uid_req = *((int*)data);
		int req_pos = instance->getPosfromUid(uid_req);
		if(req_pos != UID_NOT_FOUND)
		{
			if(!instance->clients[req_pos]->beambuffersize || !instance->clients[req_pos]->sbi)
			{
				// no data, send empty message
				Logger::log(LOG_VERBOSE,"Got beam data request from client %d for client %d. No Data, discarding request.", uid, uid_req);
				publishMode=0;
			} else
			{
				// send valid beam data
				Logger::log(LOG_VERBOSE,"Got beam data request from client %d for client %d. Valid data, sending response. (%d bytes / %d kB)", uid, uid_req, instance->clients[pos]->beambuffersize, instance->clients[pos]->beambuffersize/1024);
				int buf_size = instance->clients[pos]->beambuffersize;
				simple_beam_info *bbuf = instance->clients[pos]->sbi;
				// XXX: fix streams
				instance->clients[pos]->broadcaster->queueMessage(uid_req, MSG2_VEHICLE_BEAMS, (unsigned int)buf_size, (char*)bbuf);
				publishMode=0;
			}
		} else
		{
			// no data, send empty message
			Logger::log(LOG_VERBOSE,"Got beam data request from client %d for client %d. Requested Client unknown, discarding request.", uid, uid_req);
			publishMode=0;
		}
	}
#endif //0
	else if (type==MSG2_DELETE)
	{
		// from client
		Logger::log(LOG_INFO, "user %s disconnects on request", instance->clients[pos]->nickname);

		//char tmp[1024];
		//sprintf(tmp, "user %s disconnects on request", instance->clients[pos]->nickname);
		//serverSay(std::string(tmp), -1);
		disconnect(instance->clients[pos]->uid, "disconnected on request", false);
	}
	else if (type == MSG2_CHAT)
	{
		Logger::log(LOG_INFO, "CHAT| %s: %s", instance->clients[pos]->nickname, data);
		publishMode=3;

		// no broadcast of server commands!
		if(data[0] == '!') publishMode=0;

#ifdef WITH_ANGELSCRIPT
		if(instance->script)
		{
			int scriptpub = instance->script->playerChat(instance->clients[pos]->uid, data);
			if(scriptpub>0) publishMode = scriptpub;
		}
#endif //WITH_ANGELSCRIPT

		if(!strcmp(data, "!version"))
		{
			serverSay(std::string(VERSION), uid);
		}
		if(!strcmp(data, "!list"))
		{
			serverSay(std::string(" uid | auth   | nick                 | vehicle"), uid);
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				char authst[5] = "";
				if(instance->clients[i]->authstate & AUTH_ADMIN) strcat(authst, "A");
				if(instance->clients[i]->authstate & AUTH_MOD) strcat(authst, "M");
				if(instance->clients[i]->authstate & AUTH_RANKED) strcat(authst, "R");
				if(instance->clients[i]->authstate & AUTH_BOT) strcat(authst, "B");
				if(instance->clients[i]->authstate & AUTH_BANNED) strcat(authst, "X");\

				char tmp2[256]="";
				sprintf(tmp2, "% 3d | %-6s | %-20s | %s", instance->clients[i]->uid, authst, instance->clients[i]->nickname, instance->clients[i]->vehicle_name);
				serverSay(std::string(tmp2), uid);
			}

		}
		if(!strncmp(data, "!bans", 5))
		{
			serverSay(std::string("uid | IP              | nickname             | banned by"), uid);
			for (unsigned int i = 0; i < instance->bans.size(); i++)
			{
				char tmp[256]="";
				sprintf(tmp, "% 3d | %-15s | %-20s | %-20s", 
					instance->bans[i]->uid, 
					instance->bans[i]->ip, 
					instance->bans[i]->nickname, 
					instance->bans[i]->bannedby_nick);
				serverSay(std::string(tmp), uid);
			}
		}
		if(!strncmp(data, "!unban", 6) && strlen(data) > 7)
		{
			if(instance->clients[pos]->authstate & AUTH_MOD || instance->clients[pos]->authstate & AUTH_ADMIN)
			{
				int buid=-1;
				int res = sscanf(data+7, "%d", &buid);
				if(res != 1 || buid == -1)
				{
					serverSay(std::string("usage: !unban <uid>"), uid);
					serverSay(std::string("example: !unban 3"), uid);
				} else
				{
					if(unban(buid))
						serverSay(std::string("ban not removed: error"), uid);
					else
						serverSay(std::string("ban removed"), uid);
				}
			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to unban people!"), uid);
			}
		}
		if(!strncmp(data, "!ban ", 5) && strlen(data) > 5)
		{
			if(instance->clients[pos]->authstate & AUTH_MOD || instance->clients[pos]->authstate & AUTH_ADMIN)
			{
				int buid=-1;
				char banmsg_tmp[256]="";
				int res = sscanf(data+5, "%d %s", &buid, banmsg_tmp);
				std::string banMsg = std::string(banmsg_tmp);
				banMsg = trim(banMsg);
				if(res != 2 || buid == -1 || !banMsg.size())
				{
					serverSay(std::string("usage: !ban <uid> <message>"), uid);
					serverSay(std::string("example: !ban 3 swearing"), uid);
				} else
				{
					bool banned = ban(buid, uid, banMsg.c_str());
					if(!banned)
						serverSay(std::string("kick + ban not successful: uid not found!"), uid);
				}
			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to ban people!"), uid);
			}
		}
		if(!strncmp(data, "!kick ", 6) && strlen(data) > 6)
		{
			if(instance->clients[pos]->authstate & AUTH_MOD || instance->clients[pos]->authstate & AUTH_ADMIN)
			{
				int kuid=-1;
				char kickmsg_tmp[256]="";
				int res = sscanf(data+6, "%d %s", &kuid, kickmsg_tmp);
				std::string kickMsg = std::string(kickmsg_tmp);
				kickMsg = trim(kickMsg);
				if(res != 2 || kuid == -1 || !kickMsg.size())
				{
					serverSay(std::string("usage: !kick <uid> <message>"), uid);
					serverSay(std::string("example: !kick 3 bye!"), uid);
				} else
				{
					bool kicked  = kick(kuid, uid, kickMsg.c_str());
					if(!kicked)
						serverSay(std::string("kick not successful: uid not found!"), uid);
				}
			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to kick people!"), uid);
			}
		}

		// add to chat log
		{
			time_t lotime = time(NULL);
			char timestr[50];
			memset(timestr, 0, 50);
			ctime_r(&lotime, timestr);
			// remove trailing new line
			timestr[strlen(timestr)-1]=0;

			if(instance->chathistory.size() > 500)
				instance->chathistory.pop_front();
			chat_save_t ch;
			ch.msg = std::string(data);
			ch.nick = std::string(instance->clients[pos]->nickname);
			ch.source = instance->clients[pos]->uid;
			ch.time = std::string(timestr);
			instance->chathistory.push_back(ch);
		}
	}
	else if (type==MSG2_PRIVCHAT)
	{
		// private chat message
		int destuid = *(int*)data;
		int destpos = instance->getPosfromUid(destuid);
		if(destpos != UID_NOT_FOUND)
		{
			char *chatmsg = data + sizeof(int);
			int chatlen = len - sizeof(int);
			instance->clients[destpos]->broadcaster->queueMessage(uid, MSG2_CHAT, 1, chatlen, chatmsg);
			// use MSG2_PRIVCHAT later here maybe?
			publishMode=0;
		}
	}
	else if (type==MSG2_VEHICLE_DATA)
	{
#ifdef WITH_ANGELSCRIPT
		float* fpt=(float*)(data+sizeof(oob_t));
		instance->clients[pos]->position=Vector3(fpt[0], fpt[1], fpt[2]);
#endif //WITH_ANGELSCRIPT
		/*
		char hex[255]="";
		SHA1FromBuffer(hex, data, len);
		printf("R > %s\n", hex);

		std::string hexc = hexdump(data, len);
		printf("RH> %s\n", hexc.c_str());
		*/

		publishMode=1;
	}
#if 0
	else if (type==MSG2_FORCE)
	{
		//this message is to be sent to only one destination
		unsigned int destuid=((netforce_t*)data)->target_uid;
		for ( unsigned int i = 0; i < instance->clients.size(); i++)
		{
			if(i >= instance->clients.size())
				break;
			if (instance->clients[i]->status == USED &&
				instance->clients[i]->flow &&
				instance->clients[i]->uid==destuid)
				instance->clients[i]->broadcaster->queueMessage(
						instance->clients[pos]->uid, type, len, data);
		}
		publishMode=0;
	}
#endif //0
	if(publishMode>0)
	{
		instance->clients[pos]->streams_traffic[streamid].bandwidthIncoming += len;

		
		if(publishMode == 1 || publishMode == 3)
		{
			bool toAll = (publishMode == 3);
			// just push to all the present clients
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				if (instance->clients[i]->status == USED && instance->clients[i]->flow && (i!=pos || toAll))
				{
					instance->clients[i]->streams_traffic[streamid].bandwidthOutgoing += len;
					instance->clients[i]->broadcaster->queueMessage(type, instance->clients[pos]->uid, streamid, len, data);
				}
			}
		} else if(publishMode == 2)
		{
			// push to all bots and authed users above auth level 1
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				if (instance->clients[i]->status == USED && instance->clients[i]->flow && i!=pos && (instance->clients[i]->authstate & AUTH_ADMIN))
				{
					instance->clients[i]->streams_traffic[streamid].bandwidthOutgoing += len;
					instance->clients[i]->broadcaster->queueMessage(type, instance->clients[pos]->uid, streamid, len, data);
				}
			}
		}
	}
}

Notifier *Sequencer::getNotifier()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->notifier;
}


std::deque <chat_save_t> Sequencer::getChatHistory()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->chathistory;
}

std::vector<client_t> Sequencer::getClients()
{
    STACKLOG;
    Sequencer* instance = Instance();
	std::vector<client_t> res;
    MutexLocker scoped_lock(instance->clients_mutex);
	SWBaseSocket::SWBaseError error;

	for (unsigned int i = 0; i < instance->clients.size(); i++)
	{
		client_t c = *instance->clients[i];
		strcpy(c.ip_addr, instance->clients[i]->sock->get_peerAddr(&error).c_str());
		res.push_back(c);
	}
	return res;
}

int Sequencer::getStartTime()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->startTime;
}

client_t *Sequencer::getClient(int uid)
{
    STACKLOG;
	Sequencer* instance = Instance();

    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return 0;

	return instance->clients[pos];
}

void Sequencer::updateMinuteStats()
{
    STACKLOG;
    Sequencer* instance = Instance();
	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED)
		{
			for(std::map<unsigned int, stream_traffic_t>::iterator it = instance->clients[i]->streams_traffic.begin(); it!=instance->clients[i]->streams_traffic.end(); it++)
			{
				it->second.bandwidthIncomingRate = (it->second.bandwidthIncoming - it->second.bandwidthIncomingLastMinute)/60;
				it->second.bandwidthIncomingLastMinute = it->second.bandwidthIncoming;
				it->second.bandwidthOutgoingRate = (it->second.bandwidthOutgoing - it->second.bandwidthOutgoingLastMinute)/60;
				it->second.bandwidthOutgoingLastMinute = it->second.bandwidthOutgoing;
			}
		}
	}
}

// clients_mutex needs to be locked wen calling this method
void Sequencer::printStats()
{
    STACKLOG;
	if(!Config::getPrintStats()) return;
    Sequencer* instance = Instance();
	SWBaseSocket::SWBaseError error;
	{
		Logger::log(LOG_INFO, "Server occupancy:");

		Logger::log(LOG_INFO, "Slot Status   UID IP                  Colour, Nickname, Vehicle");
		Logger::log(LOG_INFO, "--------------------------------------------------");
		for (unsigned int i = 0; i < instance->clients.size(); i++)
		{
			// some auth identifiers
			char authst[5] = "";
			if(instance->clients[i]->authstate & AUTH_ADMIN) strcat(authst, "A");
			if(instance->clients[i]->authstate & AUTH_MOD) strcat(authst, "M");
			if(instance->clients[i]->authstate & AUTH_RANKED) strcat(authst, "R");
			if(instance->clients[i]->authstate & AUTH_BOT) strcat(authst, "B");
			if(instance->clients[i]->authstate & AUTH_BANNED) strcat(authst, "X");

			// construct screen
			if (instance->clients[i]->status == FREE)
				Logger::log(LOG_INFO, "%4i Free", i);
			else if (instance->clients[i]->status == BUSY)
				Logger::log(LOG_INFO, "%4i Busy %5i %-16s % 4s %d, %s, %s", i,
						instance->clients[i]->uid, "-",
						authst,
						instance->clients[i]->colournumber,
						instance->clients[i]->nickname,
						instance->clients[i]->vehicle_name);
			else
				Logger::log(LOG_INFO, "%4i Used %5i %-16s % 4s %d, %s, %s", i,
						instance->clients[i]->uid,
						instance->clients[i]->sock->get_peerAddr(&error).c_str(),
						authst,
						instance->clients[i]->colournumber,
						instance->clients[i]->nickname,
						instance->clients[i]->vehicle_name);
		}
		Logger::log(LOG_INFO, "--------------------------------------------------");
		int timediff = Messaging::getTime() - instance->startTime;
		int uphours = timediff/60/60;
		int upminutes = (timediff-(uphours*60*60))/60;
		stream_traffic_t traffic = Messaging::getTraffic();

		Logger::log(LOG_INFO, "- traffic statistics (uptime: %d hours, %d "
				"minutes):", uphours, upminutes);
		Logger::log(LOG_INFO, "- total: incoming: %0.2fMB , outgoing: %0.2fMB",
				traffic.bandwidthIncoming/1024/1024,
				traffic.bandwidthOutgoing/1024/1024);
		Logger::log(LOG_INFO, "- rate (last minute): incoming: %0.1fkB/s , "
				"outgoing: %0.1fkB/s",
				traffic.bandwidthIncomingRate/1024,
				traffic.bandwidthOutgoingRate/1024);
	}
}
// used to access the clients from the array rather than using the array pos it's self.
unsigned short Sequencer::getPosfromUid(unsigned int uid)
{
    STACKLOG;
    Sequencer* instance = Instance();

    for (unsigned short i = 0; i < instance->clients.size(); i++)
    {
        if(instance->clients[i]->uid == uid)
            return i;
    }

    Logger::log( LOG_DEBUG, "could not find uid %d", uid);
    return UID_NOT_FOUND;
}

void Sequencer::unregisterServer()
{
	if( Instance()->notifier )
		Instance()->notifier->unregisterServer();
}
