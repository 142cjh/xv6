// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
//#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define NBUF (NBUCKET * 3)


struct {
  struct spinlock lock; //表级锁
  struct buf buf[NBUF];
}bcache;

struct bucket{
  struct spinlock lock;//桶级锁
  struct buf head;
} hashtable[NBUCKET];

int hash(int dev,uint blockno)
{
  return blockno%NBUCKET;
}

//对哈希表进行初始化
//将bcache.buf[NBUF]中的块平均分配给每个桶，记得设置b->blockno使块的hash与桶相对应
//通过块->进行哈希映射->查找对应的缓存桶
void
binit(void)
{
  struct buf *b;

  //初始化表级锁
  initlock(&bcache.lock, "bcache");

  //初始化桶中的元素即单个锁,这里39个buffer锁
  for(b=bcache.buf;b<bcache.buf+NBUF;b++)
  {
    initsleeplock(&b->lock,"buffer");
  }

  //对哈希表的桶级锁进行初始化
  b=bcache.buf;//
  for(int i=0;i<NBUCKET;i++)
  {
    initlock(&hashtable[i].lock,"bcache_buucket");
    //一个key下分配3个缓存块
    for(int j=0;j<NBUF/NBUCKET;j++)
    {
      b->blockno=i;
      b->next=hashtable[i].head.next;
      hashtable[i].head.next=b;
      b++;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
//在对应的桶中查找当前块是否被缓存
//*****注意不要同时获取两个锁，导致死锁问题
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  //当前桶
  int idx=hash(dev,blockno);
  struct bucket* bucket=hashtable+idx;

  acquire(&bucket->lock);

  // Is the block already cached?
  //当前块是否已经被缓存,若在当前哈希桶中找到，增加引用计数
  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      //对得到的缓存块处理，先加锁
      acquiresleep(&b->lock);
      return b;
    }
  }

  //******这里替换的是当前桶中所有缓存块最近最少使用的缓存块
  //当前桶中没有缓存这个块，先查询当前桶中是否有空闲块，替换具有最小时间戳的空闲块
  int min_time=0x8fffffff;
  //被替换的空闲块
  struct buf* replace_buf=0;
  for(b=bucket->head.next;b!=0;b=b->next)
  {
    //若引用计数为0（没有进程引用当前块即为空闲块），
    //并且空闲块时间戳小于当前块的时间戳
    if(b->refcnt==0&&b->timestamp<min_time)
    {
      replace_buf=b;
      min_time=b->timestamp;
    }
  }
  //找到被替换的空闲块,将其改变为当前要缓存的块属性
  if(replace_buf)
  {
    goto find;
  }

  //******防止死锁问题,当前桶中找不到要的空闲块，释放桶级锁
  release(&bucket->lock);

  //******这里替换的是表中所有缓存块最近最少使用的缓存块
  //到别的哈希桶中找寻空闲块，替换表中具有最小时间戳的空闲块为当前块
  //先锁定表级锁，需要整个表中查找，在39个缓存块中查找空闲块
  //全局查找，获取全局锁
  acquire(&bcache.lock);
  refind:
  for(b=bcache.buf;b<bcache.buf+NBUF;b++)
  {
    if(b->refcnt==0&&b->timestamp<min_time)
    {
      replace_buf=b;
      min_time=b->timestamp;
    }
  }
  //********防止死锁，查找之后，解锁
  release(&bcache.lock);
  //找到表中最近最少使用的块后，上此块的桶级锁，替换属性
  if(replace_buf)
  {
    int ridx=hash(replace_buf->dev,replace_buf->blockno);
    //********由于没有关中断
    //********在全局中找到一个空闲块后，要对其加锁处理之间有一段时间，
    //********这个时间可能先被其他线程获取这个空闲块的桶的锁并处理，即空闲块可能不存在了，
    //********再获取空闲块的锁后要再次判断空闲块是否已经被使用

    //获取要替换的空闲块所在桶的锁
    acquire(&hashtable[ridx].lock);
    if(replace_buf->refcnt != 0)  // be used in another bucket's local find between finded and acquire
    {
      release(&hashtable[ridx].lock);
      goto refind;
    }
    struct buf *pre=&hashtable[ridx].head;
    struct buf *p=hashtable[ridx].head.next;
    //找到桶中要替换的块的位置
    while(p!=replace_buf)
    {
      pre=pre->next;
      p=p->next;
    }
    //在桶中去掉找到的空闲块
    pre->next=p->next;
    //replace_buf空闲块原来所在的桶操作完成，解锁
    release(&hashtable[ridx].lock);

    //操作当前的哈希桶，加桶级锁
    //将找到的空闲块移到当前的哈希桶中
    acquire(&bucket->lock);
    replace_buf->next=hashtable[idx].head.next;
    hashtable[idx].head.next=replace_buf;
    goto find;
  }
  else
  {
    panic("bget:no buffers");
  }



//找到空闲块后，替换空闲块的属性
  find:
  replace_buf->dev=dev;
  replace_buf->blockno=blockno;
  replace_buf->valid=0;
  replace_buf->refcnt=1;
  release(&bucket->lock);//释放当前桶的锁
  //获取buffer的锁，后续可以对这一个缓存块进行处理
  acquiresleep(&replace_buf->lock);
  return replace_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
//将链表的锁替换为桶级锁(处理完一个buffer后，必须调用brelse来释放它)
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx=hash(b->dev,b->blockno);

  acquire(&hashtable[idx].lock);
  b->refcnt--;
  //当前缓冲区没有线程或进程引用,refcnt=0，
  if (b->refcnt == 0) {
    b->timestamp=ticks;
  }
  
  release(&hashtable[idx].lock);
}

void
bpin(struct buf *b) {
  int idx=hash(b->dev,b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt++;
  release(&hashtable[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx=hash(b->dev,b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt--;
  release(&hashtable[idx].lock);
}


