//#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//-----------------------------------------------------------------------------
struct _node
{
	int16_t x;
	int16_t y;
	struct _node *next;
};
//-----------------------------------------------------------------------------
/*
// display the list
void printList(struct _node **head)
{
	struct _node *p = *head;
	printf("\n[");

	//start from the beginning
	while(p != NULL)
	{
		printf(" (%d, %d) ",p->x, p->y);
		p = p->next;
	}
	printf("]");
}
*/
//-----------------------------------------------------------------------------
//insertion at the beginning
void insert_at_begin(struct _node **head, int16_t x, int16_t y)
{
    //create a link
    // For some reason this points to a place in memory that we are not allowed to access and when we try to change any values of this node, the processor sends a fault interrupt
    struct _node *lk = (struct _node*) calloc(1, sizeof(struct _node));
    if (lk == NULL)
    {
        return;
    }
    lk->next = NULL;
    lk->x = x;
    lk->y = y;

    // point it to old first node
    lk->next = *head;

    //point first to new first node
    *head = lk;
}
//-----------------------------------------------------------------------------
void insert_at_end(struct _node **head, int16_t x, int16_t y)
{
    // If list is empty
    if(*head == NULL)
    {
        insert_at_begin(head, x, y);
    return;
    }

    //create a link
    struct _node *lk = (struct _node*) calloc(1, sizeof(struct _node));
    if (lk == NULL)
    {
        return;
    }
    lk->next = NULL;
    lk->x = x;
    lk->y = y;
    struct _node *linkedlist = *head;

    // point it to old first node
    while(linkedlist->next != NULL)
        linkedlist = linkedlist->next;

    //point first to new first node
    linkedlist->next = lk;
}
//-----------------------------------------------------------------------------
void insert_after_node(struct _node **head, struct _node *list, int16_t x, int16_t y)
{
    struct _node *lk = (struct _node*) calloc(1, sizeof(struct _node));
    if (lk == NULL)
    {
        return;
    }
    lk->next = NULL;
    lk->x = x;
    lk->y = y;
    lk->next = list->next;
    list->next = lk;
}
//-----------------------------------------------------------------------------
void delete_at_begin(struct _node **head)
{
    if(!(*head))
        return;

    struct _node *temp = *head;
    *head = (*head)->next;
    free(temp);
}
//-----------------------------------------------------------------------------
void delete_at_end(struct _node **head)
{
    if(!(*head))
        return;

    struct _node *temp = *head;
    if(temp->next == NULL)
    {
    free(temp);
    *head = NULL;
    }
    while (temp->next->next != NULL)
        temp = temp->next;

    free(temp->next);
    temp->next = NULL;
}
//-----------------------------------------------------------------------------
void delete_node(struct _node **head, int16_t x, int16_t y)
{
    struct _node *temp = *head, *prev;
    if (temp != NULL && temp->x == x && temp->y == y)
    {
        *head = temp->next;
    free(temp);
        return;
    }

    // Find the key to be deleted
    while (temp != NULL && temp->x != x || temp->y != y ) {
        prev = temp;
        temp = temp->next;
    }

    // If the key is not present
    if (temp == NULL) return;

    // Remove the node
    prev->next = temp->next;
    free(temp);
}
//-----------------------------------------------------------------------------
int search_list(struct _node **head, int16_t x, int16_t y)
{
    struct _node *temp = *head;
    while(temp != NULL)
    {
        if (temp->x == x && temp->y == y)
        {
            return 1;
        }
        temp=temp->next;
    }
    return 0;
}
//-----------------------------------------------------------------------------
struct _node get_last(struct _node **head)
{
    struct _node *linkedlist = *head;
    while (linkedlist->next->next != NULL)
        linkedlist = linkedlist->next;
    return *linkedlist;
}
//-----------------------------------------------------------------------------
