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

// This module contains the interface to the ZLIB (either gzip or bzip)
// libary

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "..\zlib\zlib.h"


#include "Compress.hpp"
#include "MemStreamer.hpp"
#include "Input.hpp"
#include "Output.hpp"


extern unsigned char zlib_compressidx;


//************************************************************************

#ifdef ZALLOC_COUNT
unsigned long allocsize=0;
#endif

// Function zalloc is called by the zlib library to allocate memory

#ifdef USE_BZIP
void *zalloc(void *opaque,int items,int size)
#else
void *zalloc(void *opaque,unsigned items,unsigned size)
#endif
{
#ifdef ZALLOC_COUNT
   void *ptr=malloc(items*size+4);
#else
   void *ptr=malloc(items*size);
#endif
   if(ptr==NULL)
      ExitNoMem();

//   printf("zalloc : %lu * %lu ==> %lX\n",items,size,ptr);

#ifdef ZALLOC_COUNT
   allocsize+=size;
   *(unsigned *)ptr=size;
   return (void *)(((char *)ptr)+4);
#else
   return (void *)ptr;
#endif
}

// Function zfree is called by the zlib library to release memory

void zfree(void *opaque,void *ptr)
{
#ifdef ZALLOC_COUNT
   allocsize-=*((unsigned *)ptr-1);
   free((char *)ptr-4);
#else
   free((char *)ptr);
#endif
}

//******************************************************************
//******************************************************************

// The compressor part



Compressor::Compressor(Output *myoutput)
   // The constructor
{
#ifdef USE_BZIP
   state.bzalloc=zalloc;
   state.bzfree=zfree;
#else
   state.zalloc=zalloc;
   state.zfree=zfree;
#endif

   isinitialized=0;

   output=myoutput;
};

Compressor::~Compressor()
   // The deconstructor
{
   if(isinitialized)
      // We finish compression, if there has been some data in the queue
   {
#ifdef USE_BZIP
      bzCompressEnd(&state);
#else
      deflateEnd(&state);
#endif

//      deflateReset(&state);
      isinitialized=0;
   }
}

void Compressor::CompressMemStream(MemStreamer *memstream)
   // Reads the data from 'memstream' and sends
   // it to the compressor
{
   MemStreamBlock *curblock=memstream->GetFirstBlock();
   char           first=1;
   int            saveavail;

   // Any data there? 
   if(memstream->GetSize()==0)
      return;

#ifdef USE_BZIP
   state.next_out=(char *)output->GetBufPtr((int *)&state.avail_out);
   state.next_in=(char *)curblock->data;
#else
   state.next_out=(unsigned char *)output->GetBufPtr((int *)&state.avail_out);
   state.next_in=(unsigned char *)curblock->data;
#endif

   // If there is no space in the output buffer, we need to flush
   if(state.avail_out==0)
   {
      // If the output buffer is full, we flush
      output->Flush();

      // We get the next piece of output buffer that will be filled up
#ifdef USE_BZIP
      state.next_out=(char *)output->GetBufPtr((int *)&state.avail_out);
#else
      state.next_out=(unsigned char *)output->GetBufPtr((int *)&state.avail_out);
#endif
   }

//   state.avail_in=curblock->cursize;

   // If we haven't initialized yet, we do that now
   if(isinitialized==0)
   {
#ifdef USE_BZIP
      if(bzCompressInit(&state,7,0,0)!=BZ_OK)
#else
      if(deflateInit(&state,zlib_compressidx)!=Z_OK)
#endif
      {
         Error("Error while compressing container!");
         Exit();
      }
      state.total_out=0;
      state.total_in=0;

      isinitialized=1;
   }

   do
   {
      // Let's the input block to 'curblock'
#ifdef USE_BZIP
      state.next_in=(char *)curblock->data;
#else
      state.next_in=(unsigned char *)curblock->data;
#endif
      state.avail_in=curblock->cursize;

      // As long as we still have data in the input block, we continue
      while(state.avail_in>0)
      {
         saveavail=state.avail_out;

#ifdef USE_BZIP
         if(bzCompress(&state,BZ_RUN)!=BZ_RUN_OK)
#else
         if(deflate(&state,Z_NO_FLUSH)!=Z_OK)
#endif
         {
            Error("Error while compressing container!");
            Exit();
         }

         // We tell the output stream that 'saveavail-state.avail_out' bytes
         // are now ready for output
         output->SaveBytes(saveavail-state.avail_out);

         if((state.avail_in==0)&&(state.avail_out>0))
            // Is the input buffer is empty ? ==> We go to next block
            break;

         // If the output buffer is full, we flush
         output->Flush();

         // We get the next piece of output buffer that will be filled up
#ifdef USE_BZIP
         state.next_out=(char *)output->GetBufPtr((int *)&state.avail_out);
#else
         state.next_out=(unsigned char *)output->GetBufPtr((int *)&state.avail_out);
#endif
      }

      // There is no more input data available ==> We go to next block in MemStreamer
      curblock=curblock->next;
   }
   while(curblock!=NULL);
}

