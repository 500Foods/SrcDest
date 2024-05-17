#include "agentc_logmessage.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

Queue *log_message_queue;
pthread_mutex_t log_message_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int log_message_queue_active = 1;

void log_message_enqueue(const char *format, ...) {
    va_list args;
    va_start(args, format);

    LogMessage log_message;
    snprintf(log_message.format, sizeof(log_message.format), "%s", format);
    va_copy(log_message.args, args);

    QueueItem item;
    item.data = &log_message;

    pthread_mutex_lock(&log_message_queue_mutex);
    queue_push(log_message_queue, &item);
    pthread_mutex_unlock(&log_message_queue_mutex);

    va_end(args);
}

void log_message_process() {
    while (1) {
        pthread_mutex_lock(&log_message_queue_mutex);
        if (log_message_queue_active && !queue_is_empty(log_message_queue)) {
            QueueItem item;
            queue_pop(log_message_queue, &item);
            LogMessage *log_message = (LogMessage *)item.data;

            pthread_mutex_unlock(&log_message_queue_mutex);

            // Process the log message
            char message[1024];
            vsnprintf(message, sizeof(message), log_message->format, log_message->args);
            printf("%s", message);

            va_end(log_message->args);
        } else {
            pthread_mutex_unlock(&log_message_queue_mutex);
            break;
        }
    }
}

void *log_message_thread(void *arg) {
    while (1) {
        log_message_process();
        // Sleep or yield if needed
    }
    return NULL;
}
