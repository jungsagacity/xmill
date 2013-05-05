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

      11/30/99  - initial release by Hartmut Liefke, liefke@seas.upenn.edu
                                     Dan Suciu,      suciu@research.att.com
*/

//**************************************************************************
//**************************************************************************

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>

#include <ctype.h>

#ifdef WIN32
#include <conio.h>
#else
#endif

#include "CompressMan.hpp"
#include "VPathExprMan.hpp"
#include "Input.hpp"
#include "CurPath.hpp"


#ifdef XMILL
#include "Output.hpp"
#include "XMLParse.hpp"
#include "SAXClient.hpp"
#include "Compress.hpp"
#include "ContMan.hpp"
#endif

#ifdef XDEMILL
#include "LabelDict.hpp"
#include "XMLOutput.hpp"
#include "SmallUncompress.hpp"
#include "UnCompCont.hpp"
#endif

#define MAGIC_KEY 0x5e3d29e
   // The uncompressed first block of an XMill file 
   // must start with these bytes

CurPath           curpath;          // The current path in the XML document
LabelDict         globallabeldict;  // The label dictionary

VPathExprMan         pathexprman;   // The path manager

#ifdef XMILL
PathTree             pathtree;      // The path tree

CompressContainerMan    compresscontman;  // The user compressor manager


// We keep the pointers to the special containers

CompressContainerBlock  *globalcontblock;
   // The first container block in the system contains
   // those three containers

CompressContainer       *globalwhitespacecont;  // The special container for white spaces
CompressContainer       *globalspecialcont;     // The special container for special sequences (DTDs, PIs...)
CompressContainer       *globaltreecont;        // The structure container

// We keep the accumulated (un)compressed size of the
// header while compressing the blocks
extern unsigned long fileheadersize_orig;
extern unsigned long fileheadersize_compressed;
#endif

//**********************************

#ifdef XDEMILL

XMLOutput output;
   // The output file

extern UncompressContainerMan  uncomprcont;
   // The container manager for the decompressor

// To read the structural information at the beginning
// of each file and each run in the file, a input buffer
// is kept that can change (increase) in size.
// The input buffer here can be larger than the input buffer
// in 'Input'. The buffer here can contain an entire block
// that is decompressed
unsigned char *memoryalloc_buf=NULL;
unsigned char *memoryalloc_curptr=NULL;
unsigned long memoryalloc_bufsize=0;

#endif

// Several flags (defined in Options.hpp)
extern char usestdout;
extern char no_output;
#ifdef TIMING
extern char timing;
#endif
extern char globalfullwhitespacescompress;
extern char verbose;
extern char output_initialized;
extern char delete_inputfiles;
extern unsigned long memory_cutoff;

//**********************************

// There are three separate memory spaces used:
MemStreamer tmpmem(5);        // The temporary memory space
MemStreamer mainmem(1000);    // The main memory space:
                              // Takes information about path expressions
MemStreamer blockmem(1000);   // The block memory space:
                              // This space is deleted after each run
                              // It contains all structural information from the
                              // containers, the path dictionary, etc.

// Several functions prototypes
#ifdef XMILL
void FSMInit();   // Initializes the FSM machinery
                  // It creates a '#' and '@#' label

void InitSpecialContainerSizeSum(); // Resets the accumulate size for special containers
void PrintSpecialContainerSizeSum();// Prints the accumulate size for special containers

void Compress(char *srcfile,char *destfile);
#endif

#ifdef XDEMILL
void Uncompress(char *sourcefile,char *destfile);
#endif

// Defined in Options.cpp
void PrintUsage(char showmoreoptions);
int HandleAllOptions(char **argv,int argc);

//************************************************************************
//************************************************************************

extern char overwrite_files;  // Is 1, if the user wants to overwrite all files
extern char skip_all_files;   // Is 1, if the user want to skip all remaining files

char AskOverwriteFile(char *file)
   // Asks the user whether to overwrite
   // Returns 1, if yes,
   // Returns 0, if no,
   // Returns -1, if quit
{
   int c;

   if((!FileExists(file))||(overwrite_files))
      // If the file doesn't exist or we automatically overwrite it,
      // then we can simply return true
      return 1;

   printf("Overwrite file %s ? (Y(es) | N(o) | A(ll) | Q(uit)) ",file);

   // We wait until the user presses 'y', 'n', 'q', or 'a'
   do
   {
#ifdef WIN32
      c=_getch();
      printf("\n");
#else
      c=getchar();
#endif
      switch(toupper(c))
      {
      case 'Y':   return 1;
      case 'N':   return 0;
      case 'Q':   skip_all_files=1;return 0;
      case 'A':   overwrite_files=1;return 1;
      }
   }
   while(1);
}

