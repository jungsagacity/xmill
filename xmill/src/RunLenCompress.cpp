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

//***************************************************************************
//***************************************************************************

// This module contains the run-length encoder 'rl'

#include "CompressMan.hpp"

#ifdef XDEMILL
#include "UnCompCont.hpp"
#endif

//***************************************************************************
//***************************************************************************

extern MemStreamer blockmem;
//MemStreamer *compressmem=&blockmem;

#define RUNLENGTH_ITEM_MINSIZE 16

// The run length item is used to store the current run-length item.
// The run length item can be quite large - therefore, we store it
// in a single-chained list. The list grows bigger, if longer items
// need to be stored.

struct CurRunLengthItem
   // The string is stored in the current item
{
   CurRunLengthItem  *next;
   unsigned short    len;     // The current length of data within this item
   unsigned short    size;    // The size of the item

   char *GetStrPtr() {  return (char *)(this+1);   }
      // Returns the pointer to the data
};

struct CurRunLengthState
   // Represents the state of the run-length encoder
{
   CurRunLengthItem  *items;        // The list of items
   unsigned short    totallen;      // The overall length of the string
   short             runlencount;   // If -1, if we just started

   void KeepNewString(unsigned char *str,unsigned short len)
      // Keeps the current string in the memory
      // If the previous memory is not enough, we need to allocate more
      // CurRunLengthItem
   {
      CurRunLengthItem  **itemref;
      unsigned          allocsize;

#ifdef XMILL
      runlencount=0;
#endif

      totallen=len;

      itemref=&items;

      // First, we copy the new string over the sequence of existing
      // CurRunLengthItem's

      while(*itemref!=NULL)
      {
         (*itemref)->len = ((*itemref)->size < len) ? (*itemref)->size : len;
            
         mymemcpy((*itemref)->GetStrPtr(),(char *)str,(*itemref)->len);

         str+=(*itemref)->len;
         len-=(*itemref)->len;

         itemref=&((*itemref)->next);

         if(len==0)  // Did we reach the end of new data?
                     // (I.e. the new run length item is shorter)
                     // We set the length of the remaining items to 0
         {
            while(*itemref!=NULL)
            {
               (*itemref)->len=0;
               itemref=&((*itemref)->next);
            }
            break;
         }
      }

      if(len>0)
         // If there is still data left in the new run length string,
         // we need to allocate new memory for the remaining bytes
      {
         if(len<RUNLENGTH_ITEM_MINSIZE)
            allocsize=RUNLENGTH_ITEM_MINSIZE;
         else
            allocsize=((len-1)|3)+1;

         *itemref=(CurRunLengthItem *)blockmem.GetByteBlock(sizeof(CurRunLengthItem)+allocsize);

         (*itemref)->size=allocsize;
         (*itemref)->len=len;

         mymemcpy((*itemref)->GetStrPtr(),(char *)str,len);

         (*itemref)->next=NULL;
      }
   }
};

#ifdef XMILL

class RunLengthCompressor : public UserCompressor
{
public:
   RunLengthCompressor()
   {
      datasize=sizeof(CurRunLengthState);
      contnum=1;  // We need one container for ourselves
      isrejecting=0;
      canoverlap=0;
      isfixedlen=0;
   }

// Compression functions

   void InitCompress(CompressContainer *cont,char *dataptr)
   {
      ((CurRunLengthState *)dataptr)->items=NULL;
      ((CurRunLengthState *)dataptr)->runlencount=-1;
   }

   // Note that we don't have function 'ParseString', since it compressor
   // is always accepting

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      CurRunLengthState *state=(CurRunLengthState *)dataptr;

      if(state->runlencount!=-1) // Do we already have an run-length item
                                 // that is currently active ?
      {
         CurRunLengthItem  *item=state->items;
         char              *curptr=str;
         unsigned          curlen=len;

         // Let's check whether new item is the same as the old one.
         while(item!=NULL)
         {
            if((item->len>curlen)||
               (memcmp(item->GetStrPtr(),curptr,item->len)!=0))
               break;

            curptr+=item->len;
            curlen-=item->len;

            item=item->next;

            if((curlen==0)&&(item==NULL))
               // The current item matches the new string ==> We simply increase the count
            {
               state->runlencount++;
               return;
            }
         }
         // The items don't match
         // ==> We must store the count and the current string

         cont->StoreUInt32(state->totallen);

         item=state->items;

         while(item!=NULL)
         {
            cont->StoreData(item->GetStrPtr(),item->len);
            item=item->next;
         }
         cont->StoreUInt32(state->runlencount);
      }
      else
         dataptr=NULL;

      // We keep the new string in memory

      state->KeepNewString((unsigned char *)str,len);
   }

   void FinishCompress(CompressContainer *cont,char *dataptr)
      // The compression is finalized by storing the current
      // run length and the value.
   {
      CurRunLengthState *state=(CurRunLengthState *)dataptr;

      if(state->runlencount!=-1)
         // Do we have a piece of data in memory ?
      {
         CurRunLengthItem *item;

         // We store the run-length count and the actual string
         cont->StoreUInt32(state->totallen);

         item=state->items;

         while(item!=NULL)
         {
            cont->StoreData(item->GetStrPtr(),item->len);
            item=item->next;
         }
         cont->StoreUInt32(state->runlencount);
      }
   }
};

#endif

//****************************************************************************************

// The decompressor for run length encoder

#ifdef XDEMILL

class RunLengthUncompressor : public UserUncompressor
{
public:
   RunLengthUncompressor()
   {
      datasize=sizeof(CurRunLengthState);
      contnum=1;  // We need one container for ourselves
   }

// Compression functions

   void InitUncompress(UncompressContainer *cont,char *dataptr)
      // The initialization for the state
   {
      ((CurRunLengthState *)dataptr)->items=NULL;
      ((CurRunLengthState *)dataptr)->runlencount=-1;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      CurRunLengthState *state=(CurRunLengthState *)dataptr;

      if(state->runlencount==-1) // Do we already have an run-length item
                                 // that is currently active ?
      {
         // We load the next item from the container
         // and the runlength number

         int len=cont->LoadUInt32();
         unsigned char *ptr=cont->GetDataPtr(len);

         output->characters((char *)ptr,len);

         state->runlencount=((short)cont->LoadUInt32())-1;

         state->KeepNewString(ptr,len);
      }
      else
      {
         // If we have a runlength count >-1, then we simply output
         // the current object and decrease the count
         CurRunLengthItem  *item=state->items;

         while(item!=NULL)
         {
            output->characters(item->GetStrPtr(),item->len);
            item=item->next;
         }
         state->runlencount--;
      }
   }
};

#endif

//****************************************************************************************

// The compressor factory

class RunLengthCompressorFactory : public UserCompressorFactory
{
#ifdef XDEMILL
   RunLengthUncompressor uncompressor;
#endif

public:
   char *GetName()         {  return "rl"; }
   char *GetDescription()  {  return "Run-length encoder for arbitrary text strings"; }

#ifdef XMILL
   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      if(paramstr!=NULL)
      {
         Error("Run length compressor 'rl' should not have any arguments ('");
         ErrorCont(paramstr,len);
         Error("')");
         Exit();
      }
      return new RunLengthCompressor();
   }
#endif

#ifdef XDEMILL
   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      return &uncompressor;
   }
#endif
};

RunLengthCompressorFactory  runlengthcompressor;
