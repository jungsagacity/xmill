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

// This module contains the Division-Compressor 'seq'
// The Div-compressor compresses a string by applying a sequence of subcompressors
// to the string. The substrings for the compressors are separated by some delimiter strings

#include "CompressMan.hpp"


#include "UnCompCont.hpp"


struct DivCompressorItem
   // Each subcompressor and the following delimiter string
   // are stored in a 'DivCompressorItem'
{
   DivCompressorItem *next;               // The next subcompressor item

   UserCompressor    *usercompressor;     // The sub compressor


   UserUncompressor  *useruncompressor;   // The sub decompressor


   // The separator between subcompressors
   // This can be empty (afterseparator=NULL), if this is the last subcompressor
   // and there is no separator string at the end
   char              *afterseparator;
   unsigned short    afterseparatorlen;

   unsigned short    curitemlen;          // After parsing a string, we keep the
                                          // length of the string that matches the compressor
                                          // 'usercompressor'

   void *operator new(size_t size)  {  return mainmem.GetByteBlock(size);}
   void operator delete(void *ptr)  {}
};

struct DivSepComprInfo
   // Stores all the information about the subcompressors and delimiters
{
   // If there is a separator at the beginning, we store it here
   char              *startseparator;
   unsigned long     startseparatorlen;

   DivCompressorItem *subcompressors;  // The list of subcompressors

   void CreateSubCompressors(char *paramstr,int len)
      // Generates the subcompressors from the parameter string
   {
      char *endstringptr;
      DivCompressorItem **curitemref;

      char  *ptr=paramstr;
      char  *endptr=paramstr+len;

      if(*ptr=='"')  // Did we find a separator string at the beginning ?
      {
         endstringptr=::ParseString(ptr,ptr+len);

         startseparator=ptr+1;
         startseparatorlen=endstringptr-ptr-1;

         ptr=endstringptr+1;
      }
      else  // Otherwise, we don't have any start separator
      {
         startseparator=NULL;
         startseparatorlen=0;
      }

      // We now parse the subcompressors and separator strings

      curitemref=&subcompressors;
      *curitemref=NULL;
      
      do
      {
         ptr=SkipWhiteSpaces(ptr,endptr);
         if(ptr==endptr)   // Did we reach the end?
            break;

         // First, we identify a compressor

         *curitemref=new DivCompressorItem();

         (*curitemref)->usercompressor=compressman.CreateCompressorInstance(ptr,endptr);


         (*curitemref)->useruncompressor=compressman.CreateUncompressorInstance(ptr,endptr);


         (*curitemref)->next=NULL;

         // Next, we identify a delimiter string

         ptr=SkipWhiteSpaces(ptr,endptr);
         if(ptr==endptr)   // Did we reach the end?
            break;

         // There must be string afterwards
         endstringptr=::ParseString(ptr,endptr);

         // Let's store the separator string
         (*curitemref)->afterseparator=ptr+1;
         (*curitemref)->afterseparatorlen=endstringptr-ptr-1;

         ptr=endstringptr+1;

         curitemref=&((*curitemref)->next);
         *curitemref=NULL;
      }
      while(1);

      if(*curitemref!=NULL)
      {
         (*curitemref)->afterseparator=NULL;
         (*curitemref)->afterseparatorlen=0;
      }
   }
};

//*************************************************************************

// The 'seq' compressor

class DivSepCompressorFactory;



class DivSepCompressor : public UserCompressor
{
   friend DivSepCompressorFactory;
protected:

   DivSepComprInfo info;   // The information about the subcompressors

public:
   void ComputeProperties()
   {
      isrejecting=1;canoverlap=1;
      isfixedlen=1;
      datasize=0;
      contnum=0;

      DivCompressorItem *item=info.subcompressors;
      while(item!=NULL)
      {
         datasize+=item->usercompressor->GetUserDataSize();
         contnum+=item->usercompressor->GetUserContNum();

         if(item->usercompressor->CanOverlap()==0)
            canoverlap=0;

         // If one of the subcompressors is not fixed-length, then the entire
         // compressor is not fixed-length
         if(item->usercompressor->IsFixedLen()==0)
            isfixedlen=0;

         item=item->next;
      }
   }

   void CreateSubCompressors(char *paramstr,int len)
   {
      info.CreateSubCompressors(paramstr,len);
   }

