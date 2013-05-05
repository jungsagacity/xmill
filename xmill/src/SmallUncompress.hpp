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

// This module contains a special decompressor sequential decompression
// of small blocks of data. The decompressor maintains a temporary buffer
// that keeps the small blocks and that can be read sequentially by
// the client.

#ifndef SMALLUNCOMPRESS_HPP
#define SMALLUNCOMPRESS_HPP

#include <string.h>

#include "Compress.hpp"

class SmallBlockUncompressor : public Uncompressor
{
   // The buffer
   unsigned char  *buf;
   unsigned long  bufsize;

   
   unsigned char  *curptr;       // The current pointer into the buffer
   unsigned long  curdatasize;   // The remaining size

   Input          *input;        // The input file
public:

   void UncompressMore(unsigned long *len)
      // Uncompresses at least len bytes
   {
      if(curdatasize>=*len) // Already enough data in the buffer?
         return;

      if(*len>bufsize)
         // Do we need more space than we have in the buffer?
      {
         // We take 'len' as the new buffer size.

         // But we allocate at least 30000 bytes!
         if(*len<30000)
            *len=30000;

         unsigned char *newbuf=(unsigned char *)malloc(*len);
         if(newbuf==NULL)
            ExitNoMem();

         // Copy the already existing data
         if(curdatasize>0)
            memcpy(newbuf,curptr,curdatasize);

         if(buf!=NULL)  // Release the old buffer
            free(buf);

         bufsize=*len;
         buf=newbuf;
         curptr=newbuf;
      }
      else
      {
         // We have enough space in the buffer
         // ==> Move existing data to the beginning
         if(curdatasize>0)
            memmove(buf,curptr,curdatasize);

         curptr=buf;
      }

      // Now we can load more data from the decompressor
      // and store it in the buffer

      // We try to read as mu
      unsigned long maxlen=bufsize-curdatasize;

      Uncompress(input,buf+curdatasize,&maxlen);

      curdatasize+=maxlen;

      // We didn't read everything that was requested?
      if(curdatasize<*len)
         *len=curdatasize;
   }

public:

   SmallBlockUncompressor(Input *myinput)
   {
      curdatasize=0;
      bufsize=0;
      input=myinput;
      curptr=buf=NULL;
   }

   ~SmallBlockUncompressor()
   {
      if(buf!=NULL)
         free(buf);
   }

   void Init()
   {
      // We initialize the buffer with a size of 65536L
      buf=(unsigned char *)malloc(65536L);
      if(buf==0)
         ExitNoMem();
      bufsize=65536L;
      curptr=buf;
   }

   unsigned char *GetCurPtr()       {  return curptr; }
   unsigned long GetCurDataSize()   {  return curdatasize;  }


   unsigned char *LoadData(unsigned long len)
      // Loads 'len' bytes from the decompressor
      // and returns the pointer
   {
      unsigned long resultlen=len;

      UncompressMore(&resultlen);
      if(resultlen!=len)
      {
         Error("Corrupt file!");
         Exit();
      }
      curptr+=len;
      curdatasize-=len;

      return curptr-len;
   }

   unsigned long LoadUInt32()
      // Loads a compressed integer
   {
      unsigned long saveval;

      if(curdatasize<4)
      {
         saveval=1;
         UncompressMore(&saveval);
      }
      if(*curptr<128)
      {
         curptr++;
         curdatasize--;
         return (unsigned long)curptr[-1];
      }
      else
      {
         if(*curptr<192)
         {
            saveval=2;
            UncompressMore(&saveval);
            curptr+=2;
            curdatasize-=2;
            return ((unsigned)(curptr[-2]-128)<<8)+(unsigned)curptr[-1];
         }
         else
         {
            saveval=4;
            UncompressMore(&saveval);
            curptr+=4;
            curdatasize-=4;
            return   ((unsigned)(curptr[-4]-192)<<24)+
                     ((unsigned)curptr[-3]<<16)+
                     ((unsigned)curptr[-2]<<8)+
                     (unsigned)curptr[-1];
         }
      }
   }

   unsigned long LoadSInt32(char *isneg)
      // Loads a compressed signed integer
   {
      unsigned long saveval;
      if(curdatasize<4)
      {
         saveval=1;
         UncompressMore(&saveval);
      }
      if(*curptr<128)
      {
         *isneg=((*curptr & 64) ? 1 : 0);

         curptr++;
         curdatasize--;
         return (unsigned long)(curptr[-1]&63);
      }
      else
      {
         *isneg=((*curptr & 32) ? 1 : 0);

         if(*curptr<192)
         {
            saveval=2;
            UncompressMore(&saveval);
            curptr+=2;
            curdatasize-=2;
            return ((unsigned)(curptr[-2]&31)<<8)+(unsigned)curptr[-1];
         }
         else
         {
            saveval=4;
            UncompressMore(&saveval);
            curptr+=4;
            curdatasize-=4;
            return   ((unsigned)(curptr[-4]&31)<<24)+
                     ((unsigned)curptr[-3]<<16)+
                     ((unsigned)curptr[-2]<<8)+
                     (unsigned)curptr[-1];
         }
      }
   }

   unsigned long LoadString(char **str)
      // Loads a string - a string starts with a length field
      // and the actual data follows
      // The pointer to the string is stored in '*str' and
      // the length is returns
   {
      unsigned long len=LoadUInt32();
      unsigned long mylen=len;

      UncompressMore(&mylen);
      *str=(char *)curptr;
      curptr+=len;
      curdatasize-=len;
      return len;
   }
};

#endif
