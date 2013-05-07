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

//***********************************************************************
//***********************************************************************

// This module contains the handling of options specified at the command line

#include <stdio.h>
#include <stdlib.h>

#include "Types.hpp"
#include "Input.hpp"
#include "VPathExprMan.hpp"


#include "XMLOutput.hpp"


// Determines whether CR/LF (dos) or just LF (unix) should be used
// if the XML is printed using formatting
#ifdef WIN32
char usedosnewline=1;
#else
char usedosnewline=0;
#endif

// The following flags determine how white spaces should be stored
char globalfullwhitespacescompress     =WHITESPACE_IGNORE;

extern VPathExprMan pathexprman;   // The path manager

// The memory limit for the compressor
// For the decompressor, it contains a size of the buffer needed to decompress
// the header
unsigned long memory_cutoff=8L*1024L*1024L;

// Determines whether to keep the input file
char  delete_inputfiles=0;

char overwrite_files=0; // Is 1, if the user wants to overwrite all files
char skip_all_files=0;  // Is 1, if the user want to skip all remaining files

//********** Flags for compression ************

// Describe the handling of left, right, and attribute white spaces
char globalleftwhitespacescompress     =WHITESPACE_IGNORE;
char globalrightwhitespacescompress    =WHITESPACE_IGNORE;
char globalattribwhitespacescompress   =WHITESPACE_IGNORE;

// Flags for ignoring comment, CDATA, DOCTYPE, and PI sections
char ignore_comment=0;
char ignore_cdata=0;
char ignore_doctype=0;
char ignore_pi=0;

// The compression ratio index for the zlib library
unsigned char zlib_compressidx=6;




extern XMLOutput output;


// *********** Common flags
char no_output=0;          // No output
char usestdout=0;          // Use the standard output
char verbose=0;            // Verbose mode
char output_initialized=0; // output has been initalized


#ifdef TIMING
char timing=0;       // Do timing
#endif

//**********************************************************
//**********************************************************

// Several auxiliary function for managing 

// We keep a global state while traversing the sequence of options
char  **argstrings;        // The set of option strings
int   argnum,              // The number of option stringd
      curargidx;           // The index of the current option string
char  *curargptr;          // The current pointer into the current option string

char  *optfiledata;        // If we loaded a file, then this pointer refers to the file data
char  *curfileptr;         // The current pointer in the file

inline void InitArguments(char **argv,int argc)
   // Initializes the option reader state above
{
   argstrings=argv;
   argnum=argc;

   curargidx=0;
   curargptr=argstrings[curargidx];

   optfiledata=NULL;
   curfileptr=NULL;
}

inline char *GetNextArgument(int *len)
   // Reads the next argument using the option reader state
   // and stores the (maximal) length (to the next white space) in *len.
   // The actual length of an option can be smaller
{
   do
   {
      // We don't have a file loaded
      if(curfileptr==NULL)
      {
         // Let's skip white spaces
         while((*curargptr==' ')||(*curargptr=='\t')||(*curargptr=='\n')||(*curargptr=='\r'))
            curargptr++;

         // If we have some actual option strings left, then scan up to the
         // next white space
         if(*curargptr!=0)
         {
            char *endptr=curargptr+1;
            while((*endptr!=0)&&
                  (*endptr!=' ')&&(*endptr!='\t')&&
                  (*endptr!='\n')&&(*endptr!='\r'))
               endptr++;

            *len=(endptr-curargptr);
            return curargptr;
         }

         // We didn't find an option string ==> Go to next string
         curargidx++;
         if(curargidx==argnum)   // No more string? => Exit
            return NULL;

         curargptr=argstrings[curargidx];
      }
      else  // If we have a file, then we do something very similar
      {
         // Let's skip white spaces
         while((*curfileptr==' ')||(*curfileptr=='\t')||(*curfileptr=='\n')||(*curfileptr=='\r'))
            curfileptr++;

         if(*curfileptr!=0)
            return curfileptr;

         // Let's get rid of the file data
//         delete[] optfiledata;
         // We keep the file, since in VPathExprMan, the pathstrings
         // are direct pointers into the memory of the file data
         // This is a bad solution!

         curfileptr=NULL;
         curfileptr=NULL;
      }
   }
   while(1);
}

