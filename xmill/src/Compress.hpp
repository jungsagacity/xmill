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

// This module implements 'Compressor' and 'Uncompressor' -
// the interfaces to the ZLIB (either gzip or bzip) libary


#ifndef COMPRESS_HPP
#define COMPRESS_HPP

#include "../zlib/zlib.h"


class Output;
class MemStreamer;
class Input;

//************************************************************************

// The Compressor



class Compressor
{
   // We store the state for the zlib compressor
#ifdef USE_BZIP
   bz_stream      state;
#else
   z_stream       state;
#endif
   Output         *output;
   unsigned char  isinitialized:1;

public:
   Compressor(Output *myoutput);
   ~Compressor();
   void CompressMemStream(MemStreamer *memstream);
      // Reads the data from 'memstream' and sends
      // it to the compressor

   void CompressData(unsigned char *ptr,unsigned len);
      // Compresses the data at position 'ptr' of length 'len'

   void FinishCompress(unsigned long *uncompressedsize,unsigned long *compressedsize);
      // Finishes the compression and stores the input data size and
      // the output data size in 'uncompressedsize' and 'compressedsize'
};


//************************************************************************

// The Decompressor



class Uncompressor
{
   // We store the states for the zlib compressor
#ifdef USE_BZIP
   bz_stream      state;
#else
   z_stream       state;
#endif

public:
   unsigned char     isinitialized:1;

public:

   Uncompressor() {  isinitialized=0;  }

   char Uncompress(Input *input,unsigned char *dataptr,unsigned long *len);
      // Decompresses the data from 'input' and stores
      // the result in 'dataptr'. It decompresses at most *len
      // bytes. Afterwards, '*len' is set to the actual number
      // of bytes uncompressed.
      // The function returns 1, if output buffer is full and
      // there is more data to read. Otherwise, the function returns 0.
};


#endif
