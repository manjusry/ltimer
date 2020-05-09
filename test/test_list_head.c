#include <stdio.h>
#include <stdlib.h>

#include "list_head.h"

typedef struct node
{
	list_head_t entry;
	int val;
}node_t;

static int cmp_func(list_head_t* item1, list_head_t* item2)
{
	node_t* p1 = list_entry(item1, node_t, entry);
	node_t* p2 = list_entry(item2, node_t, entry);
	return (p1->val - p2->val);
}

static void node_init(int val, node_t* ptr)
{
	list_init(&ptr->entry);
	ptr->val = val;
}

int main()
{
	LIST_HEAD(head);
	node_t n1, n2, n3, n4, n5;

	node_init(1, &n1);
	node_init(3, &n2);
	node_init(7, &n3);
	node_init(4, &n4);
	node_init(8, &n5);

	list_insert(&n1.entry, &head, cmp_func, 1);
	list_insert(&n2.entry, &head, cmp_func, 1);
	list_insert(&n3.entry, &head, cmp_func, 1);
	list_insert(&n4.entry, &head, cmp_func, 1);
	list_insert(&n5.entry, &head, cmp_func, 1);

	node_t *pos;
	list_for_each_entry(pos, &head, entry) {
		printf("%d   ", pos->val);
	}
	printf("\n");
	
	return 0;
}