   void InitCompress(CompressContainer *cont,char *dataptr)
      // Initializes the the compressors
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->usercompressor->InitCompress(cont,dataptr);
         cont+=item->usercompressor->GetUserContNum();
         dataptr+=item->usercompressor->GetUserDataSize();
         item=item->next;
      }
   }

   char ParseString(char *str,unsigned len,char *dataptr)
   {
      char  *ptr1=str,
            *ptr2,*curptr,*endptr,*itemptr;
      DivCompressorItem *item;
      unsigned i;

      // Do we have a start separator ?
      if(info.startseparator!=NULL)
      {
         ptr1=str;
         ptr2=info.startseparator;

         // If the the entire string is shorter than the separator length,
         // the string can definitely not match
         if(len<info.startseparatorlen)
            return 0;

         // Let's check whether the strings are equal
         for(i=0;i<info.startseparatorlen;i++)
         {
            if(*ptr1!=*ptr2)
               return 0;
            ptr1++;
            ptr2++;
         }
         itemptr=ptr1;
      }
      else
         itemptr=str;

      item=info.subcompressors;

      // As long as we have more subcompressors, we continue parsing
      // If 'item->next' is NULL, then the subcompressor in 'item->usercompressor'
      // must match the *rest* of the string and this is done outside of the loop.
      while(item->next!=NULL)
      {
         curptr=itemptr;
         endptr=str+len-item->afterseparatorlen;   // The endptr is the end of the entire string
                                                   // minus the length of the separator string

         // We search for the separator
         while(curptr<endptr)
         {
            ptr1=curptr;
            ptr2=item->afterseparator;

            for(i=0;i<item->afterseparatorlen;i++)
            {
               if(*ptr1!=*ptr2)
                  break;
               ptr1++;
               ptr2++;
            }
            // We found the separator ?
            if(i==item->afterseparatorlen)
               break;

            curptr++;
         }

         if(curptr>=endptr)
            // We didn't find the separator ? => Exit
            return 0;

         // We keep the length of the string up to the delimiter
         // This will be used in 'CompressString' later.
         item->curitemlen=curptr-itemptr;

         // Let's see if the subcompressor can parse the string
         if(item->usercompressor->ParseString(itemptr,curptr-itemptr,dataptr)==0)
            return 0;

         dataptr+=item->usercompressor->GetUserDataSize();

         itemptr=ptr1;
         item=item->next;
      }

      endptr=str+len;

      // If we have a delimiter at the end, then we check whether it matches
      // the end of 'str'
      if(item->afterseparator!=NULL)
      {
         ptr1=endptr-item->afterseparatorlen;
         if(ptr1<itemptr)
            return 0;
         ptr2=item->afterseparator;

         for(i=0;i<item->afterseparatorlen;i++)
         {
            if(*ptr1!=*ptr2)  // Tail delimiter doesn't match?
               return 0;
            ptr1++;
            ptr2++;
         }
         endptr-=item->afterseparatorlen;
      }

      item->curitemlen=endptr-itemptr;

      // Let's do the parsing for the last subcompressor
      if(item->usercompressor->ParseString(itemptr,endptr-itemptr,dataptr)==0)
         return 0;

      return 1;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
      // Compresses the given input string
      // 'dataptr' denotes the state
      // Since the compressor is 'rejecting', the function can expect
      // that 'ParseString' has been called before and
      // 'item->curitemlen' has been set for each subcompressor
   {
      DivCompressorItem *item=info.subcompressors;
      char              *startitemptr;

      // Skip the first separator
      startitemptr=str+info.startseparatorlen;

      while(item!=NULL)
      {
         // Compress using the next subcompressor and the length stored
         // in 'item->curitemlen'
         item->usercompressor->CompressString(startitemptr,item->curitemlen,cont,dataptr);

         cont+=item->usercompressor->GetUserContNum();
         dataptr+=item->usercompressor->GetUserDataSize();

         // Skip the next separator and go to the next 
         startitemptr+=item->curitemlen+item->afterseparatorlen;
         item=item->next;
      }
   }

   void FinishCompress(CompressContainer *cont,char *dataptr)
      // Finishes the compression - the compressor should write any
      // remaining data to the containers
      // For the seq-compressor, all subcompressors are 'finished'
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->usercompressor->FinishCompress(cont,dataptr);

         cont+=item->usercompressor->GetUserContNum();
         dataptr+=item->usercompressor->GetUserDataSize();

         item=item->next;
      }
   }

   void PrintCompressInfo(char *dataptr,unsigned long *overalluncomprsize,unsigned long *overallcomprsize)
      // Prints statistical information about how well the compressor compressed
      // the data
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->usercompressor->PrintCompressInfo(dataptr,overalluncomprsize,overallcomprsize);

         dataptr+=item->usercompressor->GetUserDataSize();

         item=item->next;
      }
   }
};



//************************************************************************
//************************************************************************

//** The Decompressor ********


