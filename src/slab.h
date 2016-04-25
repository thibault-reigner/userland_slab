#ifndef USERLAND_SLAB_H
#define USERLAND_SLAB_H

#include <stdint.h>

#define is_slab_full(slab)			\
  ((slab)->free_objs_count == 0)

#define is_slab_empty(slab, max_objs)		\
  ((slab)->free_objs_count == (max_objs))


struct Obj_header{
  union {
    struct{
      struct Obj_header *next; //pointer to the next free object
    }if_free;
    struct{
      struct Userland_slab *slab; //the slab which contains this object
    }if_used;
  }header;
};


struct Userland_slab{
  unsigned int free_objs_count;
  //size_t wasted_memory;
  
  struct Obj_header *first_free_obj;
  struct Obj_header *objs;
  
  struct Userland_slab *prev,*next;
};


struct Objs_cache{
  size_t obj_size;
  size_t actual_obj_size;  //size of the object + size of its header

  struct{
    //unsigned int compact_objs :1;
    unsigned int slab_descr_on_slab :1;
  }flags;
  
  unsigned int pages_per_slab;
  size_t slab_size;
  unsigned int objs_per_slab;
  size_t wasted_memory_per_slab;
  
  unsigned int free_objs_count;
  unsigned int used_objs_count;
  
  unsigned int slab_count;
  unsigned int free_slabs_count, partial_slabs_count, full_slabs_count;
  
  struct Userland_slab *free_slabs, *partial_slabs, *full_slabs;

  struct Objs_cache *slab_descr_cache;  //used in case flags.slab_descr_on_slab == 0
};



struct Objs_cache *objs_cache_init(struct Objs_cache *cache, size_t obj_size, uint32_t pages_per_slab, unsigned int slab_descr_on_slab);
void objs_cache_destroy(struct Objs_cache *cache);
void *objs_cache_alloc(struct Objs_cache *cache);
void objs_cache_free(struct Objs_cache *cache, void *obj);

#endif
