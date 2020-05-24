TOP=$(shell pwd)
#CROSS=/opt/hisi-linux/x86-arm/arm-hisiv300-linux/target/bin/arm-hisiv300-linux-
#CROSS=arm-hisiv300-linux-

MKDIR_P = mkdir -p

OBJDIR = ./obj

CC=$(CROSS)gcc
CPP=$(CROSS)g++

#INCLUDE_DIRS := ./sys ./include ./inc ./inc_http ./inc_aio ./port
#SOURCE_DIRS  := . ./src ./lib_aio ./lib_http


INCLUDE_DIRS := . ./include
SOURCE_DIRS  := . ./src

CFLAGS   += -c -Wall -g -O0 -I$(APP_INC_DIR)
CPPFLAGS += -g -std=c++0x  -I$(APP_INC_DIR)

#DEFINES += DEBUG_INFO_DUMP
#DEFINES = DEBUG _DEBUG
#DEFINES = DEBUG_SEND_FILE
#DEFINES += DEBUG_LAST
#DEFINES += DEBUG_INFO_
#DEFINES += DEBUG_INFO
#DEFINES += DEBUG_GETPARAM

#LIBPATHS = ./libaio/libaio.so

LDFLAGS += -lm -lstdc++ -lpthread

#STATIC_LIBS = ./libhttp/libhttp.a


SOURCE_FILES := $(wildcard $(addsuffix /*.c,   $(SOURCE_DIRS) ) )
SOURCE_FILES += $(wildcard $(addsuffix /*.cpp, $(SOURCE_DIRS) ) )

OBJECT_FILES := $(notdir $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE_FILES))))

OBJ = $(addprefix $(OBJDIR)/, $(OBJECT_FILES))

EXECUTABLE=mpegdash

VPATH := $(SOURCE_DIRS)

.PHONY : all install clean

all: directories $(EXECUTABLE)

directories: ${OBJDIR}

${OBJDIR}:
	${MKDIR_P} ${OBJDIR}

$(EXECUTABLE): $(OBJECT_FILES)
	@echo "---------- Build ${EXECUTABLE} application"
	@$(CC) $(OBJ) -o $@ $(LDFLAGS) $(STATIC_LIBS) $(LIBPATHS)
	$(CROSS)strip mpegdash

%.o: %.cpp
	echo "---------- Build $<"
	$(CPP) $< -c $(CPPFLAGS) $(addprefix -I, $(INCLUDE_DIRS)) $(addprefix -I, $(SOURCE_DIRS)) $(addprefix -D,$(DEFINES)) -o ./obj/$@

%.o: %.c
	echo "---------- Build $<"
	$(CC) $< -c $(CFLAGS) $(addprefix -I, $(INCLUDE_DIRS)) $(addprefix -I, $(SOURCE_DIRS)) $(addprefix -D,$(DEFINES)) -o ./obj/$@

install:
	@echo -e ${BLUE}'Installing monitor...'${WHITE}
	install mpegdash $(APP_OUT_DIR)/bin

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f $(OBJ)


