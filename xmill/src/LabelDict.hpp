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

// This module contains the label dictionary manager
// The label dictionary stores the associations between label IDs and
// the actual label name.


#ifndef LABELDICT_HPP
#define LABELDICT_HPP

// Maximum number of labels
#define MAXLABEL_NUM       65535L

#include "Types.hpp"
#include "MemStreamer.hpp"
#include "Load.hpp"

#ifdef XMILL
#include "Compress.hpp"
#endif

#ifdef XDEMILL
#include "SmallUncompress.hpp"
#endif

#define ISATTRIB(labelid)  (((labelid)&ATTRIBLABEL_STARTIDX)?1:0)

extern MemStreamer mainmem;

#ifdef XMILL

// For the compressor, the dictionary is implemented as a hash table,
// since we need to look up the ID for a given name

#define HASHTABLE_SIZE    256
#define HASHTABLE_MASK    255
#define HASHTABLE_SHIFT   8
#define HASHTABLE_SHIFT2  16
#define HASHTABLE_SHIFT3  24

// For the compressor, we store each label dictionary item
// in the following data structure.
// The label data is stored *directly* after the CompressLabelDictItem object.

struct CompressLabelDictItem

{
   unsigned short          len;           // The length of the label
   TLabelID                labelid;       // The label ID
   CompressLabelDictItem   *nextsamehash; // The next label with the same hash value
   CompressLabelDictItem   *next;         // The label with the next higher ID

   char IsAttrib()   {  return ISATTRIB(labelid);  }

   char *GetLabelPtr()  {  return (char *)(this+1); }
      // Returns the pointer to the label name

   unsigned short GetLabelLen()  {  return len; }
      // Returns the label length
};
#endif

//******************************************************************************

#ifdef XDEMILL

// For the decompressor, the label dictionary is implemented through a
// lookup table. New entries can be added through the decompressor runs

struct UncompressLabelDictItem
   // Each label is represented in this structure
{
   unsigned short    len;        // The length of the label
   unsigned char     isattrib;   // Is it an attribute?
   char              *strptr;    // The pointer to the actual string
};

struct UncompressLabelDict
   // Represents a sequence of labels.
   // The labels are stored in 'UncompressLabelDictItem'-objects directly
   // after the 'UncompressLabelDict'-object.
{
   TLabelID                labelnum;   // The number of labels in this sequence 
   UncompressLabelDict     *next;      // The next label sequence

   UncompressLabelDictItem* GetArrayPtr()
      // Returns the pointer to the UncompressLabelDictItem
   {
      return (UncompressLabelDictItem *)(this+1);
   }
};
#endif

//******************************************************************************