inline void SkipArgumentString(int len)
   // After parsing an option, this function is called to go to string
   // data after the option
{
   if(curfileptr!=NULL)
      curfileptr+=len;
   else
      curargptr+=len;
}

//*********************************************************************

inline void ParseOptionFile(char *filename)
   // Loads the option file
{
   Input input;
   char  *ptr;
   int   len;

   // We exit, if there is already an option file
   if(optfiledata!=NULL)
   {
      Error("Only one option file allowed!");
      Exit();
   }

   if(input.OpenFile(filename)==0)
   {
      Error("Could not open parameter file '");
      ErrorCont(filename);
      ErrorCont("'!");
      Exit();
   }

   len=input.GetCurBlockPtr(&ptr);

   if(len>30000)  // Just to make sure that the file fits into one
                  // single buffer -- i.e. we don't read another block
   {
      Error("Input file '");
      ErrorCont(filename);
      ErrorCont("' is too large!");
      Exit();
   }

   // Let's allocate and copy the data
   optfiledata=new char[len+1];
   memcpy(optfiledata,ptr,len);
   optfiledata[len]=0;

   curfileptr=optfiledata;

   input.CloseFile();
}

//*********************************************************************

void InterpretOptionString(char *option)
   // Interprets a specific option
{
   int   len;

   switch(*option)
   {
      // Includes an option file
   case 'i':SkipArgumentString(1);
            option=GetNextArgument(&len);
            if(option[len]!=0)   // white space in file name?
            {
               Error("Invalid filename for option '-f'");
               Exit();
            }
            SkipArgumentString(len);
            ParseOptionFile(option);
            return;

   case 'v':   verbose=1;SkipArgumentString(1);return;
   case 't':   no_output=1;SkipArgumentString(1);return;
   case 'c':   usestdout=1;SkipArgumentString(1);return;   
//   case 'k':   delete_inputfiles=0;SkipArgumentString(1);return;
   case 'd':   delete_inputfiles=1;SkipArgumentString(1);return;
   case 'f':   overwrite_files=1;SkipArgumentString(1);return;
#ifdef TIMING
   case 'T':   timing=1;SkipArgumentString(1);return; 
#endif

#ifdef XMILL
      // Sets the memory window size
   case 'm':SkipArgumentString(1);
            option=GetNextArgument(&len);
            SkipArgumentString(len);
            memory_cutoff=atoi(option);
            if(memory_cutoff<1)
            {
               Error("Option '-m' must be followed be a number >=1");
               Exit();
            }
            memory_cutoff*=1024L*1024L;
            return;

      // Reads a path expression
   case 'p':   SkipArgumentString(1);
               option=GetNextArgument(&len);
               {
               char *ptr=option;
               pathexprman.AddNewVPathExpr(ptr,option+strlen(option));
                  // 'ptr' is moved to the characters after the path expression
               SkipArgumentString(ptr-option);
               }

               return;

      // Sets the left white space handling
   case 'l':   option++;
               switch(*option)
               {
               case 'i':   globalleftwhitespacescompress=WHITESPACE_IGNORE;SkipArgumentString(2);return;
               case 'g':   globalleftwhitespacescompress=WHITESPACE_STOREGLOBAL;SkipArgumentString(2);return;
               case 't':   globalleftwhitespacescompress=WHITESPACE_STORETEXT;SkipArgumentString(2);return;
               }
               break;

      // Sets the right white space handling
   case 'r':   option++;
               switch(*option)
               {
               case 'i':   globalrightwhitespacescompress=WHITESPACE_IGNORE;SkipArgumentString(2);return;
               case 'g':   globalrightwhitespacescompress=WHITESPACE_STOREGLOBAL;SkipArgumentString(2);return;
               case 't':   globalrightwhitespacescompress=WHITESPACE_STORETEXT;SkipArgumentString(2);return;
               }
               break;

      // Sets the full white space handling
   case 'w':   option++;
               switch(*option)
               {
               case 'i':   globalfullwhitespacescompress=WHITESPACE_IGNORE;SkipArgumentString(2);return;
               case 'g':   globalfullwhitespacescompress=WHITESPACE_STOREGLOBAL;SkipArgumentString(2);return;
               case 't':   globalfullwhitespacescompress=WHITESPACE_STORETEXT;SkipArgumentString(2);return;
               default:    
                           globalleftwhitespacescompress=WHITESPACE_STOREGLOBAL;
                           globalrightwhitespacescompress=WHITESPACE_STOREGLOBAL;
                           globalfullwhitespacescompress=WHITESPACE_STOREGLOBAL;
                           globalattribwhitespacescompress=WHITESPACE_STOREGLOBAL;
                           SkipArgumentString(1);return;
               }
               break;

      // Sets the attribute white space handling
   case 'a':   option++;
               switch(*option)
               {
               case 'i':   globalattribwhitespacescompress=WHITESPACE_IGNORE;SkipArgumentString(2);return;
               case 'g':   globalattribwhitespacescompress=WHITESPACE_STOREGLOBAL;SkipArgumentString(2);return;
               }
               break;

      // Sets the handling of special sections (comment, CDATA, DOCTYPE, PI)
   case 'n':   option++;
               switch(*option)
               {
               case 'c':   ignore_comment=1;SkipArgumentString(2);return;
               case 't':   ignore_doctype=1;SkipArgumentString(2);return;
               case 'p':   ignore_pi=1;SkipArgumentString(2);return;
               case 'd':   ignore_cdata=1;;SkipArgumentString(2);return;
               }
               break;
#endif
#ifdef XDEMILL
      // All options for output formatting
   case 'o':   option++;
               switch(*option)
               {
               case 's':   // Use space indentation
               {
                  SkipArgumentString(2);
                  option=GetNextArgument(&len);
                  SkipArgumentString(len);

                  int spccount=atoi(option);
                  if(spccount<=0)
                  {
                     Error("Option '-os' must be followed be a positive integer");
                     Exit();
                  }
                  output.Init(XMLINTENT_SPACES,0,spccount);
                  output_initialized=1;
                  return;
               }
                  // Use tab indentation
               case 't':   SkipArgumentString(2);
                           output.Init(XMLINTENT_TABS);
                           output_initialized=1;
                           return;

                  // Use no indentation
               case 'n':   SkipArgumentString(2);
                           output.Init(XMLINTENT_NONE);
                           output_initialized=1;
                           return;

                  // Determines type of newline (DOS or UNIX)
               case 'd':   usedosnewline=1;
                           SkipArgumentString(2);
                           return;
               case 'u':   usedosnewline=0;
                           SkipArgumentString(2);
                           return;
/*
               case 'w':
               {
                  int wrapcount;
                  option+=2;
                  wrapcount=atoi(option);
                  if(wrapcount<=0)
                  {
                     Error("Option '-ow' must be followed be a positive integer");
                     Exit();
                  }
                  while((*option>='0')&&(*option<='9'))
                     option++;
                  output.Init(XMLINTENT_WRAP,0,wrapcount);
                  output_initialized=1;
                  return;
               }
*/
               }
#endif // XDEMILL
   }
#ifdef XMILL
   // Determines the compression rate index for zlib
   if((*option>='1')&&(*option<='9'))
   {
      zlib_compressidx=(unsigned char)(*option-'0');
      SkipArgumentString(1);
      return;
   }
#endif

   Error("Invalid option '-");
   ErrorCont(option);
   ErrorCont("'");
   Exit();
}

