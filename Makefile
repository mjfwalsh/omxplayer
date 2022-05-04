CFLAGS+=-pipe -mfloat-abi=hard -mcpu=arm1176jzf-s -fomit-frame-pointer -mabi=aapcs-linux
CFLAGS+=-mtune=arm1176jzf-s -mfpu=vfp -Wno-psabi -g -std=c++0x -D__STDC_CONSTANT_MACROS
CFLAGS+=-D__STDC_LIMIT_MACROS -DTARGET_POSIX -DTARGET_LINUX -fPIC -DPIC -D_REENTRANT
CFLAGS+=-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CMAKE_CONFIG -D__VIDEOCORE4__
CFLAGS+=-U_FORTIFY_SOURCE -Wall -DOMX_SKIP64BIT -ftree-vectorize

LDFLAGS+=-L/opt/vc/lib -L./ $(addprefix -L,$(wildcard ffmpeg/lib[as]*))

LDLIBS+=-lasound -lavcodec -lavformat -lavutil -lbcm_host -lbrcmEGL -lbrcmGLESv2 -lc -lcairo
LDLIBS+=-ldbus-1 -ldl -lopenmaxil -lpcre2-8 -lpthread -lrt -lswresample -lswscale -lvchiq_arm
LDLIBS+=-lvchostif -lvcos -lz

INCLUDES+=-I./ -Ilinux -isystem ffmpeg -isystem/usr/include/dbus-1.0
INCLUDES+=-isystem/usr/lib/arm-linux-gnueabihf/dbus-1.0/include -isystem/usr/include/cairo
INCLUDES+=-isystem/opt/vc/include -isystem/opt/vc/include/interface/vcos/pthreads

DIST ?= omxplayer-dist
STRIP ?= strip

SRC=$(wildcard *.cpp linux/*.cpp utils/*.cpp)

OBJS=$(SRC:.cpp=.o)
DEPS=$(SRC:.cpp=.d)

# sets up ffmpeg-install requirement when ffmpeg source code dir is present
NEED_FFMPEG_INSTALL=$(shell test -d ffmpeg && echo ffmpeg-install)

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

omxplayer.o: version.h keys.h help.h

omxplayer.bin: $(OBJS)
	$(CXX) -o omxplayer.bin $(OBJS) $(LDFLAGS) $(LDLIBS)

help.h: README.md
	awk '/SYNOPSIS/{p=1;print;next} p&&/KEY BINDINGS/{p=0};p' $< \
	| sed -e '1,3 d' -e 's/^/"/' -e 's/$$/\\n"/' > $@

keys.h: README.md
	awk '/KEY BINDINGS/{p=1;print;next} p&&/KEY CONFIG/{p=0};p' $< \
	| sed -e '1,3 d' -e 's/^/"/' -e 's/$$/\\n"/' > $@

omxplayer.1: README.md
	sed -e '/This fork/,/sudo make install/ d; /DBUS/,$$ d' $< | \
	sed -r 's/\[(.*?)\]\(http[^\)]*\)/\1/g' > MAN
	curl -F page=@MAN http://mantastic.herokuapp.com 2>/dev/null >$@

.PHONY: clean
clean:
	rm -f $(OBJS) deps.mak help.h keys.h omxplayer.bin $(DIST).tgz version.h MAN omxplayer.1


.PHONY: ffmpeg
ffmpeg:
	$(MAKE) -f Makefile.ffmpeg

.PHONY: ffmpeg-%
ffmpeg-%:
	$(MAKE) -f Makefile.ffmpeg $(patsubst ffmpeg-%,%,$@)

.PHONY: dist
dist: $(NEED_FFMPEG_INSTALL)
	$(STRIP) omxplayer.bin
	tar -cPf $(DIST).tgz \
	--transform 's,^omxplayer$$,/usr/bin/omxplayer,S' \
	--transform 's,^omxplayer.bin$$,/usr/bin/omxplayer.bin,S' \
	--transform 's,^COPYING$$,/usr/share/doc/omxplayer/COPYING,S' \
	--transform 's,^README.md$$,/usr/share/doc/omxplayer/README,S' \
	--transform 's,^omxplayer.1$$,/usr/share/man/man1/omxplayer.1,S' \
	--transform 's,^ffmpeg_compiled/usr/local/lib/,/usr/lib/omxplayer/,S' \
	omxplayer omxplayer.bin COPYING README.md omxplayer.1 \
	`if [ -e ffmpeg_compiled ]; then echo ffmpeg_compiled/usr/local/lib/*.so*; fi`

.PHONY: install
install: $(NEED_FFMPEG_INSTALL)
	$(STRIP) omxplayer.bin
	cp omxplayer omxplayer.bin /usr/bin/
	mkdir -p /usr/share/doc/omxplayer
	cp COPYING /usr/share/doc/omxplayer/COPYING
	cp README.md /usr/share/doc/omxplayer/README
	cp omxplayer.1 /usr/share/man/man1
	if [ -e ffmpeg_compiled ]; then mkdir /usr/lib/omxplayer && \
	cp -P ffmpeg_compiled/usr/local/lib/*.so* /usr/lib/omxplayer/; fi

.PHONY: uninstall
uninstall:
	rm -f /usr/bin/omxplayer
	rm -f /usr/bin/omxplayer.bin
	rm -fR /usr/lib/omxplayer
	rm -fR /usr/share/doc/omxplayer
	rm -f /usr/share/man/man1/omxplayer.1

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
	$(CXX) -MM -MG -MF - $(CFLAGS) $(INCLUDES) -c $< >> deps.mak
