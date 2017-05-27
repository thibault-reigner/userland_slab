#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#include <sys/mman.h>

#include "slab.h"
#include "queue.h"

#define DEFAULT_MAX_FREE_SLABS_ALLOWED 5

#define ROUNDUP(x,align) ({ ((x/align) + (x % align ? 1UL : 0UL))*align;})
#define ROUNDDOWN(x, align) ({ (x/align)*align;})
#define MAX(a,b) (((a) > (b))? (a) : (b))

static struct Obj * initialize_page_free_objs_list(void *pg,
						   size_t pg_sz,
						   struct Obj *first_obj,
						   size_t obj_sz);
static struct Userland_slab * create_slab(unsigned int pgs_per_slab,
					  size_t pg_sz,
					  size_t actual_obj_sz,
					  struct Objs_cache *cache_slab_descr);
static void destroy_slab(struct Userland_slab *slab, size_t slab_sz);
static void * alloc_obj_from_slab(struct Userland_slab *slab);
static void free_obj_from_slab(struct Userland_slab *slab, struct Obj *obj);
static struct Userland_slab * get_owning_slab(void *obj, size_t pg_sz);

/*******************************************************
                        Private data
*******************************************************/

/* The following cache cache_Userland_slab is used internly to allocate
   the objects Userland_slab each time a new slab has to be created for
   an objects cache which do not require the Userland_slab structures to
   be contained by the new slab itself.

   To break the recursivity induces by the case where we have to 
   allocate a new slab for the cache cache_Userland_slab, its slabs will
   contains their descriptors (SLAB_DESCR_ON_SLAB).
*/

#define CACHE_USERLAND_SLAB_PAGES_PER_SLAB 1 

//cache used to allocate Userland_slab objects
static struct Objs_cache cache_Userland_slab;

/********************************************************
 *                       Private methods
 *******************************************************/

static struct Userland_slab * get_owning_slab(void *obj, size_t pg_sz)
{
  return *((struct Userland_slab**)ROUNDDOWN((uintptr_t)obj, pg_sz)); 
}

/* Initialize the linked list of free objects of a given page.
 * The first free objects in the page (all the following
 * objects are assumed to be free too) is given as parameter
 * first_obj.
 * Return the last object of the linked list of free
 * objects.
 */
static struct Obj* initialize_page_free_objs_list(void *pg,
						  size_t pg_size,
						  struct Obj *first_obj,
						  size_t obj_size)
{

  assert(pg != NULL);
  assert(first_obj != NULL);
  assert((uintptr_t)pg <= (uintptr_t)first_obj);
  assert((uintptr_t)first_obj - (uintptr_t)pg <= pg_size);
  
  struct Obj *current_obj = first_obj;
  struct Obj *next_obj    = first_obj;

  while ((uintptr_t)next_obj + obj_size - (uintptr_t)pg <= pg_size) {
    next_obj = (struct Obj*)((uintptr_t)current_obj + obj_size);
    current_obj->header.if_free.next = next_obj;

    if ((uintptr_t)next_obj + obj_size - (uintptr_t)pg <= pg_size)
      current_obj = next_obj;
  }
  
  current_obj->header.if_free.next = NULL;

  return current_obj;
}

/* Create a new slab to be added to a cache.
 * pgs_per_slab : the number of pages used by this slab
 * pg_sz : the size in bytes of a page
 * obj_sz : (actual) size of an object
 * cache_slab_descr : if non null, the cache from where to allocate the slab descriptor
 * instead of storing it at the beginning of the slab.
 * Return this adress of the new slab's descriptor if successful, NULL otherwise
 */
static struct Userland_slab *create_slab(unsigned int pgs_per_slab,
					 size_t pg_sz,
					 size_t obj_sz,
					 struct Objs_cache *cache_slab_descr)
{
  assert(pgs_per_slab > 0);

  size_t pg_metadata_sz = sizeof(struct Userland_slab *);
  int on_slab_descriptor = (cache_slab_descr == NULL);
  
  struct Userland_slab *new_slab_descr = NULL;
  size_t slab_sz = pgs_per_slab*pg_sz;


