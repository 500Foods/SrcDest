#ifndef QUEUE_H
#define QUEUE_H

typedef struct {
    void *data;
} QueueItem;

typedef struct {
    QueueItem *items;
    int front;
    int rear;
    int size;
    int capacity;
    int paused;
} Queue;

Queue *queue_create(int capacity);
void queue_destroy(Queue *queue);
int queue_is_full(Queue *queue);
int queue_is_empty(Queue *queue);
void queue_push(Queue *queue, const QueueItem *item);
void queue_pop(Queue *queue, QueueItem *item);
int queue_size(Queue *queue);
int queue_capacity(Queue *queue);
int queue_is_paused(Queue *queue);
void queue_pause(Queue *queue);
void queue_resume(Queue *queue);

#endif
