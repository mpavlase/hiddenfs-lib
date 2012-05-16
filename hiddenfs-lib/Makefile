# Makefile

# Project:  HiddenFS-lib
# Author:   Martin Pavlásek (xpavla07@stud.fit.vutbr.cz)
#

INCLUDE=

PROJECT=mp3fs
OUTPUT=$(PROJECT).bin

CXX=gcc
CXXFLAGS=-g -Wall -pedantic
CXXFLAGS=-g -std=c++0x -Wall
OBJFILES=structTable.o contentTable.o hiddenfs-lib.o hashTable.o common.o superBlock.o

.PHONY: doc clean

all: $(PROJECT)

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

superBlock.o: superBlock.h superBlock.cpp
	$(CXX) $(CXXFLAGS) `pkg-config fuse --cflags` $(INCLUDE) -c -o superBlock.o superBlock.cpp

$(PROJECT).o: $(PROJECT).cpp $(OBJFILES)
	$(CXX) $(CXXFLAGS) `pkg-config fuse --cflags` $(INCLUDE) -c -o $(PROJECT).o $(PROJECT).cpp

# `pkg-config taglib --libs` `pkg-config taglib --cflags`
$(PROJECT): $(PROJECT).cpp $(OBJFILES)
	$(CXX) $(CXXFLAGS) `pkg-config fuse --libs` `pkg-config fuse --cflags` -lcrypto++ -lid3 -lm $(INCLUDE) $(PROJECT).cpp $(OBJFILES) -o $(OUTPUT)
#	@echo "---"
#	@echo "Kompilace je OK."
###############################################

doc:
	doxygen

clean:
	rm -f *.bin *.o *.a
	rm -rf doc-api/html/*

