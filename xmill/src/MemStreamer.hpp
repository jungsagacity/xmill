/*
This product contains certain software code or other information
("AT&T Software") proprietary to AT&T Corp. ("AT&T").  The AT&T
Software is provided to you "AS IS".  YOU ASSUME TOTAL RESPONSIBILITY
AND RISK FOR USE OF THE AT&T SOFTWARE.  AT&T DOES NOT MAKE, AND
EXPRESSLY DISCLAIMS, ANY EXPRESS OR IMPLIED WARRANTIES OF ANY KIND
WHATSOEVER, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, WARRANTIES OF
TITLE OR NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS, ANY
WARRANTIES ARISING BY USAGE OF TRADE, COURSE OF DEALING OR COURSE OF
PERFORMANCE, OR ANY WARRANTY THAT THE AT&T SOFTWARE IS "ERROR FREE" OR
WILL MEET YOUR REQUIREMENTS.

Unless you accept a license to use the AT&T Software, you shall not
reverse compile, disassemble or otherwise reverse engineer this
product to ascertain the source code for any AT&T Software.

(c) AT&T Corp. All rights reserved.  AT&T is a registered trademark of AT&T Corp.

***********************************************************************

History:

      24/11/99  - initial release by Hartmut Liefke, liefke@seas.upenn.edu
                                     Dan Suciu,      suciu@research.att.com
*/

//**************************************************************************
//**************************************************************************

// This module contains the memory streamer class
// The memory streamer represents a sequence of memory blocks
// that grows over time. The blocks at the beginning are small, while
// later, larger blocks are allocated.
// The memory streamer is the core of the container implementation

#ifndef STREAMMEM_HPP
#define STREAMMEM_HPP

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Types.hpp"

#include "MemMan.hpp"   // The global memory manager

//**********************************************************************************

// For the MemStreamer, the block size increase slowly in 16 steps.
// 'blocksizeidxs' contains the block size indices for each step

#define BLOCKSIZE_IDXNUM 16
extern unsigned char blocksizeidxs[];

class Output;

struct MemStreamBlock
   // Defines one single data block
{
   MemStreamBlock *next;            // The next data block of the same streamer
   unsigned long  blocksize:28;     // The size of this block
   unsigned long  blocksizeidxidx:4;// The block size index (i.e. the index within blocksizeidxs)
   unsigned long  cursize;          // The current number of bytes in this block

//   MemStreamBlock *myprev,*mynext;   // Previous and next block
//   unsigned long  idx;

   char           data[1];       // The first byte of the data
                                 // (The other data comes DIRECTLY afterwards)
};

//extern MemStreamBlock    *blocklist;
   // The list of all blocks

struct MemStreamMarker
   // Represents a marker in the memory stream
   // Markers are used so that everything appended to the Streamer
   // after the marker can be deleted
   // Several markers can exist in the Streamer and they are
   // in backwards references in a single-chained list.
{
   MemStreamBlock    *block;  // The block of the previous marker,
   MemStreamMarker   *pos;    // The pointer to the previous marker
                              // The pointers are NULL, if there is no previous marker
};

class MemStreamer
{
   MemStreamBlock    *firstblock;         // The first block of the streamer
   MemStreamBlock    *curblock;           // The current block, which is also the last block
   unsigned long     overallsize:28;      // The overall size of the memory streamer
   unsigned long     curblocksizeidxidx:4;
   MemStreamMarker   curmarker;           // The position of the current marker is stored here
                                          // If there is no marker, then both pointers are NULL

   void ReleaseBlock(MemStreamBlock *block)
      // Releases the block from the mem streamer
      // Typically, this is the last block in the chain - but it could
      // also be in the middle
   {
/*
      if(block->myprev!=NULL)
         block->myprev->mynext=block->mynext;
      else
         blocklist=block->mynext;
      if(block->mynext!=NULL)
         block->mynext->myprev=block->myprev;
*/
//      memset(block->data,0xcd,block->cursize);
      FreeBlock((char *)block,blocksizeidxs[block->blocksizeidxidx]);
   }

   MemStreamBlock *AllocateNewBlock()
      // Allocates a new block
      // The size is determined by the next index in blocksizeidxs
   {
      MemStreamBlock *newblock;

      // Let's get the new block
      newblock=(MemStreamBlock *)AllocateBlock(blocksizeidxs[curblocksizeidxidx]);

      // The usable data size is the block size - the size of the header
      newblock->blocksize=GetBlockSize(blocksizeidxs[curblocksizeidxidx])-(sizeof(MemStreamBlock)-1);
      newblock->blocksizeidxidx=curblocksizeidxidx;

      // Do we still have more steps to go in the block size increase?
      // => Go to next step
      if(curblocksizeidxidx<BLOCKSIZE_IDXNUM-1)
         curblocksizeidxidx++;

      newblock->next=NULL;
      newblock->cursize=0;
/*
      // Let's insert the block at the end of the block-chain
      newblock->mynext=blocklist;
      if(blocklist!=NULL)
         blocklist->myprev=newblock;
      newblock->myprev=NULL;
      blocklist=newblock;
*/
      return newblock;
   }

public:

