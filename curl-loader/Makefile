# Output binary to be built
TARGET=curl-loader
TAGFILE=.tagfile

CURL_VER:=7.16.0
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
LDFLAGS=-L./lib  -L/usr/local/ssl/lib

#Link Libraries. RedHat/FC require sometimes lidn
LIBS= -ldl -lpthread -lrt -lidn -lssl -lcrypto -lcurl -lz#-lcares

# Include directories
INCDIR=-I. -I./inc -I/usr/local/ssl/include

LIBCURL=libcurl

all: $(TARGET)

$(TARGET): $(LIBCURL) $(OBJ)
	$(LD) $(DEBUG_FLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

nobuildcurl: $(OBJ)
	$(LD) $(DEBUG_FLAGS) -o $(TARGET) $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) core*

cleanall: clean
	rm -rf ./build ./packages/curl-$(CURL_VER) ./inc ./lib ./bin $(TAGFILE) *~ *.log *.txt *.ctx

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
