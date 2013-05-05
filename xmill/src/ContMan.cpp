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

//*************************************************************************************
//*************************************************************************************

// This module contains the container manager for the compressor XMill

#ifdef XMILL

#include "ContMan.hpp"
#include "Output.hpp"
#include "Load.hpp"
#include "PathDict.hpp"
#include "VPathExprMan.hpp"
#include "Types.hpp"

extern char verbose; // We need to reference the 'verbose' flag

unsigned long structcontsizeorig=0;
unsigned long structcontsizecompressed=0;
unsigned long whitespacecontsizeorig=0;
unsigned long whitespacecontsizecompressed=0;
unsigned long specialcontsizeorig=0;
unsigned long specialcontsizecompressed=0;
unsigned long fileheadersize_orig=0;
unsigned long fileheadersize_compressed=0;
unsigned long compressorcontsizeorig=0;
unsigned long compressorcontsizecompressed=0;
unsigned long datacontsizeorig=0;
unsigned long datacontsizecompressed=0;

void InitSpecialContainerSizeSum()
{
   structcontsizeorig=0;
   structcontsizecompressed=0;
   whitespacecontsizeorig=0;
   whitespacecontsizecompressed=0;
   specialcontsizeorig=0;
   specialcontsizecompressed=0;
   compressorcontsizeorig=0;
   compressorcontsizecompressed=0;
   datacontsizeorig=0;
   datacontsizecompressed=0;
}

void PrintSpecialContainerSizeSum()
{
   printf("Header:          Uncompressed: %8lu   Compressed: %8lu (%f%%)\n",
      fileheadersize_orig,fileheadersize_compressed,
      100.0f*(float)fileheadersize_compressed/(float)fileheadersize_orig);
   printf("Structure:       Uncompressed: %8lu   Compressed: %8lu (%f%%)\n",
      structcontsizeorig,structcontsizecompressed,
      100.0f*(float)structcontsizecompressed/(float)structcontsizeorig);
   if(whitespacecontsizeorig!=0)
      printf("Whitespaces:     Uncompressed: %8lu   Compressed: %8lu (%f%%)\n",
         whitespacecontsizeorig,whitespacecontsizecompressed,
         100.0f*(float)whitespacecontsizecompressed/(float)whitespacecontsizeorig);
   if(specialcontsizeorig!=0)
      printf("Special:         Uncompressed: %8lu   Compressed: %8lu (%f%%)\n",
         specialcontsizeorig,specialcontsizecompressed,
         100.0f*(float)specialcontsizecompressed/(float)specialcontsizeorig);
   if(compressorcontsizeorig!=0)
      printf("User Compressor: Uncompressed: %8lu   Compressed: %8lu (%f%%)\n",
         compressorcontsizeorig,compressorcontsizecompressed,
         100.0f*(float)compressorcontsizecompressed/(float)compressorcontsizeorig);
   if(datacontsizeorig!=0)
      printf("Data:            Uncompressed: %8lu   Compressed: %8lu (%f%%)\n",
         datacontsizeorig,datacontsizecompressed,
         100.0f*(float)datacontsizecompressed/(float)datacontsizeorig);

   printf("                                          --------------------\n");
   printf("Sum:                                      Compressed: %8lu\n",
         fileheadersize_compressed+structcontsizecompressed+
         whitespacecontsizecompressed+specialcontsizecompressed+
         compressorcontsizecompressed+datacontsizecompressed);
}
 
//*************************************************************************************
//*************************************************************************************

inline unsigned long CompressContainerBlock::GetDataSize()
   // Computes the overall size of the container block
   // by size of the containers
{
   unsigned long size=0;

   for(int i=0;i<contnum;i++)
      size+=GetContainer(i)->GetSize();

   return size;
}

unsigned long CompressContainerMan::GetDataSize()
   // Computes the overall size of all containers
{
   CompressContainerBlock  *block=blocklist;
   unsigned long           size=0;

   // First, we compress all small containers using the
   // same compressor

   while(block!=NULL)
   {
      size+=block->GetDataSize();
      block=block->nextblock;
   }
   return size;
}

inline void CompressContainerBlock::CompressSmallContainers(Compressor *compress)
   // Compresses the small containers in the container block
{
   for(int i=0;i<contnum;i++)
   {
      if(GetContainer(i)->GetSize()<SMALLCONT_THRESHOLD)
         compress->CompressMemStream(GetContainer(i));
   }
}

void CompressContainerMan::CompressSmallContainers(Compressor *compress)
   // Compresses the small containers of all container blocks
{
   CompressContainerBlock *block=blocklist;

   // First, we compress all small containers using the
   // same compressor

   while(block!=NULL)
   {
      block->CompressSmallContainers(compress);
      block=block->nextblock;
   }
}

