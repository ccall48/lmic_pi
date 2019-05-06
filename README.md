# lmic_pi with a lmicd example which sends arbitrary bytes to TTN.

Raspberry Pi port LoRaMAC in C / LoRaWAN in C http://www.research.ibm.com/labs/zurich/ics/lrsc/lmic.html

This is a port of IBM's LMIC 1.5 to the Raspberry Pi using the wiringPi library for GPIO/SPI.
It is adapted for TTN (http://thethingsnetwork.org).

It has been tested with an HopeRF RFM95W chip, but should also work with a SX1272 or SX1276.

Some of the changes were taken from or inspired by the arduino-lmic-v1.5 port of tftelkamp (https://github.com/tftelkamp/arduino-lmic-v1.5.git) 

The connections of the pins are defined in the main programs in the examples directory.
Standard connections are:
  WiringPi 6  == nss
  
  not connected == rxtx: not used for RFM95
  
  WiringPi 0 == reset (needed for RFM92/RFM95)
  
  WiringPi 7,4,5 == dio0, dio1, dio2
  
  WiringPi 12 == MOSI
  
  WiringPi 13 == MISO
  
  WiringPi 14 == SCK
  
  GND  == GND
  
  3.3V  == +3.3V
 
This version has a new example called lmicd. This runs a mqtt server and receives strings from the ttn-send client program and sends them out during the upload cycle. To compile the sample you will need to mosquitto-dev package installed

An example run is this:

Start the lmic core and the mqtt server:
./lmicd -p 1883 &

Send strings to the mqtt broker:

./send-ttn -p 1883 -h 127.0.0.1 -a 70B3D57ED0012BD2 -d 0047F5BD541A3688 -n 760A9100DF266D853F42EAFE47B81530 -s 3BDCBA20FE2F99B9A1A2FAD989B0A520 -e 2602119A -x 010203040506

Args are:

p - port of mqtt server

h - host of mqtt server

a - appeui

d - deveui

n - network key (devkey)

s - session key (artkey)

e - devaddr of node

x - arbitrary string of hex bytes to send

Given the above input it should send the bytes 010203040506 to TTN.
