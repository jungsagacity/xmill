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

// This module contains the container manager for the decompressor

#include "Load.hpp"
#include "MemStreamer.hpp"
#include "VPathExprMan.hpp"

extern MemStreamer blockmem;

class VPathExpr;
class Input;
class UserUncompressor;

class UncompressContainer
   // This class represents a single decompressor container
   // Each decompressor container has a fixed pointer to some data space
   // that contains the data
{
   unsigned char        *dataptr;   // The pointer to the data
   unsigned long        size;       // The size of the container
   unsigned char        *curptr;    // The current position in the container

public:

   void *operator new(size_t size)  {  return blockmem.GetByteBlock(size); }
   void operator delete(void *ptr)  {}

   void SetSize(unsigned long mysize)
      // This sets the initial size - the memory is allocated later
      // and the data is loaded later
   {
      dataptr=curptr=NULL;
      size=mysize;
   }
   unsigned long GetSize() {  return size;   }

   void AllocateContMem(unsigned long mincontsize);
      // Allocates the memory - but only if the size is larger than 'mincontsize'
      // This will allows us to allocate memory starting with the largest
      // containers. Then, smaller containers can be allocated.

   void ReleaseContMem();
      // Releases the container manager

      // Decompresses the container data and stores
      // it in the data buffer
   void UncompressSmallContainer(SmallBlockUncompressor *uncompressor);
   void UncompressLargeContainer(Input *input);

   unsigned char *GetDataPtr()   {  return curptr; }

   unsigned char *GetDataPtr(int len)
   {
      curptr+=len;
      return curptr-len;
   }

   // Some auxiliary functions for reading integers and strings
   unsigned long LoadUInt32()
   {
      return ::LoadUInt32(curptr);
   }

   unsigned long LoadSInt32(char *isneg)
   {
      return ::LoadSInt32(curptr,isneg);
   }

   long LoadSInt32()
   {
      char isneg;
      long val=::LoadSInt32(curptr,&isneg);
      if(isneg)
         return 0L-val;
      else
         return val;
   }

   char LoadChar()
   {
      return ::LoadChar(curptr);
   }

   unsigned char *LoadString(unsigned *len)
   {
      *len=LoadUInt32();

      curptr+=*len;
      return curptr-*len;
   }
};

//**************************************************************************
//**************************************************************************

class UncompressContainerBlock
   // Containers a grouped in container blocks
   // Each container block has a path expression (with user decompressor)
   // that takes care of the decompression
{
   VPathExpr            *pathexpr;  // The path expression
   UncompressContainer  *contarray; // The array of containers
   unsigned long        contnum;    // The number of containers in the array

   // Note: The state space for the user compressor is stored *directly*
   // after the container array space !

public:

   void *operator new(size_t size)  {  return blockmem.GetByteBlock(size); }
   void operator delete(void *ptr)  {}

   void Load(SmallBlockUncompressor *uncompressor);
      // Loads the container block information (number+size of containers)
      // from the small decompressor object

   void AllocateContMem(unsigned long mincontsize);
      // Allocates the memory for those containers with size>mincontsize
   void ReleaseContMem();
      // Release the container memory

      // Decompresses the container data and stores
      // it in the containers
   void UncompressSmallContainers(SmallBlockUncompressor *uncompressor);
   void UncompressLargeContainers(Input *input);

   UncompressContainer  *GetContainer(unsigned idx)   {  return contarray+idx;   }

   UserUncompressor *GetUserUncompressor()
   {
      return pathexpr->GetUserUncompressor();
   }

   char *GetUserDataPtr()
      // Returns the pointer to the user decompressor state data
   {
      return (char *)(contarray+contnum);
   }

   void Init()
      // Initializes the decompress container block
      // This initializes the decompressor state data
   {
      if(pathexpr!=NULL)
         GetUserUncompressor()->InitUncompress(GetContainer(0),GetUserDataPtr());
   }

   void UncompressText(XMLOutput *output)
      // Decompresses a text item and prints it to 'output'
   {
      GetUserUncompressor()->UncompressItem(GetContainer(0),GetUserDataPtr(),output);
   }

   void FinishUncompress()
      // After all items have been read, this function cleans
      // up the user decompressor states
   {
      if(pathexpr!=NULL)
         pathexpr->GetUserUncompressor()->FinishUncompress(GetContainer(0),GetUserDataPtr());
   }
};

class UncompressContainerMan
   // The decompressor container manager handles the set of container blocks
   // and their containers
{
   UncompressContainerBlock   *blockarray;   // The array of container blocks
   unsigned long              blocknum;      // The number of container blocks

public:
   void Load(SmallBlockUncompressor *uncompress);
      // Loads the structural information from the small block decompressor

   void AllocateContMem();
      // Allocates the memory for the container sequentially.
      // Loads the container block information (number+size+structure of container blocks)
      // from the small decompressor object
      // This function starts with large containers and then allocates
      // memory for smaller containers

   void ReleaseContMem();
      // Release the container memory

      // Decompresses the container data and stores
      // it in the containers
   void UncompressLargeContainers(Input *input);
   void UncompressSmallContainers(SmallBlockUncompressor *uncompressor);

   void Init();   // Initializes the state data for all container blocks

   UncompressContainerBlock  *GetContBlock(unsigned idx)   {  return blockarray+idx;   }

   void FinishUncompress();
      // Finishes the decompression
};
