# Userland slab
Example of a small implementation in C of a slab allocator in user space.

## What is a slab allocator ?
The slab allocator is a memory allocator initialy designed for the Solaris kernel and since used in other kernels such as Linux (more details regarding the SLAB allocator in Linux : https://www.kernel.org/doc/gorman/html/understand/understand011.html).

The main idea behind the slab allocator is that most of the time, programs, especially kernels and drivers, allocate objects of the same size. 
Whereas usual memory allocators make no assumption on the size of allocated objects and hence require more metadata per object to keep note of the free and used bytes, the slab allocator allocates objects which all have the same fixed size, from a **cache**.

A cache of objects is a set of **slabs**, a slab can be seen a pack of objects which can be free or allocated. When you try to allocate an object from a cache, the cache first checks if there is a slab which contains a free object. If not, a new slab is allocated and then a free object is allocated.
A slab is composed of one or more virtual pages, on most system a page contains 4096 bytes.

## What are the advantages of the slab allocator ?
The slab allocator use a very low amount of metadata, in fact there are metadata for each slab but not for each object in the slab. It induces a huge gain of memory compared to the standard allocator of the C library (malloc()) for instance.

Allocation and desallocation of objects are also O(1). To be fair the worst case scenario for allocation is when the cache needs to allocate a new slab from the system. From a technical point of view, virtual pages are allocated with mmap() which relies on the kernel virtual memory allocator.


## How to use this program ?

The slab allocator has to be initialised once and for all before use by calling :
```c
int slab_allocator_init(void);
```

Similarly, all the memory allocated by the slab allocator can be freed by calling :
```c
void slab_allocator_destroy(void);
```

Once the slab allocator is initialised, one can use the following function to create a cache for a specific kind of object:
```c
struct Objs_cache * objs_cache_init(struct Objs_cache *cache,
				    size_t obj_size,
				    void (*ctor)(void *));
```

Example to create a cache to allocate **unsigned long** integers:
```c
struct Objs_cache a_cache;
objs_cache_init(&a_cache, sizeof(unsigned long), NULL)
```

Note one can specified a pointer to a constructor as third parameter to objs\_cache\_init(). This constructor function will be called during each object allocation, with the allocated object as parameter and allows a specific initialisation of object.

One can allocate/free objects from a cache using the two following functions, whose behavior is similar to malloc()/free()
```c
void * objs_cache_alloc(struct Objs_cache *cache);
void objs_cache_free(struct Objs_cache *cache, void *obj);
```

Once a cache has become useless, all the memory used by it can be freed by calling :
```c
void objs_cache_destroy(struct Objs_cache *cache);
```

## Example & benchmark

A main.c file is provided. It accepts a parameter to compare through an external software (e.g : top) the memory consumption between malloc() and the slab allocator.
Set the parameter as 1 to allocate with malloc(), 2 with the slab allocator.

Benchmark: 

On Archlinux x86_64, gcc 6.2.1<br>

Comparison of the memory consummed by the program (as showed by top) by allocating 1,000,000 objects with malloc() and the slab allocator.

|Size of an object in bytes|malloc()|slab allocator|
|:--------------------------:|--------:|--------------:|
| 8 | 38 Mio | 15.3 Mio|
| 16 | 38 Mio | 23.0 Mio |
| 32 | 53.4 Mio | 38.7 Mio |
| 48 | 68.7 Mio | 54.0 Mio |
| 64 | 83.9 Mio | 70.2 Mio |
| 128 | 145.0 Mio | 135.2 Mio |
| 256 | 266.9 Mio | 271.0 Mio |

One can observe that for small objects, the slab allocator outcompetes the memory allocator of the C library because the last one consumes metadata for each allocated objects. But as the size of objects increases the slab-allocator starts to be less efficient, this is due to several reasons :

specific to this implementation :
* objects have to be contained within one page, when objects are big, a huge part of a page can be wasted.

non-specific to this implementation :
* the default slab size is 1 page (4096 bytes) 

## Idea of improvement

The main performance issue of the slab allocator in user space is to find to which slab belongs a given object (especially when it has to be freed). If the Linux kernel for instance uses a dedicated structure (the array of physical pages descriptors) to reverse map (virtual address) --> (slab), it is not possible to do this in the user space.

There are at least two possible solutions to this problem :
* add a header to each object which contains a pointer to the slab which contains this object. This is detrimental when objects are small, for instance on a 64 bits system, it would add 8 bytes to the actual size of an object.
* add a pointer at the beginning of each page **(solution chosed for this implementation)** to the slab which owns this page. However objects have to be contained within one page which is detrimental for big objects.

It would be possible to implement both behaviors based on the size of objects to allocate.



