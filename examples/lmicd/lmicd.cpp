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
#include <mosquitto.h>

// LoRaWAN Application identifier (AppEUI)
// Not used in this example
static u1_t APPEUI[8] =
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN DevEUI, unique device ID (LSBF)
// Not used in this example
static u1_t DEVEUI[8] =
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN NwkSKey, network session key
// Use this key for The Things Network
static u1_t DEVKEY[16] =
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN AppSKey, application session key
// Use this key to get your data decrypted by The Things Network
static u1_t ARTKEY[16] =
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
static u4_t DEVADDR = 0x0;

static struct mosquitto *mosq = NULL;
static bool session_started = false;
static bool joined = false;
int parse_msg(char * buffer);

//////////////////////////////////////////////////
// APPLICATION CALLBACKS
//////////////////////////////////////////////////

// provide application router ID (8 bytes, LSBF)
void os_getArtEui(u1_t* buf)
{
    printf("GETTING APPEUI\n");
    memcpy(buf, APPEUI, 8);
}

// provide device ID (8 bytes, LSBF)
void os_getDevEui(u1_t* buf)
{
    printf("GETTING DEVEUI\n");
    memcpy(buf, DEVEUI, 8);
}

// provide device key (16 bytes)
void os_getDevKey(u1_t* buf)
{
    printf("GETTING DEVKEY\n");
    memcpy(buf, DEVKEY, 16);
}

u4_t cntr = 0;
u1_t mydata[255];
static int mydatalen = 0;

static osjob_t sendjob;

// Pin mapping
lmic_pinmap pins =
    { .nss = 6, .rxtx = UNUSED_PIN, // Not connected on RFM92/RFM95
            .rst = 0, // Needed on RFM92/RFM95
            .dio =
                { 7, 4, 5 } };

void print_msg(unsigned char* buffer, int len)
{
    fprintf(stdout, "BUFFER=");
    for(int i = 0; i < len; i++)
    {
        fprintf(stdout, "%x", buffer[i]);
    }
    fprintf(stdout, "\n");
}

void onEvent(ev_t ev)
{
    //debug_event(ev);

    switch(ev)
    {
    // scheduled data sent (optionally data received)
    // note: this includes the receive window!
    case EV_TXCOMPLETE:
        // use this event to keep track of actual transmissions
        fprintf(stdout, "Event EV_TXCOMPLETE, time: %d\n", millis() / 1000);
        if(LMIC.dataLen)
        { // data received in rx slot after tx
            //debug_buf(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
            fprintf(stdout, "Data Received!\n");
        }
        break;

    case EV_SCAN_TIMEOUT:
        fprintf(stdout, "EV_SCAN_TIMEOUT");
        break;
    case EV_BEACON_FOUND:
        fprintf(stdout, "EV_BEACON_FOUND");
        break;
    case EV_BEACON_MISSED:
        fprintf(stdout, "EV_BEACON_MISSED");
        break;
    case EV_BEACON_TRACKED:
        fprintf(stdout, "EV_BEACON_TRACKED");
        break;
    case EV_JOINING:
        //fprintf(stdout, "EV_JOINING");
        break;
    case EV_JOINED:
        fprintf(stdout, "EV_JOINED");
        joined = true;
        break;
    case EV_RFU1:
        fprintf(stdout, "EV_RFU1");
        break;
    case EV_JOIN_FAILED:
        fprintf(stdout, "EV_JOIN_FAILED");
        joined = false;
        break;
    case EV_REJOIN_FAILED:
        fprintf(stdout, "EV_REJOIN_FAILED");
        joined = false;
        break;
    case EV_LOST_TSYNC:
        fprintf(stdout, "EV_LOST_TSYNC");
        break;
    case EV_RESET:
        fprintf(stdout, "EV_RESET");
        break;
    case EV_RXCOMPLETE:
        // data received in ping slot
        fprintf(stdout, "EV_RXCOMPLETE");
        break;
    case EV_LINK_DEAD:
        fprintf(stdout, "EV_LINK_DEAD");
        break;
    case EV_LINK_ALIVE:
        fprintf(stdout, "EV_LINK_ALIVE");
        break;
    default:
        fprintf(stdout, "Unknown event");
        break;
    }
    fprintf(stdout, "\n");
}

static void do_send(osjob_t* j)
{

    if (mydatalen == 0)
    {
        return;
    }

    time_t t = time(NULL);
    //fprintf(stdout, "do_send [%x] (%ld) %s\n", hal_ticks(), t, ctime(&t));
    // Show TX channel (channel numbers are local to LMIC)
    // Check if there is not a current TX/RX job running
    //if(LMIC.opmode & (1 << 7))
    //{
    //    fprintf(stdout, "OP_TXRXPEND, not sending\n");
    //}
    //else
    {
        // Prepare upstream data transmission at the next possible time.
        fprintf(stdout, "SENDING DATA=");
        for(int i = 0; i < mydatalen; i++)
        {
            fprintf(stdout, "%x", mydata[i]);
        }
        fprintf(stdout, "\n");
        LMIC_setTxData2(1, mydata, mydatalen, 0);
        mydatalen = 0;
    }
    // Schedule a timed job to run at the given timestamp (absolute system time)
    //os_setTimedCallback(j, os_getTime() + sec2osticks(2000), do_send);

}

