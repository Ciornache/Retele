CXX = g++
CXXFLAGS = -O2 
LDFLAGS = -ltinyxml2 -lsqlite3
SRC_CLIENT = client.cpp
SRC_SERVER = server.cpp
SRC_TIMESTAMP = timestamp.cpp 
SRC_DATABASE= database.cpp
SRC_TRAIN = train.cpp
SRC_ROAD = road.cpp

TARGETS = client server

all: $(TARGETS)

client: $(SRC_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^

server: $(SRC_SERVER) $(SRC_TIMESTAMP) $(SRC_TRAIN) $(SRC_ROAD) $(SRC_DATABASE)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean

clean:
	rm -f $(TARGETS)