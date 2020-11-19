all:
CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE

# Comment this line to disable flashing support and don't require https://github.com/wjwwood/serial
HAS_SERIAL = 1

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

ifeq ($(HAS_SERIAL),1)
bootloader_interface.o: bootloader_interface.h
package$(EXE_EXTENSION) bootloader_driver$(EXE_EXTENSION): bootloader_interface.o
package$(EXE_EXTENSION) bootloader_driver$(EXE_EXTENSION): LDLIBS += -lserial
package$(EXE_EXTENSION): CXXFLAGS += -DHAS_SERIAL
EXECS += bootloader_driver
endif

EXECS := $(addsuffix $(EXE_EXTENSION),$(EXECS))

.PHONY: all clean
CXX = g++
all: $(EXECS)
update$(EXE_EXTENSION): LDLIBS += -lfmt
clean:
	-rm $(EXECS) *.o
