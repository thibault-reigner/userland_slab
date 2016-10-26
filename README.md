# Userland slab
Example of a small implementation in C of a slab allocator in user space.

## What is a slab allocator ?
The slab allocator is a memory allocator initialy designed for the Solaris kernel and since used in other kernels such as Linux (more details regarding the SLAB allocator in Linux : https://www.kernel.org/doc/gorman/html/understand/understand011.html).

The main idea behind the slab allocator is that most of the time, programs, especially kernels and drivers, allocate objects of the same size. 
Whereas usual memory allocators make no assumption on the size of allocated objects and hence require more metadata per object to keep note of the free and used bytes, the slab allocator allocates objects which all have the same fixed size, from a **cache**.

A cache of objects is a set of **slabs**, a slab can be seen a pack of objects which can be free or allocated. . When you try to allocate an object from a cache, the cache first checks if there is a slab which contains a free object. If not a new slab is allocated and then a free object is allocated.

## What are the advantages of the slab allocator ?
The slab allocator use a very low amount of metadata, in fact there are metadata for each slab but not for each object in the slab. It induces a huge gain of memory compared to the standard allocator of the C library (malloc()) for instance.

Allocation and desallocation of objects are also O(1). To be fair the worst case scenario for allocation is when the cache needs to allocate a new slab from the system. From a technical point of view, virtual pages are allocated with mmap() which relies on the kernel memory virtual memory allocator.


## How to use this program ?

The slab allocator has to be initialised once and for all before use by calling :
'''c
int slab\_allocator\_init(void);
'''

Similarly, all the memory allocated by the slab allocator can be freed by calling :
'''c
void slab\_allocator\_destroy(void);
'''

Once the slab allocator is initialised, one can call use the following function to create a cache :
'''c
struct Objs\_cache * objs\_cache\_init(struct Objs\_cache *cache,
				    size_t obj\_size,
				    void (*ctor)(void *));
'''

Example to create a cache to allocate **unsigned long** integers:
'''c
struct Objs\_cache a_cache;
objs\_cache\_init(&a\_cache, sizeof(unsigned long), NULL)
'''

Note one can specified a pointer to a constructor as third parameter to objs\_cache\_init(). This constructor function will be called during each object allocation, with the allocated object as parameter and allows a specific initialisation of object.

One can allocate/free objects from a cache using the two following functions, whose behavior is similar to malloc()/free()
'''c
void * objs\_cache_alloc(struct Objs\_cache *cache);
void objs\_cache\_free(struct Objs\_cache *cache, void *obj);
'''

Once a cache has become useless, all the memory used by it can be freed by calling :
'''c
void objs\_cache\_destroy(struct Objs\_cache *cache);
'''

## Example & benchmark

A main.c file is provided. It accepts a parameter to compare through an external software (e.g : top) the memory consumption between malloc() and the slab allocator.
Set the parameter as 1 to allocate with malloc(), 2 with the slab allocator.

Benchmark: 
On Archlinux x86_64, gcc 6.2.1
Allocation of 1000000 long integers with malloc() : 38 Mio
Allocation of 1000000 long integers with slab allocator : 15.3 Mio


