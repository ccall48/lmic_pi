/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello, world!", that
 * will be processed by The Things Network server.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in g1,
 *  0.1% in g2).
 *
 * Change DEVADDR to a unique address!
 * See http://thethingsnetwork.org/wiki/AddressSpace
 *
 * Do not forget to define the radio type correctly in config.h, default is:
 *   #define CFG_sx1272_radio 1
 * for SX1272 and RFM92, but change to:
 *   #define CFG_sx1276_radio 1
 * for SX1276 and RFM95.
 *
 *******************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <wiringPi.h>
#include <lmic.h>
#include <hal.h>
#include <local_hal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <fcntl.h> /* Added for the nonblocking socket */

// LoRaWAN Application identifier (AppEUI)
// Not used in this example
static u1_t APPEUI[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN DevEUI, unique device ID (LSBF)
// Not used in this example
static u1_t DEVEUI[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN NwkSKey, network session key
// Use this key for The Things Network
static u1_t DEVKEY[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN AppSKey, application session key
// Use this key to get your data decrypted by The Things Network
static u1_t ARTKEY[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
static u4_t DEVADDR = 0x0;

static int server_fd;

int parse_buffer(char * buffer);

//////////////////////////////////////////////////
// APPLICATION CALLBACKS
//////////////////////////////////////////////////

// provide application router ID (8 bytes, LSBF)
void os_getArtEui(u1_t* buf) {
	memcpy(buf, APPEUI, 8);
}

// provide device ID (8 bytes, LSBF)
void os_getDevEui(u1_t* buf) {
	memcpy(buf, DEVEUI, 8);
}

// provide device key (16 bytes)
void os_getDevKey(u1_t* buf) {
	memcpy(buf, DEVKEY, 16);
}

u4_t cntr = 0;
u1_t mydata[255];
static int mydatalen = 0;

static osjob_t sendjob;

// Pin mapping
lmic_pinmap pins = { .nss = 6, .rxtx = UNUSED_PIN, // Not connected on RFM92/RFM95
		.rst = 0,  // Needed on RFM92/RFM95
		.dio = { 7, 4, 5 } };

void print_buffer(unsigned char* buffer, int len)
{
    fprintf(stdout, "BUFFER=");
    for (int i = 0; i < len; i++) {
        fprintf(stdout, "%x", buffer[i]);
    }
    fprintf(stdout, "\n");
}

void onEvent(ev_t ev) {
	//debug_event(ev);

	switch (ev) {
	// scheduled data sent (optionally data received)
	// note: this includes the receive window!
	case EV_TXCOMPLETE:
		// use this event to keep track of actual transmissions
		fprintf(stdout, "Event EV_TXCOMPLETE, time: %d\n", millis() / 1000);
		if (LMIC.dataLen) { // data received in rx slot after tx
			//debug_buf(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
			fprintf(stdout, "Data Received!\n");
		}
		break;
	default:
		break;
	}
}

static void do_send(osjob_t* j) {
	time_t t = time(NULL);
	fprintf(stdout, "do_send [%x] (%ld) %s\n", hal_ticks(), t, ctime(&t));
	// Show TX channel (channel numbers are local to LMIC)
	// Check if there is not a current TX/RX job running
	if (LMIC.opmode & (1 << 7)) {
		fprintf(stdout, "OP_TXRXPEND, not sending\n");
	} else if (mydatalen > 0) {
		// Prepare upstream data transmission at the next possible time.
		fprintf(stdout, "SENDING DATA=");
		for (int i = 0; i < mydatalen; i++) {
			fprintf(stdout, "%x", mydata[i]);
		}
		fprintf(stdout, "\n");
		LMIC_setTxData2(1, mydata, mydatalen, 0);
		mydatalen = 0;
	} else {
		fprintf(stdout, "NO DATA TO SEND\n");
	}
	// Schedule a timed job to run at the given timestamp (absolute system time)
	os_setTimedCallback(j, os_getTime() + sec2osticks(20), do_send);

}

void setup() {
	// LMIC init
	wiringPiSetup();

	os_init();
	// Reset the MAC state. Session and pending data transfers will be discarded.
	LMIC_reset();
	// Set static session parameters. Instead of dynamically establishing a session
	// by joining the network, precomputed session parameters are be provided.
	LMIC_setSession(0x1, DEVADDR, (u1_t*) DEVKEY, (u1_t*) ARTKEY);
	// Disable data rate adaptation
	LMIC_setAdrMode(0);
	// Disable link check validation
	LMIC_setLinkCheckMode(0);
	// Disable beacon tracking
	LMIC_disableTracking();
	// Stop listening for downstream data (periodical reception)
	LMIC_stopPingable();
	// Set data rate and transmit power (note: txpow seems to be ignored by the library)
	LMIC_setDrTxpow(DR_SF7, 14);
	//
}

void reverse_array(unsigned char *array, int n) {
    int x;
    unsigned char t;
    n --;
    for(x = 0; x < n; x ++, n --) {
        t = array[x];
        array[x] = array[n];
        array[n] = t;
    }
}
int convert(const char *hex_str, unsigned char *byte_array,
		int byte_array_max) {
	int hex_str_len = strlen(hex_str);
	int i = 0, j = 0;

	int byte_array_size = (hex_str_len + 1) / 2;

	if (byte_array_size > byte_array_max) {
		// Too big for the output array
		return -1;
	}

	if (hex_str_len % 2 == 1) {
		if (sscanf(&(hex_str[0]), "%1hhx", &(byte_array[0])) != 1) {
			return -1;
		}

		i = j = 1;
	}

	for (; i < hex_str_len; i += 2, j++) {
		if (sscanf(&(hex_str[i]), "%2hhx", &(byte_array[j])) != 1) {
			return -1;
		}
	}


	return byte_array_size;
}

int socket_server_loop(int port) {
	int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in my_addr; /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size;
	char string_read[255];
	int n, i;
	int last_fd; /* Thelast sockfd that is connected	*/
	int init = 0;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	last_fd = sockfd;

	my_addr.sin_family = AF_INET; /* host byte order */
	my_addr.sin_port = htons(port); /* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
	bzero(&(my_addr.sin_zero), 8); /* zero the rest of the struct */

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr))
			== -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, 1) == -1) {
		perror("listen");
		exit(1);
	}
	fcntl(sockfd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/

	while (1) {

		if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size))
				== -1) {
			//perror("accept");
		} else {

			n = recv(new_fd, string_read, sizeof(string_read), 0);
			if (n < 1) {
				perror("recv - non blocking \n");
			} else {
				string_read[n] = 0;
				printf("The string is: %s \n", string_read);
				parse_buffer(string_read);
				if (!init) {
					init = 1;
					setup();
					do_send(&sendjob);
				} else {
					LMIC_setSession(0x1, DEVADDR, (u1_t*) DEVKEY,
							(u1_t*) ARTKEY);
				}
			}
		}
		//printf("RUNNING OS LOOP\n");
		os_runloop_once();
	}

	return 0;
}

