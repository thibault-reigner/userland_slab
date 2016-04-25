#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>

#include "queue.h"

#include "slab.h"




#if defined(_WIN32)
#include <Windows.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#define _XOPEN_SOURCE 500
#include <unistd.h>	/* POSIX flags */
#include <time.h>	/* clock_gettime(), time() */
#include <sys/time.h>	/* gethrtime(), gettimeofday() */

#if defined(__MACH__) && defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#else
#error "Unable to define getRealTime( ) for an unknown OS."
#endif


/**
 * Returns the real time, in seconds, or -1.0 if an error occurred.
 *
 * Time is measured since an arbitrary and OS-dependent start time.
 * The returned real time is only useful for computing an elapsed time
 * between two calls to this function.
 */
double getRealTime( )
{
#if defined(_WIN32)
    FILETIME tm;
    ULONGLONG t;
#if defined(NTDDI_WIN8) && NTDDI_VERSION >= NTDDI_WIN8
    /* Windows 8, Windows Server 2012 and later. ---------------- */
    GetSystemTimePreciseAsFileTime( &tm );
#else
    /* Windows 2000 and later. ---------------------------------- */
    GetSystemTimeAsFileTime( &tm );
#endif
    t = ((ULONGLONG)tm.dwHighDateTime << 32) | (ULONGLONG)tm.dwLowDateTime;
    return (double)t / 10000000.0;
    
#elif (defined(__hpux) || defined(hpux)) || ((defined(__sun__) || defined(__sun) || defined(sun)) && (defined(__SVR4) || defined(__svr4__)))
    /* HP-UX, Solaris. ------------------------------------------ */
    return (double)gethrtime( ) / 1000000000.0;
    
#elif defined(__MACH__) && defined(__APPLE__)
    /* OSX. ----------------------------------------------------- */
    static double timeConvert = 0.0;
    if ( timeConvert == 0.0 )
    {
        mach_timebase_info_data_t timeBase;
        (void)mach_timebase_info( &timeBase );
        timeConvert = (double)timeBase.numer /
        (double)timeBase.denom /
        1000000000.0;
    }
    return (double)mach_absolute_time( ) * timeConvert;
    
#elif defined(_POSIX_VERSION)
    /* POSIX. --------------------------------------------------- */
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
    {
        struct timespec ts;
#if defined(CLOCK_MONOTONIC_PRECISE)
        /* BSD. --------------------------------------------- */
        const clockid_t id = CLOCK_MONOTONIC_PRECISE;
#elif defined(CLOCK_MONOTONIC_RAW)
        /* Linux. ------------------------------------------- */
        const clockid_t id = CLOCK_MONOTONIC_RAW;
#elif defined(CLOCK_HIGHRES)
        /* Solaris. ----------------------------------------- */
        const clockid_t id = CLOCK_HIGHRES;
#elif defined(CLOCK_MONOTONIC)
        /* AIX, BSD, Linux, POSIX, Solaris. ----------------- */
        const clockid_t id = CLOCK_MONOTONIC;
#elif defined(CLOCK_REALTIME)
        /* AIX, BSD, HP-UX, Linux, POSIX. ------------------- */
        const clockid_t id = CLOCK_REALTIME;
#else
        const clockid_t id = (clockid_t)-1;	/* Unknown. */
#endif /* CLOCK_* */
        if ( id != (clockid_t)-1 && clock_gettime( id, &ts ) != -1 )
        return (double)ts.tv_sec +
        (double)ts.tv_nsec / 1000000000.0;
        /* Fall thru. */
    }
#endif /* _POSIX_TIMERS */
    
    /* AIX, BSD, Cygwin, HP-UX, Linux, OSX, POSIX, Solaris. ----- */
    struct timeval tm;
    gettimeofday( &tm, NULL );
    return (double)tm.tv_sec + (double)tm.tv_usec / 1000000.0;
#else
    return -1.0;		/* Failed. */
#endif
}





#define N 100000000

struct Userland_slab *get_owning_slab(void *obj);


int main()
{

  struct Objs_cache cache;
  objs_cache_init(&cache,sizeof(unsigned int),1,1);
  display_cache_info(&cache);

  unsigned int **els1 = calloc(N, sizeof(int*));

  if(els1 == NULL)
    {
      printf("Not enough memory\n");
      exit(-1);
    }

  double start = getRealTime();
  for(int i=0;i<N;i++)
    {
      els1[i] = malloc(sizeof(unsigned int));
      if(els1[i] == NULL)
  	{
  	  printf("Allocation failed (i= %d)\n",i);
  	  exit(-1);
  	}
    }
  printf("Temps malloc() : %.3f\n", getRealTime() - start);
  for(int i=0;i<N;i++)
    free(els1[i]);
  

  start = getRealTime();
  for(int i=0;i<N;i++)
    {
      els1[i] = objs_cache_alloc(&cache);
      if(els1[i] == NULL)
  	{
  	  printf("Allocation failed (i= %d)\n",i);
  	  exit(-1);
  	}
    }
    printf("Temps objs_cache_alloc() : %.3f\n", getRealTime() - start);
  for(int i=0;i<N;i++)
    objs_cache_free(&cache,els1[i]);

  
  /* struct Objs_cache cache, cache2; */
  /* void *obj1, *obj2, *obj3; */
  /* printf("size of object header : %lu\n",sizeof(struct Obj_header)); */
  /* printf("size of slab descriptor : %lu\n",sizeof(struct Userland_slab)); */
  /* objs_cache_init(&cache,1200,1,1); */
  /* display_cache_info(&cache); */
  
  /* obj1 = objs_cache_alloc(&cache); */
  /* printf("1 allocated object : %lu %lu \n",(uintptr_t)obj1,(uintptr_t)get_owning_slab(obj1)); */
  /* obj2 = objs_cache_alloc(&cache); */
  /* printf("2 allocated object : %lu %lu\n",(uintptr_t)obj2,(uintptr_t)get_owning_slab(obj2)); */
  /* obj3 = objs_cache_alloc(&cache); */
  /* printf("3 allocated object : %lu %lu\n",(uintptr_t)obj3,(uintptr_t)get_owning_slab(obj3)); */
  /* display_cache_info(&cache); */
  /* objs_cache_free(&cache, obj3); */
  /* display_cache_info(&cache); */
  /* obj3 = objs_cache_alloc(&cache); */
  /* printf("4 allocated object : %lu %lu\n",(uintptr_t)obj3,(uintptr_t)get_owning_slab(obj3)); */
  /* display_cache_info(&cache); */

  /* void *obj4 = objs_cache_alloc(&cache); */
  /* printf("5 allocated object : %lu %lu\n",(uintptr_t)obj4,(uintptr_t)get_owning_slab(obj4)); */
  /* display_cache_info(&cache); */
  /* struct Userland_slab *slab = get_owning_slab(obj4); */
  /* display_slab_info(slab); */
  /* objs_cache_free(&cache,obj4); */
  /* display_slab_info(slab); */
  /* display_cache_info(&cache); */
  /* display_slab_info(get_owning_slab(obj3)); */

  /* objs_cache_destroy(&cache); */
  return 0;
}

