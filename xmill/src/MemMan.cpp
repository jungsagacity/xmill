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

// This module contains several auxiliary function for the
// memory management

#include <stdlib.h>

#include "MemMan.hpp"

unsigned long  allocatedmemory=0;

unsigned long blocksizes[BLOCKSIZE_NUM]=
{TINYBLOCK_SIZE,SMALLBLOCK_SIZE,MEDIUMBLOCK_SIZE,LARGEBLOCK_SIZE};
   // For each possible block size,
   // we store the size

char *freeblocklists[BLOCKSIZE_NUM]={NULL,NULL,NULL,NULL};
   // For each possible block size,
   // we store the list of free blocks

// The memory management for large blocks

char *AllocateBlockRecurs(unsigned char blocksizeidx)
   // Allocates a new large block recursively
   // I.e. if there is no block in the free list, we try to allocate
   // a block of the next higher size
{
   char *ptr;

   // Do we have any free block?
   if(freeblocklists[blocksizeidx]!=NULL)
   {
      ptr=freeblocklists[blocksizeidx];
      freeblocklists[blocksizeidx]=*(char **)freeblocklists[blocksizeidx];
      return ptr;
   }
   else
   {
      if(blocksizeidx==BLOCKSIZE_NUM-1)
         // Is this the largest possible block size??
         // We must allocate new space!
      {
         ptr=(char *)malloc(blocksizes[blocksizeidx]);
         if(ptr==NULL)
            ExitNoMem();

         return ptr;
      }
      else  // If we haven't reached the largest possible block size,
            // We allocate a block of the next larger block size
            // and use it as a container for our blocks
      {
         ptr=AllocateBlockRecurs(blocksizeidx+1);

         // We add the new blocks to the free list
         char  *ptr1=ptr+blocksizes[blocksizeidx],
               *endptr=ptr+blocksizes[blocksizeidx+1];

         do
         {
            *(char **)ptr1=freeblocklists[blocksizeidx];
            freeblocklists[blocksizeidx]=ptr1;

            ptr1+=blocksizes[blocksizeidx];
         }
         while(ptr1<endptr);

         return ptr;
      }
   }
}
