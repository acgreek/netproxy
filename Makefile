CXXFLAGS = -ggdb3 -Wall  -std=c++11
LDFLAGS = 
LDLIBS= -lboost_system -lpthread

nproxy_server: nproxy_server.o
	 $(CXX) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean

clean:
	    rm -f *~ *.o nproxy_server
