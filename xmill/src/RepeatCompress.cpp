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

// This module contains the repeat compressor 'rep'

// We implemented the 'separate' repeat compressor:
// The count is stored in a different container separate from
// the data stored by the subcompressor

// Note that the subcompressor is *always* required to be a non-restricting -
// i.e. the subcompressor must always accept the strings.

#include "CompressMan.hpp"

#ifdef XDEMILL
#include "UnCompCont.hpp"
#endif

class RepeatSepCompressorFactory;

// The structure for representing the the subcompressor. There is exactly
// one repeat subcompressor and one optional tail compressor.
struct RepeatSepCompressorInfo
{
   union{
#ifdef XMILL
      UserCompressor       *subcompressor;
#endif
#ifdef XDEMILL
      UserUncompressor     *subuncompressor;
#endif
   };
   union{
#ifdef XMILL
      UserCompressor       *subcompressor2;
#endif
#ifdef XDEMILL
      UserUncompressor     *subuncompressor2;
#endif
   };

   // The delimiter
   char                 *delimiter;
   unsigned long        delimiterlen;

   void ScanParamString(char *paramstr,int len)
      // Parses a parameter string
   {
      char  *endptr=paramstr+len,*endstringptr;

      // Skip all white spaces
      while(paramstr<endptr)
      {
         if((*paramstr!=' ')&&(*paramstr!=',')&&(*paramstr!='\t')&&
            (*paramstr!='\r')&&(*paramstr!='\n'))
            break;
         paramstr++;
      }

      // Parse the delimiter string
      endstringptr=::ParseString(paramstr,endptr);
      if(endstringptr==NULL)
      {
         Error("First parameter in rep(");
         ErrorCont(paramstr,len);
         ErrorCont(") should be a string!");
         Exit();
      }

      // Store the delimiter string
      delimiter=paramstr+1;
      delimiterlen=endstringptr-delimiter;

      // Skip white spaces
      endstringptr++;

      while(endstringptr<endptr)
      {
         if((*endstringptr!=' ')&&(*endstringptr!=',')&&(*endstringptr!='\t')&&
            (*endstringptr!='\r')&&(*endstringptr!='\n'))
            break;
         endstringptr++;
      }

      // Next, there must be the compressor as a parameter
#ifdef XMILL
      subcompressor2=NULL;
      subcompressor=compressman.CreateCompressorInstance(endstringptr,endptr);

      if(subcompressor->IsRejecting())
      {
         Error("Compressor '");
         ErrorCont(endstringptr,endptr-endstringptr);
         ErrorCont("' must always accept the string!\n");
         Exit();
      }
#endif
#ifdef XDEMILL
      subuncompressor2=NULL;
      subuncompressor=compressman.CreateUncompressorInstance(endstringptr,endptr);
#endif
      // Skip white spaces
      while(endstringptr<endptr)
      {
         if((*endstringptr!=' ')&&(*endstringptr!=',')&&(*endstringptr!='\t')&&
            (*endstringptr!='\r')&&(*endstringptr!='\n'))
            break;
         endstringptr++;
      }

      // No additional tail compressor?
      if(*endstringptr==')')
         return;

      // Otherwise find the tail compressor
#ifdef XMILL
      subcompressor2=compressman.CreateCompressorInstance(endstringptr,endptr);
#endif
#ifdef XDEMILL
      subuncompressor2=compressman.CreateUncompressorInstance(endstringptr,endptr);
#endif

      // Skip white spaces
      while(endstringptr<endptr)
      {
         if((*endstringptr!=' ')&&(*endstringptr!=',')&&(*endstringptr!='\t')&&
            (*endstringptr!='\r')&&(*endstringptr!='\n'))
            break;
         endstringptr++;
      }

      // Last character must be ')'
      if(*endstringptr!=')')
      {
         Error("Missing closed parenthesis in '");
         ErrorCont(paramstr,len);
         Exit();
      }
   }
};

//********************************************************************************

// The repeat compressor compressor

#ifdef XMILL

class RepeatSepCompressor : public UserCompressor
{
   friend RepeatSepCompressorFactory;
protected:

   RepeatSepCompressorInfo info;
   unsigned long           curcount;   // The number strings already parsed

public:
   void ComputeProperties()
      // Determines the properties of the compressor
      // Also determines the number of containers and the size of the user data
   {
      isrejecting=0;canoverlap=1;
      isfixedlen=0;
      contnum=1+info.subcompressor->GetUserContNum();
      datasize=info.subcompressor->GetUserDataSize();

      // If there is a tail compressor, then the compressor can
      // be rejecting
      if(info.subcompressor2!=NULL)
      {
         isrejecting=info.subcompressor2->IsRejecting();
         contnum+=info.subcompressor2->GetUserContNum();
         datasize+=info.subcompressor2->GetUserDataSize();
      }
   }

   void InitCompress(CompressContainer *cont,char *dataptr)
      // Initializes the subcompressors
   {
      info.subcompressor->InitCompress(cont+1,dataptr);

      if(info.subcompressor2!=NULL)
         info.subcompressor2->InitCompress(
            cont+1+info.subcompressor->GetUserContNum(),
            dataptr+info.subcompressor->GetUserDataSize());
   }

   char ParseString(char *str,unsigned len,char *dataptr)
      // Parses the string and returns 1, if accepted, otherwise 0.
      // ParseString is *only* called if there is a tail compressor!
      // Only in this case, the compressor is rejecting.

