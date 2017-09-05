//
// CS252: MyMalloc Project
//
// The current implementation gets memory from the OS
// every time memory is requested and never frees memory.
//
// You will implement the allocator as indicated in the handout.
// 
// Also you will need to add the necessary locking mechanisms to
// support multi-threaded programs.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include "MyMalloc.h"

#define ALLOCATED 1
#define NOT_ALLOCATED 0
#define ARENA_SIZE 2097152

pthread_mutex_t mutex;

static bool verbose = false;

extern void atExitHandlerInC()
{
  if (verbose)
    print();
}

static void * getMemoryFromOS(size_t size)
{
  // Use sbrk() to get memory from OS
  _heapSize += size;
 
  void *mem = sbrk(size);

  if(!_initialized){
      _memStart = mem;
  }

  return mem;
}


/*
 * @brief retrieves a new 2MB chunk of memory from the OS
 * and adds "dummy" boundary tags
 * @param size of the request
 * @return a FreeObject pointer to the beginning of the chunk
 */
static FreeObject * getNewChunk(size_t size)
{
  void *mem = getMemoryFromOS(size);

  // establish fence posts
  BoundaryTag *fencePostHead = (BoundaryTag *)mem;
  setAllocated(fencePostHead, ALLOCATED);
  setSize(fencePostHead, 0);

  char *temp = (char *)mem + size - sizeof(BoundaryTag);
  BoundaryTag *fencePostFoot = (BoundaryTag *)temp;
  setAllocated(fencePostFoot, ALLOCATED);
  setSize(fencePostFoot, 0);
 
  return (FreeObject *)((char *)mem + sizeof(BoundaryTag));
}

/**
 * @brief If no blocks have been allocated, get more memory and 
 * set up the free list
 */
static void initialize()
{
  verbose = true;

  pthread_mutex_init(&mutex, NULL);

  // print statistics at exit
  atexit(atExitHandlerInC);

  FreeObject *firstChunk = getNewChunk(ARENA_SIZE);

  // initialize the list to point to the firstChunk
  _freeList = &_freeListSentinel;
  setSize(&firstChunk->boundary_tag, ARENA_SIZE - (2*sizeof(BoundaryTag))); // ~2MB
  firstChunk->boundary_tag._leftObjectSize = 0;
  setAllocated(&firstChunk->boundary_tag, NOT_ALLOCATED);

  // link list pointer hookups
  firstChunk->free_list_node._next = _freeList;
  firstChunk->free_list_node._prev = _freeList;
  _freeList->free_list_node._prev = firstChunk;
  _freeList->free_list_node._next = firstChunk;

  _initialized = 1;
}

/**
 * @brief TODO: PART 1
 * This function should perform allocation to the program appropriately,
 * giving pieces of memory that large enough to satisfy the request. 
 * Currently, it just sbrk's memory every time.
 *
 * @param size size of the request
 *
 * @return pointer to the first usable byte in memory for the requesting
 * program
 */
static void * allocateObject(size_t size)
{
  // Make sure that allocator is initialized

    	if (!_initialized)
    		initialize();
       
	pthread_mutex_unlock(&mutex);
	

 
	//Step 2
	// real_size = roundup8(req size) + sizeof(header)
	size_t remainder = 8 - (size % 8);
	if(remainder < 8) size += remainder;	
	size += sizeof(BoundaryTag);
    
    if(size == 0 || size > ((ARENA_SIZE) - (3*sizeof(BoundaryTag)))){
        errno = ENOMEM;
        
        return NULL;
    }	

	//Step 3
	if(size < 32){
		size = 32;
	}

	//pointer used 
	FreeObject * curr = _freeList;	
	
	curr = curr->free_list_node._next;
	while(curr != _freeList){
		if(getSize(&curr->boundary_tag) >= size){
			//Split the mem
			if((getSize(&curr->boundary_tag) - size) >= sizeof(FreeObject)){
			
				size_t diff = getSize(&curr->boundary_tag) - size;
				setSize(&curr->boundary_tag, diff);
                
				FreeObject * temp = (FreeObject *)((char *) curr + diff);
                		BoundaryTag * rightObject = (BoundaryTag *)(temp + getSize(&temp->boundary_tag));
                
				setSize(&temp->boundary_tag, size);
				setAllocated(&temp->boundary_tag, ALLOCATED);
				temp->boundary_tag._leftObjectSize = diff;
                
                		rightObject->_leftObjectSize = size;
				return (void *)((char *)temp + sizeof(BoundaryTag));
			}else{ // Don't split
				FreeObject * temp = curr;
				temp->free_list_node._prev->free_list_node._next = temp->free_list_node._next;
				temp->free_list_node._next->free_list_node._prev = temp->free_list_node._prev;
				setAllocated(&temp->boundary_tag, ALLOCATED);		
				//curr->free_list_node._next->boundary_tag._leftObjectSize = size;
				 return (char *)temp + sizeof(BoundaryTag);

		
			}
		}
		curr = curr->free_list_node._next;
	}
	
	//init newChunk
	FreeObject * newChunk = getNewChunk(ARENA_SIZE);	
	setSize(&newChunk->boundary_tag, ARENA_SIZE - (2 * sizeof(BoundaryTag)));
	newChunk->boundary_tag._leftObjectSize = 0; 
	setAllocated(&newChunk->boundary_tag, NOT_ALLOCATED);		

	//hook ptrs up
	newChunk->free_list_node._next = _freeList->free_list_node._next;
	newChunk->free_list_node._prev = _freeList;
	_freeList->free_list_node._next->free_list_node._prev = newChunk;
	_freeList->free_list_node._next = newChunk;

	if(_freeList->free_list_node._prev = _freeList){
		_freeList->free_list_node._prev = newChunk;
	}	

	size_t diffSize = size - sizeof(BoundaryTag);

  
  	return allocateObject(diffSize);
}