void HandleSingleFile(char *file)
   // Considers a single file 'file' and (de)compresses it
   // Most importantly, the name of the destination file is
   // determines by modifying/adding/removing extensions '.xml', '.xmi', '.xm'
{
   int len=strlen(file);
   char  *outfilename=file+len+5;
      // We use the space after the input file 
      // and leave a little bit of space for possible extension '.xmi' or '.xm'

   strcpy(outfilename,file);

   try{

#ifdef XMILL
   // For the compressor, we replace ending '.xml' with '.xmi'
   // Or, if there is no ending '.xml', we replace by '.xm'

   if((len>=4)&&(strcmp(file+len-4,".xml")==0))
      strcpy(outfilename+len-4,".xmi");
   else
      strcat(outfilename,".xm");

   Compress(file,usestdout ? NULL : outfilename);

#ifdef PROFILE
   if(verbose)
      globallabeldict.PrintProfile();
#endif

#endif

#ifdef XDEMILL
   // For decompression, we omit ending '.xm' or replace
   // ending '.xmi' with '.xml'
   if((len>=3)&&(strcmp(file+len-3,".xm")==0))
      // Do we have ending '.xm' ?
   {
      outfilename[len-3]=0;   // We eliminate the ending in the out file name
      Uncompress(file,usestdout ? NULL : outfilename);
   }
   else
   {
      // We replace '.xmi' by '.xml'
      if((len>=4)&&(strcmp(file+len-4,".xmi")==0))
      {
         strcpy(outfilename+len-4,".xml");
         Uncompress(file,usestdout ? NULL : outfilename);
      }
      else
      {
         // Otherwise, we assume the user specified the *uncompressed*
         // file and we try to either replace '.xml' by '.xmi'
         // or append '.xm'.

         if((len>=4)&&(strcmp(file+len-4,".xml")==0))
         {
            strcpy(file+len-4,".xmi");
            if(FileExists(file))
            {
               Uncompress(file,usestdout ? NULL : outfilename);
               return;
            }
            strcpy(file+len-4,".xml");
         }

         // Let's try to append '.xm'
         strcpy(file+len,".xm");

         if(FileExists(file)==0)
         {
            strcpy(file+len,"");
            Error("Could not find file '");
            ErrorCont(file);
            ErrorCont("' with extension '.xm'!");
            PrintErrorMsg();
            return;
         }
         Uncompress(file,usestdout ? NULL : outfilename);
         return;
      }
   }
#endif
   }
   catch(XMillException *)
      // An error occurred
   {
      Error("Error in file '");
      ErrorCont(file);
      ErrorCont("':");
      PrintErrorMsg();
   }
}

void HandleFileArg(char *filepattern)
   // Takes a file name argument from the command line
   // and forward the file names to 'HandleSingleFile'
   // In Windows, file patterns with '*' and '?' must be explicitly
   // resolved.
{
   char        fullpath[400];
#ifdef WIN32
   _finddata_t finddata;
   long        handle;
   char        *ptr;
   int         fullpathlen;

   // Let's check if we have any meta characters '*' or '?' ?
   // We don't have them, we go directly to 'HandleSingleFile'

   ptr=filepattern;
   while(*ptr!=0)
   {
      if((*ptr=='*')||(*ptr=='?'))
         break;
      ptr++;
   }

   if(*ptr==0) // We didn't find any metacharacter?
               // The file name gets directly forwarded to HandleSingleFile
   {
      strcpy(fullpath,filepattern);
      HandleSingleFile(fullpath);
      return;
   }
   // Otherwise, we apply functions '_findfirst' and '_findnext'

   // We scan from the back of the file name and look
   // for a separator
   ptr=filepattern+strlen(filepattern)-1;

   while(ptr>=filepattern)
   {
      if((*ptr=='\\')||(*ptr=='/'))
         break;
      ptr--;
   }

   if(ptr<filepattern)   // We didn't find a separator ?
   {
      // The file path is empty
      *fullpath=0;
      fullpathlen=0;
   }
   else
   {
      // We the path part from the file pattern including
      // the separator that we found
      memcpy(fullpath,filepattern,ptr-filepattern+1);
      fullpath[ptr-filepattern+1]=0;
      fullpathlen=ptr-filepattern+1;
   }

   // Let's now look for the file
   handle=_findfirst(filepattern,&finddata);
   if(handle==-1)
   {
      printf("Could not find %s!\n",filepattern);
      return;
   }

   do
   {
      // We concatenate the file name to the path
      strcpy(fullpath+fullpathlen,finddata.name);

      HandleSingleFile(fullpath);
      if(skip_all_files)
         break;

      if(_findnext(handle,&finddata)!=0)
         break;
   }
   while(1);

   _findclose(handle);
#else

   // In UNIX, the file name expansion is done by the shell
   // ==> We only need to look at the specific file
   strcpy(fullpath,filepattern);
   HandleSingleFile(fullpath);
#endif
}

