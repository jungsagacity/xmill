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

// This module contains the main functions for reading a file

#ifndef FILE_HPP
#define FILE_HPP

#include <stdio.h>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "Error.hpp"
extern int errno;

class CFile
{
   FILE  *file;         // The file handle
   char  *savefilename; // We save the file name

protected:
   unsigned filepos;    // Current file position
   char     iseof;      // Did we reach the end of the file?


public:

   CFile()
   {
      file=NULL;
   }
  
   char OpenFile(char *filename)
      // Opens a file (if filename==NULL, then the standard input is opened)
      // Returns 1, if okay, otherwise 0
   {
      if(filename==NULL)
      {
         file=stdin;
#ifdef WIN32
         _setmode(_fileno(stdin),_O_BINARY);
#endif
      }
      else
      {
         if(*filename==0)  // Empty file name ?
            return 0;

         file=fopen(filename,"rb");

         if(file==NULL) // We couldn't open the file?
            return 0;
      }
      filepos=0;
      iseof=0;

      savefilename=filename;
      return 1;
   }

   unsigned GetFilePos()  { return filepos;}
      // Returns the current position in the file

   unsigned ReadBlock(char *dest,unsigned bytecount)
      // Reads a data block into the memory at 'dest'. The maximum size is 'bytecount'
      // The function returns the number of bytes read or -1, if something fails
      // If the result is smaller than bytecount, the end of the file has been reached
      // and flag eof is set to 1.
   {
      if(iseof)
         return 0;

      // let's try to reach 'bytecount' bytes
      unsigned bytesread=(unsigned)fread(dest,1,bytecount,file);

      if(bytesread<bytecount)
         // We didn't read all of them?
      {
         if(feof(file))
            iseof=1;
         else
         {
            if(ferror(file))
            {
               char tmpstr[300];
               if(savefilename!=NULL)
                  sprintf(tmpstr,"Could not read from file '%s' (Error %lu)!",savefilename,errno);
               else
                  sprintf(tmpstr,"Could not read from standard input (Error %lu)!",errno);
               Error(tmpstr);
               Exit();
            }
         }
      }
      filepos+=bytesread;
      return bytesread;
   }

   void CloseFile()
      // Closes the file
   {
      if((file==NULL)||(file==stdout))
         return;

      fclose(file);
      file=NULL;
   }
};

#endif
