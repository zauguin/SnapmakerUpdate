CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE
.PHONY: all obj cleanobj clean
all: obj package update
obj: cleanobj
	mkdir obj
update: obj fmt
	g++ $(CXXFLAGS) update.cpp -lfmt -o update
fmt: obj
	g++ format.cc -c -o obj\libfmt.o
	ar rcs obj\libfmt.a obj\libfmt.o
cleanobj: 
	rm -rf obj
clean: cleanobj
	rm *.exe