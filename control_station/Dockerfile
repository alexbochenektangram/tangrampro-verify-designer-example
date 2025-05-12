FROM tangramflex/pro-builder:latest

USER root

RUN mkdir /component && chown 1001 /component

USER 1001:1001

WORKDIR /component

COPY code-gen/ code-gen/
RUN cd code-gen && make clean && make -j && make install

COPY control_station/main.cpp /component/main.cpp
COPY control_station/Makefile /component/Makefile

ENV LD_LIBRARY_PATH=/component/code-gen/local_install/lib

RUN make -j

ENTRYPOINT ["./simple"]