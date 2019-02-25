CFLAGS=-I../../lmic
LDFLAGS=-lwiringPi

join: main.c
	cd ../../lmic && $(MAKE)
	$(CC) $(CFLAGS) -o join main.c ../../lmic/*.o $(LDFLAGS)

all: join

.PHONY: clean

clean:
	rm -f *.o join
