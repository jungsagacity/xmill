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

// This module contains the input file class
// Based on class 'CFile', the new class 'Input' implements the
// buffer management


#ifndef INPUT_HPP
#define INPUT_HPP

#include <stdio.h>
#include <string.h>

#include "File.hpp"

#ifndef min
#define min(x,y)  ( (x)>(y) ? (y) : (x))
#endif

// The standard size of the file buffer
#define FILEBUF_SIZE 65536L

// A macro for refilling the buffer to *at least* 'mylen' characters
// If there are not enough characters in the file, then the program exits
#define FillBufLen(mylen)  if(endptr-curptr<(mylen))  { FillBuf(); if(endptr-curptr<(mylen)) {Error("Unexpected end of file!");Exit();}}

class Input : public CFile
{
   char  databuf[FILEBUF_SIZE];  // The data buffer
   char  *curptr;                // The current buffer position
   char  *endptr;                // Points to the end of the valid data
                                 // (curptr-endptr) determines the number
                                 // of remaining bytes
   unsigned long curlineno;      // The current line number
public:
   Input()
   {
      curptr=endptr=NULL;
      curlineno=1;
   }

   void FillBuf()  // The function fills the buffer as much as possible
   {
      int bytesread;

      if(endptr-curptr>0)
         // Is there some unread data ?
      {
         memmove(databuf,curptr,endptr-curptr);
         endptr=databuf+(endptr-curptr);
         curptr=databuf;
      }
      else
         curptr=endptr=databuf;

      // We try to fill the rest of the buffer
      bytesread=ReadBlock(endptr,databuf+FILEBUF_SIZE-endptr);

      endptr+=bytesread;
   }

   char OpenFile(char *filename)
      // Opens the file and fills the buffer
   {
      if(CFile::OpenFile(filename)==0)
         return 0;

      curptr=endptr=databuf;

      FillBuf();

      curlineno=1;

      return 1;
   }

   char ReadData(char *dest,int len)
      // Reads 'len' characters into the buffer 'dest'
      // If the data is already in memory, we simply copy
      // Otherwise, we iteratively read more from the file
      // The functions returns 0 if 'len' bytes are read
      // It returns 1, if the end of the file has been reached
   {
      int savelen=len;

      while(endptr-curptr<len)
      {
         if(IsEndOfFile())
            return 1;

         len-=endptr-curptr;

         while(curptr<endptr)
         {
            *dest=*curptr;
            if(*curptr=='\n')
               curlineno++;
            dest++;
            curptr++;
         }
         FillBuf();
      }

      while(len>0)
      {
         *dest=*curptr;
         if(*curptr=='\n')
            curlineno++;
         dest++;
         curptr++;
         len--;
      }
      return 0;
   }

   void GetChar(char *ptr)
      // Reads one single character and stores it in *ptr
   {
      FillBufLen(1);
      *ptr=*curptr;
      if(*curptr=='\n')
         curlineno++;
      curptr++;
   }

//********************************************************************

   void ReadFullUInt32(unsigned long *data)
      // Reads a full integer (32 bit)
   {
      FillBufLen(sizeof(unsigned long));

      *data=*(unsigned long *)curptr;
      curptr+=4;
   }

   void ReadUInt32(unsigned long *data)
      // Reads a compressed integer
      // If the MSB (most significant bit) of the first byte is 0, then
      // we have a vlaue <128
      // If the MSB is zero, then we look at the second MSB - if it is 0, then
      // we have a value <16384, otherwise, a value <2^30
   {
      FillBufLen(1);

      if(*(unsigned char *)curptr<128)
         *data=(unsigned)(unsigned char)*(curptr++);
      else
      {
         if(*(unsigned char *)curptr<192)
         {
            FillBufLen(1);
            *data=(((unsigned)(unsigned char)*(curptr++)-128)<<8)+(unsigned)(unsigned char)*(curptr++);
         }
         else
         {
            FillBufLen(3);

            *data=   (((unsigned)(unsigned char)*(curptr++)-192)<<24)+
                     (((unsigned)(unsigned char)*(curptr++))<<16)+
                     (((unsigned)(unsigned char)*(curptr++))<<8)+
                     (unsigned)(unsigned char)*(curptr++);
         }
      }
   }

//**********************************************************************

   void PeekData(char *ptr,int len)
      // The functions looks ahead 'len' bytes and stores them in 'ptr'
   {
      FillBufLen(len);
      mymemcpy(ptr,curptr,len);
   }

   void PeekChar(char *ptr)
      // Peeks one character ahead and stores it in *ptr
      // Returns -1 if there was an error
      // Returns -2 if there is no character left
      // Returns 0 if everything is okay
   {
      FillBufLen(1);
      *ptr=*curptr;
   }

   void SkipData(int len)
      // Skips 'len' characters
      // If the data is already in memory, we simply skip
      // Otherwise, we iteratively read more from the file
      // The functions returns 0 if 'len' bytes are skipped
      // Otherwise, it returns -1, if the was some I/O error
      // If we reached EOF, we return -2
   {
      while(endptr-curptr<len)
      {
         len-=endptr-curptr;
         curptr=endptr=databuf;

         FillBuf();
      }
      curptr+=len;
   }

   void FastSkipData(int len)
      // Does a fast skip - the data is already expected to be in the buffer
   {
      curptr+=len;
   }

   void SkipChar()
      // Skips one single character
   {
      FillBufLen(1);
      if(*curptr=='\n')
         curlineno++;
      curptr++;
   }

   char IsEndOfFile(unsigned len=0)
      // Checks whether we reached the end of the file
   {
      return (curptr+len==endptr)&&(iseof);
   }

   int GetCurBlockPtr(char **ptr)
      // Returns the length of rest of data in the buffer and
      // stores the data pointer in *ptr
   {
      *ptr=curptr;
      return endptr-curptr;
   }

   void RefillAndGetCurBlockPtr(char **ptr,int *len)
      // Refills and
      // returns the length of rest of data in the buffer and
      // stores the data pointer in *ptr
   {
      FillBuf();
      *ptr=curptr;
      *len=endptr-curptr;
   }

   void UndoReadChar()
      // Unreads the last character that was read
   {
      curptr--;
   }
};

#endif
