CFLAGS+=-pipe -mfloat-abi=hard -mcpu=arm1176jzf-s -fomit-frame-pointer -mabi=aapcs-linux
CFLAGS+=-mtune=arm1176jzf-s -mfpu=vfp -g -std=c++17 -D__STDC_CONSTANT_MACROS
CFLAGS+=-D__STDC_LIMIT_MACROS -DTARGET_POSIX -DTARGET_LINUX -fPIC -DPIC -D_REENTRANT
CFLAGS+=-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CMAKE_CONFIG -D__VIDEOCORE4__
CFLAGS+=-U_FORTIFY_SOURCE -Wall -DOMX_SKIP64BIT -ftree-vectorize

LDFLAGS+=-L/opt/vc/lib -L./ $(addprefix -L,$(wildcard ffmpeg/lib[as]*))

LDLIBS+=-lasound -lavcodec -lavformat -lavutil -lbcm_host -lbrcmEGL -lbrcmGLESv2 -lc -lcairo
LDLIBS+=-ldbus-1 -ldvdread -lopenmaxil -lpcre2-8 -lpthread -lrt -lswresample -lvchiq_arm
LDLIBS+=-lvchostif -lvcos -lz -lfreetype

INCLUDES+=-I./ -isystem ffmpeg -isystem/usr/include/dbus-1.0
INCLUDES+=-isystem/usr/lib/arm-linux-gnueabihf/dbus-1.0/include
INCLUDES+=-isystem/opt/vc/include -isystem/opt/vc/include/interface/vcos/pthreads
INCLUDES+=-isystem/usr/include/freetype2

DIST ?= omxplayer-dist
STRIP ?= strip

SRC=$(wildcard *.cpp linux/*.cpp utils/*.cpp)

OBJS=$(SRC:.cpp=.o)
DEPS=$(SRC:.cpp=.d)

.PHONY: all
all: omxplayer.bin omxplayer.1

# avoid including deps.mak for a make clean
ifneq ($(MAKECMDGOALS),clean)
include deps.mak
endif

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@

version.h:
	perl gen_version.pl

omxplayer.o: version.h

omxplayer.bin: $(OBJS)
	$(CXX) -o omxplayer.bin $(OBJS) $(LDFLAGS) $(LDLIBS)

omxplayer.1: omxplayer.pod
	pod2man -c 'multimedia' -r '' $< > $@

.PHONY: clean
clean:
	rm -f $(OBJS) deps.mak omxplayer.bin $(DIST).tgz version.h omxplayer.1

.PHONY: dist
dist:
	$(STRIP) omxplayer.bin
	tar -cPf $(DIST).tgz \
	--transform 's,^omxplayer$$,/usr/local/bin/omxplayer,S' \
	--transform 's,^omxplayer\.bin$$,/usr/local/bin/omxplayer.bin,S' \
	--transform 's,^COPYING$$,/usr/local/share/doc/omxplayer/COPYING,S' \
	--transform 's,^README\.md$$,/usr/local/share/doc/omxplayer/README,S' \
	--transform 's,^omxplayer\.1$$,/usr/local/share/man/man1/omxplayer.1,S' \
	--transform 's,^/opt/vc/lib/,/usr/local/lib/omxplayer/,S' \
	omxplayer omxplayer.bin COPYING README.md omxplayer.1 \
	/opt/vc/lib/libbrcmEGL.so /opt/vc/lib/libbrcmGLESv2.so /opt/vc/lib/libopenmaxil.so

.PHONY: install
install:
	$(STRIP) omxplayer.bin
	cp omxplayer omxplayer.bin /usr/local/bin/
	mkdir -p /usr/local/share/doc/omxplayer
	cp COPYING /usr/local/share/doc/omxplayer/COPYING
	cp README.md /usr/local/share/doc/omxplayer/README
	cp omxplayer.1 /usr/local/share/man/man1

.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/omxplayer
	rm -f /usr/local/bin/omxplayer.bin
	rm -fR /usr/local/share/doc/omxplayer
	rm -f /usr/local/share/man/man1/omxplayer.1

.PHONY: rm-dep-mak
rm-dep-mak:
	rm -f deps.mak

# only generate this file if it is missing
deps.mak: $(shell if [ ! -e deps.mak ]; then echo deps; fi )

.PHONY: deps
deps: $(DEPS)
	@true

.PHONY: %.d
%.d: %.cpp rm-dep-mak
	$(CXX) -MM -MG -MF - -MT $(@:.d=.o) $(CFLAGS) $(INCLUDES) -c $< >> deps.mak
