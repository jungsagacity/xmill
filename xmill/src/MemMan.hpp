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

// This module contains the memory manager for XMill
// The memory manager can handle four different block sizes (see below).
// and the blocks are hierarchically organized.

#include <string.h>

#define BLOCKSIZE_NUM   4

#define LARGEBLOCK_SIZE    65536
#define MEDIUMBLOCK_SIZE   8192
#define SMALLBLOCK_SIZE    1024
#define TINYBLOCK_SIZE     256

extern unsigned long blocksizes[];  // For each possible block size,
                                    // we store the size

extern char *freeblocklists[];      // For each possible block size,
                                    // we store the list of free blocks


#include "Error.hpp"

//**********************************************************************
//**********************************************************************

extern unsigned long  allocatedmemory;

// The memory management for blocks

char *AllocateBlockRecurs(unsigned char blocksizeidx);
   // Allocates a block of size blocksizes[blocksizeidx]

inline char *AllocateBlock(unsigned char blocksizeidx)
   // Allocates a new memory block
   // and increases the allocated memory count
{
   allocatedmemory+=blocksizes[blocksizeidx];
   return AllocateBlockRecurs(blocksizeidx);
}

inline void FreeBlock(char *ptr,unsigned char blocksizeidx)
   // Frees a memory block
{
   *(char **)ptr=freeblocklists[blocksizeidx];
   freeblocklists[blocksizeidx]=ptr;

#ifdef RELEASEMEM_SAFE
   memset(ptr+4,0xcd,blocksizes[blocksizeidx]-4);
#endif

   allocatedmemory-=blocksizes[blocksizeidx];
}

inline unsigned long GetBlockSize(unsigned char blocksizeidx)
   // Returns the block size for a specific index
{
   return blocksizes[blocksizeidx];
}

//**********************************************************************
//**********************************************************************

// For the decompressor, we implement an additional memory management
// A single memory block is allocated (and can be reallocated, if it is
// is too small) and data is stored within that block
// from the start to the end



//extern MemStreamer blockmem;

#define MEMBLOCK_THRESHOLD 8000

extern unsigned char *memoryalloc_buf;
extern unsigned char *memoryalloc_curptr;
extern unsigned long memoryalloc_bufsize;

inline void SetMemoryAllocationSize(unsigned long allocsize)
   // Sets the amount of memory needed. If the current block is too small,
   // the current block is reallocated.
{
   if(memoryalloc_bufsize<allocsize)
      // Current block is too small?
   {
      // Let's reallocate
      if(memoryalloc_buf!=NULL)
         free(memoryalloc_buf);

      memoryalloc_buf=(unsigned char *)malloc(allocsize);
      if(memoryalloc_buf==NULL)
         ExitNoMem();

      memoryalloc_curptr   =memoryalloc_buf;
      memoryalloc_bufsize  =allocsize;
   }
   else
      // If we have enough space, we just use the existing block
      memoryalloc_curptr=memoryalloc_buf;
}

inline void WordAlignMemBlock()
   // We align the current pointer to an address divisible by 4
{
   memoryalloc_curptr=
      (unsigned char *)
      (((unsigned long)memoryalloc_curptr+3)&0xFFFFFFFCL);
}

inline unsigned char *AllocateMemBlock(unsigned long size)
   // We allocate a new piece of data
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
   // We forget about freeing the block, sine we will use the entire block
   // later again
{
}


