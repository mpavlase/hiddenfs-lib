#
# @author Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
#

INCLUDE=

PROJECT=mp3fs
OUTPUT=$(PROJECT).bin

CXX=gcc
CXXFLAGS=-g -Wall -pedantic
CXXFLAGS=-g -std=c++0x -Wall
OBJFILES=structTable.o contentTable.o hiddenfs-lib.o hashTable.o common.o

.PHONY: all valgrind clean mount umount doc

all: $(PROJECT)

# taglib:
#	cmake taglib-1.7/
#	make -f taglib-1.7/Makefile

structTable.o: structTable.h structTable.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o structTable.o structTable.cpp

contentTable.o: contentTable.h contentTable.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o contentTable.o contentTable.cpp

hashTable.o: hashTable.h hashTable.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o hashTable.o hashTable.cpp

common.o: common.h common.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o common.o common.cpp

hiddenfs-lib.o: hiddenfs-lib.h hiddenfs-lib.cpp
	$(CXX) $(CXXFLAGS) `pkg-config fuse --cflags` $(INCLUDE) -c -o hiddenfs-lib.o hiddenfs-lib.cpp

$(PROJECT).o: $(PROJECT).cpp $(OBJFILES)
	$(CXX) $(CXXFLAGS) `pkg-config fuse --cflags` $(INCLUDE) -c -o $(PROJECT).o $(PROJECT).cpp

# $(PROJECT): $(PROJECT).o $(OBJFILES)
#	$(CXX) $(CXXFLAGS) `pkg-config fuse --libs` -lcrypto++ $(INCLUDE) $(PROJECT).o $(OBJFILES) -o $(OUTPUT)

$(PROJECT): $(PROJECT).cpp $(OBJFILES)
	$(CXX) $(CXXFLAGS) `pkg-config fuse --libs` `pkg-config fuse --cflags` -lcrypto++ `pkg-config taglib --libs` `pkg-config taglib --cflags` -lm $(INCLUDE) $(PROJECT).cpp $(OBJFILES) -o $(OUTPUT)
	@echo "---"
	@echo "Kompilace je OK."
###############################################
valgrind: all
	valgrind -v --leak-check=full --show-reachable=yes ./$(OUTPUT) -d ./vfs

mount: all
	./$(OUTPUT) -d vfs/

debug: all umount
	gdb -x list.gdb --args ./$(OUTPUT) -d ./vfs

umount:
	./umount.sh

run: umount all mount
	@echo "mp3fs je už nyní odpojený."

doc:
	doxygen

clean:
	rm -f *.bin *.o *.a
	rm -rf doc/html/*

