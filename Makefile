CXXFLAGS += -std=c++20 -Wall
all: package update
update: update.cpp -lfmt
