#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>

#include "queue.h"
#include "slab.h"

#define N 1000000


int main(int argc, char **argv)
{

  /*You can compare the memory consumed by this program.
    <program> <param>
    <param> = 1 - malloc based allocation
    <param> = 2 - slab based allocation
  */
  
  if (argc < 2)
    return 0;

  long *array[N];
    
  if (argv[1][0] == '1'){
    printf("Allocation of %d long integers with malloc()\n", N);

    if (array == NULL){
      printf("Allocation failed\n");
      return -1;
    }
    
    for (int i = 0; i < N; i++){
      array[i] = malloc(sizeof(long));
      *array[i] = i;
    }
    
    getchar();
  }
  else {  
    printf("Allocation of %d long integers with the slab allocator\n", N);
    
    struct Objs_cache cache;

    //we initialise the slab allocator once and for all
    if ( !slab_allocator_init()){
      printf("Error : slab allocator initialisation failed !\n");
      exit(-1);
    }

    //we initialise a cache to allocate objects of size sizeof(unsigned int)

    if ( !objs_cache_init(&cache,sizeof(long), NULL)) {
      printf("Error : cache initialisation failed !\n");
      exit(-1);
    }

    for (int i = 0; i < N;i++){
      array[i] = objs_cache_alloc(&cache);
      if ( !array[i]){
	printf("Failed to allocate an object from a cache !\n");
	exit(-1);
      }
    }

    getchar();

    //We do something with the allocated objects
    // ...
  
    objs_cache_destroy(&cache);

    slab_allocator_destroy();
  }

  return 0;
}

