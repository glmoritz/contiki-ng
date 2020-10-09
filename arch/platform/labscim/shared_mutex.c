#include "shared_mutex.h"
#include <errno.h> // errno, ENOENT
#include <fcntl.h> // O_RDWR, O_CREATE
#include <linux/limits.h> // NAME_MAX
#include <sys/mman.h> // shm_open, shm_unlink, mmap, munmap,
                      // PROT_READ, PROT_WRITE, MAP_SHARED, MAP_FAILED
#include <unistd.h> // ftruncate, close
#include <stdio.h> // perror
#include <stdlib.h> // malloc, free
#include <string.h> // strcpy

shared_mutex_t shared_mutex_init(char *name, char clear)
{
  shared_mutex_t mutex = {NULL, NULL, NULL, 0, NULL, 0};
  errno = 0;

  // Open existing shared memory object, or create one.
  // Two separate calls are needed here, to mark fact of creation
  // for later initialization of pthread mutex.
  mutex.shm_fd = shm_open(name, O_RDWR, 0666);
  if((errno != ENOENT) && clear)
  {
	  if (close(mutex.shm_fd)) {
		  perror("close");
		  return mutex;
	  }
	  if (shm_unlink(name))
	  {
	      perror("shm_unlink");
	      return mutex;
	  }
  }

  if ((errno == ENOENT) || clear)
  {
    mutex.shm_fd = shm_open(name, O_RDWR|O_CREAT, 0666);
    mutex.created = 1;
  }
  if (mutex.shm_fd == -1) {
    perror("shm_open");
    return mutex;
  }

  // Truncate shared memory segment so it would contain
  // pthread_mutex_t.
  if (ftruncate(mutex.shm_fd, sizeof(pthread_mutex_t)+2*sizeof(pthread_cond_t)) != 0) {
    perror("ftruncate");
    return mutex;
  }

  // Map pthread mutex into the shared memory.
  void *addr = mmap(
    NULL,
	sizeof(pthread_mutex_t)+2*sizeof(pthread_cond_t),
    PROT_READ|PROT_WRITE,
    MAP_SHARED,
    mutex.shm_fd,
    0
  );
  if (addr == MAP_FAILED) {
    perror("mmap");
    return mutex;
  }
  pthread_mutex_t *mutex_ptr = (pthread_mutex_t *)addr;
  pthread_cond_t* more_ptr = (pthread_cond_t *)(addr+sizeof(pthread_mutex_t));
  pthread_cond_t* less_ptr = (pthread_cond_t *)(addr+sizeof(pthread_mutex_t)+sizeof(pthread_cond_t));

  // If shared memory was just initialized -
  // initialize the mutex as well.
  if (mutex.created) {
    pthread_mutexattr_t attr;
    pthread_condattr_t cattr;

    if (pthread_mutexattr_init(&attr)) {
      perror("pthread_mutexattr_init");
      return mutex;
    }
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) {
      perror("pthread_mutexattr_setpshared");
      return mutex;
    }
    if (pthread_mutex_init(mutex_ptr, &attr)) {
      perror("pthread_mutex_init");
      return mutex;
    }

    if (pthread_condattr_init(&cattr)) {
        perror("pthread_condattr_init");
        return mutex;
    }
    if (pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED)) {
        perror("pthread_condattr_setpshared");
        return mutex;
    }


    if (pthread_cond_init(more_ptr, &cattr))
    {
    	perror("pthread_cond_init");
    	return mutex;
    }
    if (pthread_cond_init(less_ptr, &cattr))
    	{
    	perror("pthread_cond_init");
    	return mutex;
    }
  }
  mutex.mutex = mutex_ptr;
  mutex.more = more_ptr;
  mutex.less = less_ptr;
  mutex.name = (char *)malloc(NAME_MAX+1);
  strcpy(mutex.name, name);
  return mutex;
}

int shared_mutex_close(shared_mutex_t mutex) {
  if (munmap((void *)mutex.mutex, sizeof(pthread_mutex_t)+2*sizeof(pthread_cond_t))) {
    perror("munmap");
    return -1;
  }
  mutex.mutex = NULL;
  if (close(mutex.shm_fd)) {
    perror("close");
    return -1;
  }
  mutex.shm_fd = 0;
  free(mutex.name);
  return 0;
}

int shared_mutex_destroy(shared_mutex_t mutex) {
  if ((errno = pthread_mutex_destroy(mutex.mutex))) {
    perror("pthread_mutex_destroy");
    return -1;
  }
  if ((errno = pthread_cond_destroy(mutex.more))) {
	  perror("pthread_cond_destroy");
	  return -1;
  }

  if ((errno = pthread_cond_destroy(mutex.less))) {
	  perror("pthread_cond_destroy");
	  return -1;
  }

  if (munmap((void *)mutex.mutex, sizeof(pthread_mutex_t)+2*sizeof(pthread_cond_t))) {
    perror("munmap");
    return -1;
  }
  mutex.mutex = NULL;
  if (close(mutex.shm_fd)) {
    perror("close");
    return -1;
  }
  mutex.shm_fd = 0;
  if (shm_unlink(mutex.name)) {
    perror("shm_unlink");
    return -1;
  }
  free(mutex.name);
  return 0;
}
