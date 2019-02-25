CFLAGS=-I../../lmic
LDFLAGS=-lwiringPi

thethingsnetwork-send-v1: thethingsnetwork-send-v1.cpp
	cd ../../lmic && $(MAKE)
	$(CC) $(CFLAGS) -o thethingsnetwork-send-v1 thethingsnetwork-send-v1.cpp ../../lmic/*.o $(LDFLAGS)

send-ttn: send-ttn.cpp
	$(CC) $(CFLAGS) -o send-ttn send-ttn.cpp

all: thethingsnetwork-send-v1 send-ttn

.PHONY: clean

clean:
	rm -f *.o thethingsnetwork-send-v1 send-ttn