  void *new_slab_pgs = mmap(NULL,
			    slab_sz,
			    PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, 
			    -1,
			    0);
  //NB: the pages don't have to be cleared since MAP_ANONYMOUS flag implies they are initialised to 0
  
  if (new_slab_pgs == MAP_FAILED)
    return NULL;

  if (!on_slab_descriptor) {
    //off-slab slab descriptor
    new_slab_descr = objs_cache_alloc(cache_slab_descr);

    if (new_slab_descr == NULL) {
      munmap(new_slab_pgs, slab_sz);
      return NULL;
    }
  }
  else {
    //on-slab slab descriptor
    //the slab descriptor is on the first page of the slab
    new_slab_descr = (struct Userland_slab*)((uintptr_t)new_slab_pgs + pg_metadata_sz);
  }

  new_slab_descr->pages = new_slab_pgs;
  
  //At the beginning of each page we define a pointer to the slab descriptor to which this page belongs
  struct Userland_slab **ptr = new_slab_pgs;
  for (unsigned int i = 0; i < pgs_per_slab; i++) {
    *ptr = new_slab_descr;
    ptr = (struct Userland_slab **)((uintptr_t)ptr + pg_sz);
  }
  
  unsigned int free_objs_first_pg = (pg_sz - pg_metadata_sz - (on_slab_descriptor ? sizeof(struct Userland_slab) : 0)) / obj_sz;
  unsigned int free_objs_pg = (pg_sz - pg_metadata_sz) / obj_sz;

  new_slab_descr->free_objs_count = free_objs_first_pg + (pgs_per_slab - 1) * free_objs_pg;

  new_slab_descr->first_free_obj = (struct Obj*)((uintptr_t)new_slab_pgs + pg_metadata_sz + (on_slab_descriptor ? sizeof(struct Userland_slab) : 0));
  new_slab_descr->objs = new_slab_descr->first_free_obj;

  
  //we set up the linked list of free objects

  struct Obj *current_obj = new_slab_descr->first_free_obj;
  struct Obj *last_obj = NULL;
  
  void *pg = new_slab_pgs;
  
  for (unsigned int i = 1; i <= pgs_per_slab; i++) {
    last_obj = initialize_page_free_objs_list(pg,
					      pg_sz,
					      current_obj,
					      obj_sz);
    if (i < pgs_per_slab) {
      pg = (void*)((uintptr_t)pg + pg_sz);
      current_obj = (struct Obj*)((uintptr_t)pg + pg_metadata_sz);
      last_obj->header.if_free.next = current_obj;
    }
  }

  return new_slab_descr;
}

static void destroy_slab(struct Userland_slab *slab, size_t slab_sz)
{
  assert(slab != NULL);
  munmap(slab, slab_sz);
}


static void *alloc_obj_from_slab(struct Userland_slab *slab)
{
  assert(slab != NULL);
  assert(slab->first_free_obj != NULL);
  
  assert(slab->free_objs_count > 0);
	  
  struct Obj *obj =slab->first_free_obj;

  slab->free_objs_count--;
      
  //remove this object from the list of free objects
  slab->first_free_obj = obj->header.if_free.next;
  obj->header.if_free.next = NULL;

  return obj; 
}

static void free_obj_from_slab(struct Userland_slab *slab, struct Obj *obj)
{
  assert(slab != NULL);
  assert(obj != NULL);

  slab->free_objs_count++;
  obj->header.if_free.next = slab->first_free_obj;
  slab->first_free_obj = obj;
}

static void default_slab_freeing_policy(struct Objs_cache *cache)
{
  if (cache != NULL) {
    struct Userland_slab *slab = cache->free_slabs;
    for (;cache->free_slabs_count > DEFAULT_MAX_FREE_SLABS_ALLOWED; cache->free_slabs_count--) {
      slab = dlist_pop_head_generic(cache->free_slabs, prev, next);
      destroy_slab(slab, cache->slab_size);
    }
  }
}

/********************************************************
 *                       Public methods
 *******************************************************/


