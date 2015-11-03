CXXFLAGS = -ggdb3 -Wall 
LDFLAGS = 
LDLIBS= -lboost_system

nproxy_server: nproxy_server.o
	 $(CXX) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean

clean:
	    rm -f *~ *.o nproxy_server
