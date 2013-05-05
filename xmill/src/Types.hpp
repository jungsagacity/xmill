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

// This file contains several global definitions of types and constants

#ifndef TYPES_HPP
#define TYPES_HPP

typedef unsigned short TLabelID;
#define LABEL_UNDEFINED ((TLabelID)65535)
#define ATTRIBLABEL_STARTIDX ((TLabelID)32768)

#define GET_LABELID(l)  ((l)&32767)

typedef unsigned short TContID;
#define CONTID_UNDEFINED ((TContID)65535)

#define SMALLCONT_THRESHOLD   2000


#define LABELIDX_TOKENOFFS          5

#define TREETOKEN_ENDLABEL          0
#define TREETOKEN_EMPTYENDLABEL     1
#define TREETOKEN_WHITESPACE        2
#define TREETOKEN_ATTRIBWHITESPACE  3
#define TREETOKEN_SPECIAL           4

#define WordAlignUInt(v)    ((((unsigned)(v)+3)|3)-1)

// We define three types of white-space handling. Each of the
// white-space handlings can be applied to full white space string,
// left white space string and right white space strings.

#define WHITESPACE_IGNORE        0  // Ignores the white space sequence
#define WHITESPACE_STORETEXT     1  // Considers the white space sequence as normal text
#define WHITESPACE_STOREGLOBAL   2  // Stores the white space sequence in a special container
#define WHITESPACE_DEFAULT       3  // Use the gloval setting (one of the three above values)
                                    // as the default for path expressions

#include <string.h>
/*
inline int mymemcmp(const char *buf1,const char *buf2,int len)
{
   if(len>20)
      return memcmp(buf1,buf2,len);
   else
   {
      while(len--)
      {
         if(*buf1!=*buf2)
            return 1;
         buf1++;buf2++;
      }
      return 0;
   }
}

inline void mymemcpy(char *dest,const char *src, int count)
{
   if(count>20)
      memcpy(dest,src,count);
   else
   {
      while(count--)
      {
         *dest=*src;
         dest++;src++;
      }
   }
}
*/
#define mymemcmp(b1,b2,len) memcmp(b1,b2,len)
#define mymemcpy(b1,b2,len) memcpy(b1,b2,len)

#define mymemset(b1,c,len)  memset(b1,c,len)
/*
inline void mymemset(char *dest,char c, int count)
{
   while(count--)
   {
      *dest=c;
      dest++;
   }
}
*/


#endif