int slab_allocator_init(void)
{
  struct Objs_cache *ptr = _objs_cache_init(&cache_Userland_slab,
					    sizeof(struct Userland_slab),
					    CACHE_USERLAND_SLAB_PAGES_PER_SLAB,
					    SLAB_DESCR_ON_SLAB,
					    NULL,
					    NULL);
  return (ptr != NULL);
}

void slab_allocator_destroy(void)
{
  objs_cache_destroy(&cache_Userland_slab);
}

/* Initialize a cache
 *
 *
 * Return cache on success, NULL otherwise
 */
struct Objs_cache * objs_cache_init(struct Objs_cache *cache,
				    size_t obj_size,
				    void (*ctor)(void *))
{
  return _objs_cache_init(cache,
			  obj_size,
			  1,
			  0,
			  ctor,
			  NULL);
}

struct Objs_cache * _objs_cache_init(struct Objs_cache *cache,
				     size_t obj_size,
				     unsigned int pages_per_slab,
				     unsigned int flags,
				     void (*ctor)(void *),
				     void (*slab_freeing_policy)(struct Objs_cache*))
{

  if (cache == NULL || pages_per_slab == 0)
    return NULL;
  
  cache->obj_size = obj_size;
  //when an object is free, its bytes are used as a pointer to the next free object
  //so an object has to be at least the big enough to store this pointer
  cache->actual_obj_size = MAX(obj_size, sizeof(void*));
  cache->flags = flags;
  cache->ctor = ctor;

  if (slab_freeing_policy == NULL)
    cache->slab_freeing_policy = default_slab_freeing_policy;
  else
    cache->slab_freeing_policy = slab_freeing_policy;
  
  if ( !(flags & SLAB_DESCR_ON_SLAB))
    cache->cache_slab_descr = &cache_Userland_slab;
      
  cache->pages_per_slab = pages_per_slab;
  cache->page_size = sysconf(_SC_PAGESIZE);
  cache->slab_size = cache->pages_per_slab*cache->page_size;

  size_t pg_metadata_sz = sizeof(void*);
  unsigned int free_objs_first_pg = (cache->page_size - pg_metadata_sz - (flags & SLAB_DESCR_ON_SLAB ? sizeof(struct Userland_slab) : 0)) / cache->actual_obj_size;
  unsigned int free_objs_pg = (cache->page_size - pg_metadata_sz) / cache->actual_obj_size;
  
  cache->objs_per_slab = free_objs_pg + free_objs_first_pg * (cache->pages_per_slab - 1);;

  cache->wasted_memory_per_page = cache->page_size % cache->actual_obj_size;
  cache->wasted_memory_per_slab = cache->wasted_memory_per_page * cache->pages_per_slab;

  cache->free_objs_count = 0;
  cache->used_objs_count = 0;
  
  cache->slab_count = 0;
  cache->free_slabs_count = 0;
  cache->partial_slabs_count = 0;
  cache->full_slabs_count = 0;
      
  cache->free_slabs = NULL;
  cache->partial_slabs = NULL;
  cache->full_slabs = NULL;

  return cache;
}

void objs_cache_destroy(struct Objs_cache *cache)
{
  if (cache != NULL) {
    struct Userland_slab *current, *next;

    current = cache->free_slabs;
    while (current != NULL) {
      next = current->next;
      destroy_slab(current, cache->slab_size);
      current = next;
    }
      
    current = cache->partial_slabs;
    while (current != NULL) {
      next = current->next;
      destroy_slab(current, cache->slab_size);
      current = next;
    }
      
    current = cache->full_slabs;
    while (current != NULL) {
      next = current->next;
      destroy_slab(current, cache->slab_size);
      current = next;
    }
  }
}
			