inline void CompressContainerBlock::CompressLargeContainers(Output *output)
   // Compresses the large containers of the block
{
   Compressor        compress(output);

   // In 'verbose' mode, we output information about the
   // path dictionary
   if((verbose)&&(pathdictnode!=NULL))
   {
      pathdictnode->PrintInfo();
      printf("\n");
   }

   // Each (large) container is compressed separately
   
   unsigned long  sumuncompressed=0,sumcompressed=0,
                  uncompressedsize,compressedsize;

   // First, we print the status information of the user compressors.
   // For example, for dictionary compressors, the dictonary
   // must be compressed - hence, we output the size of the
   // dictionary before and after compression

   if(verbose & (pathexpr!=NULL))
      pathexpr->GetUserCompressor()->PrintCompressInfo(
         GetUserDataPtr(),&sumuncompressed,&sumcompressed);

   compressorcontsizeorig+=sumuncompressed;
   compressorcontsizecompressed+=sumcompressed;

   char printsum=0;

   if(sumuncompressed>0)
      printsum=1;

   for(int i=0;i<contnum;i++)
   {
      if(verbose)
      {
         if(pathdictnode!=NULL)  // Do we have a normal block?
            printf("       %4lu: ",i);
         else
         {
            switch(i)
            {
            case 0: printf("   Structure:");break;
            case 1: printf(" Whitespaces:");break;
            case 2: printf("     Special:");break;
            }
         }
      }

      if(GetContainer(i)->GetSize()>=SMALLCONT_THRESHOLD)
      {
         sumuncompressed+=GetContainer(i)->GetSize();

         compress.CompressMemStream(GetContainer(i));
         compress.FinishCompress(&uncompressedsize,&compressedsize);

         if(verbose)
            printf("%8lu ==> %8lu (%f%%)\n",uncompressedsize,compressedsize,100.0f*(float)compressedsize/(float)uncompressedsize);

         sumcompressed+=compressedsize;

         if(pathdictnode==NULL)  // The first block?
         {
            // For the three special container, we keep track of the sum
            // of the (un)compressed data size
            switch(i)
            {
            case 0:  structcontsizeorig+=uncompressedsize;
                     structcontsizecompressed+=compressedsize;
                     break;
            case 1:  whitespacecontsizeorig+=uncompressedsize;
                     whitespacecontsizecompressed+=compressedsize;
                     break;
            case 2:  specialcontsizeorig+=uncompressedsize;
                     specialcontsizecompressed+=compressedsize;
            }
         }
         else
         {
            datacontsizeorig+=uncompressedsize;
            datacontsizecompressed+=compressedsize;
         }
      }
      else
      {
         if(verbose)
            printf("%8lu ==> Small...\n",GetContainer(i)->GetSize());
      }
   }
   if(verbose)
   {
      if((contnum>1)||printsum)
         printf("        Sum: %8lu ==> %8lu (%f%%)\n\n",sumuncompressed,sumcompressed,100.0f*(float)sumcompressed/(float)sumuncompressed);
      else
         printf("\n");
   }
}

void CompressContainerMan::CompressLargeContainers(Output *output)
{
   CompressContainerBlock *block=blocklist;

   while(block!=NULL)
   {
      block->CompressLargeContainers(output);
      block=block->nextblock;
   }
}

//**********************************************************************
//**********************************************************************

inline void CompressContainerBlock::FinishCompress()
{
   if(pathexpr!=NULL)
      pathexpr->FinishCompress(GetContainer(0),GetUserDataPtr());
}

void CompressContainerMan::FinishCompress()
{
   CompressContainerBlock *block=blocklist;
   while(block!=NULL)
   {
      block->FinishCompress();
      block=block->nextblock;
   }
}

//***********************************************************************************
//***********************************************************************************
//***********************************************************************************
//***********************************************************************************

inline void CompressContainerBlock::StoreMainInfo(MemStreamer *output)
{
   output->StoreUInt32((pathexpr!=NULL) ? pathexpr->GetIdx() : 0);
   output->StoreUInt32(contnum);

   for(int i=0;i<contnum;i++)
      // First, we store the number of data containers
      output->StoreUInt32(GetContainer(i)->GetSize());
}

void CompressContainerMan::StoreMainInfo(MemStreamer *memstream)
{
   // First, we store the number of container block
   memstream->StoreUInt32(blocknum);

   CompressContainerBlock *block=blocklist;

   while(block!=NULL)
   {
      block->StoreMainInfo(memstream);
      block=block->nextblock;
   }

//   compressor->CompressMemStream(memstream);
}

//*************************************************************************
//*************************************************************************

inline void CompressContainerBlock::ReleaseMemory()
   // Releases the memory of the containers
{
   for(int i=0;i<contnum;i++)
      // Let's release the memory of all containers
      GetContainer(i)->ReleaseMemory(0);
}

void CompressContainerMan::ReleaseMemory()
{
   CompressContainerBlock *block=blocklist;

   while(block!=NULL)
   {
      block->ReleaseMemory();
      block=block->nextblock;
   }

   containernum=0;
   blocknum=0;
   blocklist=lastblock=NULL;
}


CompressContainerBlock *CompressContainerMan::CreateNewContainerBlock(unsigned contnum,unsigned userdatasize,PathDictNode *mypathdictnode,VPathExpr *pathexpr)
   // Creates a new container block with 'contnum' containers , with user data size
   // given by 'userdatasize', with the path dictionary node given by 'mypathdictnode'
   // and the path expression given by 'pathexpr'
   // If there is not enough memory, the function returns NULL.
{
   CompressContainerBlock *block=new(contnum,userdatasize) CompressContainerBlock(blocknum);
   if(block==NULL)
      return NULL;

   block->contnum=(unsigned short)contnum;
   block->userdatasize=(unsigned short)userdatasize;

   block->pathdictnode=mypathdictnode;
   block->pathexpr=pathexpr;

   // Let's add the container block at the end of the list
   block->nextblock=NULL;

   if(blocklist==NULL)
      blocklist=lastblock=block;
   else
   {
      lastblock->nextblock=block;
      lastblock=block;
   }

   // We initialize the memory for all the containers
   for(unsigned i=0;i<contnum;i++)
      block->GetContainer(i)->Initialize();

   blocknum++;
   containernum+=contnum;

   return block;
}

#endif
