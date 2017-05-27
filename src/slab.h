#ifndef USERLAND_SLAB_H
#define USERLAND_SLAB_H

#include <stdint.h>


#define COMPACT_OBJS 1
#define SLAB_DESCR_ON_SLAB 2

#define is_slab_full(slab)			\
  ((slab)->free_objs_count == 0)

#define is_slab_empty(slab, max_objs)		\
  ((slab)->free_objs_count == (max_objs))


struct Obj{
  union {
    struct{
      struct Obj *next; //pointer to the next free object
    }if_free;
    struct{
      char _data_first_bytes[sizeof(struct Obj *)];
    }if_used;
  }header;

  char _data[];
};


struct Userland_slab{
  void *pages;
  unsigned int free_objs_count;
  //size_t wasted_memory;

  struct Obj *first_free_obj;
  struct Obj *objs;
  
  struct Userland_slab *prev,*next;
};


struct Objs_cache{
  size_t obj_size;
  size_t actual_obj_size;  //size of the object + size of its header

  unsigned int flags;

  void (*ctor)(void *);

  void (*slab_freeing_policy)(struct Objs_cache *);
  
  struct Objs_cache *cache_slab_descr;
  
  unsigned int pages_per_slab;
  size_t page_size;
  size_t slab_size;

  unsigned int objs_per_page;
  unsigned int objs_per_slab;

  size_t wasted_memory_per_page;
  size_t wasted_memory_per_slab;
  
  unsigned int free_objs_count;
  unsigned int used_objs_count;
  
  unsigned int slab_count;
  unsigned int free_slabs_count, partial_slabs_count, full_slabs_count;
  
  struct Userland_slab *free_slabs, *partial_slabs, *full_slabs;
};


int slab_allocator_init(void);
void slab_allocator_destroy(void);

struct Objs_cache * objs_cache_init(struct Objs_cache *cache,
				    size_t obj_size,
				    void (*ctor)(void *));
struct Objs_cache * _objs_cache_init(struct Objs_cache *cache,
				     size_t obj_size,
				     unsigned int pages_per_slab,
				     unsigned int flags,
				     void (*ctor)(void *),
				     void (*slab_freeing_policy)(struct Objs_cache*));
void objs_cache_destroy(struct Objs_cache *cache);
void * objs_cache_alloc(struct Objs_cache *cache);
void objs_cache_free(struct Objs_cache *cache, void *obj);


void display_cache_info(const struct Objs_cache *cache);
void display_slab_info(const struct Userland_slab *slab);

#endif
