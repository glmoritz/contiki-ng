/*
 * labscim_linked_list.h
 *
 *  Created on: 4 de jun de 2020
 *      Author: root
 */

#ifndef LABSCIM_LINKED_LIST_H_
#define LABSCIM_LINKED_LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

struct labscim_ll_node {
	void* data;
	struct labscim_ll_node *next;
	struct labscim_ll_node *previous;
};

struct labscim_ll {
	struct labscim_ll_node* head;
	struct labscim_ll_node* tail;
	uint32_t count;
};

void labscim_ll_init_list(struct labscim_ll* linked_list);
void labscim_ll_deinit_list(struct labscim_ll* linked_list);

void labscim_ll_insert_at_front(struct labscim_ll* linked_list,  void* data);
void labscim_ll_insert_at_back(struct labscim_ll* linked_list, void* data);

void labscim_ll_insert_after(struct labscim_ll* linked_list,  struct labscim_ll_node* node, void* data);
void labscim_ll_insert_before(struct labscim_ll* linked_list,  struct labscim_ll_node* node, void* data);

void* labscim_ll_pop_front(struct labscim_ll* linked_list);
void* labscim_ll_peek_front(struct labscim_ll* linked_list);

void* labscim_ll_pop_back(struct labscim_ll* linked_list);
void* labscim_ll_peek_back(struct labscim_ll* linked_list);

void* labscim_ll_pop_node(struct labscim_ll* linked_list, struct labscim_ll_node* node);

uint32_t labscim_ll_size(struct labscim_ll* linked_list);

#endif /* LABSCIM_LINKED_LIST_H_ */
