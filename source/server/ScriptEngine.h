/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifdef WITH_ANGELSCRIPT

#include "UnicodeStrings.h"
#include "CurlHelpers.h"
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include "angelscript.h"
#include "rornet.h"
#include "scriptmath3d/scriptmath3d.h" // angelscript addon

class ExampleFrameListener;

class GameScript;

class Beam;

class Sequencer;

/**
 * This struct holds the information for a script callback.
 */
struct callback_t {
    asIScriptObject *obj;  //!< The object instance that will need to be used with the function.
    asIScriptFunction *func; //!< The function or method pointer that will be called.
};
typedef std::vector<callback_t> callbackList;

class ScriptEngine {
public:
    enum class ThreadState
    {
        NOT_RUNNING,
        RUNNING,
        STOP_REQUESTED
    };

    ScriptEngine(Sequencer *seq);

    ~ScriptEngine();

    /// @name callbacks
    /// @{

    int loadScript(std::string scriptName);

    void playerDeleted(int uid, int crash, bool doNestedCall = false);

    void playerAdded(int uid);

    int streamAdded(int uid, RoRnet::StreamRegister *reg);

    int playerChat(int uid, std::string msg);

    void gameCmd(int uid, const std::string &cmd);

    /**
     * Params `n1`, `n2` and `message` depend on status type :
     * - CURL_STATUS_PROGRESS: n1 = bytes downloaded, n2 = total bytes, message = empty
     * - CURL_STATUS_SUCCESS: n1 = CURL return code, n2 = HTTP result code, message = payload as string
     * - CURL_STATUS_FAILURE: n1 = CURL return code, n2 = HTTP result code, message = CURL error string
     */
    void curlStatus(CurlStatusType type, int n1, int n2, std::string displayname, std::string message);

    int frameStep(float dt);

    /// @}

    /**
     * Gets the currently used AngelScript script engine.
     * @return a pointer to the currently used AngelScript script engine
     */
    asIScriptEngine *getEngine() { return engine; };

    /**
     * Sets an exception that aborts the currently running script and shows the exception in the log file.
     * @param message A descriptive error message.
     */
    void setException(const std::string &message);

    /**
     * Adds a script callback.
     * @param type The type of the callback. This can be one of the following: 'frameStep', 'playerChat', 'gameCmd', 'playerAdded', 'playerDeleted'.
     * @param func A pointer to a script function.
     * @param obj A pointer to the object of the method or NULL if func is a global function.
     */
    void addCallback(const std::string &type, asIScriptFunction *func, asIScriptObject *obj);

    /**
     * This method checks and converts the parameters and then adds a script callback.
     * @param type The type of the callback. \see addCallback
     * @param func The name of a script function.
     * @param obj A pointer to the object of the method or NULL if func is a global function.
     */
    void addCallbackScript(const std::string &type, const std::string &func, asIScriptObject *obj);

    /**
     * Deletes a script callback.
     * @param type The type of the callback. \see addCallback
     * @param func A pointer to a script function.
     * @param obj A pointer to the object of the method or NULL if func is a global function.
     */
    void deleteCallback(const std::string &type, asIScriptFunction *func, asIScriptObject *obj);

    /**
     * This method checks and converts the parameters and then deletes a script callback.
     * @param type The type of the callback. \see addCallback
     * @param func The name of a script function.
     * @param obj A pointer to the object of the method or NULL if func is a global function.
     */
    void deleteCallbackScript(const std::string &type, const std::string &_func, asIScriptObject *obj);

    /**
     * Deletes all script callbacks.
     */
    void deleteAllCallbacks();

    /**
     * This checks if a script callback exists.
     * @param type The type of the callback. \see addCallback
     * @param func A pointer to a script function.
     * @param obj A pointer to the object of the method or NULL if func is a global function.
     * @return true if the callback exists
     */
    bool callbackExists(const std::string &type, asIScriptFunction *func, asIScriptObject *obj);

    // Timer thread control
    void        EnsureTimerThreadRunning();
    void        StopTimerThread();
    ThreadState GetTimerThreadState();

protected:
    Sequencer *seq;
    asIScriptEngine *engine;                //!< instance of the scripting engine
    asIScriptContext *context;              //!< context in which all scripting happens
    std::map<std::string, callbackList> callbacks; //!< A map containing the script callbacks by type.