int HandleAllOptions(char **argv,int argc)
   // Reads all the options from 'argv'.
   // It returns the index of the first non-option string
   // i.e. the string not starting with '-'
{
   char  *option;
   int   len;

   InitArguments(argv,argc);

   while((option=GetNextArgument(&len))!=NULL)
   {
      if(*option!='-')
         break;

      SkipArgumentString(1);
      option++;

      InterpretOptionString(option);
   }

   return curargidx;
}

//********************************************************************
//********************************************************************

void PrintUsage(char showmoreoptions)
{
   printf("XMill 0.7 (30 Nov 99) - a compressor for XML\n");
   printf("Copyright (C) 1999 AT&T Labs Research\n");

#ifdef XMILL

   if(showmoreoptions==0)
      printf("\nUsage:\n\n  xmill [-i file] [-v] [-p path] [-m num] [-1..9] [-c] [-d] [-r] [-w] [-h] file ...\n\n");
   else
   {
      printf("\nUsage:\n\n  xmill [-i file] [-v] [-p path] [-m num] [-1..9] [-c] [-d] [-r] [-w] [-h]\n");
      printf("        [-w(i|g|t)] [-l(i|g|t)] [-r(i|g|t)] [-a(i|g)] [-n(c|t|p|d)]  file ...\n\n");
   }

   printf(" -i file  - include options from file\n");
   printf(" -v       - verbose mode\n");
   printf(" -p path  - define path expression\n");
   printf(" -m num   - set memory limit\n");
   printf(" -1..9    - set the compression factor of zlib (default=6)\n");
//   printf(" -t       - test mode (no output)\n");
   printf(" -c       - write on standard output\n");
//   printf(" -k       - keep original files unchanged (default)\n");
   printf(" -d       - delete input files\n");
   printf(" -f       - force overwrite of output files\n");
   printf(" -w       - preserve white spaces\n");
   printf(" -h       - show extended white space options and user compressors\n");

   if(showmoreoptions)
   {
      printf("\n  Extended options:\n\n");
      printf("    -wi      - ignore complete white spaces (default)\n");
      printf("    -wg      - store complete white spaces in global container\n");
      printf("    -wt      - store complete white spaces as normal text\n");
      printf("    -li      - ignore left white spaces (default)\n");
      printf("    -lg      - store left white spaces in global container\n");
      printf("    -lt      - store left white spaces as normal text\n");
      printf("    -ri      - ignore right white spaces (default)\n");
      printf("    -rg      - store right white spaces in global container\n");
      printf("    -rt      - store right white spaces as normal text\n");
      printf("    -ai      - ignore attribute white spaces (default)\n");
      printf("    -ag      - store attribute white spaces in global container\n");
      printf("\n");
      printf("    -nc      - ignore comments\n");
      printf("    -nt      - ignore DOCTYPE sections\n");
      printf("    -np      - ignore PI sections\n");
      printf("    -nd      - ignore CDATA sections\n");
      printf("\n");
      printf("\n  User compressors:\n\n");
      compressman.PrintCompressorInfo();
   }
#endif

#ifdef XDEMILL
   printf("Usage:\n\n\t xdemill [-i file] [-v] [-c] [-d] [-r] [-os num] [-ot] [-oz] [-od] [-ou] file ...\n\n");
   printf(" -i file  - include options from file\n");
   printf(" -v       - verbose mode\n");
   printf(" -c       - write on standard output\n");
//   printf(" -k       - keep original files unchanged\n");
   printf(" -d       - delete input files\n");
   printf(" -f       - force overwrite of output files\n");
//   printf(" -t       - test mode (no output)\n");
   printf(" -os num  - output formatted XML with space intentation\n");
   printf(" -ot      - output formatted XML with tabular intentation\n");
   printf(" -oz      - output unformatted XML (without white spaces)\n");
#ifdef WIN32
   printf(" -od      - uses DOS newline convention (default)\n");
   printf(" -ou      - uses UNIX newline convention\n");
#else
   printf(" -od      - uses DOS newline convention\n");
   printf(" -ou      - uses UNIX newline convention (default)\n");
#endif
//   printf(" -ow num     - wrap XML output after specified number of characters\n");
#endif
}
