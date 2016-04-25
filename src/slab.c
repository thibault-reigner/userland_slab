#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#include <sys/mman.h>

#include "slab.h"
#include "queue.h"





/********************************************************
 *                       Private methods
 *******************************************************/


/* Create a new slab to be added to a cache.
 * pages_per_slab : the number of pages used by this slab
 * actual_obj_size : the size occupied by the object and its header
 * slab_descr_cache : if non null, the cache from where to allocate the slab descriptor
 * instead of storing it at the beginning of the slab.
 * Return this adress of the new slab's descriptor if successful, NULL otherwise
 */
static struct Userland_slab *create_slab(unsigned int pages_per_slab,
					 size_t actual_obj_size,
					 struct Objs_cache *slab_descr_cache)
{
  //TODO : Handle the case slab_descr_cache != NULL (slab descriptors are not on-slabs)
  
  struct Userland_slab *new_slab = NULL;
  size_t slab_size = pages_per_slab*sysconf(_SC_PAGESIZE);

  assert((slab_size - sizeof(struct Userland_slab)) / actual_obj_size > 0);
  
  if(slab_descr_cache == NULL)
    {
      //the slab descriptor is on-slab
      
      new_slab = mmap(NULL,
		      slab_size,
		      PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS,
		      -1,
		      0);
      //TODO : check if clearing the slab is necessary
      memset(new_slab, 0, slab_size);
      
      if(new_slab != MAP_FAILED)
	{
	  new_slab->free_objs_count = (slab_size - sizeof(struct Userland_slab)) / actual_obj_size;
	  //new_slab->wasted_memory   = (slab_size - sizeof(struct Userland_slab)) % actual_obj_size;

	  //the first object is right after the slab descriptor	  
	  new_slab->objs = (struct Obj_header*)((uintptr_t)new_slab + sizeof(struct Userland_slab));
	  new_slab->first_free_obj = new_slab->objs;

	  new_slab->prev = NULL;
	  new_slab->next = NULL;

	  //now we initialize the headers of each object in the slab
	  struct Obj_header *current_obj = new_slab->objs;
	  struct Obj_header *next_obj = NULL;
	  for(int i=1; i < new_slab->free_objs_count;i++)
	    {
	      next_obj = (struct Obj_header*)((uintptr_t)current_obj + actual_obj_size);
	      current_obj->header.if_free.next = next_obj;
	      current_obj = next_obj;
	    }
	  current_obj->header.if_free.next = NULL;
	}
    }
  else{
    printf("Off-slab allocation of slab descriptor not implemented !\n");
    exit(-1);
  }
  return new_slab;
}

static void destroy_slab(struct Userland_slab *slab, size_t slab_size)
{
  assert(slab != NULL);
  munmap(slab, slab_size);
}


static void *alloc_obj_from_slab(struct Userland_slab *slab)
{
  void *obj_data = NULL;

  assert(slab != NULL);
  assert(slab->first_free_obj != NULL);
  
  assert(slab->free_objs_count > 0);
	  
  struct Obj_header *obj =slab->first_free_obj;
  obj_data = (void*)((uintptr_t)obj + sizeof(struct Obj_header));

  slab->free_objs_count--;
      
  //remove this object from the list of free objects
  slab->first_free_obj = obj->header.if_free.next;
  //we note the slab who owns this object
  obj->header.if_used.slab = slab;	

  return obj_data; 
}

static void free_obj_from_slab(struct Obj_header *obj)
{
  assert(obj != NULL);
  struct Userland_slab *slab = obj->header.if_used.slab;

  slab->free_objs_count++;
  obj->header.if_free.next = slab->first_free_obj;
  slab->first_free_obj = obj;
}


/********************************************************
 *                       Public methods
 *******************************************************/

/* Initialize a cache
 *
 *
 * Return cache on success, NULL otherwise
 */
