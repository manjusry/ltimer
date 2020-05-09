#ifndef LIST_HEAD_H
#define LIST_HEAD_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef list_prefetch
#define list_prefetch(x) __builtin_prefetch((x), 0, 1)
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#ifndef container_of
#define container_of(ptr, type, member) ({ \
		const typeof(((type *)0)->member) * __mptr = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })
#endif

typedef struct list_head {
	struct list_head* prev, *next;
} list_head_t;

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#ifndef LIST_HEAD
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
#endif

static inline void list_init(list_head_t* head)
{
	head->next = head;
	head->prev = head;
}

static inline int list_empty(list_head_t* head)
{
	return (head->next == head);
}

static inline void __list_add(list_head_t *item, list_head_t *prev, list_head_t *next)
{
	next->prev = item;
	item->next = next;
	item->prev = prev;
	prev->next = item;
}

static inline void list_add_head(list_head_t* item, list_head_t* head)
{
	__list_add(item, head, head->next);
}

static inline void list_add_tail(list_head_t* item, list_head_t* head)
{
	__list_add(item, head->prev, head);
}

static inline void list_del(list_head_t* item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->next = item;
	item->prev = item;
}

static inline int list_is_first(list_head_t* item, list_head_t* head)
{
	return (head->next == item);
}

static inline int list_is_last(list_head_t* item, list_head_t* head)
{
	return (head->prev == item);
}

static inline int list_count(list_head_t* head)
{
	int cnt = 0;
	list_head_t* item;
	for (item = head->next; item != head; item = item->next) {
		cnt++;
	}
	return cnt;
}

/**
 * list_for_each - iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; list_prefetch(pos->next), pos != (head); pos = pos->next)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

/**
 * list_for_each_prev - iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; list_prefetch(pos->prev), pos != (head); pos = pos->prev)

/**
 * list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage.
 * @head:	the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head) \
	for (pos = (head)->prev, n = pos->prev; list_prefetch(pos->prev), pos != (head); \
		 pos = n, n = pos->prev)

/**
 * list_entry - get the &struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct.
 * @member: the name of the struct list head member within the struct.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/**
 * list_for_each_entry - iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member) \
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
		 list_prefetch(pos->member.next), &pos->member != (head); \
		 pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage.
 * @head:	the head for your list.
 * @member: the name of the struct list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member) \
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		 n = list_entry(pos->member.next, typeof(*pos), member); \
		 &pos->member != (head);	 \
		 pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member) \
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
		 list_prefetch(pos->member.prev), &pos->member != (head); \
		 pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * list_for_each_entry_safe_reverse
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member) \
	for (pos = list_entry((head)->prev, typeof(*pos), member),	\
		 n = list_entry(pos->member.prev, typeof(*pos), member); \
		 &pos->member != (head); \
		 pos = n, n = list_entry(n->member.prev, typeof(*n), member))

/**
 * list_cmp_func - compare function prototype, comparing two nodes which has struct list_head embeded.
 * @item1:	the &struct list_head member embeded in node.
 * @item2:	the &struct list_head member embeded in node.
 * @return: node(item1) > node(item2), return positive number, otherwise return negative or zero.
 */
typedef int (*list_cmp_func)(list_head_t* item1, list_head_t* item2);

/**
 * list_insert: insert node by comparing from head to tail.
 * @item:	the &struct list_head member of node to be insert.
 * @head:	the head for your list.
 * @crease:	increase list use 1, decrease lise use -1.
 */
static inline void list_insert(list_head_t* item, list_head_t* head, list_cmp_func func, int crease)
{
	list_head_t* pos;
	for (pos = head->next; pos != head; pos = pos->next) {
		if (func(item, pos) * crease < 0) {
			__list_add(item, pos->prev, pos);
			return;
		}
	}
	list_add_tail(item, head);
}

/**
 * list_insert_reverse: insert node by comparing from tail to head.
 * @item:	the &struct list_head member of node to be insert.
 * @head:	the head for your list.
 * @crease:	increase list use 1, decrease lise use -1.
 */
static inline void list_insert_reverse(list_head_t* item, list_head_t* head, list_cmp_func func, int crease)
{
	list_head_t* pos;
	for (pos = head->prev; pos != head; pos = pos->prev) {
		if (func(item, pos) * crease > 0) {
			__list_add(item, pos, pos->next);
			return;
		}
	}
	list_add_head(item, head);
}

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //LIST_HEAD_H
