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

// This module contains the Enumeration-Compressor 'e'
// The enumeration-compressor (or dictionary compressor) assigns a
// positive integer to each possible string occured. In the compressor,
// a dictionary of all previously seen strings is maintained and a hashtable
// allows the efficient lookup. A new
// string is assigned the next free number.

// The decompressor loads the dictionary and directly accesses the
// dictionary for a given integer.

// Note that there is *only one* hashtable for all enum-compressors !!
// I.e. all new strings are represented in the same hash tableindex number space !

#include "CompressMan.hpp"
#include "UnCompCont.hpp"
#include "SmallUncompress.hpp"


extern MemStreamer blockmem;

#define SMALLCOMPRESS_THRESHOLD  1024

//unsigned long     enuminstancecount=0;

// The MemStreamer used for storing the hashtable entries in the compressor
// and the compressor states in the decompressor
#define enumcompressmem (&blockmem)

//*************************************************************

// The compressor is based on a hash table implementation



// The size of the hash table
#define ENUMHASHTABLE_SIZE  32768
#define ENUMHASHTABLE_MASK  32767
#define ENUMHASHTABLE_SHIFT 15

struct EnumCompressState;

struct EnumHashEntry
   // Stores a hash entry
{
   EnumHashEntry     *nextsamehash; // The next entry with the same hash index
   unsigned short    datalen;       // The length of the string
   char              *dataptr;      // The pointer to the string
                                    // Strings are stored in a separate MemStreamer!
   EnumCompressState *enumstate;    // The pointer to the state of the compressor
   unsigned          localidx;      // The index number for this string
                                    // This number is unique within a given compressor state

   void *operator new(size_t size)
   {
      return enumcompressmem->GetByteBlock(size);
   }

   char *GetStrPtr() {  return dataptr;   }
};

//**************************

class EnumHashTable
   // The hash table implementation
{
   static EnumHashEntry    *hashtable[ENUMHASHTABLE_SIZE];
   static char             isinitialized;

   static inline unsigned CalcHashIdx(char *str,int len)
      // Computes the hash index for a given string
   {
      unsigned idx=len;
      while(len--)
      {
         idx=(idx<<3)+(idx>>29)+(idx>>(idx&1));
         idx+=(unsigned char)*str;
         str++;
      }
      return (idx+(idx>>ENUMHASHTABLE_SHIFT))&ENUMHASHTABLE_MASK;
   }

public:

   EnumHashTable()
   {
      isinitialized=0;
   }

   static void Initialize()
      // The hash table is emptied
   {
      if(isinitialized==0)
      {
         for(int i=0;i<ENUMHASHTABLE_SIZE;i++)
            hashtable[i]=NULL;

         isinitialized=1;
      }
   }

   static void Reset()
      // This will cause the hash table to be emptied next time we try to
      // add elements
   {
      isinitialized=0;
   }

   static EnumHashEntry *FindOrCreateEntry(char *str,int len,EnumCompressState *enumstate,char *isnew,MemStreamer *strmem)
      // Finds or creates a new hash entry
      // enumstate is the state for the compressor, *isnew will be set to 1 if a new entry has been created
      // strmem is the MemStreamer used for allocating string memory space
   {
      // Let's determine the hash table index first
      unsigned       hashidx=CalcHashIdx(str,len);
      EnumHashEntry  **hashentryref=hashtable+hashidx;
      char           *ptr1,*ptr2;
//      int            i;

      // Let's firstly look through the existing hash index list
      while(*hashentryref!=NULL)
      {
         if(((*hashentryref)->datalen==len)&&
            ((*hashentryref)->enumstate==enumstate))
         {
            ptr1=str;
            ptr2=(*hashentryref)->GetStrPtr();

            // Let's check whether the strings are equal
            if(mymemcmp(ptr1,ptr2,len)==0)
               // Did we find an entry? ==> We are done
            {
               *isnew=0;
               return *hashentryref;
            }
         }
         hashentryref=&((*hashentryref)->nextsamehash);
      }

      // None of the hash-entries matches ?
      // ==> We create a new one

      *hashentryref=new EnumHashEntry();

      (*hashentryref)->nextsamehash =NULL;
      (*hashentryref)->enumstate    =enumstate;

      // The length of the string is stored in 'strmem'
      strmem->StoreUInt32(len);

      // Now we allocate the number of bytes in MemStreamer 'strmem'
      (*hashentryref)->datalen      =len;
      (*hashentryref)->dataptr      =strmem->GetByteBlock(len);

      // Hence, 'strmem' contains a sequence of tuples (len,str) that allows us in the
      // decompressor to reconstruct the dictionary
      memcpy((*hashentryref)->dataptr,str,len);

      // The item is new
      *isnew=1;

      return *hashentryref;
   }
};

