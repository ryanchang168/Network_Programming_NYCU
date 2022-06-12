CCFLAGS = -std=c++14 -pedantic -pthread -lboost_system -O
CC_INCLUDE_DIRS = /usr/local/include
CC_INCLUDE_PARAMS = $(addprefix -I , $(CC_INCLUDE_DIRS))
CC_LIB_DIRS = /usr/local/lib
CC_LIB_PARAMS = $(addprefix -L , $(CC_LIB_DIRS))

all: http_server.cpp 
	$ g++ http_server.cpp $(CCFLAGS) $(CC_INCLUDE_PARAMS) $(CC_LIB_PARAMS) -o http_server 
	$ g++ console.cpp $(CCFLAGS) $(CC_INCLUDE_PARAMS) $(CC_LIB_PARAMS) -o console.cgi
 
clean:
	rm -f http_server
	rm -f console.cgi