//************************************************************************
//************************************************************************

#ifdef WIN32
int _cdecl main(int argc,char **argv)
#else
int main(int argc,char **argv)
#endif
{
   int fileidx;

   // We set the default file mode to 'binary'
#ifdef WIN32
   _fmode=_O_BINARY;
#endif

   if(argc==1) // No arguments for the program?
   {
      PrintUsage(0);
      return 0;
   }
   
#ifdef XMILL
   else
   {
      if((argc==2)&&(strcmp(argv[1],"-h")==0))
         // Is there is exactly on argument '-h' ?
      {
         PrintUsage(1);
         return 0;
      }
   }
#endif

   // Now we start the heavy work!

   try{

   globallabeldict.Init(); // Initialized the label dictionary

#ifdef XMILL
   // Initializes the FSM structures.
   // It creates two labels '#' and '@#'
   FSMInit();

#ifdef USE_FORWARD_DATAGUIDE
extern void InitForwardDataGuide();

   InitForwardDataGuide();
#endif

#endif

   // Parse options
   fileidx=HandleAllOptions(argv+1,argc-1)+1;

#ifdef XMILL
   // In the compressor, we append two default paths: '//#' and '/'
   // to take care of all paths
   char *pathptr="//#";
   pathexprman.AddNewVPathExpr(pathptr,pathptr+strlen(pathptr));
   pathptr="/";
   pathexprman.AddNewVPathExpr(pathptr,pathptr+strlen(pathptr));

   globallabeldict.FinishedPredefinedLabels();
      // We remember which labels are predefined (i.e. labels defined through FSMs)
      // All labels that are inserted later will be eliminated
      // between two parses of two input files.

   pathexprman.InitWhitespaceHandling();
      // If the default white space handling for the path expression
      // is the global setting, then we replace that reference
      // by the global default value.
      // This is done after all options are parsed,
      // since the global white space options could come *after*
      // the path expressions have been inserted.
#endif

   // Are there no arguments except options?
   if(fileidx>=argc)
   {
      if(usestdout)  // Did the user specify '-c' for using 'stdout'?
      {
#ifdef XMILL
         Compress(NULL,NULL);
#endif
#ifdef XDEMILL
         Uncompress(NULL,NULL);
#endif
         return 0;
      }
      else
      {
         Error("No input file specified! Specify '-c' to use stdin/stdout");
         Exit();
      }
   }

   }
   catch(XMillException *)
      // An error occurred
   {
      return -1;
   }

   // Let's look at all files
   do
   {
      HandleFileArg(argv[fileidx]);
      if(skip_all_files)
         break;
      fileidx++;
   }
   while(fileidx<argc);

   return 0;
}

//************************************************************************
//************************************************************************
//************************************************************************
//************************************************************************
//************************************************************************
//************************************************************************

#ifdef XMILL

inline void StoreFileHeader(Compressor *compressor)
{
   MemStreamer tmpoutputstream(1);

   tmpoutputstream.StoreSInt32(
      (globalfullwhitespacescompress==WHITESPACE_IGNORE) ? 1 : 0,
      MAGIC_KEY);

   pathexprman.Store(&tmpoutputstream);

   compressor->CompressMemStream(&tmpoutputstream);
}

inline void CompressBlockHeader(Compressor *compressor,unsigned long totaldatasize)
{
   MemStreamer memstream;

   memstream.StoreUInt32(totaldatasize);
// First, we put info about path expressions, containers, and labels into
// container 'tmpoutputstream'

   compresscontman.StoreMainInfo(&memstream);

   compressor->CompressMemStream(&memstream);

   // Let's store the new labels from the label dictionary
   globallabeldict.Store(compressor);

   compressman.CompressSmallGlobalData(compressor);

   compresscontman.CompressSmallContainers(compressor);
}

char fileheader_iswritten=0;

