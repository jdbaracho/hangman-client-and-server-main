all: player GS

player: client.cpp
	g++ -std=c++11 -o player client.cpp

GS: server.cpp
	g++ -std=c++11 -o GS server.cpp

clean: 
	rm player GS