class DivSepUncompressor : public UserUncompressor
{
   friend DivSepCompressorFactory;
protected:

   DivSepComprInfo info;

public:
   void ComputeProperties()
      // Computes the properties of the decompressor
   {
      DivCompressorItem *item=info.subcompressors;

      datasize=0;
      contnum=0;

      while(item!=NULL)
      {
         datasize+=item->useruncompressor->GetUserDataSize();
         contnum+=item->useruncompressor->GetUserContNum();
         item=item->next;
      }
   }

   void CreateSubCompressors(char *paramstr,int len)
      // Creates the subcompressor structures based the
      // parameter string
   {
      info.CreateSubCompressors(paramstr,len);
   }

   void InitUncompress(UncompressContainer *cont,char *dataptr)
      // Initializes the subcompressors
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->useruncompressor->InitUncompress(cont,dataptr);
         cont+=item->useruncompressor->GetUserContNum();
         dataptr+=item->useruncompressor->GetUserDataSize();
         item=item->next;
      }
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
      // Does the actual decompression of a single text item
      // and prints the text to 'output'
   {
      DivCompressorItem *item=info.subcompressors;

      // We output the start separator, if it exists
      if(info.startseparator!=NULL)
         output->characters(info.startseparator,info.startseparatorlen);

      // Each decompressor decompresses its own piece of text
      // from the corresponding containers
      while(item!=NULL)
      {
         item->useruncompressor->UncompressItem(cont,dataptr,output);

         if(item->afterseparator!=NULL)
            output->characters(item->afterseparator,item->afterseparatorlen);

         cont+=item->useruncompressor->GetUserContNum();
         dataptr+=item->useruncompressor->GetUserDataSize();

         item=item->next;
      }
   }

   void FinishUncompress(UncompressContainer *cont,char *dataptr)
      // Finishes the decompression
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->useruncompressor->FinishUncompress(cont,dataptr);

         cont+=item->useruncompressor->GetUserContNum();
         dataptr+=item->useruncompressor->GetUserDataSize();

         item=item->next;
      }
   }
};



//****************************************************************************

class DivSepCompressorFactory : public UserCompressorFactory
   // The compressor factory for 'seq'
{
public:
   char *GetName()         {  return "seq"; }
   char *GetDescription()  {  return "Sequence compressor for strings with separators"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
      // Instantiates the compressor
   {
      if(paramstr==NULL)
      {
         Error("Sequence compressor 'seq' must have a sequence of strings and compressors as parameters");
         Exit();
      }

      DivSepCompressor  *divsepcompressor=new DivSepCompressor();

      // We initialize the compressor
      divsepcompressor->CreateSubCompressors(paramstr,len);
      divsepcompressor->ComputeProperties();
      return divsepcompressor;
   }



   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
      // Instantiates the decompressor
   {
      DivSepUncompressor  *divsepuncompressor=new DivSepUncompressor();

      // Intializes the decompressor
      divsepuncompressor->CreateSubCompressors(paramstr,len);
      divsepuncompressor->ComputeProperties();
      return divsepuncompressor;
   }

};

// The actual compressor factory for the combined sequence compressor
DivSepCompressorFactory  divsepcompressfactory;


//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************

// The DivCombine compressor is similar to the DivSep compressor.
// The only difference is that the containers of the subcompressors 'overlap' -
// i.e. the overall number of containers of the 'seqcomb' compressor is
// the *maximum* number of containers of the subcompressors.



class DivCombineCompressor : public DivSepCompressor
   // The compressor class is inherited from DivSepCompressor so that
   // we can use 'ParseString'
{
   friend DivSepCompressorFactory;

public:
   void ComputeProperties()
   {
      isrejecting=1;canoverlap=1;
      isfixedlen=1;
      datasize=0;
      contnum=0;
      
      DivCompressorItem *item=info.subcompressors;
      while(item!=NULL)
      {
         datasize+=item->usercompressor->GetUserDataSize();

         // We find the maximum container number
         if(contnum<item->usercompressor->GetUserContNum())
            contnum=item->usercompressor->GetUserContNum();

         if(item->usercompressor->CanOverlap()==0)
            canoverlap=0;

         if(item->usercompressor->IsFixedLen()==0)
            isfixedlen=0;

         item=item->next;
      }
   }

   void CreateSubCompressors(char *paramstr,int len)
   {
      info.CreateSubCompressors(paramstr,len);
   }

   void InitCompress(CompressContainer *cont,char *dataptr)
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         // Note that 'cont' is the same for all subcompressors.
         item->usercompressor->InitCompress(cont,dataptr);
         dataptr+=item->usercompressor->GetUserDataSize();
         item=item->next;
      }
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
       // Compresses the given input string
      // 'dataptr' denotes the state
      // Since the compressor is 'rejecting', the function can expect
      // that 'DivSepCompressor::ParseString' has been called before and
      // 'item->curitemlen' has been set for each subcompressor
   {
      DivCompressorItem *item=info.subcompressors;
      char              *startitemptr;

      startitemptr=str+info.startseparatorlen;

      while(item!=NULL)
      {
         item->usercompressor->CompressString(startitemptr,item->curitemlen,cont,dataptr);
         dataptr+=item->usercompressor->GetUserDataSize();

         startitemptr+=item->curitemlen+item->afterseparatorlen;
         item=item->next;
      }
   }

   void FinishCompress(CompressContainer *cont,char *dataptr)
      // Finishes the compression - the compressor should write any
      // remaining data to the containers
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->usercompressor->FinishCompress(cont,dataptr);
         dataptr+=item->usercompressor->GetUserDataSize();
         item=item->next;
      }
   }

   void PrintCompressInfo(char *dataptr,unsigned long *overalluncomprsize,unsigned long *overallcomprsize)
      // Prints statistical information about how well the compressor compressed
      // the data
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->usercompressor->PrintCompressInfo(dataptr,overalluncomprsize,overallcomprsize);

         dataptr+=item->usercompressor->GetUserDataSize();

         item=item->next;
      }
   }
};



