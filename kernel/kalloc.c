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

//内存（锁+空闲页链表）
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

//cpu对应锁的名字,每个cpu的内存页链表一把锁
char *lock_names[NCPU]={
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};

void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  for(int i=0;i<NCPU;i++)
  {
    initlock(&kmem[i].lock,lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  //关中断
  push_off();
  int cpu_id=cpuid();

  r=(struct run*)pa;

  acquire(&kmem[cpu_id].lock);
  r->next=kmem[cpu_id].freelist;
  kmem[cpu_id].freelist=r;
  release(&kmem[cpu_id].lock);

  //开中断
  pop_off();

  // r = (struct run*)pa;

  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // kmem.freelist = r;
  // release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//修改，把原来只分配一个空闲页面链表->
//每个cpu维护一个空闲页面链表，内存页不够时从别的cpu core中偷取空闲的内存页
void *kalloc(void)
{
  struct run *r;
  //关闭中断
  push_off();

  //获取当前的cpuid
  int cpu_id=cpuid();

  //获取当前cpucore的锁
  acquire(&kmem[cpu_id].lock);

  r=kmem[cpu_id].freelist;

  //当前cpucore的空闲链表为空，从别的cpucore中偷取空闲页
  if(!kmem[cpu_id].freelist)
  {
    //先解锁，防止死锁
     release(&kmem[cpu_id].lock);
    //总共要偷的空闲页数

    //偷到的链表
    struct run *steal_list=0;
    int steal_page=32;
    for(int i=0;i<NCPU;i++)
    {
      //若是当前cpucore的编号，continue
      if(i==cpu_id)
      {
        continue;
      }
      //获取第i个cpucore的锁
      acquire(&kmem[i].lock);
      // struct run *rr=kmem[i].freelist;

      //当前遍历的cpucore的空闲页表不为空,steal_page不为0
      //*******问题所在：？方法：遍历所有的cpu加锁得到所要的空闲页，再加锁重新加入当前cpu的空闲页表中
      // while(rr && steal_page)
      // {
      //   kmem[i].freelist=rr->next;
      //   rr->next=kmem[cpu_id].freelist;
      //   kmem[cpu_id].freelist=rr;
      //   rr=kmem[i].freelist;
      //   steal_page--;
      // }
      //从cpu[i]的freelist中偷取page
      for(struct run *rr=kmem[i].freelist;rr&&steal_page;rr=kmem[i].freelist)
      {
        kmem[i].freelist=rr->next;//拿出一页
        rr->next=steal_list;//将偷到的空闲页插入到链表头
        steal_list=rr;
        --steal_page;
      }
      release(&kmem[i].lock);

      //偷取所需的空闲页满足
      if(steal_page==0)break;
    }
    //********在没有锁期间若有其他cpu访问当前cpu的空闲页链表，也为空，没有影响
    //将得到的空闲页链表插入到当前cpu的空闲页链表头
    if(steal_list != 0) {
        r = steal_list;
        // 置换链表
        acquire(&kmem[cpu_id].lock);
        kmem[cpu_id].freelist = r->next;
        release(&kmem[cpu_id].lock);
      }
  }
  //当前cpu的空闲页表中存在空闲页，取出即可
  else
  {
    kmem[cpu_id].freelist=r->next;
    release(&kmem[cpu_id].lock);
  }


  if(r)
  {
    memset((char*)r,1,PGSIZE);
  }

  //开启中断
  pop_off();
  return (void*)r;
}
