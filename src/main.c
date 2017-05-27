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
    <program> <alloc_type> <size>
    <alloc_type> = 1 - malloc based allocation
    <alloc_type> = 2 - slab based allocation
    <size> = size in bytes of the objects to allocate
  */
  
  if (argc < 3)
    {
      printf("Arguments expected !\n"\
	     "program <alloc_type> <size>\n"\
	     "<alloc_type> = 1 - malloc based allocation\n"\
	     "<alloc_type> = 2 - slab based allocation\n"\
	     "<size> = size in bytes of the objects to allocate\n");
      return 0;
    }

  void *array[N];
  size_t obj_size = strtol(argv[2], NULL, 10);

  if ( !obj_size){
    printf("The second parameter should be an unsigned integer (size in bytes of object).\n");
    return -1;
  }
  
  if (argv[1][0] == '1'){
    printf("Allocation of %d objects of size %lu with malloc()\n", N, obj_size);

    if (array == NULL){
      printf("Allocation failed\n");
      return -1;
    }
    
    for (int i = 0; i < N; i++){
      array[i] = malloc(obj_size);
    }
    
    getchar();
  }
  else {  
    printf("Allocation of %d objects of size %lu with the slab allocator\n", N, obj_size);
    
    struct Objs_cache cache;

    //we initialise the slab allocator once and for all
    if ( !slab_allocator_init()){
      printf("Error : slab allocator initialisation failed !\n");
      exit(-1);
    }

    //we initialise a cache to allocate objects of obj_size

    if ( !objs_cache_init(&cache, obj_size, NULL)) {
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