inline void CompressCurrentBlock(Output *output,unsigned long totaldatasize)
{
   {
      Compressor     compressor(output);
      unsigned long  headersize,headersize_compressed;

      if(fileheader_iswritten==0)
      {
         StoreFileHeader(&compressor);
         fileheader_iswritten=1;
      }
      CompressBlockHeader(&compressor,totaldatasize);
      compressor.FinishCompress(&headersize,&headersize_compressed);

      fileheadersize_orig        +=headersize;
      fileheadersize_compressed  +=headersize_compressed;
   }

   compressman.CompressLargeGlobalData(output);

   // Let's compress the actual containers
   compresscontman.CompressLargeContainers(output);
}

void Compress(char *srcfile,char *destfile)
{
   SAXClient      saxclient;
   XMLParse       xmlparse;
   Output         output;
#ifdef TIMING
   clock_t        c1,c2,c3;
#endif
   int            parsetime=0,compresstime=0;
   char           isend;
   unsigned long  totaldatasize;

   // We count the overal sizes of the compressed/uncompressed
   // structure, white space, and special (DTD...) containers
   // We initialize the counters here
   InitSpecialContainerSizeSum();

   fileheader_iswritten=0;

   if(AskOverwriteFile(destfile)==0)
      return;

   if(xmlparse.OpenFile(srcfile)==0)
   {
      Error("Could not find file '");
      ErrorCont(srcfile);
      ErrorCont("'!");
      PrintErrorMsg();
      return;
   }

   if(output.CreateFile((no_output==0) ? destfile : "")==0)
   {
      Error("Could not create output file '");
      ErrorCont(destfile);
      PrintErrorMsg();
      xmlparse.CloseFile();
      return;
   }

   mainmem.StartNewMemBlock();

#ifdef USE_FORWARD_DATAGUIDE
   pathdict.Init();
   pathtree.CreateRootNode();
#endif


   try{
      do
      {
#ifndef USE_FORWARD_DATAGUIDE
         pathdict.Init();
         pathtree.CreateRootNode();
#else
         pathdict.ResetContBlockPtrs();
#endif

         globalcontblock      =compresscontman.CreateNewContainerBlock(3,0,NULL,NULL);
         globaltreecont       =globalcontblock->GetContainer(0);
         globalwhitespacecont =globalcontblock->GetContainer(1);
         globalspecialcont    =globalcontblock->GetContainer(2);
#ifdef TIMING
         if(timing)
            c1=clock();
#endif

         isend=xmlparse.DoParsing(&saxclient);
         if(isend)
            isend=1;

#ifdef TIMING
         if(timing)
            c2=clock();
#endif

         compresscontman.FinishCompress();

         totaldatasize= compresscontman.GetDataSize()+
                        compressman.GetDataSize();

         CompressCurrentBlock(&output,totaldatasize);
#ifdef TIMING
         if(timing)
         {
            c3=clock();
            parsetime+=(c2-c1);
            compresstime+=(c3-c2);
         }
#endif
#ifdef PROFILE
         if(verbose)
            printf("Pathtree size: %lu\n",pathtreemem.GetSize());
#endif

         if(verbose)
         {

#ifdef PROFILE
            pathtree.PrintProfile();
            pathdict.PrintProfile();
#endif
         }

#ifndef USE_FORWARD_DATAGUIDE
         pathtree.ReleaseMemory();
#endif
         compresscontman.ReleaseMemory();
         blockmem.ReleaseMemory(1000);
/*
static int count=0;
         printf("%lu\n",count);
         count++;
*/
      }
      while(isend==0);
   }
   catch(XMillException *)
   {
      output.CloseAndDeleteFile();
      xmlparse.CloseFile();
      Exit();
   }
#ifdef TIMING
   if(timing)
      printf("%fs + %fs = %fs\n",(float)parsetime/(float)CLOCKS_PER_SEC,
                                 (float)compresstime/(float)CLOCKS_PER_SEC,
                                 (float)(parsetime+compresstime)/(float)CLOCKS_PER_SEC);
#endif
   if(verbose)
      PrintSpecialContainerSizeSum();

#ifdef PROFILE
   if(verbose)
      curpath.PrintProfile();
#endif

   xmlparse.CloseFile();
   output.CloseFile();

   globallabeldict.Reset();

   mainmem.RemoveLastMemBlock();

   if(delete_inputfiles)
      RemoveFile(srcfile);
}

#endif //XMILL

//********************************************************************************
//********************************************************************************
//********************************************************************************
//********************************************************************************
//********************************************************************************

#ifdef XDEMILL

void DecodeTreeBlock(UncompressContainer *treecont,UncompressContainer *whitespacecont,UncompressContainer *specialcont,XMLOutput *output);
//****************************************************************************
//****************************************************************************