    // Timer thread context
    std::thread m_timer_thread;
    ThreadState m_timer_thread_state = ThreadState::NOT_RUNNING;
    std::mutex  m_timer_thread_mutex;

    /**
     * This function initialzies the engine and registeres all types
     */
    void init();

    /**
     * This is the callback function that gets called when script error occur.
     * When the script crashes, this function will provide you with more detail
     * @param msg arguments that contain details about the crash
     * @param param unkown?
     */
    void msgCallback(const asSMessageInfo *msg);

    /**
     * This function reads a file into the provided string.
     * @param filename filename of the file that should be loaded into the script string
     * @param script reference to a string where the contents of the file is written to
     * @return 0 on success, everything else on error
     */
    int loadScriptFile(const char *fileName, std::string &script);

    /**
     * This callback gets called when an exception occurs in the script.
     * It logs the exception message together with the place in the script where the error occurs.
     * @param ctx The context in which the exception ocurred.
     * @param param An unused parameter.
     */
    void ExceptionCallback(asIScriptContext *ctx, void *param);

    /**
     * This logs all variables and their values at the specified stack level.
     * @param ctx The context that should be used.
     * @param stackLevel A number representing the level in the stack that should be logged.
     */
    void PrintVariables(asIScriptContext *ctx, int stackLevel);

    /**
     * unused
     */
    void LineCallback(asIScriptContext *ctx, void *param);

    /**
     * A loop that periodically the frameStep() script callback.
     */
    void TimerThreadMain();
};

class ServerScript {
protected:
    ScriptEngine *mse;              //!< local script engine pointer, used as proxy mostly
    Sequencer *seq;     //!< local pointer to the main ExampleFrameListener, used as proxy mostly

public:
    ServerScript(ScriptEngine *se, Sequencer *seq);

    ~ServerScript();

    void log(std::string &msg);

    void say(std::string &msg, int uid = -1, int type = 0);

    void kick(int kuid, std::string &msg);

    void ban(int kuid, std::string &msg);

    bool unban(int kuid);

    int playerChat(int uid, char *str);

    std::string getServerTerrain();

    int sendGameCommand(int uid, std::string cmd);

    std::string getUserName(int uid);

    void setUserName(int uid, const std::string &username);

    std::string getUserAuth(int uid);

    int getUserAuthRaw(int uid);

    void setUserAuthRaw(int uid, int authmode);

    int getUserColourNum(int uid);

    void setUserColourNum(int uid, int num);

    std::string getUserToken(int uid);

    std::string getUserVersion(int uid);

    std::string getUserIPAddress(int uid);

    int getUserPosition(int uid, Vector3 &v);

    int getNumClients();

    int getStartTime();

    int getTime();

    std::string get_version();

    std::string get_asVersion();

    std::string get_protocolVersion();

    void setCallback(const std::string &type, const std::string &func, void *obj, int refTypeId);

    void deleteCallback(const std::string &type, const std::string &func, void *obj, int refTypeId);

    void throwException(const std::string &message);

    unsigned int get_maxClients();

    std::string get_serverName();

    std::string get_IPAddr();

    unsigned int get_listenPort();

    int get_serverMode();

    std::string get_owner();

    std::string get_website();

    std::string get_ircServ();

    std::string get_voipServ();

    int rangeRandomInt(int from, int to);

    void broadcastUserInfo(int uid);

    /**
     * Launches a background task, use `curlStatus` callback to monitor progress and receive result.
     * @param displayname The "correlation ID" - the label passed to the callback to identify the transfer.
     * @remark Callback signature: `curlStatus(curlStatusType, int n1, int n2, string displayname, string message)`
     * - CURL_STATUS_PROGRESS: n1 = bytes downloaded, n2 = total bytes, message = empty
     * - CURL_STATUS_SUCCESS: n1 = CURL return code, n2 = HTTP result code, message = payload as string
     * - CURL_STATUS_FAILURE: n1 = CURL return code, n2 = HTTP result code, message = CURL error string
     */
    void curlRequestAsync(std::string url, std::string displayname);
};

#endif // WITH_ANGELSCRIPT

