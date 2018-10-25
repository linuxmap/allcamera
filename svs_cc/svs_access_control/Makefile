#############################################################################
# Makefile for building: gessctrl
#############################################################################

C++             = g++ 
COMMON_DIR      = ../../svs_common/
NVS_HEADER_DIR  = ../../
BIN_DIR         = ../../build/bin/
EXTEND_ROOT     = ../../svs_extends/
EHOME_ROOT      = $(EXTEND_ROOT)ehome/
TARGET     = $(BIN_DIR)svs_access_control


INCPATH   += -I$(COMMON_DIR)common/  -I$(COMMON_DIR)common/buffer/ -I$(COMMON_DIR)common/config/  -I$(COMMON_DIR)common/log/
INCPATH   += -I$(COMMON_DIR)common/net/ -I$(COMMON_DIR)common/process/ -I$(COMMON_DIR)common/thread/ -I$(COMMON_DIR)common/timer/
INCPATH   += -I$(COMMON_DIR)common/util/ -I$(COMMON_DIR)common/hotlink/ 
INCPATH   += -I$(COMMON_DIR)common/  -I$(COMMON_DIR)/json/
INCPATH   += -I$(COMMON_DIR)util/common/ -I$(COMMON_DIR)util/time/
INCPATH   += -I$(COMMON_DIR)tinyxml/ -I$(COMMON_DIR) -I$(EXTEND_ROOT)vms/
INCPATH   += -I. -I$(EXTEND_ROOT)include/ -I$(NVS_HEADER_DIR)  -I$(COMMON_DIR)md5/ -I$(EHOME_ROOT)include/
INCPATH   += -I./inc/ -I./inc/controlstack -I./inc/access_control_manager -I./inc/device_stack -I/usr/include/x86_64-linux-gnu

LIBS      += -w -lpthread
LIBS      += -L${EXTEND_ROOT}lib/ -lACE -lvmsStack64 -L$(EHOME_ROOT)lib/ -lHCEHomeCMS -lhpr
LIBS      += $(EXTEND_ROOT)lib/libeXosip2.a $(EXTEND_ROOT)lib/libosip2.a $(EXTEND_ROOT)lib/libosipparser2.a -lresolv
LIBS      += $(EXTEND_ROOT)lib/libevent.a $(EXTEND_ROOT)lib/libevent_core.a $(EXTEND_ROOT)lib/libevent_extra.a $(EXTEND_ROOT)lib/libevent_pthreads.a

C++FLAGS  += -std=c++11 -pipe -g -Wall -O0 -DENV_LINUX -D_DEBUG -D_DEBUG_LOG -DDTRACE -DTRACE -DFILELOG -DHAS_REMOTE_DEBUG

CPPFILES += $(wildcard $(COMMON_DIR)common/buffer/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/buffer/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/buffer/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/hotlink/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/hotlink/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/hotlink/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)common/config/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/config/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/config/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)/md5/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)/md5/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)/md5/*.h)

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

CPPFILES += $(wildcard $(COMMON_DIR)common/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)common/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)common/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)aap/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)aap/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)aap/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)dbapi/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)dbapi/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)dbapi/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)record/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)record/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)record/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)tinyxml/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)tinyxml/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)tinyxml/*.h)

CPPFILES += $(wildcard $(COMMON_DIR)json/*.cpp)
CFILES += $(wildcard $(COMMON_DIR)json/*.c)
HEADFILES += $(wildcard $(COMMON_DIR)json/*.h)

CPPFILES += $(wildcard src/controlstack/*.cpp)
CFILES += $(wildcard src/controlstack/*.c)
HEADFILES += $(wildcard inc/controlstack/*.h)

CPPFILES += $(wildcard src/access_control_manager/*.cpp)
CFILES += $(wildcard src/access_control_manager/*.c)
HEADFILES += $(wildcard inc/access_control_manager/*.h)

CPPFILES += $(wildcard src/device_stack/*.cpp)
CFILES += $(wildcard src/device_stack/*.c)
HEADFILES += $(wildcard inc/device_stack/*.h)

CPPFILES += $(wildcard src/*.cpp)
CFILES += $(wildcard src/*.c)
HEADFILES += $(wildcard inc/*.h)


	   
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

