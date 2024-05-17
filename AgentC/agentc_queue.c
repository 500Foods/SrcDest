#include "agentc_queue.h"
#include <stdlib.h>

Queue *queue_create(int capacity) {
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    queue->items = (QueueItem *)malloc(capacity * sizeof(QueueItem));
    queue->front = -1;
    queue->rear = -1;
    queue->size = 0;
    queue->capacity = capacity;
    queue->paused = 0;
    return queue;
}

void queue_destroy(Queue *queue) {
    free(queue->items);
    free(queue);
}

int queue_is_full(Queue *queue) {
    return queue->size == queue->capacity;
}

int queue_is_empty(Queue *queue) {
    return queue->size == 0;
}

void queue_push(Queue *queue, const QueueItem *item) {
    if (queue_is_full(queue)) {
        // Handle queue overflow
        return;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->items[queue->rear] = *item;
    queue->size++;
    if (queue->front == -1) {
        queue->front = queue->rear;
    }
}

void queue_pop(Queue *queue, QueueItem *item) {
    if (queue_is_empty(queue)) {
        // Handle queue underflow
        return;
    }
    *item = queue->items[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    if (queue->size == 0) {
        queue->front = -1;
        queue->rear = -1;
    }
}

int queue_size(Queue *queue) {
    return queue->size;
}

int queue_capacity(Queue *queue) {
    return queue->capacity;
}

int queue_is_paused(Queue *queue) {
    return queue->paused;
}

void queue_pause(Queue *queue) {
    queue->paused = 1;
}

void queue_resume(Queue *queue) {
    queue->paused = 0;
}
