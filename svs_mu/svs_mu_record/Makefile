#############################################################################
# Makefile for building: gessctrl
#############################################################################

CC		= g++
C++             = g++

ROOT_DIR	= ../..
COMMON_DIR      = $(ROOT_DIR)/svs_common/
NVS_LIB_DIR     = $(ROOT_DIR)/svs_lib/
ACE_ROOT        = $(ROOT_DIR)/svs_extends/ACE_wrappers/
BIN_DIR         = $(ROOT_DIR)/build/bin

TARGET     = $(BIN_DIR)/svs_mu_record 

INCPATH   += -I./inc -I$(ACE_ROOT) -I$(COMMON_DIR) 
INCPATH   += -I$(COMMON_DIR)/common/ -I$(COMMON_DIR)/record/ 
INCPATH   += -I$(COMMON_DIR)common/buffer/ -I$(COMMON_DIR)common/config/  -I$(COMMON_DIR)common/log/
INCPATH   += -I$(COMMON_DIR)common/net/ -I$(COMMON_DIR)common/process/ -I$(COMMON_DIR)common/thread/ -I$(COMMON_DIR)common/timer/
INCPATH   += -I$(COMMON_DIR)common/util/ -I$(COMMON_DIR)md5/
INCPATH   += -I$(COMMON_DIR)/tinyxml/ -I$(COMMON_DIR)/vms/

LIBS      += -L$(NVS_LIB_DIR) -lACE -lpthread

C++FLAGS  += -pipe -g -Wall -O0 -DENV_LINUX

CPPFILES += $(wildcard $(COMMON_DIR)/common/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/buffer/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/buffer/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/buffer/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/config/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/config/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/config/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/log/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/log/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/log/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/net/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/net/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/net/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/process/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/process/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/process/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/thread/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/thread/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/thread/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/timer/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/timer/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/timer/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/util/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/util/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/util/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/aap/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/aap/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/aap/*.h)

#CPPFILES += $(wildcard $(COMMON_DIR)/dbapi/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/dbapi/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/dbapi/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/record/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/record/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/record/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/tinyxml/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/tinyxml/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/tinyxml/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/md5/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/md5/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/md5/*.h)

CPPFILES += $(wildcard src/*.cpp)
CFILES += $(wildcard src/*.c)
HEADFILES += $(wildcard inc/*.h)
	   
COBJS=$(CFILES:.c=.o)
CPPOBJS=$(CPPFILES:.cpp=.O)

all: $(TARGET)
$(TARGET): $(CPPOBJS) $(COBJS)
	$(C++) $(C++FLAGS) -o $@ $(CPPOBJS) $(COBJS) $(LIBS)
	
%.o : %.c 
	$(CC) -c  $(C++FLAGS) $(INCPATH) $< -o $@

%.O : %.cpp 
	$(C++) -c  $(C++FLAGS) $(INCPATH) $< -o $@	

clean:
	rm -f $(TARGET) $(CPPOBJS) $(COBJS)

