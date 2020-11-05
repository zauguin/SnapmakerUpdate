CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE
.PHONY: obj
obj: cleanobj
	mkdir obj
all: obj package update
update: obj fmt
	g++ $(CXXFLAGS) update.cpp -lfmt -o update
fmt: obj
	g++ format.cc -c -o obj\libfmt.o
	ar rcs obj\libfmt.a obj\libfmt.o
.PHONY: cleanobj
cleanobj: 
	rm -rf obj
.PHONY: clean
clean: cleanobj
	rm *.exe