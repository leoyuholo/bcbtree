CC=g++

EXE= range_count

all:$(EXE)

range_count: bcbtree_range_count.o bcbtree.o gammaEliasCode.o disk.o
	g++ range_count.cpp -o range_count `pkg-config --cflags --libs gtk+-2.0` bcbtree_range_count.o bcbtree.o gammaEliasCode.o disk.o
	
%.o: bcbtree_range_count.h bcbtree.h gammaEliasCode.h disk.h

clean:
	rm -f *.o *.gch $(EXE) 