CXX=c++11
FLAGS=-Wall
LDLIBS=-lcurses
LDD=-pthread

all: server client

server: server.cpp
	$(CXX) $(LDD) -o game $(FLAGS) server.cpp $(LDLIBS)

client: client.cpp
	$(CXX) $(LDD) -o purple $(FLAGS) client.cpp $(LDLIBS)

