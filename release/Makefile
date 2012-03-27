INCLUDE=-Ihiddenfs-lib -Itaglib-1.7/include
PROJECT=mp3fs
OUTPUT=$(PROJECT).bin
CXX=g++
CXXFLAGS=-g -Wall
#OBJFILES=hiddenfs-lib/hiddenfs-lib.a taglib-1.7/taglib/taglib.a
OBJFILES=taglib-1.7/taglib/taglib.a structTable.o contentTable.o hiddenfs-lib.o

all: $(PROJECT)
#all: hiddenfs-lib $(PROJECT)

structTable.o: structTable.h structTable.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o structTable.o structTable.cpp

contentTable.o: contentTable.h contentTable.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o contentTable.o contentTable.cpp


hiddenfs-lib.o: hiddenfs-lib.h hiddenfs-lib.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o hiddenfs-lib.a hiddenfs-lib.cpp


taglib:
	cmake -DBUILD_SHARED_LIBS=OFF taglib-1.7/
	cd taglib-1.7; make

$(PROJECT): $(PROJECT).cpp hiddenfs-lib.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(OBJFILES) -o $(OUTPUT) $(PROJECT).cpp

clean:
	rm -f $(OUTPUT) *.o

