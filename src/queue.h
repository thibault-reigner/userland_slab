/**
 * Contains useful macros (GNU C only) to manipulate doubly-linked lists.
 *
 * These macros are type-independant. The names of 'prev' and 'next'
 * fields are given as parameters.
 */
#ifndef QUEUE_H
#define QUEUE_H

#define dlist_is_empty_generic(list)		\
  ((list) == NULL)

#define dlist_is_singleton_generic(list, prev, next)	\
  ((list)->next == NULL)

#define dlist_push_head_generic(list, el, prev, next) ({	\
      if(!dlist_is_empty_generic((list)))			\
	{							\
	  (list)->prev = (el);					\
	}							\
      (el)->prev = NULL;					\
      (el)->next = (list);					\
      (list) = (el);						\
    })

#define dlist_delete_head_generic(list, head, prev, next) ({	\
      if(dlist_is_singleton_generic(list,prev,next))		\
	{							\
	  (list) = NULL;					\
	}							\
      else							\
	{							\
	  (list) = (head)->next;				\
	  (list)->prev = NULL;					\
	  (head)->prev = NULL;					\
	  (head)->next = NULL;					\
	}							\
    })

#define dlist_pop_head_generic(list, prev, next) ({	\
      typeof(list) __el2pop = (list);			\
      dlist_delete_head_generic(list, __el2pop, prev, next);	\
      __el2pop;						\
    })

#define dlist_delete_el_generic(list, el, prev, next) ({	\
      if((list) != (el))					\
	{							\
	  (el)->prev->next = (el)->next;			\
	  if((el)->next != NULL)				\
	    {							\
	      (el)->next->prev = (el)->prev;			\
	    }							\
	  (el)->prev = NULL;					\
	  (el)->next = NULL;					\
	}							\
      else{							\
	dlist_delete_head_generic(list,el,prev,next);		\
      }								\
    })


#endif 