struct Objs_cache *objs_cache_init(struct Objs_cache *cache,
				  size_t obj_size,
				  uint32_t pages_per_slab,
				  unsigned int slab_descr_on_slab)
{

  if(cache != NULL)
    {
      cache->obj_size = obj_size;
      cache->flags.slab_descr_on_slab = slab_descr_on_slab;
      cache->pages_per_slab = pages_per_slab;
      cache->slab_size = pages_per_slab*sysconf(_SC_PAGESIZE);
      cache->free_objs_count = 0;
      cache->used_objs_count = 0;
      cache->slab_count = 0;
      cache->free_slabs_count = 0;
      cache->partial_slabs_count = 0;
      cache->full_slabs_count = 0;
      cache->free_slabs = NULL;
      cache->partial_slabs = NULL;
      cache->full_slabs = NULL;
      cache->slab_descr_cache = NULL;
      
      if(slab_descr_on_slab)
	{
	  cache->actual_obj_size = obj_size + sizeof(struct Obj_header);
	  cache->wasted_memory_per_slab = (cache->slab_size - sizeof(struct Userland_slab)) % cache->actual_obj_size;
	  if((cache->slab_size - sizeof(struct Userland_slab)) / cache->actual_obj_size > 0)
	    {
	      cache->objs_per_slab = (cache->slab_size - sizeof(struct Userland_slab)) / cache->actual_obj_size;
	    }
	  else{
	    printf("The slab can't store at least one object ! (i.e : need more pages per slab)\n");
	    return NULL;
	  }
	}
      else{
	printf("Slab descriptor off-slab not implemented !\n");
	return NULL;
      }
    }
  else{
    return NULL;
  }

  return cache;
}

void objs_cache_destroy(struct Objs_cache *cache)
{
  if(cache != NULL)
    {
      struct Userland_slab *current, *next;

      current = cache->free_slabs;
      while(current != NULL)
	{
	  next = current->next;
	  destroy_slab(current, cache->slab_size);
	  current = next;
	}
      
      current = cache->partial_slabs;
      while(current != NULL)
	{
	  next = current->next;
	  destroy_slab(current, cache->slab_size);
	  current = next;
	}
      
      current = cache->full_slabs;
      while(current != NULL)
	{
	  next = current->next;
	  destroy_slab(current, cache->slab_size);
	  current = next;
	}
    }
}
			
void *objs_cache_alloc(struct Objs_cache *cache)
{
  void *allocated_obj = NULL;

  if(cache != NULL)
    {
      //we try to allocate a new object from a partially used slab
      if( !dlist_is_empty_generic(cache->partial_slabs) )
	{
	  struct Userland_slab *slab = cache->partial_slabs;
	  
	  allocated_obj = alloc_obj_from_slab(slab);
	  
	  assert(allocated_obj != NULL);
	  
	  cache->free_objs_count--;
	  cache->used_objs_count++;
	  
	  if(is_slab_full(slab))
	    {
	      //the slab is now full
	      
	      dlist_delete_head_generic(cache->partial_slabs, slab, prev, next);
	      dlist_push_head_generic(cache->full_slabs, slab, prev, next);

	      cache->partial_slabs_count--;
	      cache->full_slabs_count++;
	    }
	    
	}
      else{
	//we try to allocate a new object from a free slab

	//do we need to create a new free slab first ?
	if(dlist_is_empty_generic(cache->free_slabs))
	  {
	    cache->free_slabs = create_slab(cache->pages_per_slab, cache->actual_obj_size, cache->slab_descr_cache);

	    assert(cache->free_slabs != NULL);
	    
	    cache->free_slabs_count++;
	    cache->slab_count++;

	    cache->free_objs_count += cache->objs_per_slab;
	  }

	struct Userland_slab *slab = cache->free_slabs;

	allocated_obj = alloc_obj_from_slab(slab);

	assert(allocated_obj != NULL);

	cache->free_objs_count--;
	cache->used_objs_count++;

	if(!is_slab_full(slab))
	  {
	    //the slab is at least partially used but not full
	    
	    dlist_delete_head_generic(cache->free_slabs, slab, prev, next);
	    dlist_push_head_generic(cache->partial_slabs, slab, prev, next);

	    cache->free_slabs_count--;
	    cache->partial_slabs_count++;
	  }
	else {
	  //NB : this case only occurs when a slab can contain only one object
	  
	  dlist_delete_head_generic(cache->free_slabs, slab, prev, next);
	  dlist_push_head_generic(cache->full_slabs, slab, prev, next);

	  cache->free_slabs_count--;
	  cache->full_slabs_count++;
	}
      }
    }
  
  return allocated_obj;
}

