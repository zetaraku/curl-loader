# Output binary to be built
TARGET=curl-loader
TAGFILE=.tagfile

CURL_VER:=7.15.4
OBJ_DIR:=obj
SRC_SUFFIX:=c
OBJ:=$(patsubst %.$(SRC_SUFFIX), $(OBJ_DIR)/$(basename %).o, $(wildcard *.$(SRC_SUFFIX)))



# C compiler
CC=gcc

#C Compiler Flags
CFLAGS= -W -Wall -Wpointer-arith -pipe

#Debug flags. For optimized version remove -g and uncomment OPT_FLAGS below
# To build libcurl with optimization, go to build_curl.sh and follow instructions
DEBUG_FLAGS= -g

#Optimization flags
#OPT_FLAGS= -O3 -ffast-math -finline-functions -funroll-all-loops \
#	-finline-limit=800 -mmmx -msse -foptimize-sibling-calls -fomit-frame-pointer

#Linker mapping
LD=gcc

#Linker Flags
LDFLAGS=-L./curl-build/lib

#Link Libraries. RedHat/FC require sometimes lidn, remove first '#'
LIBS= -ldl -lpthread -lrt -lssl -lcrypto -lcurl -lz #-lidn #-lcares

# Include directories
INCDIR=-I. -I./curl-build/include -I./curl-$(CURL_VER)/lib

LIBCURL=libcurl

all: $(TARGET)

$(TARGET): $(LIBCURL) $(OBJ)
	$(LD) $(LDFLAGS) $(DEBUG_FLAGS) -o $@ $(OBJ) $(LIBS)

nobuildcurl: $(OBJ)
	$(LD) $(DEBUG_FLAGS) -o $(TARGET) $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) core*

cleanall: clean
	rm -rf curl ./curl-build ./curl-$(CURL_VER) $(TAGFILE) *~ *.log

tags:
	etags --members -o $(TAGFILE) *.h *.c

.PHONY: $(LIBCURL)
$(LIBCURL):
	./build_curl.sh

# Files types rules
.SUFFIXES: .o .c .h

*.o: *.h

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(OPT_FLAGS) $(DEBUG_FLAGS) $(INCDIR) -c -o $(OBJ_DIR)/$*.o $<
