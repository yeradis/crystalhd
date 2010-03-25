CPP := g++
CPPFLAGS += -D__LINUX_USER__
LDFLAGS += -lcrystalhd -lpthread
INCLUDES += -I../include/ -I../linux_lib/libcrystalhd/

% : %.cpp
	$(CPP) $(INCLUDES) $(CPPFLAGS) $(LDFLAGS) -o $@ $<

all: hellobcm mpeg2test

clean:
	rm hellobcm mpeg2test
