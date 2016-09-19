#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>

#include "queue.h"
#include "slab.h"


int main()
{
  struct Objs_cache cache;

  //we initialise the slab allocator once and for all
  if ( !slab_allocator_init()) {
    printf("Error : slab allocator initialisation failed !\n");
    exit(-1);
  }

  //we initialise a cache to allocate objects of size sizeof(unsigned int)

  if ( !objs_cache_init(&cache,sizeof(unsigned int),2)) {
    printf("Error : cache initialisation failed !\n");
    exit(-1);
  }


  unsigned int *obj = objs_cache_alloc(&cache);

  if ( !obj) {
    printf("Failed to allocate an object from a cache !\n");
    exit(-1);
  }
  

  //We do something with the allocated object
  // ...
  
  objs_cache_free(&cache, obj);

  objs_cache_destroy(&cache);

  return 0;
}