//**********************************************************************************

// The combined sequence decompressor



class DivCombineUncompressor : public DivSepUncompressor
   // Note that the class is denoted from 'DivSepUncompressor' so that
   // we can use the 'info' attribute
{
   friend DivSepCompressorFactory;

public:
   void ComputeProperties()
      // Computes the properties of the decompressor
      // Note that this is different from 'ComputeProperties' in the original
      // DivSepUncompressor
   {
      DivCompressorItem *item=info.subcompressors;

      datasize=0;
      contnum=0;

      while(item!=NULL)
      {
         datasize+=item->useruncompressor->GetUserDataSize();

         // We find the maximum container count:
         if(contnum<item->useruncompressor->GetUserContNum())
            contnum=item->useruncompressor->GetUserContNum();
         item=item->next;
      }
   }

   void CreateSubCompressors(char *paramstr,int len)
   {
      info.CreateSubCompressors(paramstr,len);
   }

   void InitUncompress(UncompressContainer *cont,char *dataptr)
      // Initializes the decompressor
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->useruncompressor->InitUncompress(cont,dataptr);

         dataptr+=item->useruncompressor->GetUserDataSize();
         item=item->next;
      }
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
      // Does the actual decompression of a single text item
      // and prints the text to 'output'
   {
      DivCompressorItem *item=info.subcompressors;

      if(info.startseparator!=NULL)
         output->characters(info.startseparator,info.startseparatorlen);

      while(item!=NULL)
      {
         item->useruncompressor->UncompressItem(cont,dataptr,output);
         if(item->afterseparator!=NULL)
            output->characters(item->afterseparator,item->afterseparatorlen);
         dataptr+=item->useruncompressor->GetUserDataSize();

         item=item->next;
      }
   }

   void FinishUncompress(UncompressContainer *cont,char *dataptr)
      // Finished the decompression
   {
      DivCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->useruncompressor->FinishUncompress(cont,dataptr);
         dataptr+=item->useruncompressor->GetUserDataSize();
         item=item->next;
      }
   }
};



//**********************************************************************************

class DivCombineCompressorFactory : public DivSepCompressorFactory
   // The compressor factory for combined compressor/decompressor
{
public:
   char *GetName()         {  return "seqcomb"; }
   char *GetDescription()  {  return "Combined sequence compressor for strings with separators"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      if(paramstr==NULL)
      {
         Error("Combined sequence compressor 'seqcomb' must have a sequence of strings and compressors as parameters");
         Exit();
      }

      DivCombineCompressor  *divcombcompressor=new DivCombineCompressor();

      // Initializes the decompressor with the subcompressors
      divcombcompressor->CreateSubCompressors(paramstr,len);
      divcombcompressor->ComputeProperties();
      return divcombcompressor;
   }



   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      DivCombineUncompressor  *divcombuncompressor=new DivCombineUncompressor();

      // Initializes the decompressor with the subcompressors
      divcombuncompressor->CreateSubCompressors(paramstr,len);
      divcombuncompressor->ComputeProperties();
      return divcombuncompressor;
   }

};

// The actual compressor factory for the combined sequence compressor
DivCombineCompressorFactory  divcombcompressfactory;
