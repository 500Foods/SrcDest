#include "agentc_logevent.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>

Queue *log_event_queue;
pthread_mutex_t log_event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int log_event_queue_active = 1;

void log_event_enqueue(const char *ipaddress, const char *app, const char *client, const char *event, const char *details, long duration, int fileset) {
    LogEvent log_event;
    strncpy(log_event.ipaddress, ipaddress, sizeof(log_event.ipaddress));
    strncpy(log_event.app, app, sizeof(log_event.app));
    strncpy(log_event.client, client, sizeof(log_event.client));
    strncpy(log_event.event, event, sizeof(log_event.event));
    strncpy(log_event.details, details, sizeof(log_event.details));
    log_event.duration = duration;
    log_event.fileset = fileset;

    QueueItem item;
    item.data = &log_event;

    pthread_mutex_lock(&log_event_queue_mutex);
    queue_push(log_event_queue, &item);
    pthread_mutex_unlock(&log_event_queue_mutex);
}

void log_event_process() {
    while (1) {
        pthread_mutex_lock(&log_event_queue_mutex);
        if (log_event_queue_active && !queue_is_empty(log_event_queue)) {
            QueueItem item;
            queue_pop(log_event_queue, &item);
            LogEvent *log_event = (LogEvent *)item.data;

            pthread_mutex_unlock(&log_event_queue_mutex);

            // Process the log event
            sqlite3_stmt *stmt;
            char sql[] = "INSERT INTO EVENTS (LOGSTAMP, IPADDRESS, APP, CLIENT, EVENT, DETAILS, DURATION, FILESET) VALUES (datetime('now'), ?, ?, ?, ?, ?, ?, ?)";

            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                printf("Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
                continue;
            }

            sqlite3_bind_text(stmt, 1, log_event->ipaddress, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, log_event->app, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, log_event->client, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, log_event->event, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, log_event->details, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 6, log_event->duration);
            sqlite3_bind_int(stmt, 7, log_event->fileset);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                printf("Error executing SQL statement: %s\n", sqlite3_errmsg(db));
            }

            sqlite3_finalize(stmt);
        } else {
            pthread_mutex_unlock(&log_event_queue_mutex);
            break;
        }
    }
}

void *log_event_thread(void *arg) {
    while (1) {
        log_event_process();
        // Sleep or yield if needed
    }
    return NULL;
}
