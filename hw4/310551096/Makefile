CCFLAGS = -std=c++14 -pedantic -pthread -lboost_system -O
CC_INCLUDE_DIRS = /usr/local/include
CC_INCLUDE_PARAMS = $(addprefix -I , $(CC_INCLUDE_DIRS))
CC_LIB_DIRS = /usr/local/lib
CC_LIB_PARAMS = $(addprefix -L , $(CC_LIB_DIRS))

all: socks_server.cpp 
	$ g++ socks_server.cpp $(CCFLAGS) $(CC_INCLUDE_PARAMS) $(CC_LIB_PARAMS) -o socks_server
	$ g++ console.cpp $(CCFLAGS) $(CC_INCLUDE_PARAMS) $(CC_LIB_PARAMS) -o hw4.cgi
 
clean:
	rm -f socks_server
	rm -f hw4.cgi