   void Initialize(unsigned long firstblocksizeidx=0)
      // Initializes the memstreamer and sets the first block index step
   {
      if(firstblocksizeidx>=BLOCKSIZE_IDXNUM)
         firstblocksizeidx=BLOCKSIZE_IDXNUM-1;

      curblocksizeidxidx=firstblocksizeidx;
      overallsize=0;

      curmarker.block=NULL;
      curmarker.pos=NULL;

      firstblock=curblock=NULL;
   }

   MemStreamer(unsigned long firstblocksizeidx=0)
   {
      Initialize(firstblocksizeidx);
   }

   ~MemStreamer()
   {
      ReleaseMemory(0);
   }

   int GetSize() { return overallsize; }

   MemStreamBlock *GetFirstBlock() { return firstblock; }

   void WordAlign()
      // Allocated 1 to 3 bytes to align the current memory pointer
      // to an address divisible by 4.
   {
      if(curblock==NULL)
         return;

      int addsize=3-((curblock->cursize+3)&3);
      if(addsize>0)
      {
         curblock->cursize+=addsize;
         overallsize+=addsize;
      }
   }

   char *GetByteBlock(unsigned len)
      // The main function for allocated more memory of size 'len'.
      // The function checks the current block and if there is not enough space,
      // the function 'AllocateNewBlock' is called.
   {
      if(len+sizeof(MemStreamBlock)-1>LARGEBLOCK_SIZE) // Is the requested size larger than the biggest possible block?
      {
         char str[100];
         sprintf(str,"Could not allocate %lu bytes (largest possible block size=%lu bytes) !",
            sizeof(MemStreamBlock)-1+len,
            LARGEBLOCK_SIZE);
         Error(str);
         Exit();
      }

      if(curblock==NULL)
         // We don't have a block yet ?
         firstblock=curblock=AllocateNewBlock();

      if(curblock->blocksize-curblock->cursize>=len)
         // Enough space in current block?
      {
         char *ptr=curblock->data+curblock->cursize;
         curblock->cursize+=len;
         overallsize+=len;
         return ptr;
      }
      else  // We add a new block at the end
      {
         do
         {
            curblock->next=AllocateNewBlock();
            curblock=curblock->next;
         }
         while(curblock->blocksize<len);  // If we don't have enough space,
                                          // we simply create a bigger one!

         curblock->cursize=len;
         overallsize+=len;

         return curblock->data;
      }
   }

   void ReleaseByteBlock(unsigned len)
      // Removes 'len' bytes from the current block
   {
      // If the current block is smaller than the data to be removed,
      // then there is something really wrong!
      if(curblock->cursize<len)
      {
         Error("Fatal error in ReleaseBlock !\n");
         Exit();
      }

      curblock->cursize-=len;
      overallsize-=len;

#ifdef RELEASEMEM_SAFE
      memset(curblock->data+curblock->cursize,0xcd,len);
#endif
   }

//******************************************************************************

   void StoreData(char *data,unsigned len)
      // Stores the a sequence of bytes in the memory streamer
      // The bytes can be distributed over many blocks
   {
      if(curblock==NULL)
         // We don't have a block yet ?
         firstblock=curblock=AllocateNewBlock();

      while(len>curblock->blocksize-curblock->cursize)
         // There is not enough space in the current block ?
         // ==> We fill the current block as much as possible
      {
         mymemcpy(curblock->data+curblock->cursize,data,
                  curblock->blocksize-curblock->cursize);

         data        +=curblock->blocksize-curblock->cursize;
         overallsize +=curblock->blocksize-curblock->cursize;
         len         -=curblock->blocksize-curblock->cursize;
         curblock->cursize=curblock->blocksize;

         curblock->next=AllocateNewBlock();

         curblock=curblock->next;
      }
      mymemcpy(curblock->data+curblock->cursize,data,len);
      curblock->cursize+=len;
      overallsize+=len;
   }

//******************************************************************************

// The following auxiliary functions allow the easy storage of simple data values

   void StoreChar(unsigned char c)
      // Stores one single character
   {
      if(curblock==NULL)
         // We don't have a block yet ?
         firstblock=curblock=AllocateNewBlock();
      else
      {
         if(curblock->blocksize-curblock->cursize==0)
            // The block is completely full? ==> Get a new one
         {
            curblock->next=AllocateNewBlock();
            curblock=curblock->next;
         }
      }
      curblock->data[curblock->cursize]=c;
      curblock->cursize++;
      overallsize++;
   }

   void StoreCompressedUInt(unsigned long val,unsigned char offs=0)
      // Stores a compressed unsigned integer
      // The values below the offset threshold are reserved for other use.
      // Hence, the offset is basically added to 'val'.
      // The offset *must* be smaller than 128
   {
      if(val<128-(unsigned)offs)
         StoreChar((unsigned char)val+offs);
      else
      {
         if(val<16384)
         {
            StoreChar((unsigned char)(val>>8)+128);
            StoreChar((unsigned char)val);
         }
         else
         {
            StoreChar((unsigned char)(val>>24)+192);
            StoreChar((unsigned char)(val>>16));
            StoreChar((unsigned char)(val>>8));
            StoreChar((unsigned char)(val));
         }
      }
   }

