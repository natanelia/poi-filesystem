all: main.cpp fuse-poi poi
	g++ main.cpp poi.o fuse_poi.o -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags --libs` -o mount-poi

poi : poi.h poi.cpp
	g++ -Wall -c poi.cpp -D_FILE_OFFSET_BITS=64

fuse-poi : fuse_poi.h fuse_poi.cpp
	g++ -Wall -c fuse_poi.cpp -D_FILE_OFFSET_BITS=64

clean:
	rm *~

clear:
	rm *.o

test : test/read.sh
	./test/read.sh