      // This function does not actually store/compress the string
      // But it can keep an internal state - in the next step,
      // CompressString is called.
   {
      if(info.subcompressor2==NULL)
         return 1;

      char     *delimptr,*curptr,*endptr,*saveptr;
      unsigned i;
      endptr=str+len;

      do
      {
         curptr=str;

         // Let's look for the delimiter
         while(curptr<endptr-info.delimiterlen)
         {
            saveptr=curptr;

            // Check whether the string at 'curptr' is the delimiter string
            delimptr=info.delimiter;
            for(i=0;i<info.delimiterlen;i++)
            {
               if(*curptr!=*delimptr)
                  break;
               curptr++;
               delimptr++;
            }
            // We found the separator ?
            if(i==info.delimiterlen)
               break;

            // We didn't find the separator
            // Go to next position
            curptr=saveptr+1;
         }

         // There is not enough space for another delimiter?
         // ==> We use the 
         if(curptr>=endptr-info.delimiterlen)
            return info.subcompressor2->ParseString(str,endptr-str,
                     dataptr+info.subcompressor->GetUserDataSize());

         str=curptr;
      }
      while(1);
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      char  *delimptr,*curptr,*endptr;
      unsigned i;

      endptr=str+len;

      curcount=0;

      do
      {
         curptr=str;

         while(curptr<endptr-info.delimiterlen)
         {
            delimptr=info.delimiter;
            for(i=0;i<info.delimiterlen;i++)
            {
               if(*curptr!=*delimptr)
                  break;
               curptr++;
               delimptr++;
            }
            // We found the separator ?
            if(i==info.delimiterlen)
               break;

            curptr++;
         }
         curcount++;

         if(curptr>=endptr-info.delimiterlen)
         {
            curptr=endptr-info.delimiterlen;
            if(info.subcompressor2!=NULL)
               info.subcompressor2->CompressString(str,endptr-str,
                  cont+1+info.subcompressor->GetUserContNum(),
                  dataptr+info.subcompressor->GetUserDataSize());
            else
               info.subcompressor->CompressString(str,endptr-str,cont+1,dataptr);
         }
         else
            info.subcompressor->CompressString(str,curptr-str-info.delimiterlen,cont+1,dataptr);

         str=curptr;
      }
      while(curptr<endptr-info.delimiterlen);

      cont->StoreUInt32(curcount-1);   // 'curcount' is at least 1 !!
   }

   void FinishCompress(CompressContainer *cont,char *dataptr)
   {
      info.subcompressor->FinishCompress(cont+1,dataptr);

      if(info.subcompressor2!=NULL)
         info.subcompressor2->FinishCompress(
            cont+1+info.subcompressor->GetUserContNum(),
            dataptr+info.subcompressor->GetUserDataSize());
   }

   void PrintCompressInfo(char *dataptr,unsigned long *overalluncomprsize,unsigned long *overallcomprsize)
   {
      info.subcompressor->PrintCompressInfo(dataptr,overalluncomprsize,overallcomprsize);

      if(info.subcompressor2!=NULL)
         info.subcompressor2->PrintCompressInfo(
            dataptr+info.subcompressor->GetUserDataSize(),
            overalluncomprsize,overallcomprsize);
   }
};
#endif

#ifdef XDEMILL

class RepeatSepUncompressor : public UserUncompressor
{
   friend RepeatSepCompressorFactory;

   RepeatSepCompressorInfo info;
public:

   void ComputeProperties()
   {
      contnum=1+info.subuncompressor->GetUserContNum();
      datasize=info.subuncompressor->GetUserDataSize();

      if(info.subuncompressor2!=NULL)
      {
         contnum+=info.subuncompressor2->GetUserContNum();
         datasize+=info.subuncompressor2->GetUserDataSize();
      }
   }

   void InitUncompress(UncompressContainer *cont,char *dataptr)
   {
      info.subuncompressor->InitUncompress(cont,dataptr);

      if(info.subuncompressor2!=NULL)
         info.subuncompressor2->InitUncompress(
            cont+1+info.subuncompressor->GetUserContNum(),
            dataptr+info.subuncompressor->GetUserDataSize());
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      unsigned long count=cont->LoadUInt32();

      while(count>0)
      {
         info.subuncompressor->UncompressItem(cont+1,dataptr,output);

         output->characters(info.delimiter,info.delimiterlen);
         count--;
      }

      if(info.subuncompressor2!=NULL)
         info.subuncompressor2->UncompressItem(
            cont+1+info.subuncompressor->GetUserContNum(),
            dataptr+info.subuncompressor->GetUserDataSize(),output);
      else
         info.subuncompressor->UncompressItem(cont+1,dataptr,output);
   }
};

#endif

class RepeatSepCompressorFactory : public UserCompressorFactory
{
public:
   char *GetName()         {  return "rep"; }
   char *GetDescription()  {  return "Compressor for substrings separated by some delimiter string"; }

#ifdef XMILL
   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      if(paramstr==NULL)
      {
         Error("Division compressor 'seq' requires a sequence of strings and compressors as parameters");
         Exit();
      }

      RepeatSepCompressor  *repeatcompressor=new RepeatSepCompressor();

      repeatcompressor->info.ScanParamString(paramstr,len);
      repeatcompressor->ComputeProperties();

      return repeatcompressor;
   }
#endif

#ifdef XDEMILL
   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      RepeatSepUncompressor  *repeatuncompressor=new RepeatSepUncompressor();

      repeatuncompressor->info.ScanParamString(paramstr,len);
      repeatuncompressor->ComputeProperties();

      return repeatuncompressor;
   }
#endif
};

RepeatSepCompressorFactory  repeatsepcompressfactory;
