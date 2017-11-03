CXXFLAGS = -std=gnu++0x -Wall
LDLIBS = -lstdc++ -l sqlite3
.PHONY: all clean test
all: bcwatch
clean:
	-rm  bcwatch *.o
	cd test; $(MAKE) clean

bcwatch: bcwatch.o easywsclient.o
bcwatch.o: bcwatch.cpp easywsclient.hpp
easywsclient.o: easywsclient.cpp easywsclient.hpp
