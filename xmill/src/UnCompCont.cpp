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

#include "UnCompCont.hpp"
#include "VPathExprMan.hpp"
#include "Types.hpp"
#include "SmallUncompress.hpp"

extern VPathExprMan pathexprman;

UncompressContainerMan  uncomprcont;

inline void UncompressContainer::AllocateContMem(unsigned long mincontsize)
   // Allocates the memory - but only if the size is larger than 'mincontsize'
   // This will allows us to allocate memory starting with the largest
   // containers. Then, smaller containers can be allocated.
{
   if((dataptr!=NULL)||(size<mincontsize))
      return;

   curptr=dataptr=AllocateMemBlock(size);
}

inline void UncompressContainerBlock::Load(SmallBlockUncompressor *uncompressor)
      // Loads the container block information (number+size of containers)
      // from the small decompressor object
{
   // Let's load the index of the path expression
   unsigned long pathidx=uncompressor->LoadUInt32();
   if(pathidx!=0)
      pathexpr=pathexprman.GetPathExpr(pathidx-1);
   else
      pathexpr=NULL;

   // Let's load the number of containers
   contnum=uncompressor->LoadUInt32();

   if((pathexpr!=NULL)&&(pathexpr->GetUserContNum()!=contnum))
   {
      Error("Corrupt compressed file !");
      Exit();
   }

   // Let's allocate some memory for the container structures
   // and the necessary state space
   contarray=(UncompressContainer *)blockmem.GetByteBlock(
      sizeof(UncompressContainer)*contnum+
      ((pathexpr!=NULL) ? pathexpr->GetUserDataSize() : 0));
   
   // let's load the size of each single container
   for(unsigned i=0;i<contnum;i++)
      contarray[i].SetSize(uncompressor->LoadUInt32());
};

inline void UncompressContainerBlock::AllocateContMem(unsigned long mincontsize)
   // Allocates the memory for those containers with size>mincontsize
{
   for(unsigned i=0;i<contnum;i++)
      contarray[i].AllocateContMem(mincontsize);
};

//****************************************************************************
//****************************************************************************

void UncompressContainerMan::Load(SmallBlockUncompressor *uncompressor)
   // Loads the structural information from the small block decompressor
{
   // The number of container blocks
   blocknum=uncompressor->LoadUInt32();

   // Let's allocate the container block array
   blockarray=(UncompressContainerBlock *)blockmem.GetByteBlock(sizeof(UncompressContainerBlock)*blocknum);

   // Let's load the structural information for all container blocks
   for(unsigned i=0;i<blocknum;i++)
      blockarray[i].Load(uncompressor);
}


void UncompressContainerMan::AllocateContMem()
      // Allocates the memory for the container sequentially.
      // Loads the container block information (number+size+structure of container blocks)
      // from the small decompressor object
      // This function starts with large containers and then allocates
      // memory for smaller containers
{
   unsigned i;

   // We do the container allocation in 5 steps

   for(i=0;i<blocknum;i++)
      blockarray[i].AllocateContMem(1000000L);

   for(i=0;i<blocknum;i++)
      blockarray[i].AllocateContMem(200000L);

   for(i=0;i<blocknum;i++)
      blockarray[i].AllocateContMem(40000L);

   for(i=0;i<blocknum;i++)
      blockarray[i].AllocateContMem(8000L);

   for(i=0;i<blocknum;i++)
      blockarray[i].AllocateContMem(0L);
}

//****************************************************************************
//****************************************************************************

void UncompressContainer::UncompressSmallContainer(SmallBlockUncompressor *uncompressor)
   // Decompresses the small container data and stores
   // it in the data buffer
{
   unsigned char *srcptr=uncompressor->LoadData(size);
   memcpy(dataptr,srcptr,size);
}

void UncompressContainer::UncompressLargeContainer(Input *input)
   // Decompresses the large container data and stores
   // it in the data buffer
{
   Uncompressor uncompress;
   unsigned long  uncompsize=size;

   uncompress.Uncompress(input,dataptr,&uncompsize);
   if(uncompsize!=size)
   {
      Error("Corrupt file!");
      Exit();
   }
}

//****************************************************************************

void UncompressContainerBlock::UncompressSmallContainers(SmallBlockUncompressor *uncompressor)
   // Decompresses the data of small containers and stores
   // it in the data buffers
{
   unsigned long i;

   for(i=0;i<contnum;i++)
   {
      if(contarray[i].GetSize()<SMALLCONT_THRESHOLD)
         contarray[i].UncompressSmallContainer(uncompressor);
   }
}

void UncompressContainerBlock::UncompressLargeContainers(Input *input)
   // Decompresses the data of large containers and stores
   // it in the data buffers
{
   for(unsigned long i=0;i<contnum;i++)
   {
      if(contarray[i].GetSize()>=SMALLCONT_THRESHOLD)
         contarray[i].UncompressLargeContainer(input);
   }
}

//****************************************************************************
//****************************************************************************

void UncompressContainerMan::UncompressSmallContainers(SmallBlockUncompressor *uncompressor)
   // Decompresses the data of small containers and stores
   // it in the data buffers
{
   for(unsigned long i=0;i<blocknum;i++)
      blockarray[i].UncompressSmallContainers(uncompressor);
}

void UncompressContainerMan::UncompressLargeContainers(Input *input)
   // Decompresses the data of large containers and stores
   // it in the data buffers
{
   for(unsigned long i=0;i<blocknum;i++)
      blockarray[i].UncompressLargeContainers(input);
}

//****************************************************************************

void UncompressContainerMan::Init()
   // Initializes the state data for the user compressors
   // of all container blocks
{
   for(unsigned long i=0;i<blocknum;i++)
      blockarray[i].Init();
}


//****************************************************************************
//****************************************************************************

// All functions for release the memory of the containers

inline void UncompressContainer::ReleaseContMem()
{
   FreeMemBlock(dataptr,size);
}

inline void UncompressContainerBlock::ReleaseContMem()
{
   for(unsigned long i=0;i<contnum;i++)
      contarray[i].ReleaseContMem();
}

void UncompressContainerMan::ReleaseContMem()
{
   for(unsigned long i=0;i<blocknum;i++)
      blockarray[i].ReleaseContMem();
}

void UncompressContainerMan::FinishUncompress()
{
   for(unsigned long i=0;i<blocknum;i++)
      blockarray[i].FinishUncompress();
}
