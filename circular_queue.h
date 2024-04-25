#include <stdlib.h>
#include <stdio.h> //if you don't use scanf/printf change this include
#include <limits.h>

// Structure of a Node
struct cq_node {
    int data;
    struct cq_node *next;
};

struct c_queue {
    struct cq_node *front, *rear;
};

void circular_init_queue(struct c_queue *q) {
    q->front = NULL;
    q->rear = NULL;
}

// Function to create Circular queue
void circular_enQueue(struct c_queue *q, int value) {
    struct cq_node *temp = (struct cq_node *) malloc(sizeof(struct cq_node));;
    temp->data = value;
    if (q->front == NULL)
        q->front = temp;
    else
        q->rear->next = temp;

    q->rear = temp;
    q->rear->next = q->front;
}

// Function to delete_by_key element from Circular c_queue
int circular_deQueue(struct c_queue *q) {
    if (q->front == NULL) {
        printf("c_queue is empty");
        return INT_MIN;
    }

    // If this is the last lkl_node to be deleted
    int value; // Value to be dequeued
    if (q->front == q->rear) {
        value = q->front->data;
        free(q->front);
        q->front = NULL;
        q->rear = NULL;
    } else // There are more than one nodes
    {
        struct cq_node *temp = q->front;
        value = temp->data;
        q->front = q->front->next;
        q->rear->next = q->front;
        free(temp);
    }

    return value;
}

void circular_advance_queue(struct c_queue *q) {
    // Queue is empty
    if (q->front == NULL) {
        return;
    }
    q->rear = q->front;
    q->front = q->front->next;
}

bool circular_is_empty(struct c_queue *q) {
    return q->front == NULL;
}
// we don't switch if there is only one left or the queue empty
bool circular_is_empty_or_one_left(struct c_queue *q) {
    return q->front == q->rear;
}

// Function displaying the elements of Circular c_queue
void displayQueue(struct c_queue *q) {
    struct cq_node *temp = q->front;
    printf("\nElements in Circular Queue are: ");
    while (temp->next != q->front) {
        printf("%d ", temp->data);
        temp = temp->next;
    }
    printf("%d", temp->data);
}
