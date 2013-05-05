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

// This module contains several auxiliary functions for loading compressed
// integer from a given data buffer


#ifndef LOAD_HPP
#define LOAD_HPP

typedef unsigned char TInputPtr;

inline char LoadChar(unsigned char * &ptr)
   // Loads a single character
{
   return (char)*(ptr++);
}

//***********************************************************************

inline unsigned long LoadUInt32(unsigned char * &ptr)
   // Loads a compressed unsigned integer
{
  if(*ptr<128)
     return (unsigned)*(ptr++);
  else
  {
     if(*ptr<192)
     {
        ptr+=2;
        return (((unsigned)ptr[-2]-128)<<8)+(unsigned)ptr[-1];
     }
     else
     {
        ptr+=4;
        return (((unsigned)ptr[-4]-192)<<24)+
               (((unsigned)ptr[-3])<<16)+
               (((unsigned)ptr[-2])<<8)+
               (unsigned)ptr[-1];
     }
  }
}

//***********************************************************************

inline unsigned long LoadSInt32(TInputPtr * &ptr,char *isneg)
   // Loads a compressed signed integer
{
   if(*ptr<128)
   {
      *isneg=((*ptr & 64) ? 1 : 0);
      return (unsigned long)(*(ptr++)&63);
   }
   else
   {
      *isneg=((*ptr & 32) ? 1 : 0);

      if(*ptr<192)
      {
         ptr+=2;
         return ((unsigned long)(ptr[-2]&31)<<8)+(unsigned long)ptr[-1];
      }
      else
      {
         ptr+=4;
         return ((unsigned long)(ptr[-4]&31)<<24)+((unsigned long)ptr[-3]<<16)+
                ((unsigned long)ptr[-2]<<8)+(unsigned long)ptr[-1];
      }
   }
}

//***********************************************************************

inline unsigned char *LoadData(unsigned char * &ptr,unsigned long len)
   // Returns data block of length 'len'
{
   ptr+=len;
   return ptr-len;
}

//***********************************************************************

inline void LoadDataBlock(TInputPtr * &ptr,unsigned char *dest,unsigned long len)
   // Copies a data block of length 'len' into '*dest'
{
   while(len--)
   {
      *dest=*ptr;
      ptr++;
      dest++;
   }
}

#endif
