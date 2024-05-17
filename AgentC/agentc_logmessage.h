#ifndef LOG_MESSAGE_QUEUE_H
#define LOG_MESSAGE_QUEUE_H

#include "agentc_queue.h"

typedef struct {
    char format[256];
    va_list args;
} LogMessage;

extern Queue *log_message_queue;
extern int log_message_queue_active;

void log_message_enqueue(const char *format, ...);
void log_message_process();
void *log_message_thread(void *arg);

#endif