void objs_cache_free(struct Objs_cache *cache, void *obj)
{
  if(cache != NULL)
    {
      if(obj != NULL)
	{
	  struct Obj_header *obj_h = (struct Obj_header*)((uintptr_t)obj - sizeof(struct Obj_header));
	  struct Userland_slab *slab = obj_h->header.if_used.slab;

	  assert(slab != NULL);

	  char slab_was_full = is_slab_full(slab);
	  free_obj_from_slab(obj_h);
	  char slab_is_now_free = is_slab_empty(slab, cache->objs_per_slab);

	  cache->free_objs_count++;
	  cache->used_objs_count--;
	  
	  /*We have 3 possible change of state for the slab :
	    full    -> partial
	    full    -> free (case where a slab contains 1 object)
	    partial -> free
	  */

	  if(!slab_was_full &&
	     slab_is_now_free)
	    {
	      //partial -> free
	      dlist_delete_el_generic(cache->partial_slabs, slab, prev, next);
	      dlist_push_head_generic(cache->free_slabs, slab, prev, next);
	      cache->partial_slabs_count--;
	      cache->free_slabs_count++;
	    }
	  else if(slab_was_full)
	    {
	      if(!slab_is_now_free)
		{
		  //full -> partial
		  dlist_delete_el_generic(cache->full_slabs, slab, prev, next);
		  dlist_push_head_generic(cache->partial_slabs, slab, prev, next);
		  cache->full_slabs_count--;
		  cache->partial_slabs_count++;
		}
	      else{
		//full -> free
		dlist_delete_el_generic(cache->full_slabs, slab, prev, next);
		dlist_push_head_generic(cache->free_slabs, slab, prev, next);
		cache->full_slabs_count--;
		cache->free_slabs_count++;
	      }
	    } 
	}
    }
  else{
    printf("Error : cache NULL as parameter for %s\n", __func__);
    exit(-1);
  }
}

/**********************************************
 *             Debug methods
 *********************************************/

void display_cache_info(const struct Objs_cache *cache)
{
  printf("\ndisplay_cache_info()\n" \
	 "obj_size : %lu\n" \
	 "actual_obj_size : %lu\n" \
	 "slab_descr_on_slab : %u\n" \
	 "pages_per_slab : %u\n" \
	 "slab_size : %lu\n" \
	 "objs_per_slab : %u\n" \
	 "wasted_memory_per_slab : %lu\n"\
	 "free_objs_count : %u\n" \
	 "used_objs_count : %u\n" \
	 "slab_count : %u\n" \
	 "free_slabs_count : %u\n" \
	 "partial_slabs_count : %u\n" \
	 "full_slabs_count : %u\n",
	 cache->obj_size,
	 cache->actual_obj_size,
	 cache->flags.slab_descr_on_slab,
	 cache->pages_per_slab,
	 cache->slab_size,
	 cache->objs_per_slab,
	 cache->wasted_memory_per_slab,
	 cache->free_objs_count,
	 cache->used_objs_count,
	 cache->slab_count,
	 cache->free_slabs_count,
	 cache->partial_slabs_count,
	 cache->full_slabs_count);
  printf("\n");
}

void display_slab_info(const struct Userland_slab *slab)
{
  printf("\ndisplay_slab_info()\n"\
	 "free_objs_count : %u\n",slab->free_objs_count);
  printf("\n");
}

struct Userland_slab *get_owning_slab(void *obj)
{
  struct Obj_header *obj_h = (struct Obj_header *)((uintptr_t)obj - sizeof(struct Obj_header));
  return obj_h->header.if_used.slab;
}