int parse_socket_args(char arg, const char* optarg) {
	switch (arg) {
	case 'a': {
		convert(optarg, (unsigned char*) &APPEUI, 8);
	}
		break;
	case 'd': {
		convert(optarg, (unsigned char*) &DEVEUI, 8);
	}
		break;
	case 'n': {
		convert(optarg, (unsigned char*) &DEVKEY, 16);
	}
		break;
	case 's': {
		convert(optarg, (unsigned char*) &ARTKEY, 16);
	}
		break;
	case 'e': {
		convert(optarg, (unsigned char*) &DEVADDR, 4);
                reverse_array((unsigned char*) &DEVADDR, 4);
	}
		break;
	case 'x': {
		mydatalen = convert(optarg, (unsigned char*) &mydata, 254);
		printf("DATALEN=%d\n", mydatalen);
	}
		break;
	default:
		break;
	}
	return 0;
}

int parse_buffer(char * buffer) {
	const char* arg = strtok(buffer, ":");
	do {
		const char* arg2 = strtok(NULL, ":");
		parse_socket_args(arg[0], arg2);
		arg = strtok(NULL, ":");
	} while (arg != NULL);
}

int main(int argc, char *argv[]) {
	int opt;
	unsigned int port = 0;
	while ((opt = getopt(argc, argv, "p:")) != -1) {
		switch (opt) {
		case 'p': {
			port = atoi(optarg);
		}
			break;
		default:
			break;
		}
	}
	socket_server_loop(port);
}
