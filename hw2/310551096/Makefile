CC = g++
DBGFLAGS = -g -Wall

all: build

build: clean server1 server2 server3

server1: clean np_simple.cpp
	$(CC) np_simple.cpp -O -o np_simple

server2: clean np_single_proc.cpp
	$(CC) np_single_proc.cpp -O -o np_single_proc
 
server3: clean np_multi_proc.cpp
	$(CC) np_multi_proc.cpp -O -o np_multi_proc

build-dbg: clean server1 server2 server3

clean:
	rm -rf np_single np_single_proc np_multi_proc