void Compressor::CompressData(unsigned char *ptr,unsigned len)
   // Compresses the data at position 'ptr' of length 'len'
{
   int            saveavail;

   // Let's get some space in the output buffer
#ifdef USE_BZIP
   state.next_out=output->GetBufPtr((int *)&state.avail_out);
   state.next_in=(char *)ptr;
#else
   state.next_out=(unsigned char *)output->GetBufPtr((int *)&state.avail_out);
   state.next_in=ptr;
#endif

   state.avail_in=len;

   // If we haven't initialized the compressor yet, then let's do it now
   if(isinitialized==0)
   {
#ifdef USE_BZIP
      if(bzCompressInit(&state,7,0,0)!=BZ_OK)
#else
      if(deflateInit(&state,zlib_compressidx)!=Z_OK)
#endif
      {
         Error("Error while compressing container!");
         Exit();
      }
      state.total_out=0;
      state.total_in=0;
      isinitialized=1;
   }

   do
   {
      saveavail=state.avail_out;

      // The actual compression
#ifdef USE_BZIP
      if(bzCompress(&state,BZ_RUN)!=BZ_RUN_OK)
#else
      if(deflate(&state,Z_NO_FLUSH)!=Z_OK)
#endif
      {
         Error("Error while compressing container!");
         Exit();
      }
      output->SaveBytes(saveavail-state.avail_out);
         // We tell the output stream that 'saveavail-state.avail_out' bytes
         // are now ready for output

      if((state.avail_in==0)&&(state.avail_out>0))
         // Is the input buffer is empty ? ==> We go to next block
         break;

      // If the output buffer is full, we flush
      output->Flush();

      // We get the next piece of output buffer that will be filled up
#ifdef USE_BZIP
      state.next_out=output->GetBufPtr((int *)&state.avail_out);
#else
      state.next_out=(unsigned char *)output->GetBufPtr((int *)&state.avail_out);
#endif
   }
   while(1);
}

void Compressor::FinishCompress(unsigned long *uncompressedsize,unsigned long *compressedsize)
   // Finishes the compression and stores the input data size and
   // the output data size in 'uncompressedsize' and 'compressedsize'
{
   char err;
   int   saveavail;

   do
   {
      // Let's get more space in the output buffer
#ifdef USE_BZIP
      state.next_out=output->GetBufPtr((int *)&state.avail_out);
#else
      state.next_out=(unsigned char *)output->GetBufPtr((int *)&state.avail_out);
#endif

      saveavail=state.avail_out;

#ifdef USE_BZIP
      err=bzCompress(&state,BZ_FINISH);
#else
      err=deflate(&state,Z_FINISH);
#endif

      output->SaveBytes(saveavail-state.avail_out);

#ifdef USE_BZIP
      if(err==BZ_STREAM_END)
         break;
      if(err!=BZ_FINISH_OK)
#else
      if(err==Z_STREAM_END)
         break;
      if(err!=Z_OK)
#endif
      {
         Error("Error while compressing container!");
         Exit();
      }

      // Z_OK means that the output buffer is full ! ==> We flush
      output->Flush();
   }
   while(1);

   // Let's store the input and output size
   if(uncompressedsize!=NULL) *uncompressedsize =state.total_in;
   if(compressedsize!=NULL)   *compressedsize   =state.total_out;

   state.total_out=0;
   state.total_in=0;

   // Finally, we release the internal memory
#ifdef USE_BZIP
   if(bzCompressEnd(&state)!=BZ_OK)
#else
   if(deflateReset(&state)!=Z_OK)
#endif
   {  
      Error("Error while compressing container!");
      Exit();
   }
#ifdef USE_BZIP
   // bzip must be reinitialized
   isinitialized=0;
#endif
}



