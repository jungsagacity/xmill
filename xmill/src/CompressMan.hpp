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

//***************************************************************************
//***************************************************************************

// This module contains the compressor manager, which manages the set
// of compressor factories and the (de)compressor instantiation
// process


#ifndef USERCOMPRESSMAN_HPP
#define USERCOMPRESSMAN_HPP

#include "UserCompress.hpp"

class CompressMan
{
   UserCompressorFactory   *compressorlist,  // The list of user compressors
                           **lastref;        // The 'next' element of the last user compressor

public:

   CompressMan()
      // Initializes the user compressor list as empty
   {
      // We only initialize 'lastref', if the compressorlist is NULL.
      // Otherwise, the function AddCompressFactory has been called
      // *before* the constructor and we must not initialize
      // compressorlist or lastref.

      if(compressorlist==NULL)
         lastref=&compressorlist;
   }

   void AddCompressFactory(UserCompressorFactory *compressor);
      // Adds a new user compressor factory

#ifdef XMILL   
   UserCompressor *CreateCompressorInstance(char * &compressorname,char *endptr);
      // Finds a specific user compressor factory that is specified by
      // 'compressorname' (the length is given by 'endptr-compressorname')
      // The compressor name might have parameters and after the operation,
      // the 'compressorname' is set to the next character following the
      // compressor name.

#endif
#ifdef XDEMILL
   UserUncompressor *CreateUncompressorInstance(char * &compressorname,char *endptr);
      // Finds a specific user decompressor factory that is specified by
      // 'compressorname' (the length is given by 'endptr-compressorname')
      // The compressor name might have parameters and after the operation,
      // the 'compressorname' is set to the next character following the
      // compressor name.

#endif
   UserCompressorFactory *FindCompressorFactory(char *name,int len);
      // Finds a certain user compressor factory with a given name
      // If it doesn't find such a compressor, we exit

#ifdef XMILL
   void PrintCompressorInfo();
      // Prints information about the compressors

   unsigned long GetDataSize();
      // Each compressor is allowed to store own data
      // This function determines how much space is needed by all user compressors

   void CompressSmallGlobalData(Compressor *compressor);
      // Stores own data (only the small pieces!) of the user compressor factories
      // in the output stream described by 'compressor'.

   void CompressLargeGlobalData(Output *output);
      // Stores own data (only the large pieces!) of the user compressor factories
      // in the output stream described by 'compressor'.

#endif

#ifdef XDEMILL
   void UncompressSmallGlobalData(SmallBlockUncompressor *uncompressor);
      // Reads the own (small) data from the source described by 'uncompressor' and
      // initializes the user compressor factories.

   void UncompressLargeGlobalData(Input *input);
      // Reads the own (large) data from the source described by 'uncompressor' and
      // initializes the user compressor factories.

   void FinishUncompress();
      // This is called after all user compressor data has been decompressed

   void DebugPrint();
#endif
};

// The actual user compressor manager
extern CompressMan  compressman;

#endif