/**
 * @brief TODO: PART 2
 * This funtion takes a pointer to memory returned by the program, and
 * appropriately reinserts it back into the free list.
 * You will have to manage all coalescing as needed
 *
 * @param ptr
 */
static void freeObject(void *ptr)
{
    //puts the ptr at the top at the boundary_tag
    FreeObject * curr = (FreeObject *)((char *)ptr - sizeof(BoundaryTag));
    
    FreeObject * leftNgbr = (FreeObject *)((char *)curr - (curr->boundary_tag._leftObjectSize));
    FreeObject * rightNgbr = (FreeObject *)((char *)curr + getSize(&curr->boundary_tag));
    
    if(!isAllocated(&leftNgbr->boundary_tag) || !isAllocated(&rightNgbr->boundary_tag)){
        
        if(!isAllocated(&leftNgbr->boundary_tag)){
            //get the new merged sizes
            size_t newSize = getSize(&leftNgbr->boundary_tag) + getSize(&curr->boundary_tag);
            
            //curr equals the leftNgbr to modify it and then change the size to the new one
            curr = leftNgbr;
            setSize(&curr->boundary_tag, newSize);
            
            //change the allocate status
            setAllocated(&curr->boundary_tag, NOT_ALLOCATED);
            
        }
        if(!isAllocated(&rightNgbr->boundary_tag)) {
            //get new merged size and set the size to the curr
            size_t newSize = getSize(&curr->boundary_tag) + getSize(&rightNgbr->boundary_tag);
            setSize(&curr->boundary_tag, newSize);
            
            //change the allocate status
            setAllocated(&curr->boundary_tag, NOT_ALLOCATED);
            
            //curr next is the right's next
            curr->free_list_node._next = rightNgbr->free_list_node._next;
            
            //right's next previous is the current
            rightNgbr->free_list_node._next->free_list_node._prev = curr;
            
            
        }

        //add the ptr to the beginning of the freeList
        _freeList->free_list_node._next = curr;
        curr->free_list_node._prev = &_freeListSentinel;
        
        return;
    }
    
    
    //if both aren't allocated then just add the curr ptr to the beginning of the freeList
    setAllocated(&curr->boundary_tag, NOT_ALLOCATED);
    curr->free_list_node._prev = _freeList;
    curr->free_list_node._next = _freeList->free_list_node._next;
    _freeList->free_list_node._next->free_list_node._prev = curr;
    _freeList->free_list_node._next = curr;
    return;
    
}

void print()
{
  printf("\n-------------------\n");

  printf("HeapSize:\t%zd bytes\n", _heapSize);
  printf("# mallocs:\t%d\n", _mallocCalls);
  printf("# reallocs:\t%d\n", _reallocCalls);
  printf("# callocs:\t%d\n", _callocCalls);
  printf("# frees:\t%d\n", _freeCalls);

  printf("\n-------------------\n");
}

void print_list()
{
    printf("FreeList: ");
    if (!_initialized) 
        initialize();
    FreeObject *ptr = _freeList->free_list_node._next;
    while (ptr != _freeList) {
        long offset = (long)ptr - (long)_memStart;
        printf("[offset:%ld,size:%zd]", offset, getSize(&ptr->boundary_tag));
        ptr = ptr->free_list_node._next;
        if (ptr != NULL)
            printf("->");
    }
    printf("\n");
}

void increaseMallocCalls() { _mallocCalls++; }

void increaseReallocCalls() { _reallocCalls++; }

void increaseCallocCalls() { _callocCalls++; }

void increaseFreeCalls() { _freeCalls++; }

//
// C interface
//

extern void * malloc(size_t size)
{
  pthread_mutex_lock(&mutex);
  increaseMallocCalls();
  
  return allocateObject(size);
}

extern void free(void *ptr)
{
  pthread_mutex_lock(&mutex);
  increaseFreeCalls();
  
  if (ptr == 0) {
    // No object to free
    pthread_mutex_unlock(&mutex);
    return;
  }
  
  freeObject(ptr);
}

extern void * realloc(void *ptr, size_t size)
{
  pthread_mutex_lock(&mutex);
  increaseReallocCalls();

  // Allocate new object
  void *newptr = allocateObject(size);

  // Copy old object only if ptr != 0
  if (ptr != 0) {

    // copy only the minimum number of bytes
    FreeObject *o = (FreeObject *)((char *) ptr - sizeof(BoundaryTag));
    size_t sizeToCopy = getSize(&o->boundary_tag);
    if (sizeToCopy > size) {
      sizeToCopy = size;
    }

    memcpy(newptr, ptr, sizeToCopy);

    //Free old object
    freeObject(ptr);
  }

  return newptr;
}

extern void * calloc(size_t nelem, size_t elsize)
{
  pthread_mutex_lock(&mutex);
  increaseCallocCalls();
    
  // calloc allocates and initializes
  size_t size = nelem * elsize;

  void *ptr = allocateObject(size);

  if (ptr) {
    // No error
    // Initialize chunk with 0s
    memset(ptr, 0, size);
  }

  return ptr;
}

