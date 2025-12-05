// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// SECTION 2.0: REFERENCE COUNTING (SMART ALLOCATION)

// We introduce an array to track how many processes share
// each physical page.

// 2.1 Structures for Reference Counting
struct spinlock ref_lock;
int ref_counts[PHYSTOP / PGSIZE];

// Helper to get the array index from a physical address
int get_ref_index(void *pa) {
  return (uint64)pa / PGSIZE;
}
//-------------------------------------------------------

//1.0
//---------Initiation of changes----------

//In order to provide you with a
//better understanding of the changes,
//the original code (unchanged) will be
//commented in sections so that the comparison
//is easier for you to manage.They will be taken
//in numbered sections as 1.x

//1.1 A new free page counter has been added

//______NEW COUNTER______
// We added this variable to track the number of free pages in real time.
// Originally, xv6 doesn't know how much memory it has,
// it only knows if the list is empty or not.

struct {
  struct spinlock lock;
  struct run *freelist;
  int free_pages_count;
} kmem;

//_______ORIGINAL CODE_______

//  struct {
//    struct spinlock lock;
//    struct run *freelist;
//  } kmem;


//1.2 Initialize the counter at 0
//As 'freerange' calls 'kfree', this number will go up.

void
kinit()
{
  initlock(&kmem.lock, "kmem");

	// 2.2 Initialize Reference Lock
  initlock(&ref_lock, "ref_lock");
  //-----------------------------

  kmem.free_pages_count = 0;
  freerange(end, (void*)PHYSTOP);
}

//______ORIGINAL CODE_____

//  void
//  kinit()
//  {
//    initlock(&kmem.lock, "kmem");
//    freerange(end, (void*)PHYSTOP);
//  }
//-------------------------------------

// 2.3 New Helper Functions (Increment/Decrement)
// These functions manage the shared ownership.

// Increase reference count (Used when a process forks)
void
kref_inc(void *pa)
{
  acquire(&ref_lock);
  int i = get_ref_index(pa);
  ref_counts[i]++;
  release(&ref_lock);
}

// Decrease reference count (Used when a process frees memory)
// Returns the new count.
int
kref_dec(void *pa)
{
  acquire(&ref_lock);
  int i = get_ref_index(pa);
  ref_counts[i]--;
  int current_count = ref_counts[i];
  release(&ref_lock);
  return current_count;
}
//-------------------------------------------------------

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    // We initialize the reference count to 1 before calling kfree
    // so kfree can successfully decrement it to 0 and free it.
    acquire(&ref_lock);
    ref_counts[get_ref_index(p)] = 1;
    release(&ref_lock);

    kfree(p);
  }
}

//1.3 The counter's count increases

//MIT includes comments to explain how each module works;
//in this case, we will change one line.

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

	// 2.4 SMART DEALLOCATION LOGIC
  // Instead of blindly freeing, we check if others are using it.
  // If the counter is > 0 after decrementing, we DO NOT free.
  if(kref_dec(pa) > 0) {
      return;
  }
  //-----------------------------

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;

  //------line added---------
  kmem.free_pages_count++;
  //A page has been returned to the system.
  //We increment the counter of available resources
  //-------------------------

  release(&kmem.lock);
}

//1.4

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    //A page has been delivered to a process.
    //We decrement the available resources counter.
    kmem.free_pages_count--;
    //-----------------------
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
	// 2.5 INITIALIZE REFERENCE
    // When allocated, the page has exactly 1 owner.
  acquire(&ref_lock);
  ref_counts[get_ref_index(r)] = 1;
  release(&ref_lock);
    //------------------------

  //-----------------------------PRINT-------
  //Print the memory status (only if there are fewer
  //than 30,000 pages left to avoid filling the screen).
  // This allows us to see in the console how the memory is being used.
  printf("Available memory: %d pages\n", kmem.free_pages_count);

  return (void*)r;
}

//______ORIGINAL CODE________

/*
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
*/