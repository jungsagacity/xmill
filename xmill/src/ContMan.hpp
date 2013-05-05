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

// This module contains the container manager for the compressor XMill

#ifndef CONTMAN_HPP
#define CONTMAN_HPP

#ifdef XMILL

#include "MemStreamer.hpp"
#include "Compress.hpp"
#include "Load.hpp"

// Some class definitions
class Output;
class CompressContainerBlock;
class CompressContainerMan;
class PathDictNode;
class SmallBlockUncompressor;
class VPathExpr;

extern MemStreamer blockmem;

class CompressContainer: public MemStreamer
   // The main structure for representing a container
   // A CompressContainer is simply a MemStreamer-Object
   // Containers are grouped together in CompressContainerBlocks
{
public:
   void *operator new(size_t size)  {  return blockmem.GetByteBlock(size); }
   void operator delete(void *ptr)  {}
};

class CompressContainerBlock
   // Used for grouping a sequence of CompressContainers together.
   // The sequence of CompressContainers follow *directly* the container block object
   // in memory.
{
   friend CompressContainerMan;
   CompressContainerBlock  *nextblock;       // The next container block in the list
   unsigned short          contnum;          // Number of containers
   unsigned short          userdatasize;     // The compressor associated with the container
                                             // can store user data with the container block
                                             // The user data is physically stored after
                                             // the container objects.
   PathDictNode            *pathdictnode;    // The path dictionary node corresponding to the
                                             // container block
   VPathExpr               *pathexpr;        // The corresponding path expression
   unsigned long           blockid;          // The identifier of the block

   unsigned long ComputeSmallContainerSize();
      // Determines how the accumulated size of all small containers.

   void StoreMainInfo(MemStreamer *output);
      // Stores the structural information about the container block
      // i.e. the number+size of the containers.

   unsigned long GetDataSize();
      // Computes the overall size of the container block

   void CompressSmallContainers(Compressor *compress);
      // Compresses all small containers 
   void CompressLargeContainers(Output *output);
      // Compresses all large containers 

   void FinishCompress();
      // Is called after all small/large containers have been compressed

   void ReleaseMemory();
      // Releases all the memory after compression

public:
   void *operator new(size_t size,unsigned contnum,unsigned userdatasize)  {  return blockmem.GetByteBlock(size+sizeof(CompressContainer)*contnum+userdatasize); }
   void operator delete(void *ptr)  {}
#ifdef SPECIAL_DELETE
   void operator delete(void *ptr,unsigned contnum,unsigned userdatasize)  {}
#endif

   CompressContainerBlock(unsigned long id)
   {
      blockid=id;
   }

   unsigned long GetID() {  return blockid;   }
      // Returns block ID

   CompressContainer *GetContainer(int idx)
      // Returns the container with index 'idx' (0<=idx<=contnum)
   {
      return (CompressContainer *)(this+1)+idx;
   }

   VPathExpr *GetPathExpr()   {  return pathexpr;  }
      // return the path expression of the container block

   unsigned short GetContNum()         {  return contnum;   }
      // Returns container number

   char *GetUserDataPtr()              {  return (char *)(this+1)+sizeof(CompressContainer)*contnum;  }
      // Returns a pointer to the actual user data after the container objects

   unsigned long     GetUserDataSize() {  return userdatasize;  }
      // Returns the size of the container data
};

//***********************************************************************************
//***********************************************************************************
//***********************************************************************************

class CompressContainerMan
   // Manages the sequence of container blocks
   // (and each container block has a list of containers)
{
   unsigned long           containernum;           // The number of containers
   unsigned long           blocknum;               // The number of blocks
   CompressContainerBlock  *blocklist,*lastblock;  // The list of blocks

public:

   CompressContainerMan()
   {
      containernum=0;
      blocknum=0;
      blocklist=lastblock=NULL;
   }

   CompressContainerBlock *CreateNewContainerBlock(unsigned contnum,unsigned userdatasize,PathDictNode *mypathdictnode,VPathExpr *pathexpr);
      // Creates a new container block with 'contnum' containers , with user data size
      // given by 'userdatasize', with the path dictionary node given by 'mypathdictnode'
      // and the path expression given by 'pathexpr'
      // If there is not enough memory, the function returns NULL.

   void FinishCompress();  // Is called after the compression of all containers has finished

   unsigned long GetDataSize();  // Returns the overall size of all containers

   void StoreMainInfo(MemStreamer *memstream);
      // Compresses the structural information of the container blocks

   unsigned long ComputeSmallContainerSize();
      // Determines the overall size of the small containers

   void CompressSmallContainers(Compressor *compress);
      // Compresses the small containers

   void CompressLargeContainers(Output *output);
      // Compresses the large containers

   void ReleaseMemory();
      // Releases all the memory of the containers
};

//****************************************************************************
//****************************************************************************
//****************************************************************************
//****************************************************************************

extern CompressContainerMan   compresscontman;
extern CompressContainer      *globaltreecont;
extern CompressContainer      *globalwhitespacecont;
extern CompressContainer      *globalspecialcont;

#endif

#endif
