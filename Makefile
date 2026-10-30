CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -Ibackend -Ithird_party
LDFLAGS   = -pthread
EXE       =
ifeq ($(OS),Windows_NT)
  CXXFLAGS += -D_WIN32_WINNT=0x0A00
  LDFLAGS   = -lws2_32 -lwsock32
  EXE       = .exe
endif
HDRS = $(wildcard backend/*/*.h)

all: netsim$(EXE)

netsim$(EXE): backend/main.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) backend/main.cpp -o $@ $(LDFLAGS)

run_tests$(EXE): tests/test_all.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) tests/test_all.cpp -o $@

test: run_tests$(EXE)
	./run_tests$(EXE)

run: netsim$(EXE)
	./netsim$(EXE)

clean:
	rm -f netsim netsim.exe run_tests run_tests.exe
.PHONY: all test run clean
