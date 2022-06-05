CXXFLAGS=-std=c++2a -Wall -pedantic -Wformat
BIN=pedit
SRC=$(wildcard ./*.cpp)
OBJ=$(addsuffix .o,$(basename $(SRC)))

all: executable

debug: CXXFLAGS += -DDEBUG -g
debug: executable

executable: $(OBJ)
	$(CXX) -o $(BIN) $^ $(CXXFLAGS)

clean:
	rm -f ./*.o
	rm -f ./*.out
	rm -f ./$(BIN)
