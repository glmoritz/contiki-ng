/*
 * labscim_linked_list.c
 *
 *  Created on: 4 de jun de 2020
 *      Author: root
 */


#include <stdio.h>
#include <stdlib.h>
#include "labscim_linked_list.h"


void labscim_ll_init_list(struct labscim_ll* linked_list)
{
	linked_list->count = 0;
	linked_list->head = NULL;
	linked_list->tail = NULL;
}

void labscim_ll_insert_at_front(struct labscim_ll* linked_list,  void* data)
{
	struct labscim_ll_node *t;
	t = (struct labscim_ll_node*)malloc(sizeof(struct labscim_ll_node));
	if(t==NULL)
	{
		perror("Malloc Error");
		return;
	}
	t->data = data;
	linked_list->count++;
	if (linked_list->head == NULL)
	{
		linked_list->head = t;
		linked_list->tail = t;
		t->next = NULL;
		t->previous = NULL;
	}
	else
	{
		t->next = linked_list->head;
		linked_list->head->previous = t;
		t->previous = NULL;
		linked_list->head = t;
	}
	return;
}

void labscim_ll_insert_at_back(struct labscim_ll* linked_list, void* data)
{
	struct labscim_ll_node *t, *temp;
	t = (struct labscim_ll_node*)malloc(sizeof(struct labscim_ll_node));
	if(t==NULL)
	{
		perror("Malloc Error");
		return;
	}
	t->data = data;
	linked_list->count++;
	if (linked_list->head == NULL)
	{
		linked_list->head = t;
		linked_list->tail = t;
		t->next = NULL;
		t->previous = NULL;
	}
	else
	{
		linked_list->tail->next = t;
		t->next = NULL;
		t->previous = linked_list->tail;
		linked_list->tail = t;
	}
	return;
}


void labscim_ll_insert_before(struct labscim_ll* linked_list,  struct labscim_ll_node* node, void* data)
{
	struct labscim_ll_node *t;
	struct labscim_ll_node *iter = linked_list->head;

	if(node==NULL)
	{
		labscim_ll_insert_at_front(linked_list,data);
		return;
	}

	if(iter==NULL)
	{
		if(node==NULL)
		{
			labscim_ll_insert_at_front(linked_list,data);
			return;
		}
		else
		{
			return;
		}
	}

	//check if the node is in the list
	while((iter!=node)&&iter!=NULL)
	{
		iter = iter->next;
	}
	if(iter!=node)
	{
		return;
	}

	//insert
	t = (struct labscim_ll_node*)malloc(sizeof(struct labscim_ll_node));
	if(t==NULL)
	{
		perror("Malloc Error");
		return;
	}
	t->data = data;
	linked_list->count++;

	if (node == linked_list->head)
	{
		t->next = linked_list->head;
		linked_list->head->previous = t;
		linked_list->head = t;
		t->previous = NULL;
	}
	else
	{
		node->previous->next = t;
		t->next = node;
		t->previous = node->previous;
		node->previous = t;
	}
	return;
}


void labscim_ll_insert_after(struct labscim_ll* linked_list,  struct labscim_ll_node* node, void* data)
{
	struct labscim_ll_node *t;
	struct labscim_ll_node *iter = linked_list->head;

	if(node==NULL)
	{
		labscim_ll_insert_at_front(linked_list,data);
		return;
	}

	if(iter==NULL)
	{
		if(node==NULL)
		{
			labscim_ll_insert_at_front(linked_list,data);
			return;
		}
		else
		{
			return;
		}
	}

	//check if the node is in the list
	while((iter!=node)&&iter!=NULL)
	{
		iter = iter->next;
	}
	if(iter!=node)
	{
		return;
	}

	//insert
	t = (struct labscim_ll_node*)malloc(sizeof(struct labscim_ll_node));
	if(t==NULL)
	{
		perror("Malloc Error");
		return;
	}
	t->data = data;
	linked_list->count++;

	if (node == linked_list->tail)
	{
		linked_list->tail = t;
		t->next = NULL;
		t->previous = node;
		node->next = t;
	}
	else
	{
		node->next->previous = t;
		t->next = node->next;
		t->previous = node;
		node->next = t;
	}
	return;
}



void* labscim_ll_pop_front(struct labscim_ll* linked_list)
{
	struct labscim_ll_node *popped;
	void* popped_data;
	if (linked_list->head == NULL)
	{
		return NULL;
	}
	else
	{
		popped = linked_list->head;
		linked_list->count--;
		linked_list->head = popped->next;
		if(linked_list->head!=NULL)
		{
			linked_list->head->previous = NULL;
		}
		else
		{
		    linked_list->tail = NULL;
		}
		popped_data = popped->data;
		free(popped);
	}
	return popped_data;
}


void* labscim_ll_pop_node(struct labscim_ll* linked_list, struct labscim_ll_node* node)
{
	struct labscim_ll_node* it=linked_list->head;
	void* popped_data=NULL;

	if(node==NULL)
	{
		return NULL;
	}
	//just for safety
	while((it!=node)&&it!=NULL)
	{
		it = it->next;
	}

	//pop the node, update the list if necessary
	if(it!=NULL)
	{
		if(node->previous!=NULL)
		{
			node->previous->next = node->next;
		}
		else
		{
			//node was the head
			linked_list->head = node->next;
		}
		if(node->next != NULL)
		{
			node->next->previous = node->previous;
		}
		else
		{
			//node was the tail
			linked_list->tail = node->previous;
		}
		popped_data = node->data;
		free(node);
		linked_list->count--;
		return popped_data;
	}
	else
	{
		return NULL;
	}
}

void* labscim_ll_pop_back(struct labscim_ll* linked_list)
{
	struct labscim_ll_node *popped;
	void* popped_data;
	if (linked_list->tail == NULL)
	{
		return NULL;
	}
	else
	{
		popped = linked_list->tail;
		linked_list->count--;
		linked_list->tail = popped->previous;
		if(linked_list->tail!=NULL)
		{
			linked_list->tail->next = NULL;
		}
		else
		{
		    linked_list->head = NULL;
		}
		popped_data = popped->data;
		free(popped);
	}
	return popped_data;
}


void* labscim_ll_peek_front(struct labscim_ll* linked_list)
{
	if(linked_list->head!=NULL)
	{
		return linked_list->head->data;
	}
	else
	{
		return NULL;
	}
}



void* labscim_ll_peek_back(struct labscim_ll* linked_list)
{
	if(linked_list->tail!=NULL)
	{
		return linked_list->tail->data;
	}
	else
	{
		return NULL;
	}
}

uint32_t labscim_ll_size(struct labscim_ll* linked_list)
{
	return linked_list->count;
}

void labscim_ll_deinit_list(struct labscim_ll* linked_list)
{
    struct labscim_ll_node *popped = linked_list->tail;
    struct labscim_ll_node *next = linked_list->tail;
    while(next!=NULL)
    {
        if(popped->data!=NULL)
            free(popped->data);
        next = popped->previous;
        free(popped);
    }
    linked_list->head = NULL;
    linked_list->tail = NULL;
    linked_list->count = 0;
}






