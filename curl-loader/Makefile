# Output binary to be built
TARGET=curl-loader
TAGFILE=.tagfile

BUILD=$(shell pwd)/build

CURL_BUILD=$(BUILD)/curl
CURL_VER:=7.16.1

LIBEVENT_BUILD=$(BUILD)/libevent
LIBEVENT_VER:=1.3
LIBEVENT_MAKE_DIR=$(LIBEVENT_BUILD)/libevent-$(LIBEVENT_VER)


OBJ_DIR:=obj
SRC_SUFFIX:=c
OBJ:=$(patsubst %.$(SRC_SUFFIX), $(OBJ_DIR)/$(basename %).o, $(wildcard *.$(SRC_SUFFIX)))

debug ?= 1
optimize ?= 0

# C compiler
CC=gcc

#C Compiler Flags
CFLAGS= -W -Wall -Wpointer-arith -pipe -DCURL_LOADER_FD_SETSIZE=20000

#Debug flags
ifeq ($(debug),1)
DEBUG_FLAGS+= -g
else
DEBUG_FLAGS=
OPT_FLAGS+=-fomit-frame-pointer
endif

OPENSSLDIR=$(shell $(CURDIR)/openssldir.sh)

#Optimization flags
ifeq ($(optimize),1)
OPT_FLAGS+= -O3 -ffast-math -finline-functions -funroll-all-loops \
	-finline-limit=1000 -mmmx -msse -foptimize-sibling-calls
else
OPT_FLAGS=
endif


#Linker mapping
LD=gcc

#Linker Flags
LDFLAGS=-L./lib -L$(OPENSSLDIR)/lib

# Link Libraries. RedHat/FC require sometimes lidn
LIBS= -ldl -lpthread -lrt -lidn -lcurl -levent -lz -lssl -lcrypto 

# Include directories
INCDIR=-I. -I./inc -I$(OPENSSLDIR)/include

# 
LIBCURL=./lib/libcurl.a
LIBEVENT=./lib/libevent.a

all: $(TARGET)

$(TARGET): $(LIBCURL) $(LIBEVENT) $(OBJ)
	$(LD) $(DEBUG_FLAGS) $(OPT_FLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

nobuildcurl: $(OBJ)
	$(LD) $(DEBUG_FLAGS) $(OPT_FLAGS) -o $(TARGET) $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) core*

cleanall: clean
	rm -rf ./build ./packages/curl-$(CURL_VER) ./packages/curl ./inc ./lib ./bin $(TAGFILE) \
	*.log *.txt *.ctx *~ ./mconfig/*~ ./conf-examples/*~

tags:
	etags --members -o $(TAGFILE) *.h *.c

menuconfig:
	mkdir -p build; cd ./build; tar zxfv ../packages/dialog.tar.gz
	cd ./build/dialog; patch -p1 < ../../patches/dialog-curl-loader.patch
	make -C ./build/dialog/scripts/config
	cd ./conf-user; ../build/dialog/scripts/config/mconf ../mconfig/Config.in

$(LIBEVENT):
	mkdir -p $(LIBEVENT_BUILD)
	cd $(LIBEVENT_BUILD); tar zxfv ../../packages/libevent-$(LIBEVENT_VER).tar.gz;
	cd $(LIBEVENT_MAKE_DIR); ./configure --prefix $(LIBEVENT_BUILD) \
		CFLAGS="$(DEBUG_FLAGS) $(OPT_FLAGS)"
	make -C $(LIBEVENT_MAKE_DIR); make -C $(LIBEVENT_MAKE_DIR) install
	cp -pf $(LIBEVENT_BUILD)/include/*.h ./inc
	cp -pf $(LIBEVENT_BUILD)/lib/libevent.a ./lib


$(LIBCURL):
	cd ./packages; tar zxf curl-$(CURL_VER).tar.gz; ln -sf curl-$(CURL_VER) curl; \
	patch -d curl -p1 < ../patches/patch-curl-$(CURL_VER); \
	patch -d curl -p1 < ../patches/patch-fdset-protection; \
	patch -d curl -p1 < ../patches/curl-ipv6-bind.patch
	mkdir -p $(CURL_BUILD);
	cd $(CURL_BUILD); ../../packages/curl/configure --prefix $(CURL_BUILD) \
        --enable-thread \
        --enable-ipv6 \
        --with-random=/dev/urandom \
        --with-ssl=/usr/include/openssl \
        --enable-shared=no \
        CFLAGS="$(DEBUG_FLAGS) $(OPT_FLAGS)"
	make -C $(CURL_BUILD); make -C $(CURL_BUILD) install;
	mkdir -p ./inc; mkdir -p ./lib
	cp -a $(CURL_BUILD)/include/curl ./inc/curl
	cp -pf $(CURL_BUILD)/lib/libcurl* ./lib


# Files types rules
.SUFFIXES: .o .c .h

*.o: *.h

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(OPT_FLAGS) $(DEBUG_FLAGS) $(INCDIR) -c -o $(OBJ_DIR)/$*.o $<