EnumHashTable  enumhashtable;
EnumHashEntry  *EnumHashTable::hashtable[ENUMHASHTABLE_SIZE];
char           EnumHashTable::isinitialized;

//********************************************************************************
//********************************************************************************
//********************************************************************************

// The actual compressor implementation follows now

struct EnumCompressState
   // The state for each enum-compressor 
{
   unsigned long     curidx;              // The number of already assigned strings
                                          // This is also the next available index
   MemStreamer       stringmem;           // The memory streamer used for storing
                                          // the strings
   unsigned long     compressed_size;     // We keep track of the number of compressed bytes ...
   unsigned long     uncompressed_size;   // ... and uncompressed bytes
   EnumCompressState *next;               // All enumstates are kept in a global list
                                          // Points to the next enumstate in the global list
};

//********************************************************************************

void AddEnumCompressState(EnumCompressState *state);
   // This auxiliary function registeres a new compressor state 'state'
   // within the EnumCompressFactory

//********************************************************************************

class EnumerationCompressor : public UserCompressor
   // The actual compressor
{
public:
   EnumerationCompressor()
   {
      datasize=sizeof(EnumCompressState);
      contnum=1;
      isrejecting=0;
      canoverlap=1;
      isfixedlen=0;
   }

   void InitCompress(CompressContainer *cont,char *dataptr)
      // Initializes the compressor the specific enum state 'dataptr'
   {
      ((EnumCompressState *)dataptr)->curidx=0; // We start at index 0

      // Let's initializes the MemStreamer -
      // this is the same as calling the constructor
      ((EnumCompressState *)dataptr)->stringmem.Initialize(0);

      // The state is added to a list of states
      // This is necessary so that we can store the state information as soon
      // as the data is about to be compressed
      AddEnumCompressState((EnumCompressState *)dataptr);

      // We also need to initialize the hash table (if it hasn't been initialized before)
      EnumHashTable::Initialize();
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
      // Compresses a specific string item
   {
      char isnew;

      // Let's lookup the item in the hash table
      EnumHashEntry *entry=EnumHashTable::FindOrCreateEntry(
         str,len,(EnumCompressState *)dataptr,
         &isnew,&(((EnumCompressState *)dataptr)->stringmem));
      
      if(isnew)   // Is this a new entry? ==> We need to assign a new index
      {
         entry->localidx=((EnumCompressState *)dataptr)->curidx;
         ((EnumCompressState *)dataptr)->curidx++;
      }
      // If the item is not new, then EnumCompressState::curidx is the
      // index of the corresponding string

      // We store the index of the item in the container
      cont->StoreCompressedUInt(entry->localidx);
   }

   // Note that we don't implement the 'FinishCompress' method for UserCompressors.
   // The reason is that we do the actual storage in the EnumCompressFactory-class.

   void PrintCompressInfo(char *dataptr,unsigned long *overalluncomprsize,unsigned long *overallcomprsize)
      // Prints statistical information about how well the compressor compressed
      // the data
   {
      unsigned long  uncompsize=((EnumCompressState *)dataptr)->uncompressed_size,
                     compsize=((EnumCompressState *)dataptr)->compressed_size;

      *overalluncomprsize+=uncompsize;
      *overallcomprsize+=compsize;

      if(compsize!=0)
         printf("       Enum: %8lu ==> %8lu (%f%%)\n",
            uncompsize,compsize,
            100.0f*(float)compsize/(float)uncompsize);
      else
         printf("       Enum: %8lu ==> Small...\n",uncompsize);
   }
};



//**************************************************************************
//**************************************************************************
//**************************************************************************

// The Enum-Decompress

struct EnumDictItem
   // Represents an entry in the dictionary
   // The entries are stored in a sequence in memory
{
   unsigned long  len;
   unsigned char  *dataptr;
};



struct EnumUncompressState
   // The state of the decompressor
{
   unsigned long  itemnum;       // The number of items
   unsigned long  size;          // The size of the dictionary
   EnumDictItem   *itemarray;    // The array of dictionary items
   unsigned char  *strbuf;       // The pointer to the string buffer
};

//********************************************************************************


class EnumerationUncompressor : public UserUncompressor
{
   EnumUncompressState *GetNextPossibleEnumUnCompressState();
      // This auxiliary function returns the next state from the sequence of states
      // that was previously loaded from the compressed file
      // The sequence of states is stored in the EnumCompressFactory-object

public:

   EnumerationUncompressor()
   {
      datasize=sizeof(EnumUncompressState);
      contnum=1;
   }

   void InitUncompress(UncompressContainer *cont,char *dataptr)
      // Initializes the compressor by simply retrieving the next
      // state from the list of states
   {
      *(EnumUncompressState *)dataptr=*GetNextPossibleEnumUnCompressState();
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
      // An item is decompressed by looking up the dictionary
   {
      unsigned idx=cont->LoadUInt32();
      EnumDictItem *item=((EnumUncompressState *)dataptr)->itemarray+idx;

      output->characters((char *)item->dataptr,item->len);
   }
};




//**************************************************************************

class EnumerationCompressorFactory : public UserCompressorFactory
   // The actual enum compressor factory
{
   unsigned                   enuminstancecount;   // The number of enum compressor instantiations

   // **** Additional information for the compressor

   EnumerationCompressor      enumcompress;     // We need only one compressor instance
   
   EnumCompressState          *enumstatelist,   // The global list of enumstates
                              **lastenumstateref;


   // **** Additional information for the decompressor

   EnumerationUncompressor    enumuncompress;   // We need only one decompressor instance

   // We keep a temporary array of states for the decompressors
   // The factory fills the array by decompressing the status information
   // from the input file. The actual states of the decompressors are then
   // initialized with these states
   EnumUncompressState        *enumuncompressstates;        
   unsigned long              activeenumuncompressstates;   // The number of states in the array


public:
   EnumerationCompressorFactory()
   {
      enuminstancecount=0;


      enumstatelist=NULL;
      lastenumstateref=&enumstatelist;



      activeenumuncompressstates=0;

   }

   char *GetName()         {  return "e"; }
   char *GetDescription()  {  return "Compressor for small number of distinct data values"; }
   char IsRejecting()      {  return 0;   }
   char CanOverlap()       {  return 1;   }

   // ******* For the compressor **************


   void AddEnumCompressState(EnumCompressState *state)
      // Adds a new enumstate to the global list
   {
      *lastenumstateref=state;
      lastenumstateref=&(state->next);
      state->next=NULL;
      enuminstancecount++;
   }

   UserCompressor *InstantiateCompressor(char *paramstr,int len)
      // The instantiation simply return the one instance we have
   {
      if(paramstr!=NULL)
      {
         Error("Enumeration compressor 'e' should not have any arguments ('");
         ErrorCont(paramstr,len);
         Error("')");
         return NULL;
      }
      return &enumcompress;
   }


   // ******* For the decompressor **************


   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
      // The instantiation simply return the one instance we have
   {
      return &enumuncompress;
   }


   // The compression/decompression routines for the factory
   // CompressFactoriess are also allowed to store status information
   // in the compressed file ! The following procedure as used for
   // compressing/decompressing this information
   // Small data (<1024Bytes) is stored in the header, while
   // lare data is stored in separate zlib-blocks in the output file


   void CompressSmallGlobalData(Compressor *compressor)
      // Compresses the small data
   {
      MemStreamer       headmem;
      EnumCompressState *state=enumstatelist;

      // Let's store the number of enum compressor we have
      headmem.StoreUInt32(enuminstancecount);

      // For each state, we store the number of dictonary entries and
      // the size of the string memory
      while(state!=NULL)
      {
         headmem.StoreUInt32(state->curidx);
         headmem.StoreUInt32(state->stringmem.GetSize());

         state=state->next;
      }

      compressor->CompressMemStream(&headmem);

      // Next, we also store all dictionary that are smaller than 'SMALLCOMPRESS_THRESHOLD'
      state=enumstatelist;

      while(state!=NULL)
      {
         if(state->stringmem.GetSize()<SMALLCOMPRESS_THRESHOLD)
            compressor->CompressMemStream(&(state->stringmem));
         state=state->next;
      }
   }

   void CompressLargeGlobalData(Output *output)
      // Compresses the large dictionaries
      // Furthermore, we also release all the memory of all (also the small) dictionaries
   {
      EnumCompressState *state=enumstatelist;
      Compressor        compressor(output);
      unsigned long     idx=0,uncompressedsize;

      state=enumstatelist;

      while(state!=NULL)
      {
         // We keep the uncompressed size for the statistical output at the
         // end of the compression
         state->uncompressed_size=state->stringmem.GetSize();

         if(state->stringmem.GetSize()>=SMALLCOMPRESS_THRESHOLD)
         {
            compressor.CompressMemStream(&state->stringmem);
            compressor.FinishCompress(&uncompressedsize,&(state->compressed_size));
            // We store the compressed size in 'state->compressed_size'
         }
         else
            state->compressed_size=0;

         // Releases the memory of the dictionaries
         state->stringmem.ReleaseMemory(0);

         state=state->next;
         idx++;
      }

      // We reset the hash table
      // Note that the hash entries themselves are deleted separately by releasing
      // the memory of 'blockmem'
      EnumHashTable::Reset();
      enumstatelist=NULL;
      lastenumstateref=&enumstatelist;
      enuminstancecount=0;
   }

   unsigned long GetGlobalDataSize()
      // Determines how much memory we need for the dictionaries
      // This information is later used in the decompression to allocate
      // the appropriate amount of memory
   {
      EnumCompressState *state=enumstatelist;
      unsigned long     size=0;

      while(state!=NULL)
      {
         size+=sizeof(EnumDictItem)*state->curidx+          // The size of the dictionary directoy
                  WordAlignUInt(state->stringmem.GetSize());// The size of the string space (word-aligned)
         state=state->next;
      }
      return size;
   }


//*************************************************************************
//*************************************************************************


   void UncompressSmallGlobalData(SmallBlockUncompressor *uncompressor)
   {
      MemStreamer       headmem;
      unsigned long     idx=0,i,j;
      unsigned char     *srcptr,*ptr;
      EnumDictItem      *curitem;

      // First, let's extract the number of enum states
      enuminstancecount=uncompressor->LoadUInt32();

      // We allocate the space for the enum states
      enumuncompressstates=(EnumUncompressState *)enumcompressmem->GetByteBlock(sizeof(EnumUncompressState)*enuminstancecount);

      // For each state, we load the number of items and the size of the
      // string space
      for(i=0;i<enuminstancecount;i++)
      {
         enumuncompressstates[i].itemnum=uncompressor->LoadUInt32();
         enumuncompressstates[i].size=uncompressor->LoadUInt32();
      }

      // We align the main memory block of the decompressor
      WordAlignMemBlock();

      for(i=0;i<enuminstancecount;i++)
      {
         // Let's firstly load the small dictionaries
         if(enumuncompressstates[i].size<SMALLCOMPRESS_THRESHOLD)
         {
            // Load the data. Afterwards, 'srcptr' points to the corresponding memory
            srcptr=uncompressor->LoadData(enumuncompressstates[i].size);

            // Let's allocate the memory
            enumuncompressstates[i].strbuf=AllocateMemBlock(enumuncompressstates[i].size);
            WordAlignMemBlock();
            memcpy(enumuncompressstates[i].strbuf,srcptr,enumuncompressstates[i].size);

            ptr=enumuncompressstates[i].strbuf;

            // Let's now create the lookup array
            enumuncompressstates[i].itemarray=(EnumDictItem *)AllocateMemBlock(sizeof(EnumDictItem)*enumuncompressstates[i].itemnum);

            curitem=enumuncompressstates[i].itemarray;

            // We initialize the lookup array with pointers to the actual strings
            for(j=0;j<enumuncompressstates[i].itemnum;j++)
            {
               // Let's read the length first
               curitem->len=LoadUInt32(ptr);
               // Let's read the data
               curitem->dataptr=LoadData(ptr,curitem->len);
               // 'ptr' is moved forward by these operations

               curitem++;
            }
            // The pointer does not match the predicted size?
            // ==> We have a problem.
            if(ptr!=enumuncompressstates[i].strbuf+enumuncompressstates[i].size)
               ExitCorruptFile();
         }
      }
      // THe number of initializes enum states is zero at the beginning
      // The number increases with each call of 'UnCompresssor::UncompressInit()'
      activeenumuncompressstates=0;
   }

   void UncompressLargeGlobalData(Input *input)
      // Uncompresses the large dictionaries
   {
      unsigned long  i,j,tmpsize;
      unsigned char  *ptr;
      EnumDictItem   *curitem;

      Uncompressor   uncompressor;

      WordAlignMemBlock();

      for(i=0;i<enuminstancecount;i++)
      {
         if(enumuncompressstates[i].size>=SMALLCOMPRESS_THRESHOLD)
         {
            // Let's allocate the memory for the large block
            enumuncompressstates[i].strbuf=AllocateMemBlock(enumuncompressstates[i].size);
            WordAlignMemBlock();

            tmpsize=enumuncompressstates[i].size;

            // Let's do the actual uncompression
            if(uncompressor.Uncompress(input,enumuncompressstates[i].strbuf,&tmpsize))
               ExitCorruptFile();

            // Did we uncompress less data than expected? ==> Error
            if(tmpsize!=enumuncompressstates[i].size)
               ExitCorruptFile();

            ptr=enumuncompressstates[i].strbuf;

            // Let's now create the lookup array
            enumuncompressstates[i].itemarray=(EnumDictItem *)AllocateMemBlock(sizeof(EnumDictItem)*enumuncompressstates[i].itemnum);

            curitem=enumuncompressstates[i].itemarray;
            
            // We initialize the lookup array with pointers to the actual strings
            for(j=0;j<enumuncompressstates[i].itemnum;j++)
            {
               // Let's read the length first
               curitem->len=LoadUInt32(ptr);
               // Let's read the data
               curitem->dataptr=LoadData(ptr,curitem->len);

               // 'ptr' is moved forward by these operations
               curitem++;
            }
            // The pointer does not match the predicted size?
            // ==> We have a problem.
            if(ptr!=enumuncompressstates[i].strbuf+enumuncompressstates[i].size)
               ExitCorruptFile();
         }
      }
   }

   EnumUncompressState *GetNextPossibleEnumUnCompressState()
      // Retrieves the next state from the sequence of states
      // The next call retrieves the next state and so on.
   {
      activeenumuncompressstates++;
      return enumuncompressstates+activeenumuncompressstates-1;
   }

   void FinishUncompress()
      // Releases the memory after decompression
   {
      for(unsigned long i=0;i<enuminstancecount;i++)
      {
         FreeMemBlock(enumuncompressstates[i].strbuf,enumuncompressstates[i].size);
         FreeMemBlock(enumuncompressstates[i].itemarray,sizeof(EnumDictItem)*enumuncompressstates[i].itemnum);
      }
   }

};

EnumerationCompressorFactory  enumcompressfactory;


void AddEnumCompressState(EnumCompressState *state)
{
   enumcompressfactory.AddEnumCompressState(state);
}



EnumUncompressState *EnumerationUncompressor::GetNextPossibleEnumUnCompressState()
{
   return enumcompressfactory.GetNextPossibleEnumUnCompressState();
}

