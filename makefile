all: main.cpp poifs.o fuse_impl.o
	g++ main.cpp poifs.o fuse_impl.o -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags --libs` -o poifs

poifs.o : poifs.hpp poifs.cpp
	g++ -Wall -c poifs.cpp -D_FILE_OFFSET_BITS=64

fuse_impl.o : fuse_impl.hpp fuse_impl.cpp
	g++ -Wall -c fuse_impl.cpp -D_FILE_OFFSET_BITS=64

clean:
	rm *~

clear:
	rm *.o
