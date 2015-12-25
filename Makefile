CXXFLAGS = -ggdb3 -Wall -DBOOST_LOG_DYN_LINK -std=c++11
LDFLAGS = 
LDLIBS=-lboost_log -lboost_log_setup -lboost_program_options  -lboost_system -lpthread

nproxy_server: nproxy_server.o
	 $(CXX) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean

clean:
	    rm -f *~ *.o nproxy_server
