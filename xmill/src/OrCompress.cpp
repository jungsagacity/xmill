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

// This module contains the Or-Compressor.
// The Or-compressor has name 'or' and a list of other compressors as parameters

#include "CompressMan.hpp"


#include "UnCompCont.hpp"


struct OrCompressorItem
   // Each parameter of the or-compressor is stored in this structur
{
   OrCompressorItem  *next;   // Next parameter

   UserCompressor    *usercompressor;


   UserUncompressor  *useruncompressor;


   void *operator new(size_t size)  {  return mainmem.GetByteBlock(size);}
   void operator delete(void *ptr)  {}
};

//*************************

// Until now, we only implemented the separate Or-compressor
// - i.e. each sub-compressor gets own containers and there
// is no overlapping between containers

class OrSepCompressorFactory;

struct OrSepCompressorInfo
{
   OrCompressorItem  *subcompressors;

   void CreateSubCompressors(char *paramstr,int len)
      // Parses a given parameter string and creates the structure of
      // sub-compressor parameters
   {
      OrCompressorItem **curitemref;

      char  *ptr=paramstr;
      char  *endptr=paramstr+len;

      subcompressors=NULL;

      curitemref=&subcompressors;
      
      while(ptr<endptr)
      {
         if((*ptr==' ')||(*ptr==',')||(*ptr=='\t')||(*ptr=='\r')||(*ptr=='\n'))
         {
            ptr++;
            continue;
         }

         *curitemref=new OrCompressorItem();

         // For compression, we create compressors
         // For decompression, we create decompressors

         (*curitemref)->usercompressor=compressman.CreateCompressorInstance(ptr,endptr);


         (*curitemref)->useruncompressor=compressman.CreateUncompressorInstance(ptr,endptr);

         // Pointer 'ptr' is moved forward by the previous functions

         (*curitemref)->next=NULL;

         curitemref=&((*curitemref)->next);
      }
   }
};

//**************************************************************************************



