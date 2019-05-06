CFLAGS=-I../../lmic
LDFLAGS=-lwiringPi -lmosquitto

lmicd: lmicd.cpp
	cd ../../lmic && $(MAKE)
	$(CC) $(CFLAGS) -o lmicd lmicd.cpp ../../lmic/*.o $(LDFLAGS)

send-ttn: send-ttn.cpp
	$(CC) $(CFLAGS) -o send-ttn send-ttn.cpp -lmosquitto

all: lmicd send-ttn

.PHONY: clean

clean:
	rm -f *.o lmicd send-ttn
