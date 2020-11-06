all:
CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE

ifeq ($(OS),Windows_NT)
EXE_EXTENSION = .exe
%: %.exe
	:
else
EXE_EXTENSION =
endif

EXES = package update
EXES := $(addsuffix $(EXE_EXTENSION),$(EXES))

.PHONY: all clean
CXX = g++
all: $(EXES)
update$(EXE_EXTENSION): update.cpp format.cc
clean:
	-rm $(EXES)
