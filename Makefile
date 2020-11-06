all:
CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE

ifeq ($(OS),Windows_NT)
EXE_EXTENSION = .exe
%: %.exe
	:
LDLIBS += $(WINDIR)\system32\ws2_32.dll
else
EXE_EXTENSION =
endif

%$(EXE_EXTENSION): %.cpp
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@

EXECS = package update
EXECS := $(addsuffix $(EXE_EXTENSION),$(EXECS))

.PHONY: all clean
CXX = g++
all: $(EXECS)
update$(EXE_EXTENSION): LDLIBS += -lfmt
clean:
	-rm $(EXECS)
