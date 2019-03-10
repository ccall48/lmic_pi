#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h> 
#include <string.h> 
#include <mosquitto.h>

struct mosquitto *mosq = NULL;
const char *topic = "/ttn-send/send_message";
static char buffer[1024];
static bool connected = false;
int mqtt_send(char* buffer);

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    /* Print all log messages regardless of level. */

    switch(level)
    {
    case MOSQ_LOG_DEBUG:
    case MOSQ_LOG_INFO:
    case MOSQ_LOG_NOTICE:
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR:
    {
        printf("%i:%s\n", level, str);
    }
    }

}

void mosq_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
    printf("SENDING=%s\n", buffer);
    int ret = mqtt_send(buffer);
    if(ret != 0) printf("mqtt_send error=%i\n", ret);
}

void mosq_disconnect_callback(struct mosquitto *mosq, void *obj, int rc)
{
    connected = false;
}

void mosq_publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
    printf("******PUBLISHED********");
    mosquitto_disconnect(mosq);
}

void mqtt_setup(int port, const char* host)
{

    int keepalive = 60;
    bool clean_session = true;

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq)
    {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    mosquitto_log_callback_set(mosq, mosq_log_callback);
    mosquitto_connect_callback_set(mosq, mosq_connect_callback);
    mosquitto_disconnect_callback_set(mosq, mosq_disconnect_callback);
    mosquitto_publish_callback_set(mosq, mosq_publish_callback);
    if(mosquitto_connect(mosq, host, port, keepalive))
    {
        fprintf(stderr, "Unable to connect.\n");
        exit(1);
    }

    int loop = mosquitto_loop_forever(mosq, -1, 1);
    if(loop != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Unable to start loop: %i\n", loop);
        exit(1);
    }
}

int mqtt_uninit()
{
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}

int mqtt_send(char *msg)
{
    return mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, 0);
}

int main(int argc, char *argv[])
{
    int opt, rc;
    char host[255];
    strcpy(host, "localhost");
    strcpy(buffer, "");
    unsigned int port = 1883;
    while((opt = getopt(argc, argv, "p:h:a:d:n:s:e:x:")) != -1)
    {
        switch(opt)
        {
        case 'p':
        {
            port = atoi(optarg);
        }
            break;

        case 'h':
        {
            strcpy(host, optarg);
        }
            break;

        default:
        {
            char optbuf[2];
            optbuf[0] = opt;
            optbuf[1] = 0;
            strcat(buffer, optbuf);
            strcat(buffer, ":");
            strcat(buffer, optarg);
            strcat(buffer, ":");
        }
            break;
        }
    }

    mqtt_setup(port, host);
    do
    {
        rc = mosquitto_loop(mosq, -1, 1);
    }
    while(rc == MOSQ_ERR_SUCCESS && connected);

    mqtt_uninit();

    return 0;
}
