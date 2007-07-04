# Output binary to be built
TARGET=curl-loader
TAGFILE=.tagfile

BUILD=$(shell pwd)/build

#
# Building of DNS asynch resolving c-ares library.
#
#CARES_BUILD=$(BUILD)/c-ares
#CARES_VER:=1.4
#CARES_MAKE_DIR=$(CARES_BUILD)/c-ares-$(CARES_VER)

CURL_BUILD=$(BUILD)/curl
CURL_VER:=7.16.3

LIBEVENT_BUILD=$(BUILD)/libevent
LIBEVENT_VER:=1.3
LIBEVENT_MAKE_DIR=$(LIBEVENT_BUILD)/libevent-$(LIBEVENT_VER)

OBJ_DIR:=obj
SRC_SUFFIX:=c
OBJ:=$(patsubst %.$(SRC_SUFFIX), $(OBJ_DIR)/$(basename %).o, $(wildcard *.$(SRC_SUFFIX)))

OPENSSLDIR=$(shell $(CURDIR)/openssldir.sh)

# C compiler
CC=gcc

#C Compiler Flags
CFLAGS= -W -Wall -Wpointer-arith -pipe \
	-DCURL_LOADER_FD_SETSIZE=20000 \
	-D_FILE_OFFSET_BITS=64

#
# Making options: e.g. $make optimize=1 debug=0 profile=1 
#
debug ?= 1
optimize ?= 0
profile ?= 0

#Debug flags
ifeq ($(debug),1)
DEBUG_FLAGS+= -g
else
DEBUG_FLAGS=
ifeq ($(profile),0)
OPT_FLAGS+=-fomit-frame-pointer
endif
endif

#Optimization flags
ifeq ($(optimize),1)
OPT_FLAGS+= -O3 -ffast-math -finline-functions -funroll-all-loops \
	-finline-limit=1000 -mmmx -msse -foptimize-sibling-calls
else
OPT_FLAGS=
endif

# CPU-tuning flags for Pentium-4 arch.
#OPT_FLAGS+= -mtune=pentium4 -mcpu=pentium4

#Profiling flags
ifeq ($(profile),1)
PROF_FLAG=-pg
else
PROF_FLAG=
endif


#Linker mapping
LD=gcc

#Linker Flags
LDFLAGS=-L./lib -L$(OPENSSLDIR)/lib

# Link Libraries. RedHat/FC require sometimes lidn
LIBS= -ldl -lpthread -lrt -lidn -lcurl -levent -lz -lssl -lcrypto #-lcares 

# Link Libraries. Some other distributions do not need -lidn
LIBS= -ldl -lpthread -lrt -lcurl -levent -lz -lssl -lcrypto #-lcares 

# Include directories
INCDIR=-I. -I./inc -I$(OPENSSLDIR)/include

# Targets
#LIBCARES:=./lib/libcares.a
LIBCURL:=./lib/libcurl.a
LIBEVENT:=./lib/libevent.a

# documentation directory
DOCDIR=/usr/share/doc/curl-loader/

# manual page directory
MANDIR=/usr/share/man

all: $(TARGET)

$(TARGET): $(LIBCURL) $(LIBEVENT) $(OBJ)
	$(LD) $(PROF_FLAG) $(DEBUG_FLAGS) $(OPT_FLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

nobuildcurl: $(OBJ)
	$(LD) $(PROF_FLAG) $(DEBUG_FLAGS) $(OPT_FLAGS) -o $(TARGET) $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) core*

cleanall: clean
	rm -rf ./build ./packages/curl-$(CURL_VER) \
	./packages/curl ./inc ./lib ./bin $(TAGFILE) \
	*.log *.txt *.ctx *~ ./mconfig/*~ ./conf-examples/*~

tags:
	etags --members -o $(TAGFILE) *.h *.c

install:
	cp -f ./curl-loader /usr/bin
	cp -f doc/curl-loader.1 $(MANDIR)/man1/  
	cp -f doc/curl-loader-config.5 $(MANDIR)/man5/
	@if [ ! -d $(DOCDIR) ]; then mkdir -p $(DOCDIR) ; fi
	cp -f doc/* $(DOCDIR) 
	cp -rf conf-examples $(DOCDIR)

$(LIBEVENT):
	mkdir -p $(LIBEVENT_BUILD)
	cd $(LIBEVENT_BUILD); tar zxfv ../../packages/libevent-$(LIBEVENT_VER).tar.gz;
	cd $(LIBEVENT_MAKE_DIR); patch -p1 < ../patches/libevent-nevent.patch; ./configure --prefix $(LIBEVENT_BUILD) \
		CFLAGS="$(PROF_FLAG) $(DEBUG_FLAGS) $(OPT_FLAGS)"
	make -C $(LIBEVENT_MAKE_DIR); make -C $(LIBEVENT_MAKE_DIR) install
	mkdir -p ./inc; mkdir -p ./lib
	cp -pf $(LIBEVENT_BUILD)/include/*.h ./inc
	cp -pf $(LIBEVENT_BUILD)/lib/libevent.a ./lib

#$(LIBCARES):
#	mkdir -p $(CARES_BUILD)
#	cd $(CARES_BUILD); tar zxf ../../packages/c-ares-$(CARES_VER).tar.gz;
#	cd $(CARES_MAKE_DIR); ./configure --prefix $(CARES_MAKE_DIR) \
#		CFLAGS="$(PROF_FLAG) $(DEBUG_FLAGS) $(OPT_FLAGS)"
#	make -C $(CARES_MAKE_DIR); make -C $(CARES_MAKE_DIR) install
#	mkdir -p ./inc; mkdir -p ./lib
#	cp -pf $(CARES_MAKE_DIR)/include/*.h ./inc
#	cp -pf $(CARES_MAKE_DIR)/lib/libcares* ./lib

$(LIBCURL):
	cd ./packages; tar zxf curl-$(CURL_VER).tar.gz; ln -sf curl-$(CURL_VER) curl; \
	patch -d curl -p1 < ../patches/curl-hash.patch; \
	patch -d curl -p1 < ../patches/curl-trace-info-error.patch
	mkdir -p $(CURL_BUILD);
	cd $(CURL_BUILD); ../../packages/curl/configure --prefix=$(CURL_BUILD) \
	--enable-ipv6 \
        --enable-thread \
        --with-random=/dev/urandom \
        --with-ssl=/usr/include/openssl \
        --enable-shared=no \
        CFLAGS="$(PROF_FLAG) $(DEBUG_FLAGS) $(OPT_FLAGS) -DCURL_MAX_WRITE_SIZE=8192"
	make -C $(CURL_BUILD); make -C $(CURL_BUILD) install;
	mkdir -p ./inc; mkdir -p ./lib
	cp -a $(CURL_BUILD)/include/curl ./inc/curl
	cp -pf $(CURL_BUILD)/lib/libcurl* ./lib

#--enable-ares=$(CARES_MAKE_DIR)

# Files types rules
.SUFFIXES: .o .c .h

*.o: *.h

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(PROF_FLAG) $(OPT_FLAGS) $(DEBUG_FLAGS) $(INCDIR) -c -o $(OBJ_DIR)/$*.o $<
