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

// This module contains the abstract definition of user compressors

#ifndef USERCOMPRESS_HPP
#define USERCOMPRESS_HPP

class CompressMan;

#include "MemStreamer.hpp"

#ifdef XMILL
#include "ContMan.hpp"

class UserCompressor;
class CompressContainer;
#endif


#ifdef XDEMILL
#include "XMLOutput.hpp"

class UserUncompressor;
class Input;
class SmallBlockUncompressor;
#endif

extern MemStreamer mainmem;

class UserCompressorFactory
   // Each user compressor is firstly represented by a UserCompressorFactory object
   // Typically, there is one global UserCompressorFactory object.
   // This object is then registered and provides basic capabilities
   // such as returning its name, instantiating compressor or decompressor
{
   friend CompressMan;

   UserCompressorFactory   *next;   // The next factory in the list of compressor factories

   virtual char *GetName()=0;
      // Must return the (unique!) name of the compressor

   virtual char *GetDescription()=0;
      // Should return a single-line (without '\n') description of the compressor

#ifdef XMILL
   virtual UserCompressor   *InstantiateCompressor(char *paramstr,int len)=0;
      // Instantiates the Compressor for specific parameters
#endif
#ifdef XDEMILL
   virtual UserUncompressor *InstantiateUncompressor(char *paramstr,int len)=0;
      // Instantiates the Decompressor for specific parameters
#endif

   // CompressFactories are also allowed to store status information
   // in the compressed file! The following procedure as used for
   // compressing/decompressing this information
   // Small data (<2KBytes) is stored in the header, while
   // lare data is stored in separate zlib-blocks in the output file
#ifdef XMILL
   virtual void CompressSmallGlobalData(Compressor *compressor)  {}
   virtual void CompressLargeGlobalData(Output *output)  {}

   virtual unsigned long GetGlobalDataSize() {  return 0;}
      // Determines how much memory the uncompressed status information
      // needs. This number is stored in the compressed file and
      // later used in the decompressor to allocate the right amount
      // of memory
#endif
   // Similarly, the user compressor factory on the decompressor site
   // is able to decompress the data from the input file
#ifdef XDEMILL
   virtual void UncompressSmallGlobalData(SmallBlockUncompressor *uncompressor)  {}
   virtual void UncompressLargeGlobalData(Input *input)  {}
   virtual void FinishUncompress()  {}
#endif
public:
   UserCompressorFactory();
};

//**************************************************************************
//**************************************************************************

// THe definition of the user compressor class follows

#ifdef XMILL

class UserCompressor
{
   friend CompressMan;

protected:
   unsigned short    datasize;      // The memory space needed for representing the state
                                    // of this compressor
   unsigned short    contnum:13;    // The number of containers requested by this compressor
   unsigned short    isrejecting:1; // Determines whether the compressor can potentially reject strings
   unsigned short    canoverlap:1;  // Determines whether the containers used by the compressor can also be
                                    // used by other compressor. In other words, is it possible that other
                                    // is stored in the container between the compressed data of two strings
                                    // of this compressor. For example, the run-length encode
                                    // is not 'overlapping', while most other
                                    // compressors are overlapping
   unsigned short    isfixedlen:1;  // Does the compressor accept strings of fixed length?
                                    // Most compressors don't. The constant compressor does.

public:

   void *operator new(size_t size)  {  return mainmem.GetByteBlock(size);}
   void operator delete(void *ptr)  {}
   
   // The following functions must be overloaded by the corresponding
   // user compressor

   unsigned short GetUserContNum()  {  return contnum;      }
   unsigned short GetUserDataSize() {  return datasize;     }
   unsigned char IsRejecting()      {  return (unsigned char)isrejecting;  }
   unsigned char CanOverlap()       {  return (unsigned char)canoverlap;   }
   unsigned char IsFixedLen()       {  return (unsigned char)isfixedlen;   }

// The following function are for compression

   virtual void InitCompress(CompressContainer *cont,char *dataptr)    {}
      // Before we start any compression, we initialize the compressor.
      // 'dataptr' denotes the state

   virtual char ParseString(char *str,unsigned len,char *dataptr) {  return 1; }
      // Parses the string and returns 1, if accepted, otherwise 0.
      // This function does not actually store/compreess the string
      // But it can keep an internal state - in the next step,
      // CompressString is called.

   virtual void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)=0;
      // Compresses the given input string
      // 'dataptr' denotes the state
      // If the compressor is 'rejecting', then the function can expect
      // that 'ParseString' has been called before.

   virtual void FinishCompress(CompressContainer *cont,char *dataptr)  {}
      // Finishes the compression - the compressor should write any
      // remaining data to the containers

   virtual void PrintCompressInfo(char *dataptr,unsigned long *overalluncomprsize,unsigned long *overallcomprsize) {}
      // Prints statistical information about how well the compressor compressed
      // the data
};

#endif

//***************************************************************************

#ifdef XDEMILL

class UncompressContainer;

class UserUncompressor
{
   friend CompressMan;

protected:
   unsigned short    datasize;   // The size of the user compressor state space
   unsigned short    contnum;    // The number of containers for this user compressor

public:

   void *operator new(size_t size)  {  return mainmem.GetByteBlock(size);}
   void operator delete(void *ptr)  {}
   
   unsigned short GetUserContNum()  {  return contnum;      }
   unsigned short GetUserDataSize() {  return datasize;     }

   // The following functions must be overloaded by the corresponding
   // user decompressor
   virtual void InitUncompress(UncompressContainer *cont,char *dataptr)    {}
      // Initializes the decompressor

   virtual void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)=0;
      // Does the actual decompression of a single text item
      // and prints the text to 'output'

   virtual void FinishUncompress(UncompressContainer *cont,char *dataptr)  {}
      // Finished the decompression
};

#endif

//*************************************************************************
//*************************************************************************

// An auxiliary function to identify the end of a string
inline char *ParseString(char *from,char *to)
{
   if(*from!='"')
      return NULL;

   from++;

   while(from<to)
   {
      if(*from=='"')
         return from;
      from++;
   }

   Error("String constant \"...\" expected instead of '");
   ErrorCont(from,to-from);
   Exit();
   return NULL;
}

inline char *SkipWhiteSpaces(char *ptr,char *endptr)
   // Skips white spaces starting at *ptr and returns the
   // the pointer to the first non-white-space character.
   // The iteration stops if 'endptr' is reached.
{
   while((ptr<endptr)&&
         ((*ptr==' ')||(*ptr==',')||(*ptr=='\t')||(*ptr=='\r')||(*ptr=='\n')))

      ptr++;

   return ptr;
}

#endif