//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************



char Uncompressor::Uncompress(Input *input,unsigned char *dataptr,unsigned long *len)
   // Decompresses the data from 'input' and stores
   // the result in 'dataptr'. It decompresses at most *len
   // bytes. Afterwards, '*len' is set to the actual number
   // of bytes uncompressed.
   // The function returns 1, if output buffer is full and
   // there is more data to read. Otherwise, the function returns 0.
{
   // We haven't initialized the object yet, we do that now
   if(isinitialized==0)
   {
#ifdef USE_BZIP
      state.bzalloc=zalloc;
      state.bzfree=zfree;
#else
      state.zalloc=zalloc;
      state.zfree=zfree;
#endif

#ifdef USE_BZIP
      if(bzDecompressInit(&state,0,0)!=BZ_OK)
#else
      if(inflateInit(&state)!=Z_OK)
#endif
      {
         Error("Error while compressing container!");
         Exit();
      }

      isinitialized=1;
   }

   int   save_in;

#ifdef USE_BZIP
   state.next_out=(char *)dataptr;
#else
   state.next_out=(unsigned char *)dataptr;
#endif

   // Let's remember how much space we have in the output ...
   state.avail_out=*len;

   // ... and we get the piece of data from the input
   state.avail_in=input->GetCurBlockPtr((char **)&(state.next_in));

   do
   {
      // We save the amount of input data that is available
      // This will be used to compute how much input data was
      // decompressed
      save_in=state.avail_in;

      // The actual decompression
#ifdef USE_BZIP
      switch(bzDecompress(&state))
#else
      switch(inflate(&state,Z_NO_FLUSH))
#endif
      {
         // Did we finish completely?
#ifdef USE_BZIP
      case BZ_STREAM_END:
#else
      case Z_STREAM_END:
#endif
         // We skip over the amount of data that was decompressed
         input->SkipData(save_in-state.avail_in);

         // Let's store the overall amount of "decompressed" data.
         *len=state.total_out;

         // Let's finish the decompression entirely
#ifdef USE_BZIP
         if(bzDecompressEnd(&state)!=BZ_OK)
#else
         if(inflateReset(&state)!=Z_OK)
#endif
         {
            Error("Error while compressing container!");
            Exit();
         }
#ifdef USE_BZIP
         // Only for BZIP, we need to reinitialize
         isinitialized=0;
#endif
         return 0;   // We reached the end

         
#ifdef USE_BZIP
      case BZ_OK:
#else
      case Z_OK:
#endif
         // Did we do the decompression correctly?
         // => Let's go to the next piece of data
         break;

      default:
         // In all other cases, we have an error
         Error("Error while uncompressing container!");
         Exit();
      }

      // Skip the input data that was decompressed
      input->SkipData(save_in-state.avail_in);

      if(state.avail_out==0)  // Output buffer is full
         return 1;

//      if(state.avail_in>0) // Something is wrong !
//         return -1;

      // Let's get the next input data block
      input->RefillAndGetCurBlockPtr((char **)&(state.next_in),(int *)&(state.avail_in));
   }
   while(1);
}


