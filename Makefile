

LIB_WSCLIENT = libwsclient/.libs/libwsclient.a
LIB_CJSON = cJSON/libcjson.a
LIBS = -pthread
INCLUDES = -Ilibwsclient -IcJSON

VERSIONNUM = $(shell sed '/MajorVersion / ! d ; s/.*MajorVersion *"\(.*\)"/\1/ ; s/\.0/./' VersionNum)
CHANGENUM = $(shell git rev-list --count HEAD)
VERSION = ${VERSIONNUM}.${CHANGENUM}-1

all: riscos-build-online

.PHONY: deb build_deb clean

clean:
	-rm -rf *.o riscos-build-online
	-cd libwsclient && make clean
	-rm libwsclient/Makefile
	-rm libwsclient/configure
	-cd cJSON && make clean
	-rm -rf build-deb
	-rm -rf riscos-build-online_*.deb

libwsclient/configure:
	cd libwsclient && ./autogen.sh

libwsclient/Makefile: libwsclient/configure
	cd libwsclient && ./configure --enable-static
	cd libwsclient && make config.h
	sed 's!#define HAVE_LIBSSL!//#define HAVE_LIBSSL!' libwsclient/config.h > libwsclient/config.h.new && mv libwsclient/config.h.new libwsclient/config.h

${LIB_WSCLIENT}: libwsclient/Makefile
	cd libwsclient && make

${LIB_CJSON}:
	cd cJSON && make

OBJS = build.o base64_encode.o base64_decode.o

riscos-build-online: ${OBJS} ${LIB_WSCLIENT} ${LIB_CJSON}
	gcc -g ${LIBS} -o $@ ${OBJS} ${LIB_WSCLIENT} ${LIB_CJSON}

%.o: %.c
	gcc -g -O2 ${INCLUDES} -c -o $@ $<

build.o: ${LIB_WSCLIENT} VersionNum

deb: build_deb
	dpkg-deb --build build_deb riscos-build-online_${VERSION}.deb

build_deb: riscos-build-online
	-rm -rf build_deb
	mkdir -p build_deb
	mkdir -p build_deb/DEBIAN
	sed s/VERSION/${VERSION}/ < debian/control > build_deb/DEBIAN/control
	# Once built and linked, the user is bound by the GPL, because that's required by the libwsclient library.
	cp debian/copyright build_deb/DEBIAN/copyright
	mkdir -p build_deb/usr/bin
	cp riscos-build-online build_deb/usr/bin/riscos-build-online
