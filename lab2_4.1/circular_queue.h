#include<stdio.h>

#define QUEUESIZE 40

typedef struct
{
    int16_t x;
    int16_t y;
} Coordinat;
typedef struct
{
    int16_t front;
    int16_t rear;
    Coordinat queue[QUEUESIZE];
} CircularQueue;
 

//-----------------------------------------------------------------------------
// LEGACY
// DOESNT WORK, NOT USED
// Check if snake overlaps with itself
// The logic of checking essentially boils down to checking if one part of the snakes lower left coordinate lies within any other parts of the snakes quadrants
// S i head of snake, O is another part of the snake
int16_t check_overlap(CircularQueue* q, int16_t size)
{
    // Temp is used to step through the list and check all elements
    int16_t temp = q->front;
    // Go through entire list to see if snake overlaps with any part of itself
    while(temp != q->rear)
    {
        // This will make sure we do not check with head of snake
        temp = (temp+1) % QUEUESIZE;
        /* The following is the logic in order, first the first part of the OR, then the second part of the OR etc
         * --------------------------------------------------------------------
         *
         *        +
         *        +
         *     +  +
         *     +  +
         *    -+--S++++
         *     +  -
         * ----O++++
         *     -  -
         *     -  -
         *     -
         *     -
         *
         * --------------------------------------------------------------------
         *
         *     +
         *     +
         *     +
         *     + +
         * ----S++++
         *       +
         *       +
         *   ----O++++
         *       -
         *       -
         *       -
         *       -
         *
         * --------------------------------------------------------------------
         *
         *         +
         *      +  +
         *      +  +
         *      +  +
         *     ----O++++
         *  ----S++-+
         *      -  -
         *      -  -
         *      -  -
         *      -
         *
         * --------------------------------------------------------------------
         *
         *     +
         *     +
         *     +
         *     +   +
         * ----O++++
         *     -   +
         *     -   +
         *     ----S++++
         *     -   -
         *         -
         *         -
         *         -
         *
         * --------------------------------------------------------------------
         */
        if(((q->queue[q->front].x < (q->queue[temp].x + size)) && (q->queue[q->front].y < (q->queue[temp].y + size))) ||
                ((q->queue[q->front].x > (q->queue[temp].x - size)) && (q->queue[q->front].y < (q->queue[temp].y + size))) ||
                ((q->queue[q->front].x > (q->queue[temp].x - size)) && (q->queue[q->front].y > (q->queue[temp].y - size))) ||
                ((q->queue[q->front].x < (q->queue[temp].x + size)) && (q->queue[q->front].y > (q->queue[temp].y - size))))
        {
            return 1;
        }
    }
    return 0;
}
//-----------------------------------------------------------------------------
// Here we check if the Circular queue is full or not
int16_t check_full (CircularQueue* q)
{
    if ((q->front == q->rear + 1) || (q->front == 0 && q->rear == QUEUESIZE - 1))
    {
        return 1;
    }
    return 0;
}
//-----------------------------------------------------------------------------
// Here we check if the Circular queue is empty or not
int16_t check_empty (CircularQueue* q)
{
    if (q->front == -1)
    {
        return 1;
    }
    return 0;
}
//-----------------------------------------------------------------------------
// Addtion in the Circular Queue
int16_t enqueue (CircularQueue* q, int16_t x, int16_t y)
{
    if (check_full (q))
    {
        //printf ("Overflow condition\n");
        return 0;
    }

    else
    {
        if (q->front == -1)
            q->front = 0;

        q->rear = (q->rear + 1) % QUEUESIZE;
        q->queue[q->rear].x = x;
        q->queue[q->rear].y = y;
        //UARTprintf("(%d, %d) was enqueued to circular queue\n", x, y);
        return 1;
    }
}
//-----------------------------------------------------------------------------

void empty_queue(CircularQueue* q)
{
    q->front = -1;
    q->rear = -1;
}
//-----------------------------------------------------------------------------
// Removal from the Circular Queue
Coordinat dequeue (CircularQueue* q)
{
    Coordinat temp;
    if (check_empty (q))
    {
        temp.x = -1;
        temp.y = -1;
        //printf ("Underflow condition\n");
        return temp;
    }
    else
    {
        temp.x = q->queue[q->front].x;
        temp.y = q->queue[q->front].y;
        if (q->front == q->rear)
        {
            q->front = q->rear = -1;
        }
        else
        {
            q->front = (q->front + 1) % QUEUESIZE;
        }
        //UARTprintf ("(%d, %d) was dequeued from circular queue\n", x, y);
        return temp;
    }
}
//-----------------------------------------------------------------------------
// Display the queue
void print (CircularQueue* q)
{
    int i;
    if (check_empty (q))
        printf ("Nothing to dequeue\n");
    else
    {
        printf ("\nThe queue looks like: \n");
        for (i = q->front; i != q->rear; i = (i + 1) % QUEUESIZE)
        {
            printf ("(%d, %d)", q->queue[i].x, q->queue[i].y);
        }
        printf ("(%d, %d)\n\n", q->queue[i].x, q->queue[i].y);

    }
}
//-----------------------------------------------------------------------------
