#############################################################################
# Makefile for building: gessctrl
#############################################################################

CC         = g++
LINK       = g++

ROOT_DIR  = ./
NSS_DIR   = ../../svs_common/
ACE_ROOT  = ../include/

C++FLAGS  += -pipe -g -Wall -fpic -O0 -m64 -DENV_LINUX
LIBS      += -shared -fPIC -m64 -lm -lpthread

TARGET_DIR = ../lib/
TARGET     = $(TARGET_DIR)libvmsStack64.so

INCPATH   += -I. -I$(NSS_DIR) -I$(ACE_ROOT)

CPPFILES  += $(wildcard $(ROOT_DIR)*.cpp)
CFILES    += $(wildcard $(ROOT_DIR)*.c)
HEADFILES += $(wildcard $(ROOT_DIR)*.h)
	     
COBJS     =  $(CFILES:.c=.o)
CPPOBJS   =  $(CPPFILES:.cpp=.O)

all: $(TARGET)
$(TARGET): $(CPPOBJS) $(COBJS)
	$(LINK) $(C++FLAGS) -o $@ $(CPPOBJS) $(COBJS) $(LIBS)
	
%.o : %.c $(HEADFILES)
	$(CC) -c  $(C++FLAGS) $(INCPATH) $< -o $@

%.O : %.cpp $(HEADFILES)
	$(CC) -c  $(C++FLAGS) $(INCPATH) $< -o $@	

clean:
	rm -f $(TARGET) $(CPPOBJS) $(COBJS)

