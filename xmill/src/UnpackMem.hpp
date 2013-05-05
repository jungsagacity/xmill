#define MAX_BLOCKSIZE      (2L*1024L*1024L)
#define MIN_DATABLOCKSIZE  512

struct UnpackDataBlock
{
   unsigned long     len;
   UnpackDataBlock   *next;
   // The data starts right after the last attribute !

   char *GetPtr() {  return (char *)(this+1);  }
   unsigned long GetLen() {  return len;  }
};

struct UnpackMemBlock
{
   unsigned long  len;
   unsigned long  used;
   UnpackMemBlock *next;
};

class UnpackMemMan
{
   UnpackMemBlock *firstblock,*lastblock;
   unsigned long  curmaxblocksize;
   unsigned long  neededmemoryspace;

   UnpackMemBlock *AllocateMemBlock(UnpackMemBlock **memblockref)
   {
      // We make sure that we don't try to allocate more than we need
      if(curmaxblocksize>neededmemoryspace)
         curmaxblocksize=neededmemoryspace;

      // Now we try to allocated as much memory as possible.
      // If this fails, we try to allocated half as much, and so on
      // If the block size drops below 20000K, we are running out of memory !
      do
      {
         *memblockref=(UnpackMemBlock *)malloc(curmaxblocksize+sizeof(UnpackMemBlock));
         if(*memblockref!=NULL)
            // We were successful ?? => break
         {
            (*memblockref)->len=curmaxblocksize;
            (*memblockref)->used=0;
            (*memblockref)->next=NULL;

            return *memblockref;
         }

         curmaxblocksize/=2;  // We try half the size

         if(curmaxblocksize<20000)  // Blocksize is very small ? ==> That's it, we drop out !
            return NULL;
      }
      while(1);
   }

public:

   char Init(unsigned long myneededmemoryspace,unsigned long datablockcount)
   {
      neededmemoryspace=myneededmemoryspace+datablockcount*sizeof(UnpackDataBlock);
      curmaxblocksize=MAX_BLOCKSIZE;

      // Let's see if we can already allocate as much as possible
      lastblock=AllocateMemBlock(&firstblock);
      if(lastblock==NULL)
         return -1;

      return 0;
   }

   UnpackDataBlock *AllocateDataBlocks(unsigned long len)
   {
      UnpackDataBlock *firstdatablock=NULL,**lastdatablockref=&firstdatablock;

      while(lastblock->used+sizeof(UnpackDataBlock)+len>lastblock->len)
         // As long as the data block does not fit onto the current memblock
         // We must allocate more space !
      {
         if(lastblock->used+sizeof(UnpackDataBlock)+MIN_DATABLOCKSIZE<lastblock->len)
            // If there is a certain minimum space, then we can at least use parts of the
            // block
         {
            *lastdatablockref=(UnpackDataBlock *)((char *)(lastblock+1)+lastblock->used);
            (*lastdatablockref)->len=lastblock->len-lastblock->used-sizeof(UnpackDataBlock);
            (*lastdatablockref)->next=NULL;

            len-=(*lastdatablockref)->len;
            neededmemoryspace-=(*lastdatablockref)->len;

            lastblock->used=lastblock->len;

            lastdatablockref=&((*lastdatablockref)->next);
         }

         // Now, we allocate a new block

         lastblock=AllocateMemBlock(&(lastblock->next));
         if(lastblock==NULL)
            return NULL;

         lastblock=lastblock->next;
      }

      *lastdatablockref=(UnpackDataBlock *)((char *)(lastblock+1)+lastblock->used);
      (*lastdatablockref)->len=len;
      (*lastdatablockref)->next=NULL;

      neededmemoryspace-=len+sizeof(UnpackDataBlock);

      lastblock->used+=sizeof(UnpackDataBlock)+len;

      return firstdatablock;
   }
};

extern UnpackMemMan  unpackmemman;
