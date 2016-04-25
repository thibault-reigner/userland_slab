#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>

#include "queue.h"
#include "slab.h"



int main()
{

  struct Objs_cache cache;

  objs_cache_init(&cache,sizeof(unsigned int),1,1);

  int *obj = objs_cache_alloc(&cache);
  objs_cache_free(&cache, obj);

  objs_cache_destroy(&cache);
  
  return 0;
}