class OrSepCompressor : public UserCompressor
   // The compressor part of the Or-compressor,
{
   friend OrSepCompressorFactory;
protected:

   OrSepCompressorInfo  info; // The parameters

   long                 curidx;           // This is used
   OrCompressorItem     *curcompressor;

   unsigned short       curdataoffset;
   unsigned short       curcontoffset;

public:

   void ComputeProperties()
      // We determine whether the or-compressor is rejecting or can be overlapping
      // Furthermore, we determine the number of containers and the user data size needed
   {
      datasize=0;
      contnum=1;     // We need one container for ourselves
      isrejecting=1;
      canoverlap=1;
      isfixedlen=0;

      OrCompressorItem *item=info.subcompressors;
      while(item!=NULL)
      {
         if(item->usercompressor->IsRejecting()==0)
            isrejecting=0;

         if(item->usercompressor->CanOverlap()==0)
            canoverlap=0;

         contnum+=item->usercompressor->GetUserContNum();
         datasize+=item->usercompressor->GetUserDataSize();
         item=item->next;
      }
   }

   void InitCompress(CompressContainer *cont,char *dataptr)
      // Before we start any compression, we initialize the compressor.
      // 'dataptr' denotes the state
   {
      OrCompressorItem *item=info.subcompressors;

      curidx=-1;

      cont++;

      while(item!=NULL)
      {
         item->usercompressor->InitCompress(cont,dataptr);

         cont+=item->usercompressor->GetUserContNum();
         dataptr+=item->usercompressor->GetUserDataSize();
         item=item->next;
      }
   }

   char ParseString(char *str,unsigned len,char *dataptr)
      // Parses the string and returns 1, if accepted, otherwise 0.
      // This function does not actually store/compreess the string
      // But it can keep an internal state - in the next step,
      // CompressString is called.
   {
      char  *ptr1=str;
      OrCompressorItem *item;

      item=info.subcompressors;

      curidx=0;
      curdataoffset=0;
      curcontoffset=1;

      while(item!=NULL)
      {
         if(item->usercompressor->ParseString(str,len,dataptr))
            break;

         dataptr+=item->usercompressor->GetUserDataSize();

         curidx++;
         curdataoffset+=item->usercompressor->GetUserDataSize();
         curcontoffset+=item->usercompressor->GetUserContNum();

         item=item->next;
      }

      if(item==NULL) // None of the subcompressors accepted the text ?
         return 0;

      curcompressor=item;

      return 1;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
      // Compresses the given input string
      // 'dataptr' denotes the state
      // If the compressor is 'rejecting', then the function can expect
      // that 'ParseString' has been called before.
   {
      if(curidx==-1)
         ParseString(str,len,dataptr);

      cont->StoreUInt32(curidx);

      cont+=curcontoffset;
      dataptr+=curdataoffset;

      curidx=-1;

      curcompressor->usercompressor->CompressString(str,len,cont,dataptr);
   }

   void FinishCompress(CompressContainer *cont,char *dataptr)
      // Finishes the compression - the compressor should write any
      // remaining data to the containers
   {
      OrCompressorItem *item=info.subcompressors;

      cont++;

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
      OrCompressorItem *item=info.subcompressors;

      while(item!=NULL)
      {
         item->usercompressor->PrintCompressInfo(dataptr,overalluncomprsize,overallcomprsize);

         dataptr+=item->usercompressor->GetUserDataSize();
         item=item->next;
      }
   }

};



//**********************************************************************************
//**********************************************************************************

// The decompressor for the separated Or-compressor



class OrSepUncompressor : public UserUncompressor
{
   friend OrSepCompressorFactory;
   OrSepCompressorInfo  info;

   void ComputeProperties()
      // Computes the properties. Determines the number of containers
      // and the size of the user data
   {
      datasize=0;
      contnum=1;  // We need one container for ourselves

      OrCompressorItem *item=info.subcompressors;
      while(item!=NULL)
      {
         contnum+=item->useruncompressor->GetUserContNum();
         datasize+=item->useruncompressor->GetUserDataSize();
         item=item->next;
      }
   }

   void InitUncompress(UncompressContainer *cont,char *dataptr)
      // Initialized the decompressor
      // Each subcompressor is called with the corresponding containers and state
   {
      OrCompressorItem *item=info.subcompressors;

      cont++;

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
      // We load the tag that determined which sub-compressor was used.
      unsigned long subcompressidx=cont->LoadUInt32();
      OrCompressorItem  *item=info.subcompressors;

      cont++;

      // We skip over the containers and state spaces of all previous
      // containers
      while(subcompressidx--)
      {
         cont+=item->useruncompressor->GetUserContNum();
         dataptr+=item->useruncompressor->GetUserDataSize();
         item=item->next;
      }
      // Then we do the decompression
      item->useruncompressor->UncompressItem(cont,dataptr,output);
   }
};



//**********************************************************************************
//**********************************************************************************

// The Compressor Factory for the separate Or-compressor
class OrSepCompressorFactory : public UserCompressorFactory
{
public:
   char *GetName()         {  return "or"; }
   char *GetDescription()  {  return "Variant compressor"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
      // Instantiates the Or-compressor for specific parameters
   {
      if(paramstr==NULL)
      {
         Error("Division compressor 'divsep' requires a sequence of strings and compressors as parameters");
         Exit();
      }

      // Let's create the Or-compressor and parse the parameters
      OrSepCompressor  *orsepcompressor=new OrSepCompressor();

      orsepcompressor->info.CreateSubCompressors(paramstr,len);
      orsepcompressor->ComputeProperties();

      return orsepcompressor;
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
      // Instantiates the Or-decompressor for specific parameters
   {
      // Let's create the Or-decompressor and parse the parameters
      // Note that the parameters *must* be correct.
      OrSepUncompressor  *orsepuncompressor=new OrSepUncompressor();

      orsepuncompressor->info.CreateSubCompressors(paramstr,len);
      orsepuncompressor->ComputeProperties();

      return orsepuncompressor;
   }

};

// The actual Or-compressor factory
OrSepCompressorFactory  orsepcompressfactory;

//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
