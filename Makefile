CXXFLAGS=-std=c++2a -Wall -pedantic -Wformat -Werror $(EXTRA_FLAGS)

BIN=pedit
SRC=$(wildcard ./*.cpp)
SRC:=$(filter-out ./test.cpp, $(SRC))
OBJ=$(addsuffix .o,$(basename $(SRC)))

TEST_BIN=test
TEST_SRC=$(wildcard ./*.cpp)
TEST_SRC:=$(filter-out ./pedit.cpp, $(TEST_SRC))
TEST_OBJ=$(addsuffix .o,$(basename $(TEST_SRC)))

all: executable

runall: all
	./$(BIN)

tests: test_executable

runtests: tests
	./$(TEST_BIN)

debug: CXXFLAGS += -DDEBUG -g3 -D_GLIBCXX_DEBUG
debug: executable

release: CXXFLAGS += -O3 -g0
release: executable

executable: $(OBJ)
	$(CXX) -o $(BIN) $^ $(CXXFLAGS)

test_executable: $(TEST_OBJ)
	$(CXX) -o $(TEST_BIN) $^ $(CXXFLAGS)

clean:
	rm -f ./*.o
	rm -f ./*.out
	rm -f ./$(BIN)
	rm -f ./$(TEST_BIN)