void *objs_cache_alloc(struct Objs_cache *cache)
{
  void *allocated_obj = NULL;

  if (cache != NULL) {
    //we try to allocate a new object from a partially used slab
    if ( !dlist_is_empty_generic(cache->partial_slabs)) {
      struct Userland_slab *slab = cache->partial_slabs;
	  
      allocated_obj = alloc_obj_from_slab(slab);
	  
      assert(allocated_obj != NULL);
	  
      cache->free_objs_count--;
      cache->used_objs_count++;
	  
      if (is_slab_full(slab)) {
	//the slab is now full
	      
	dlist_delete_head_generic(cache->partial_slabs, slab, prev, next);
	dlist_push_head_generic(cache->full_slabs, slab, prev, next);

	cache->partial_slabs_count--;
	cache->full_slabs_count++;
      }
	    
    }
    else {
      //we try to allocate a new object from a free slab
	
      //do we need to create a new free slab first ?
      if (dlist_is_empty_generic(cache->free_slabs)) {	  
	if (cache->flags & SLAB_DESCR_ON_SLAB) {
	  cache->free_slabs = create_slab(cache->pages_per_slab,
					  cache->page_size,
					  cache->actual_obj_size,
					  NULL);
	}
	else {
	  cache->free_slabs = create_slab(cache->pages_per_slab,
					  cache->page_size,
					  cache->actual_obj_size,
					  cache->cache_slab_descr);
	}
	    
	assert(cache->free_slabs != NULL);
	    
	cache->free_slabs_count++;
	cache->slab_count++;

	cache->free_objs_count += cache->objs_per_slab;
      }

      struct Userland_slab *slab = cache->free_slabs;

      allocated_obj = alloc_obj_from_slab(slab);

      if (allocated_obj == NULL) {
	printf("Failed to allocate an object in %s (slab corrupted) !\n", __func__);
	return NULL;
      }
      
      cache->free_objs_count--;
      cache->used_objs_count++;

      if ( !is_slab_full(slab)) {
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

  if (cache->ctor != NULL)
    cache->ctor(allocated_obj);
  
  return allocated_obj;
}

void objs_cache_free(struct Objs_cache *cache, void *obj)
{
  
  if (cache != NULL && obj != NULL) {
    struct Userland_slab *slab = get_owning_slab(obj, cache->page_size);

    if (slab == NULL) {
      printf("Failed to free an object in %s (slab corrupted) !\n", __func__);
    }

    char slab_was_full = is_slab_full(slab);
    free_obj_from_slab(slab, obj);
    char slab_is_now_free = is_slab_empty(slab, cache->objs_per_slab);

    cache->free_objs_count++;
    cache->used_objs_count--;
	  
    /*We have 3 possible change of state for the slab :
      full    -> partial
      full    -> free (case where a slab contains 1 object)
      partial -> free
    */

    if ( !slab_was_full && slab_is_now_free) {
      //partial -> free
      dlist_delete_el_generic(cache->partial_slabs, slab, prev, next);
      dlist_push_head_generic(cache->free_slabs, slab, prev, next);
      cache->partial_slabs_count--;
      cache->free_slabs_count++;
    }
    else if (slab_was_full) {
      if ( !slab_is_now_free) {
	//full -> partial
	dlist_delete_el_generic(cache->full_slabs, slab, prev, next);
	dlist_push_head_generic(cache->partial_slabs, slab, prev, next);
	cache->full_slabs_count--;
	cache->partial_slabs_count++;
      }
      else {
	//full -> free
	dlist_delete_el_generic(cache->full_slabs, slab, prev, next);
	dlist_push_head_generic(cache->free_slabs, slab, prev, next);
	cache->full_slabs_count--;
	cache->free_slabs_count++;
      }
    }

    // Try to free some slabs
    cache->slab_freeing_policy(cache);
  }
  else {
    printf("Error : cache NULL as parameter for %s\n", __func__);
    exit(-1);
  }
}

/**********************************************
 *             Debug methods
 *********************************************/

void display_cache_info(const struct Objs_cache *cache)
{
  if (cache != NULL) {
    printf("\ndisplay_cache_info()\n" \
	   "obj_size : %lu\n" \
	   "actual_obj_size : %lu\n" \
	   "flags : %u\n" \
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
	   cache->flags,
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
}

void display_slab_info(const struct Userland_slab *slab)
{
  if (slab != NULL) {
    printf("\ndisplay_slab_info()\n"\
	   "free_objs_count : %u\n",slab->free_objs_count);
    printf("\n");
  }
}

