FROM alpine:latest

RUN apk add --no-cache git build-base cmake libuv-dev libressl-dev gcc g++ && \
    apk add --no-cache --repository http://dl-cdn.alpinelinux.org/alpine/edge/testing hwloc-dev && \
    rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/Bendr0id/xmrigCC.git && \
	cd xmrigCC && \
	cmake . -DWITH_ZLIB=ON -DWITH_CC_SERVER=OFF && \
	make 
	
COPY Dockerfile /Dockerfile

ENTRYPOINT  ["/xmrigCC/xmrigDaemon"]
