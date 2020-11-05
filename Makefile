CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE
all: package update
update: update.cpp
	g++ $(CXXFLAGS)	update.cpp -lfmt -o update