   void StoreCompressedSInt(char isneg,unsigned long val)
      // Stores a signed integer in compressed format
      // The sign is in 'isneg' and 'val' is the basis
   {
      if(val<64)
         StoreChar((unsigned char)val+(isneg ? 64 : 0));
      else
      {
         if(val<8192)
         {
            StoreChar((unsigned char)(val>>8)+(isneg ? (128+32) : 128));
            StoreChar((unsigned char)val);
         }
         else
         {
            StoreChar((unsigned char)(val>>24)+(isneg ? (192+32) : 192));
            StoreChar((unsigned char)(val>>16));
            StoreChar((unsigned char)(val>>8));
            StoreChar((unsigned char)val);
         }
      }
   }

   void StoreCompressedSInt(long val)
      // Stores a signed integer in compressed format
   {
      if(val&0x80000000L)
         StoreCompressedSInt(1,-(long)val);
      else
         StoreCompressedSInt(0,val);
   }

   void StoreUInt32(unsigned long val)
      // Stores a unsigned integer in compressed format
   {
      StoreCompressedUInt(val);
   }

   void StoreSInt32(char isneg,unsigned long val)
      // Stores a signed integer in compressed format
   {
      StoreCompressedSInt(isneg,val);
   }

//**********************************************************************************
//**********************************************************************************

   void StartNewMemBlock()
      // Creates a new marker on the memory streamer
   {
      MemStreamMarker *newmarker=(MemStreamMarker *)GetByteBlock(sizeof(MemStreamMarker));

      *newmarker=curmarker;   // We copy previous marker

      // We save the new marker position
      curmarker.block=curblock;
      curmarker.pos=newmarker;
   }

   void RemoveLastMemBlock()
      // Removes a the data up to the last memory marker
   {
      MemStreamBlock *block,*nextblock;
      int            newsize;

      // We remove all blocks after the block of the last marker

      block=curmarker.block->next;

      while(block!=NULL)
      {
         overallsize-=block->cursize;
         nextblock=block->next;
         ReleaseBlock(block);
         block=nextblock;
      }
      curmarker.block->next=NULL;

      // We save block pointer -> This block will be truncated to size 'newsize'
      block=curblock=curmarker.block;

      // The remaining size
      newsize=(char *)curmarker.pos-block->data;

      // We set the marker
      curmarker=*(curmarker.pos);

#ifdef RELEASEMEM_SAFE
      // We overwrite the empty space ==> Just for protection
      // We can take this out later
      memset(block->data+newsize,0xcd,block->cursize-newsize);
#endif

      overallsize-=(block->cursize-newsize);
      block->cursize=newsize;

      if(block->blocksizeidxidx<BLOCKSIZE_IDXNUM-1)
         // Not the largest block size?
         curblocksizeidxidx=block->blocksizeidxidx+1;
      else
         curblocksizeidxidx=block->blocksizeidxidx;
   }

   void ReleaseMemory(unsigned long firstblocksizeidx)
      // Releases the entire memory of the memstreamer
   {
      MemStreamBlock    *block=firstblock,
                        *nextblock;
      while(block!=NULL)
      {
         nextblock=block->next;
         ReleaseBlock(block);
         block=nextblock;
      }
      Initialize(firstblocksizeidx);
   }

   void *operator new(size_t size, MemStreamer *mem)  {  return mem->GetByteBlock(size);  }
   void operator delete(void *ptr)  {}
};

//*********************************************************************
//*********************************************************************

// For the decompressor, we implement an additional memory management

/*
#ifdef XDEMILL

extern MemStreamer blockmem;

#define MEMBLOCK_THRESHOLD 8000

extern unsigned char *memoryalloc_buf;
extern unsigned char *memoryalloc_curptr;
extern unsigned long memoryalloc_bufsize;

inline void SetMemoryAllocationSize(unsigned long allocsize)
{
   if(memoryalloc_bufsize<allocsize)
   {
      if(memoryalloc_buf!=NULL)
         free(memoryalloc_buf);

      memoryalloc_buf=(unsigned char *)malloc(allocsize);
      if(memoryalloc_buf==NULL)
         ExitNoMem();

      memoryalloc_curptr=memoryalloc_buf;

      memoryalloc_bufsize=allocsize;
   }
   else
      memoryalloc_curptr=memoryalloc_buf;

}

inline void WordAlignMemBlock()
{
   memoryalloc_curptr=
      (unsigned char *)
      (((unsigned long)memoryalloc_curptr+3)&0xFFFFFFFCL);
}

inline unsigned char *AllocateMemBlock(unsigned long size)
{
   memoryalloc_curptr+=size;
   if(memoryalloc_curptr>memoryalloc_buf+memoryalloc_bufsize)
   {
      Error("Fatal Error!");
      Exit();
   }
   return memoryalloc_curptr-size;
}

inline void FreeMemBlock(void *ptr,unsigned long size)
{
}
*/
//#endif

#endif
