FROM anotherfoxguy/ror-build-box

RUN mkdir -p /src/
WORKDIR /src/

COPY . /src/
RUN cmake -DRORSERVER_CRASHHANDLER:BOOL=ON -DRORSERVER_GUI:BOOL=OFF -DRORSERVER_WITH_ANGELSCRIPT:BOOL=ON -DRORSERVER_WITH_WEBSERVER:BOOL=ON .
RUN make -j2
