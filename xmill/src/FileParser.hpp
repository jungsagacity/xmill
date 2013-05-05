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

// This module contains a few Input file parsing routines used for
// parsing XML files. Most importantly, it keeps track of the line number

#include "Input.hpp"

class FileParser : public Input
{
   unsigned curlineno;  // The current line number

public:

   char OpenFile(char *filename)
      // Opens the file and fills the buffer
   {
      curlineno=1;

      return Input::OpenFile(filename);
   }

   unsigned GetCurLineNo() {  return curlineno; }

   char ReadStringUntil(char **destptr,int *destlen,char stopatwspace,char c1,char c2)
   // This function scans ahead in the buffer until some character c1 or c2 occurs
   // If 'stopwspace=1', then we also stop at the first white space detected.
   // In this case, *destptr and *destlen will contain the pointer and length
   // of the buffer *with* the character c1 or c2 and the functions returns 1.
   // If a white space has been detected, then the function also returns 1,
   // but the white-space is *not* included in the length.
   // If the function couldn't find such a character in the current buffer, we try to refill
   // If it is still not found, then the function returns the current buffer in *destptr and *destlen
   // and returns 0.
   {
      char *curptr,*ptr;
      int  len,i;

      // Let's get as much as possible from the input buffer
      len=GetCurBlockPtr(&ptr);
      curptr=ptr;

      i=len;
      // We search for characters 'c1', 'c2', ' ', '\t' ...
      // If we find such a character, then we store the pointer in 'destptr'
      // and the length in 'destlen' and exit.
      while(i!=0)
      {
         if(*curptr=='\n')
            curlineno++;

         if((*curptr==c1)||(*curptr==c2))
         {
            curptr++;
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         if((stopatwspace)&&((*curptr==' ')||(*curptr=='\t')||(*curptr=='\r')||(*curptr=='\n')))
         {
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         curptr++;
         i--;
      }

      // We couldn't find characters --> Try to refill

      RefillAndGetCurBlockPtr(&ptr,&len);

      curptr=ptr;

      i=len;

      // Now we try the same thing again:

      // We search for characters 'c1', 'c2', ' ', '\t' ...
      // If we find such a character, then we store the pointer in 'destptr'
      // and the length in 'destlen' and exit.
      while(i!=0)
      {
         if(*curptr=='\n')
            curlineno++;

         if((*curptr==c1)||(*curptr==c2))
         {
            curptr++;
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         if((stopatwspace)&&((*curptr==' ')||(*curptr=='\t')||(*curptr=='\r')||(*curptr=='\n')))
         {
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         curptr++;
         i--;
      }

      *destptr=ptr;
      *destlen=len;
      FastSkipData(len);
      return 0;
   }

   char ReadStringUntil(char **destptr,int *destlen,char c1)
      // This function scans ahead in the buffer until some character c1 occurs
      // In this case, *destptr and *destlen will contain the pointer and length
      // of the buffer *with* the character and the functions returns 0.

      // If the function couldn't find such a character in the current buffer, we try to refill
      // If it is still not found, then the function returns the current buffer in *destptr and *destlen
      // and returns 0.
   {
      char *curptr,*ptr;
      int  len,i;

      len=GetCurBlockPtr(&ptr);
      curptr=ptr;

      i=len;

      // We search for character 'c1'.
      // If we find such a character, then we store the pointer in 'destptr'
      // and the length in 'destlen' and exit.
      while(i!=0)
      {
         if(*curptr==c1)
         {
            curptr++;
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         if(*curptr=='\n')
            curlineno++;
         curptr++;
         i--;
      }

      // We couldn't find characters --> Try to refill

      RefillAndGetCurBlockPtr(&ptr,&len);
      curptr=ptr;

      i=len;

      // Now we try the same thing again:

      // We search for character 'c1'.
      // If we find such a character, then we store the pointer in 'destptr'
      // and the length in 'destlen' and exit.
      while(i!=0)
      {
         if(*curptr==c1)
         {
            curptr++;
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         if(*curptr=='\n')
            curlineno++;
         curptr++;
         i--;
      }

      *destptr=ptr;
      *destlen=len;
      FastSkipData(len);
      return 0;
   }

   char ReadWhiteSpaces(char **destptr,int *destlen)
      // This function scans ahead in the buffer over all white-spaces
      // *destptr and *destlen will contain the pointer and length
      // of the buffer with the white-spaces

      // If the buffer got fill without reaching a non-whitespace character,
      // then the function returns the current buffer in *destptr and *destlen
      // and returns 1. Otherwise, 
   {
      char *curptr,*ptr;
      int  len,i;

      len=GetCurBlockPtr(&ptr);
      curptr=ptr;

      i=len;

      // We search for non-white-space characters
      // If we find such a character, then we store the pointer in 'destptr'
      // and the length in 'destlen' and exit.
      while(i!=0)
      {
         if((*curptr!=' ')&&(*curptr!='\t')&&(*curptr!='\r')&&(*curptr!='\n'))
            // No white space?
         {
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         if(*curptr=='\n')
            curlineno++;
         curptr++;
         i--;
      }

      // We couldn't find characters --> Try to refill

      RefillAndGetCurBlockPtr(&ptr,&len);
      curptr=ptr;

      i=len;

      // Now we try the same thing again:

      // We search for non-white-space characters
      // If we find such a character, then we store the pointer in 'destptr'
      // and the length in 'destlen' and exit.
      while(i!=0)
      {
         if((*curptr!=' ')&&(*curptr!='\t')&&(*curptr!='\r')&&(*curptr!='\n'))
            // No white space?
         {
            *destptr=ptr;
            *destlen=curptr-ptr;
            FastSkipData(*destlen);
            return 1;
         }
         if(*curptr=='\n')
            curlineno++;
         curptr++;
         i--;
      }

      // We look through the entire buffer and couldn't find a non-white-space
      // character?

      *destptr=ptr;
      *destlen=len;
      FastSkipData(len);
      return 0;
   }

   char ReadStringUntil(char **destptr,int *destlen,char *searchstr)
   // This function scans ahead in the buffer until string 'searchstr'
   // is reached. In this case, *destptr and *destlen will contain the pointer and length
   // of the buffer *with* the search string at the end and the functions returns 0.
   // If the function couldn't find such a character in the current buffer, we try to refill
   // If it is still not found, then the function returns the current buffer in *destptr and *destlen
   // and returns 1.
   {
      char  *ptr;
      int   len,stringlen;
      char  refilled=0;
      int   curoffset=0,i;
      char  *curptr;

      len=GetCurBlockPtr(&ptr);
      stringlen=strlen(searchstr);

      do
      {
         // We try to find the first character
         curptr=ptr+curoffset;
         i=len-curoffset;
         while(i!=0)
         {
            if(*curptr==searchstr[0])
               break;
            if(*curptr=='\n')
               curlineno++;
            curptr++;
            i--;
         }
         if(i==0)
            // We couldn't find characters --> Try to refill
         {
            if(!refilled)
            {
               curoffset=len; // We can skip the part that we already scanned
               refilled=1;
               RefillAndGetCurBlockPtr(&ptr,&len);
               continue;
            }
            else
               // We couldn't find the first character in the current block --> We return current block
            {
               *destptr=ptr;
               *destlen=len;
               FastSkipData(len);
               return 0;
            }
         }

         // We found the first character at *curptr

         curoffset=curptr-ptr;

         if(curoffset+stringlen>len)
            // There is not enough room for the search string?
         {
            if(!refilled) // If we didn't try refill yet, we try it now
            {
               refilled=1;
               RefillAndGetCurBlockPtr(&ptr,&len);
            }
            if(curoffset+stringlen>len) // Still not enough space?
            {
               *destptr=ptr;
               *destlen=curoffset; // We take everything up to (but excluding) the first character at *curptr
               FastSkipData(curoffset);
               return 0;
            }
         }
         // Now we check whether ptr+offset is equal to the searchstring

         if(memcmp(ptr+curoffset,searchstr,stringlen)==0)
         // We found it !
         {
            *destptr=ptr;
            *destlen=curoffset+stringlen;
            FastSkipData(*destlen);
            return 1;
         }
         // We didn't find it ==> We go to next character after ptr+curoffset
         curoffset++;
      }
      while(1);
   }

   void SkipChar()
      // Skips the next character
   {
      char c;
      Input::GetChar(&c);
      if(c=='\n')
         curlineno++;
   }

   void GetChar(char *c)
      // Reads the next character
   {
      Input::GetChar(c);
      if(*c=='\n')
         curlineno++;
   }

   void SkipData(int len)
      // Skips 'len' characters
   {
      char *ptr;
      int  mylen;
      mylen=GetCurBlockPtr(&ptr);

      for(int i=0;i<len;i++)
      {
         if(*ptr=='\n')
            curlineno++;
         ptr++;
      }
      Input::SkipData(len);
   }
};
