#############################################################################
# Makefile for building: gessctrl
#############################################################################

C++             = g++ 
COMMON_DIR      = ../../svs_common/
EXTEND_DIR      = ../../svs_extends/
EHOME_ROOT      = $(EXTEND_DIR)ehome/
NVS_HEADER_DIR  = ../../
BIN_DIR         = ../../build/bin/
VMS_MSG_INC     = ./inc/vms_message/
VMS_MSG_SRC     = ./src/vms_message/
RTSP_SERVER_INC = ./inc/rtspserver/
RTSP_SERVER_SRC = ./src/rtspserver/
RTMP_SERVER_INC  = ./inc/rtmpserver/
RTMP_SERVER_SRC  = ./src/rtmpserver/
HLS_SERVER_INC  = ./inc/hlsserver/
HLS_SERVER_SRC  = ./src/hlsserver/
TARGET     = $(BIN_DIR)svs_mu_stream


INCPATH   += -I./inc/ -I$(EXTEND_DIR)include/ -I$(NVS_HEADER_DIR) -I$(COMMON_DIR) -I$(COMMON_DIR)common/
INCPATH   += -I$(COMMON_DIR)common/hotlink/ -I$(COMMON_DIR)md5/ -I$(COMMON_DIR)common/thread/
INCPATH   += -I$(COMMON_DIR)common/process/ -I$(COMMON_DIR)config/ -I$(COMMON_DIR)nat/
INCPATH   += -I$(EXTEND_DIR)vms/ -I$(EHOME_ROOT)include/
INCPATH   += -I$(VMS_MSG_INC) -I$(RTSP_SERVER_INC) -I$(HLS_SERVER_INC) -I$(RTMP_SERVER_INC)
INCPATH   += -I$(COMMON_DIR)/common/log/ -I$(COMMON_DIR)/common/config/ -I$(COMMON_DIR)/common/util/

LIBS      += -ldl -L$(EXTEND_DIR)lib/ -lACE -lpthread -lvmsStack64 -L$(EHOME_ROOT)lib/ -lHCEHomeStream -lhpr
LIBS      += -lm -lz 


C++FLAGS  += -pipe -g -Wall -O0 -DENV_LINUX -fPIC

CPPFILES += $(wildcard $(COMMON_DIR)/common/log/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/log/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/log/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/common/process/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/process/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/process/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/common/thread/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/thread/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/thread/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/common/config/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/config/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/config/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/md5/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/md5/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/md5/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/nat/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/nat/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/nat/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/common/hotlink/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/hotlink/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/hotlink/*.h)


CPPFILES += $(wildcard $(COMMON_DIR)/common/util/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/common/util/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/common/util/*.h)

CPPFILES += $(wildcard $(RTSP_SERVER_SRC)*.cpp)
CFILES += $(wildcard $(RTSP_SERVER_SRC)*.c)
HEADFILES += $(wildcard $(RTSP_SERVER_INC)*.h)

CPPFILES += $(wildcard $(RTMP_SERVER_SRC)*.cpp)
CFILES += $(wildcard $(RTMP_SERVER_SRC)*.c)
HEADFILES += $(wildcard $(RTMP_SERVER_INC)*.h)

CPPFILES += $(wildcard $(HLS_SERVER_SRC)*.cpp)
CFILES += $(wildcard $(HLS_SERVER_SRC)*.c)
HEADFILES += $(wildcard $(HLS_SERVER_INC)*.h)

CPPFILES += $(wildcard $(VMS_MSG_SRC)*.cpp)
CFILES += $(wildcard $(VMS_MSG_SRC)*.c)
HEADFILES += $(wildcard $(VMS_MSG_INC)*.h)

CPPFILES += $(wildcard ./src/*.cpp)
CFILES += $(wildcard ./src/*.c)
HEADFILES += $(wildcard ./inc/*.h)


	   
COBJS=$(CFILES:.c=.o)
CPPOBJS=$(CPPFILES:.cpp=.O)


all: $(TARGET)
$(TARGET): $(CPPOBJS) $(COBJS)
	$(C++) $(C++FLAGS) -o $@ $(CPPOBJS) $(COBJS) $(LIBS)
	
%.o : %.c $(HEADFILES)
	$(C++) -c  $(C++FLAGS) $(INCPATH) $< -o $@

%.O : %.cpp $(HEADFILES)
	$(C++) -c  $(C++FLAGS) $(INCPATH) $< -o $@	

clean:
	rm -f $(TARGET) $(CPPOBJS) $(COBJS)

