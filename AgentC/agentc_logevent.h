#ifndef LOG_EVENT_QUEUE_H
#define LOG_EVENT_QUEUE_H

#include "agentc_queue.h"

typedef struct {
    char ipaddress[256];
    char app[256];
    char client[256];
    char event[256];
    char details[256];
    long duration;
    int fileset;
} LogEvent;

extern Queue *log_event_queue;
extern int log_event_queue_active;

void log_event_enqueue(const char *ipaddress, const char *app, const char *client, const char *event, const char *details, long duration, int fileset);
void log_event_process();
void *log_event_thread(void *arg);

#endif