void UncompressFileHeader(SmallBlockUncompressor *uncompressor)
{
   char iswhitespaceignore;
   
   if(uncompressor->LoadSInt32(&iswhitespaceignore)!=MAGIC_KEY)
   {
      Error("The file is not a compressed XMill file!");
      Exit();
   }

   if(iswhitespaceignore)
   {
      globalfullwhitespacescompress=WHITESPACE_IGNORE;
      if(output_initialized==0)
         output.Init(XMLINTENT_SPACES,0,1);
   }
   else
   {
      globalfullwhitespacescompress=WHITESPACE_STOREGLOBAL;
      if(output_initialized==0)
         output.Init(XMLINTENT_NONE,0,1);
   }
   pathexprman.Load(uncompressor);
}

static char fileheader_isread=0;

char UncompressBlockHeader(Input *input)
{
   SmallBlockUncompressor  uncompressor(input);

   if(fileheader_isread==0)
   {
      UncompressFileHeader(&uncompressor);
      fileheader_isread=1;
   }
   else
   {
      if(input->IsEndOfFile())
         return 1;
   }

   memory_cutoff=uncompressor.LoadUInt32();

   SetMemoryAllocationSize(memory_cutoff);

   uncomprcont.Load(&uncompressor);

   globallabeldict.Load(&uncompressor);

   compressman.UncompressSmallGlobalData(&uncompressor);

   uncomprcont.AllocateContMem();

   uncomprcont.UncompressSmallContainers(&uncompressor);

   return 0;
}

#undef CreateFile

void Uncompress(char *sourcefile,char *destfile)
   // The main compres function
{
   Input                input;
#ifdef TIMING
   clock_t              c1,c2,c3,ct1=0,ct2=0;
#endif

   UncompressContainer  *uncomprtreecont;
   UncompressContainer  *uncomprwhitespacecont;
   UncompressContainer  *uncomprspecialcont;

   if(AskOverwriteFile(destfile)==0)
      return;

   if(input.OpenFile(sourcefile)==0)
   {
      Error("Could not find file '");
      ErrorCont(sourcefile);
      PrintErrorMsg();
      return;
   }

   globallabeldict.Init();

   if(output.CreateFile((no_output==0) ? destfile : "")==0)
   {
      Error("Could not create output file '");
      ErrorCont(destfile);
      PrintErrorMsg();
      input.CloseFile();
      return;
   }
#ifdef TIMING
   c1=clock();
#endif

   unsigned long blockidx=0;

   mainmem.StartNewMemBlock();

   while(UncompressBlockHeader(&input)==0)
   {
      compressman.UncompressLargeGlobalData(&input);
      uncomprcont.UncompressLargeContainers(&input);

      uncomprcont.Init();

      uncomprtreecont      =uncomprcont.GetContBlock(0)->GetContainer(0);
      uncomprwhitespacecont=uncomprcont.GetContBlock(0)->GetContainer(1);
      uncomprspecialcont   =uncomprcont.GetContBlock(0)->GetContainer(2);
#ifdef TIMING
      c2=clock();
#endif

      DecodeTreeBlock(uncomprtreecont,uncomprwhitespacecont,uncomprspecialcont,&output);
#ifdef TIMING
      c3=clock();
#endif
/*
      if(verbose)
         printf("#%3lu  => Uncompress: %f sec   Decode: %f sec\n",
            blockidx,
            (float)(c2-c1)/(float)CLOCKS_PER_SEC,
            (float)(c3-c2)/(float)CLOCKS_PER_SEC);
*/
#ifdef TIMING
      ct1+=c2-c1;
      ct2+=c3-c2;
#endif

      uncomprcont.FinishUncompress();
      uncomprcont.ReleaseContMem();
      compressman.FinishUncompress();
      blockmem.ReleaseMemory(1000);
#ifdef TIMING
      c1=clock();
#endif
      blockidx++;
   }
#ifdef TIMING
   if(verbose)
      printf("%fs + %fs = %fs\n",(float)ct1/(float)CLOCKS_PER_SEC,
                                 (float)ct2/(float)CLOCKS_PER_SEC,
                                 (float)(ct1+ct2)/(float)CLOCKS_PER_SEC);
#endif
   input.CloseFile();
   output.CloseFile();

   if(delete_inputfiles)
      RemoveFile(sourcefile);

   globallabeldict.Reset();

   fileheader_isread=0;

   mainmem.RemoveLastMemBlock();
}

#endif // XDEMILL

//******************************************************************************
//******************************************************************************
//******************************************************************************

/*
#ifdef WIN32
void * _cdecl ::operator new(size_t size)
#else
void * operator new(size_t size)
#endif
{
   return malloc(size);
}
*/