class LabelDict
   // The actual label dictionary class
{
   TLabelID                   labelnum;            // The number of labels

#ifdef XMILL
   TLabelID                   predefinedlabelnum; 
      // The number of labels predefined through the FSMs
      // Those labels will *not* be deleted in a Reset() invocation

   CompressLabelDictItem      **hashtable;         // The hash table
   CompressLabelDictItem      *labels,**labelref;  // The list of labels

   // This describes how many of the labels already have been saved
   // in the compressed file - i.e. in a previous run
   TLabelID                   savedlabelnum;    
   CompressLabelDictItem      **savedlabelref;
#endif

#ifdef XDEMILL
   UncompressLabelDict        *labeldictlist;      // The list of label groups
   UncompressLabelDict        *lastlabeldict;      // The last label group
#endif

public:
#ifdef PROFILE
   unsigned long              lookupcount,hashitercount;
#endif

   void Init()
   {
#ifdef PROFILE
      lookupcount=
      hashitercount=0;
#endif

      labelnum=0;

#ifdef XMILL
      // No labels
      labels=NULL;
      labelref=&labels;

      // no saved labels until now
      savedlabelnum=0;
      savedlabelref=&labels;

      // let's get some memory for the hash table
//      mainmem.WordAlign();
//      hashtable=(CompressLabelDictItem **)mainmem.GetByteBlock(sizeof(CompressLabelDictItem *)*HASHTABLE_SIZE);
      hashtable=(CompressLabelDictItem **)new char[sizeof(CompressLabelDictItem *)*HASHTABLE_SIZE];
      if(hashtable==NULL)
         ExitNoMem();

      for(int i=0;i<HASHTABLE_SIZE;i++)
         hashtable[i]=NULL;
#endif
#ifdef XDEMILL
      // No labels until now
      labeldictlist=NULL;
      lastlabeldict=NULL;
#endif
   }

   void Reset()
   {  
#ifdef XMILL
      int      i;
      TLabelID labelid;
      CompressLabelDictItem **hashtableref;

      // First, let's scan over the hash table to remove all
      // entris of not predefined labels
      for(i=0;i<HASHTABLE_SIZE;i++)
      {
         hashtableref=hashtable+i;
         while(*hashtableref!=NULL)
         {
            if((*hashtableref)->labelid>=predefinedlabelnum)
               // Not a predefined label ID?
               // ==> Delete
               *hashtableref=(*hashtableref)->nextsamehash;
            else
               // Otherwise, we go to the next label
               hashtableref=&((*hashtableref)->nextsamehash);
         }
      }

      // Now we need to cut the global list of labels.
      // We keep the first 'predefinedlabelnum' predefined labels.
      CompressLabelDictItem **curlabelref=&labels;

      for(labelid=0;labelid<predefinedlabelnum;labelid++)
         curlabelref=&((*curlabelref)->next);

      *curlabelref=NULL;

      labelref=curlabelref;
      labelnum=predefinedlabelnum;

      // no saved labels until now
      savedlabelnum=0;
      savedlabelref=&labels;
#endif
#ifdef XDEMILL
      // No labels until now
      labelnum=0;
      labeldictlist=NULL;
      lastlabeldict=NULL;
#endif
   }
#ifdef XMILL
   void FinishedPredefinedLabels()
      // Finished the creation of predefined labels
      // ==> We keep the number of predefined labels
   {
      predefinedlabelnum=labelnum;
   }
#endif

// ************** These are functions for the compressor ****************************

#ifdef XMILL

   int CalcHashIdx(char *label,int len)
      // Computes the hash value for a given label name
   {
      int val=0;

      while(len>0)
      {
         val+=*label;
         val=(val<<(*label&7))+(val>>(32-(*label&7)));
         label++;
         len--;
      }
      return ((val>>HASHTABLE_SHIFT)+(val>>HASHTABLE_SHIFT2)+(val>>HASHTABLE_SHIFT3)+val)&HASHTABLE_MASK;
   }

   TLabelID FindLabelOrAttrib(char *label,unsigned len,unsigned char isattrib)
      // Finds a given label (element tag or attribute)
   {
      CompressLabelDictItem **hashref=hashtable+CalcHashIdx(label,len);

#ifdef PROFILE
      lookupcount++;
#endif

      // We look through the linked list of hashtable entries
      while(*hashref!=NULL)
      {
#ifdef PROFILE
         hashitercount++;
#endif
         if(((*hashref)->len==len)&&
            ((*hashref)->IsAttrib()==isattrib)&&
            (mymemcmp((*hashref)->GetLabelPtr(),label,len)==0))

            return (*hashref)->labelid;

         hashref=&((*hashref)->nextsamehash);
      }
      return LABEL_UNDEFINED;
   }

   TLabelID CreateLabelOrAttrib(char *label,unsigned len,unsigned char isattrib)
      // Creates a new label in the hash table
   {
      // Let's get some memory first
      mainmem.WordAlign();
      CompressLabelDictItem *item=(CompressLabelDictItem *)mainmem.GetByteBlock(sizeof(CompressLabelDictItem)+len);
      mainmem.WordAlign();

      // We copy the label name
      item->len=(unsigned short)len;
      mymemcpy(item->GetLabelPtr(),label,len);

      // We insert it into the hashtable
      CompressLabelDictItem **hashref=hashtable+CalcHashIdx(label,len);
      item->nextsamehash=*hashref;
      *hashref=item;

      // Let's add the label to the list of labels
      item->next=NULL;

      *labelref=item;
      labelref=&(item->next);

      item->labelid=labelnum;
      labelnum++;

      if(isattrib)
         item->labelid|=ATTRIBLABEL_STARTIDX;

      return item->labelid;
   }

   TLabelID GetLabelOrAttrib(char *label,unsigned len,unsigned char isattrib)
      // Finds or creates a label/attribute
   {
      TLabelID labelid=FindLabelOrAttrib(label,len,isattrib);

      if(labelid==LABEL_UNDEFINED)
         return CreateLabelOrAttrib(label,len,isattrib);
      else
         return labelid;
   }

   void Store(Compressor *compressor)
      // Stores the current content of the label dictionary in the output
      // compressor. Only the labels since the last storing are copied.
   {
      MemStreamer mem;

      // Let's store the number of labels that were inserted
      // since the previous storing
      mem.StoreUInt32(labelnum-savedlabelnum);

      // We go through all new labels and store them.
      while(*savedlabelref!=NULL)
      {
         mem.StoreSInt32(((*savedlabelref)->labelid&ATTRIBLABEL_STARTIDX)?1:0,(*savedlabelref)->GetLabelLen());
         mem.StoreData((*savedlabelref)->GetLabelPtr(),(*savedlabelref)->GetLabelLen());
         savedlabelref=&((*savedlabelref)->next);
      }

      compressor->CompressMemStream(&mem);

      savedlabelnum=labelnum;
   }
#endif

//**********************************************************************************
//**********************************************************************************
//**********************************************************************************

#ifdef XDEMILL

#define LABELDICT_MINLABELNUM 32

// ************** These are functions for the uncompressor ****************************

   void Load(SmallBlockUncompressor *uncompress)
      // Loads the next block of labels and appends the block to the
      // already existing blocks in the dictionary
   {
      UncompressLabelDictItem *dictitemptr;
      char                    isattrib;
      unsigned                dictlabelnum;

      // Let's get the number of labels first
      unsigned mylabelnum=uncompress->LoadUInt32();
      if(mylabelnum>MAXLABEL_NUM)
         ExitCorruptFile();

      // No new labels?
      if(mylabelnum==0)
         return;

      if(lastlabeldict!=NULL)
         // We already have some labels in the dictionary?
      {
         if(lastlabeldict->labelnum<LABELDICT_MINLABELNUM)
            // Is there some space for more labels in the current block?
            // We copy as many labels as we can
         {
            dictlabelnum=LABELDICT_MINLABELNUM-lastlabeldict->labelnum;
            if(dictlabelnum>mylabelnum)
               dictlabelnum=mylabelnum;

            mylabelnum-=dictlabelnum;

            dictitemptr=lastlabeldict->GetArrayPtr()+lastlabeldict->labelnum;

            lastlabeldict->labelnum+=dictlabelnum;
            labelnum+=dictlabelnum;

            // We copy the actual labels now
            // Each label is represented by the length and the attribute-flag
            // Then, the actual name follows
            while(dictlabelnum--)
            {
               dictitemptr->len=(unsigned short)uncompress->LoadSInt32(&isattrib);
               dictitemptr->isattrib=isattrib;

               dictitemptr->strptr=mainmem.GetByteBlock(dictitemptr->len);

               mymemcpy(dictitemptr->strptr,
                        uncompress->LoadData(dictitemptr->len),
                        dictitemptr->len);

               dictitemptr++;
            }
            if(mylabelnum==0)
               return;
         }
         // We create a new label block for the new labels
         lastlabeldict->next=(UncompressLabelDict *)mainmem.GetByteBlock(
                                 sizeof(UncompressLabelDict)+
                                 ((mylabelnum>LABELDICT_MINLABELNUM)? mylabelnum : LABELDICT_MINLABELNUM)*sizeof(UncompressLabelDictItem));
         lastlabeldict=lastlabeldict->next;
      }
      else
         labeldictlist=lastlabeldict=(UncompressLabelDict *)mainmem.GetByteBlock(
                                       sizeof(UncompressLabelDict)+
                                       ((mylabelnum>LABELDICT_MINLABELNUM)? mylabelnum : LABELDICT_MINLABELNUM)*sizeof(UncompressLabelDictItem));

      lastlabeldict->next=NULL;

//******

      lastlabeldict->labelnum=mylabelnum;
      labelnum+=mylabelnum;

      dictlabelnum=mylabelnum;

      dictitemptr=lastlabeldict->GetArrayPtr();

      // We copy the actual labels now
      // Each label is represented by the length and the attribute-flag
      // Then, the actual name follows
      while(dictlabelnum--)
      {
         dictitemptr->len=(unsigned short)uncompress->LoadSInt32(&isattrib);
         dictitemptr->isattrib=isattrib;

         dictitemptr->strptr=mainmem.GetByteBlock(dictitemptr->len);
         mainmem.WordAlign();

         mymemcpy(dictitemptr->strptr,
                  uncompress->LoadData(dictitemptr->len),
                  dictitemptr->len);

         dictitemptr++;
      }
   }

   unsigned long LookupLabel(TLabelID labelid,char **ptr,unsigned char *isattrib)
      // Find the name of the label with a given ID
   {
      UncompressLabelDict *labeldict=labeldictlist;

      // We look through all the blocks
      while(labelid>=labeldict->labelnum)
      {
         labelid-=labeldict->labelnum;
         labeldict=labeldict->next;
      }
      UncompressLabelDictItem *item=labeldict->GetArrayPtr()+labelid;

      *isattrib=item->isattrib;
      *ptr=item->strptr;
      return item->len;
   }
#endif

//**********************************************************************************
//**********************************************************************************

#ifdef XMILL
   unsigned long LookupCompressLabel(TLabelID labelid,char **ptr)
      // Finds the name of a label with a given ID in the compressor
      // In the compressor, we need to traverse the entire list
      // of labels ==> Relatively inefficient - but we only
      // need that for printing
   {
      CompressLabelDictItem *item=labels;

      labelid=(labelid&(ATTRIBLABEL_STARTIDX-1));

      while(labelid--)
         item=item->next;

      *ptr=item->GetLabelPtr();
      return item->GetLabelLen();
   }
#endif

   void PrintLabel(TLabelID labelid)
      // Prints the label with labelid 'labelid'
   {
      char           *ptr;
      unsigned long  len;
      unsigned char  isattrib=ISATTRIB(labelid);

      labelid=(labelid&(ATTRIBLABEL_STARTIDX-1));

#ifdef XMILL
      len=LookupCompressLabel(labelid,&ptr);
#endif
#ifdef XDEMILL
      len=LookupLabel(labelid,&ptr,&isattrib);
#endif

      if(isattrib)   // Attribute names start with '@'
      {
         printf("@");
         fwrite(ptr,len,1,stdout);
      }
      else
         fwrite(ptr,len,1,stdout);
   }

   void Print()
      // Prints all labels in the dictionary
   {
      for(unsigned long i=0;i<labelnum;i++)
      {
         printf("%lu : ",i);
         PrintLabel((TLabelID)i);
         printf("\n");
      }
   }

#ifdef PROFILE
   void PrintProfile()
   {
      printf("Labeldict: count=%lu lookupcount=%lu   hashitercount=%lu\n",labelnum,lookupcount,hashitercount);
   }
#endif
};


//***************************************************************

// There is one global dictionary
extern LabelDict globallabeldict;

#endif
