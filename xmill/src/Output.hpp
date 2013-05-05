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

// This module contains the output file management


#ifndef OUTPUT_HPP
#define OUTPUT_HPP

#include <string.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#endif

#include "Types.hpp"
#include "Error.hpp"

extern char usedosnewline;

// For now, we do not have a static implementation
//#define SET_OUTPUT_STATIC

#ifdef SET_OUTPUT_STATIC
#define OUTPUT_STATIC static
#else
#define OUTPUT_STATIC
#endif

class Output
{
   OUTPUT_STATIC FILE  *output;        // The output file handler
   OUTPUT_STATIC char  *buf;           // the output buffer
   OUTPUT_STATIC char  *savefilename;  // the name of the output file
   OUTPUT_STATIC int   bufsize,curpos; // buffer size and current position
   OUTPUT_STATIC int   overallsize;    // the accumulated size of the output data

public:
   char OUTPUT_STATIC CreateFile(char *filename,int mybufsize=65536)
      // Creates the output file. If 'filename==NULL', then the standard
      // output is used.
   {
      if(filename==NULL)
      {
         savefilename=NULL;
         output=stdout;

         // We allocate the memory
         buf=(char *)malloc(mybufsize);
         if(buf==NULL)
            ExitNoMem();

#ifdef WIN32
         _setmode(_fileno(stdout),_O_BINARY);
#endif
      }
      else
      {
         // We allocate the output buffer memory and a little more for the filename
         buf=(char *)malloc(mybufsize+strlen(filename)+1);
         if(buf==NULL)
            ExitNoMem();

         savefilename=buf+mybufsize;
         strcpy(savefilename,filename);

         // We only open the output file, if strlen(filename)>0.
         // Otherwise, no output is performed at all.
         // (This is the test-case)
         if(strlen(filename)!=0)
         {
            output=fopen(filename,"wb");
            if(output==NULL)
               return 0;
         }
         else
            output=NULL;
      }
      bufsize=mybufsize;
      curpos=0;
      overallsize=0;
      return 1;
   }

   void OUTPUT_STATIC CloseFile()
      // Writes the remaining output to the file and closes the file
   {
      Flush();
      if(savefilename!=NULL)
      {
         if(output!=NULL)
            fclose(output);
      }
      free(buf);
   }

   void OUTPUT_STATIC CloseAndDeleteFile()
      // Writes the remaining data and closes the file and removes it
   {
      Flush();
      if(savefilename!=NULL)
      {
         if(output!=NULL)
            fclose(output);
         unlink(savefilename);
      }
      free(buf);
      return;
   }

   void OUTPUT_STATIC Flush()
      // Flushes the output file
   {
      overallsize+=curpos;

      if(output==NULL)
      {
         curpos=0;
         return;
      }

      char  *ptr=buf;
      int   byteswritten;

      // We write the output between '0' and 'curpos'
      // We only write at most 30000 bytes - to avoid some glitches
      // in the implementation of 'fwrite'.

      while(curpos>0)
      {
         byteswritten=fwrite(ptr,1,(curpos<30000) ? curpos : 30000,output);
         if(byteswritten==0)
         {
            Error("Could not write output file!");
            Exit();
         }
         curpos-=byteswritten;
         ptr+=byteswritten;
      }
   }

   char OUTPUT_STATIC *GetBufPtr(int *len)
      // Returns the current empty buffer space and its size
   {
      *len=bufsize-curpos;
      return buf+curpos;
   }

   void OUTPUT_STATIC SaveBytes(int bytecount)
      // After the user put some data into the empty buffer space,
      // it must be acknowledges with 'SaveBytes'
   {
      curpos+=bytecount;
   }

   int OUTPUT_STATIC GetCurFileSize() {  return overallsize+curpos; }
      // Returns the current file size

//********************************************

   void OUTPUT_STATIC StoreData(char *ptr,int len)
      // Stores the data at position 'ptr' of length 'len'
   {
      while(bufsize-curpos<len)
      {
         mymemcpy(buf+curpos,ptr,bufsize-curpos);
         len-=bufsize-curpos;
         ptr+=bufsize-curpos;
         curpos=bufsize;
         Flush();
      }
      mymemcpy(buf+curpos,ptr,len);
      curpos+=len;
   }

   void OUTPUT_STATIC StoreInt32(int val)
      // Stores a simple uncompressed integer
   {
      if(bufsize-curpos<4)
         Flush();

      *(int *)(buf+curpos)=val;
      curpos+=4;
   }

   void OUTPUT_STATIC StoreChar(char c)
      // Stores a character
   {
      if(bufsize-curpos<1)
         Flush();

      buf[curpos]=c;
      curpos++;
   }

   void OUTPUT_STATIC StoreNewline()
      // Stores a new line
   {
      if(usedosnewline)
      {
         if(bufsize-curpos<2)
            Flush();

         buf[curpos++]='\r';
      }
      else
      {
         if(bufsize-curpos<1)
            Flush();
      }
      buf[curpos++]='\n';
   }

   char OUTPUT_STATIC *GetDataPtr(int len)
      // Returns a buffer space big enough to contain 'len' bytes
      // The buffer pointer is automatically updated
   {
      if(bufsize-curpos<len)
      {
         Flush();
         if(bufsize-curpos<len)
         {
            Error("Fatal Error !");
            Exit();
         }
      }
      curpos+=len;
      return buf+curpos-len;
   }

//******************************************************************

};

inline char FileExists(char *filename)
{
#ifdef WIN32
   _finddata_t fileinfo;
   long        handle=_findfirst(filename,&fileinfo);
   if(handle==-1)
      return 0;
   _findclose(handle);
   return 1;
#else
   FILE *file=fopen(filename,"r");
   if(file==NULL)
      return 0;
   fclose(file);
   return 1;
#endif
}

inline void RemoveFile(char *filename)
{
   unlink(filename);
}
#endif
