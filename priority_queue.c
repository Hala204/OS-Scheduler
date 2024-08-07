#include "priority_queue.h"

/*
    Function to initialize the min heap with size = 0
*/
minHeap init_min_heap()
{
    minHeap hp;
    hp.size = 0;
    hp.arrival = 0;
    return hp;
}

/*
    Function to swap priority within two nodes of the min heap using pointers
*/
void swap(node *n1, node *n2)
{
    node temp = *n1;
    *n1 = *n2;
    *n2 = temp;
}

/*
    Heapify function is used to make sure that the heap property is never violated
    In case of deletion of a node, or creating a min heap from an array, heap property
    may be violated. In such cases, heapify function can be called to make sure that
    heap property is never violated
*/
void heapify(minHeap *hp, int i)
{
    int smallest; // = (LCHILD(i) < hp->size && hp->elem[LCHILD(i)].priority < hp->elem[i].priority) ? LCHILD(i) : i;
    if (LCHILD(i) < hp->size && hp->elem[LCHILD(i)].priority <= hp->elem[i].priority)
    {
        if (hp->elem[LCHILD(i)].priority == hp->elem[i].priority)
        {
            if (hp->elem[LCHILD(i)].my_arrival < hp->elem[i].my_arrival)
                smallest = LCHILD(i);
            else
                smallest = i;
        }
        else
            smallest = LCHILD(i);
    }
    else
        smallest = i;
    if (RCHILD(i) < hp->size && hp->elem[RCHILD(i)].priority <= hp->elem[smallest].priority)
    {
        if (hp->elem[RCHILD(i)].priority == hp->elem[smallest].priority)
        {
            if (hp->elem[RCHILD(i)].my_arrival < hp->elem[smallest].my_arrival)
                smallest = RCHILD(i);
        }
        else
            smallest = RCHILD(i);
    }
    if (smallest != i)
    {
        swap(&(hp->elem[i]), &(hp->elem[smallest]));
        heapify(hp, smallest);
    }
}

/*
    Function to insert a node into the min heap, by allocating space for that node in the
    heap and also making sure that the heap property and shape propety are never violated.
*/
void push(minHeap *hp, int priority, int data)
{
    if (hp->size)
    {
        hp->elem = realloc(hp->elem, (hp->size + 1) * sizeof(node));
    }
    else
    {
        hp->elem = malloc(sizeof(node));
    }

    node nd;
    nd.priority = priority;
    nd.data = data;
    nd.my_arrival = (hp->arrival)++;
    // printf("add node with arrival %d\n",nd.arrival) ;

    int i = (hp->size)++;
    while (i && nd.priority < hp->elem[PARENT(i)].priority)
    {
        hp->elem[i] = hp->elem[PARENT(i)];
        i = PARENT(i);
    }
    hp->elem[i] = nd;
    // printf("add hp elem with arrival %d\n",hp->elem[i].arrival) ;
}

int is_empty(minHeap *hp)
{
    return hp->size == 0;
}
/*
    Function to delete a node from the min heap
    It shall remove the root node, and place the last node in its place
    and then call heapify function to make sure that the heap property
    is never violated
*/
node *pop(minHeap *hp)
{
    if (hp->size)
    {
        struct node n;
        struct node *temp = &n;
        temp->data = hp->elem[0].data;
        temp->priority = hp->elem[0].priority;
        temp->my_arrival = hp->elem[0].my_arrival;
        hp->elem[0] = hp->elem[--(hp->size)];
        hp->elem = realloc(hp->elem, hp->size * sizeof(node));
        heapify(hp, 0);
        return temp;
    }
    else
    {
        free(hp->elem);
        return NULL;
    }
}

struct node *peek(minHeap *hp)
{
    if (hp->size)
    {
        struct node n;
        struct node *temp = &n;
        temp->data = hp->elem[0].data;
        temp->priority = hp->elem[0].priority;
        temp->my_arrival = hp->elem[0].my_arrival;
        return temp;
    }
    else
        return NULL;
}
