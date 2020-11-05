CXXFLAGS += -std=c++20 -Wall -D_DEFAULT_SOURCE
.PHONY: obj
obj:
	mkdir obj
all: obj package update
update: obj fmt
	g++ $(CXXFLAGS) update.cpp -lfmt -o update
fmt: obj
	g++ format.cc -c -o obj\libfmt.o
	ar rcs obj\libfmt.a obj\libfmt.o
.PHONY: clean
clean:
	rm -rf obj
	rm *.exe