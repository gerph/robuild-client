

LIB_WSCLIENT = libwsclient/.libs/libwsclient.a
LIB_CJSON = cJSON/libcjson.a
INCLUDES = -Ilibwsclient -IcJSON

all: build

libwsclient/configure:
	cd libwsclient && ./autogen.sh

libwsclient/Makefile: libwsclient/configure
	cd libwsclient && ./configure --enable-static
	sed 's!#define HAVE_LIBSSL!//#define HAVE_LIBSSL!' libwsclient/config.h > libwsclient/config.h.new && mv libwsclient/config.h.new libwsclient/config.h

${LIB_WSCLIENT}: libwsclient/Makefile
	cd libwsclient && make

${LIB_CJSON}:
	cd cJSON && make

OBJS = build.o base64_encode.o base64_decode.o

build: ${OBJS} ${LIB_WSCLIENT} ${LIB_CJSON}
	gcc -g -o $@ ${OBJS} ${LIB_WSCLIENT} ${LIB_CJSON}

%.o: %.c
	gcc -g -O2 ${INCLUDES} -c -o $@ $<