void startsession()
{
    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    // start joining
    printf("SETTING UP SESSION\n");
    // LMIC_startJoining();
    LMIC_setSession(0x1, DEVADDR, (u1_t*)DEVKEY, (u1_t*)ARTKEY);
}

void setup()
{
    // LMIC init
    wiringPiSetup();

    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    startsession();
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
    session_started = true;
    joined = true;
}

void reverse_array(unsigned char *array, int n)
{
    int x;
    unsigned char t;
    n--;
    for(x = 0; x < n; x++, n--)
    {
        t = array[x];
        array[x] = array[n];
        array[n] = t;
    }
}
int convert(const char *hex_str, unsigned char *byte_array, int byte_array_max)
{
    int hex_str_len = strlen(hex_str);
    int i = 0, j = 0;

    int byte_array_size = (hex_str_len + 1) / 2;

    if(byte_array_size > byte_array_max)
    {
        // Too big for the output array
        return -1;
    }

    if(hex_str_len % 2 == 1)
    {
        if(sscanf(&(hex_str[0]), "%1hhx", &(byte_array[0])) != 1)
        {
            return -1;
        }

        i = j = 1;
    }

    for(; i < hex_str_len; i += 2, j++)
    {
        if(sscanf(&(hex_str[i]), "%2hhx", &(byte_array[j])) != 1)
        {
            return -1;
        }
    }

    return byte_array_size;
}

int main_loop()
{
    while(1)
    {
        mosquitto_loop(mosq, -1, 1);

        if(session_started == true)
        {
            os_runloop_once();
        }
        if(joined == true)
        {
            do_send(&sendjob);
        }

    }

    return 0;
}

int parse_socket_args(char arg, const char* optarg)
{

    switch(arg)
    {
    case 'a':
    {
        convert(optarg, (unsigned char*)&APPEUI, 8);
        //reverse_array((unsigned char*)&APPEUI, 8);
    }
        break;
    case 'd':
    {
        convert(optarg, (unsigned char*)&DEVEUI, 8);
        //reverse_array((unsigned char*)&DEVEUI, 8);
    }
        break;
    case 'n':
    {
        convert(optarg, (unsigned char*)&DEVKEY, 16);
        //reverse_array((unsigned char*)&DEVKEY, 16);
    }
        break;
    case 's':
    {
        convert(optarg, (unsigned char*)&ARTKEY, 16);
        //reverse_array((unsigned char*)&ARTKEY, 16);
    }
        break;
    case 'e':
    {
        convert(optarg, (unsigned char*)&DEVADDR, 4);
        reverse_array((unsigned char*)&DEVADDR, 4);
    }
        break;
    case 'x':
    {
        mydatalen = convert(optarg, (unsigned char*)&mydata, 254);
        printf("DATALEN=%d\n", mydatalen);
    }
        break;
    default:
        break;
    }

    return 0;
}

int parse_msg(char * buffer)
{
    const char* arg = strtok(buffer, ":");
    do
    {
        const char* arg2 = strtok(NULL, ":");
        parse_socket_args(arg[0], arg2);
        arg = strtok(NULL, ":");
    }
    while(arg != NULL);
}
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    bool match = 0;
    /*
     if(message->payloadlen){
     printf("%s %s\n", message->topic, message->payload);
     }else{
     printf("%s (null)\n", message->topic);
     }
     */
    mosquitto_topic_matches_sub("/ttn-send/send_message", message->topic, &match);
    if(match)
    {
        printf("********MESSAGE RECEIVED******\n");
        printf("got send message\n");
        parse_msg((char*)message->payload);
        printf("********MESSAGE PARSED******\n");
        if(session_started == false)
        {
            setup();
        }
    }

    fflush(stdout);
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    int i;
    if(!result)
    {
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
    }
    else
    {
        fprintf(stderr, "Connect failed\n");
    }
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    int i;

    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for(i = 1; i < qos_count; i++)
    {
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}

void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    /* Pring all log messages regardless of level. */
    //printf("%s\n", str);
}

int init_mosquitto(unsigned int port)
{
    int i;
    const char *host = "localhost";
    int keepalive = 60;
    bool clean_session = true;

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq)
    {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }
    mosquitto_log_callback_set(mosq, my_log_callback);
    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

    if(mosquitto_connect(mosq, host, port, keepalive))
    {
        fprintf(stderr, "Unable to connect.\n");
        return 1;
    }

    mosquitto_subscribe(mosq, NULL, "/ttn-send/send_message", 0);
}

int uninit_mosquitto()
{
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}

int main(int argc, char *argv[])
{
    int opt;
    unsigned int port = 1883;
    while((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch(opt)
        {
        case 'p':
        {
            port = atoi(optarg);
        }
            break;
        default:
            break;
        }
    }
    init_mosquitto(port);
    main_loop();
    uninit_mosquitto();
    return 0;
}
