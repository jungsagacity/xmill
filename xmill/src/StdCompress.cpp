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

//********************************************************************
//********************************************************************

// This module contains the standard user compressors, such as 'u', 'u8', 'di', ...

#include "CompressMan.hpp"

//********************************************************************

// For decompression, we have a couple of utility functions



#include "UnCompCont.hpp"

inline char *IntToStr(long val)
   // Convers an integer to a string and returns a pointer to the string
{
static char  tmpstr[20];

   char *ptr=tmpstr+19; // We start from the back of the string
   *ptr=0;

   if(val<0)
   {
      val=-val;
      do
      {
         ptr--;
         *ptr=(val%10)+'0';
         val=val/10;
      }
      while(val>0);

      ptr--;
      *ptr='-';
      return ptr;
   }
   else
   {
      do
      {
         ptr--;
         *ptr=(val%10)+'0';
         val=val/10;
      }
      while(val>0);

      return ptr;
   }
}

inline void PrintZeros(XMLOutput *output,unsigned long zeronum)
   // Prints a sequence of zero's
{
   static char zerostr[]="0000000000";

   while(zeronum>10)
   {
      output->characters(zerostr,10);
      zeronum-=10;
   }
   output->characters(zerostr,zeronum);
}

inline void PrintInteger(unsigned long val,char isneg,unsigned mindigits,XMLOutput *output)
{
   char *ptr=IntToStr(val);
   unsigned len=strlen(ptr);

   if(isneg)
   {
      output->characters("-",1);
      if(mindigits>len+1)
         PrintZeros(output,mindigits-len-1);
   }
   else
   {
      if(mindigits>len)
         PrintZeros(output,mindigits-len);
   }
   output->characters(ptr,len);
}



//***************************************************************************

// The simple text compressor 't'


class PlainTextCompressor : public UserCompressor
{
public:
   PlainTextCompressor()
   {
      datasize=0;contnum=1;isrejecting=0;canoverlap=1;isfixedlen=0;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      cont->StoreUInt32(len);
      cont->StoreData(str,len);
   }
};

class PlainTextUncompressor : public UserUncompressor
{
public:
   PlainTextUncompressor()
   {
      datasize=0;contnum=1;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      unsigned long len=cont->LoadUInt32();

      output->characters((char *)cont->GetDataPtr(len),len);
   }
};


class PlainTextCompressorFactory : public UserCompressorFactory
{

   PlainTextUncompressor   uncompressor;

public:
   char *GetName()         {  return "t"; }
   char *GetDescription()  {  return "Plain text compressor"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      if(paramstr!=NULL)
      {
         Error("Plain text compressor 't' should not have any arguments ('");
         ErrorCont(paramstr,len);
         Error("')");
         Exit();
      }
      return new PlainTextCompressor();
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      return &uncompressor;
   }

};

//***************************************************************************
//***************************************************************************

// The simple print compressor 'p'


class PrintCompressor : public UserCompressor
{
public:
   PrintCompressor()
   {
      datasize=0;contnum=0;isrejecting=1;canoverlap=1;isfixedlen=0;
   }

   char ParseString(char *str,unsigned len,char *dataptr)
   {
      printf("Could not compress '");
      fwrite(str,len,1,stdout);
      printf("'...\n");
      return 0;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
   }
};

class PrintUncompressor : public UserUncompressor
{
public:
   PrintUncompressor()
   {
      datasize=0;contnum=0;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
   }
};


class PrintCompressorFactory : public UserCompressorFactory
{

   PrintCompressor   compressor;



   PrintUncompressor uncompressor;

public:
   char *GetName()         {  return "p"; }
   char *GetDescription()  {  return "Print compressor"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      return &compressor;
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      return &uncompressor;
   }

};

PrintCompressorFactory printcompressor;

//***************************************************************************
//***************************************************************************


char ParseUnsignedInt(char *str,int len,unsigned long *val)
   // Parses a given string as a unsigned integer
   // Returns 1 if successful, otherwise 0.
{
   *val=0;

   if(len==0)
      return 0;

   do
   {
      if((*val)>=107374180L)
         return 0;

      (*val)*=10;

      if((*str<'0')||(*str>'9'))
         return 0;

      (*val)+=(unsigned)(*str-'0');
      str++;
      len--;
   }
   while(len>0);

   return 1;
}

inline char ParseSignedInt(char *str,int len,unsigned long *val,unsigned char *neg)
   // Parses a given string as a signed integer
   // Returns 1 if successful, otherwise 0.
{
   if(len==0)
      return 0;

   *val=0;

   if(*str=='-')
   {
      *neg=1;
      str++;
      len--;
      if(len==0)
         return 0;
   }
   else
      *neg=0;

   do
   {
      if(*val>=536870000L)
         return 0;

      (*val)*=10;

      if((*str<'0')||(*str>'9'))
         return 0;

      (*val)+=(unsigned)(*str-'0');
      str++;
      len--;
   }
   while(len>0);

   return 1;
}

inline char ParseSignedInt(char *str,int len,long *val)
   // Parses a given string as a signed integer
   // Returns 1 if successful, otherwise 0.
{
   unsigned char isneg;
   if(ParseSignedInt(str,len,(unsigned long *)val,&isneg)==0)
      return 0;

   if(isneg)
      *val=0xFFFFFFFFL-*val;
   return 1;
}


//*****************************************************************************
//*****************************************************************************

// The standard integer user compressor 'u'


class UnsignedIntCompressor : public UserCompressor
{
   unsigned long val;
   unsigned long mindigits;
public:

   UnsignedIntCompressor(unsigned long mymindigits)
   {
      mindigits=mymindigits;
      datasize=0;contnum=1;isrejecting=1;canoverlap=1;isfixedlen=0;
   }

// Compression functions

   char ParseString(char *str,unsigned len,char *dataptr)
      // We keep the parsed value in 'val'
   {
      return ParseUnsignedInt(str,len,&val);
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      cont->StoreUInt32(val);
   }
};

class UnsignedIntUncompressor : public UserUncompressor
{
   unsigned long mindigits;

public:

   UnsignedIntUncompressor(unsigned long mymindigits=0)
   {
      mindigits=mymindigits;
      datasize=0;contnum=1;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      PrintInteger(cont->LoadUInt32(),0,mindigits,output);
   }
};


class UnsignedIntCompressorFactory : public UserCompressorFactory
{

   UnsignedIntUncompressor uncompressor;

public:
   char *GetName()         {  return "u"; }
   char *GetDescription()  {  return "Compressor for unsigned integers"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         if(mindigits<=0)
         {
            Error("Invalid parameter '");
            ErrorCont(paramstr,len);
            ErrorCont("' for compressor 'u8'!");
            Exit();
         }
      }
      return new UnsignedIntCompressor(mindigits);
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         if(mindigits<=0)
         {
            Error("Invalid parameter '");
            ErrorCont(paramstr,len);
            ErrorCont("' for compressor 'u'!");
            Exit();
         }
         return new UnsignedIntUncompressor(mindigits);
      }
      else
         return &uncompressor;
   }

};

//*****************************************************************************
//*****************************************************************************

// The standard byte integer compressor 'u8'


class UnsignedInt8Compressor : public UserCompressor
{
   unsigned long val;
   unsigned long mindigits;
public:
   UnsignedInt8Compressor(unsigned long mymindigits)
   {
      mindigits=mymindigits;
      datasize=0;contnum=1;isrejecting=1;canoverlap=1;isfixedlen=0;
   }

// Compression functions

   char ParseString(char *str,unsigned len,char *dataptr)
      // We keep the parsed value in 'val'
   {
      if(ParseUnsignedInt(str,len,&val)==0)
         return 0;

      return val<256;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      cont->StoreChar((unsigned char)val);
   }
};

class UnsignedInt8Uncompressor : public UserUncompressor
{
   unsigned long mindigits;
public:

   UnsignedInt8Uncompressor(unsigned long mymindigits=0)
   {
      mindigits=mymindigits;
      datasize=0;contnum=1;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      PrintInteger((unsigned long)(unsigned char)cont->LoadChar(),0,mindigits,output);
   }
};


class UnsignedInt8CompressorFactory : public UserCompressorFactory
{

   UnsignedInt8Uncompressor   uncompressor;


public:
   char *GetName()         {  return "u8"; }
   char *GetDescription()  {  return "Compressor for integers between 0 and 255"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         if(mindigits<=0)
         {
            Error("Invalid parameter '");
            ErrorCont(paramstr,len);
            ErrorCont("' for compressor 'u8'!");
            Exit();
         }
      }
      return new UnsignedInt8Compressor(mindigits);
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         return new UnsignedInt8Uncompressor(mindigits);
      }
      else
         return &uncompressor;
   }

};

//***************************************************************************************
//***************************************************************************************

// The standard signed integer compressor 'i'


class SignedIntCompressor : public UserCompressor
{
   unsigned long  val;
   unsigned char  isneg;
   unsigned long  mindigits;
public:
   SignedIntCompressor(unsigned long mymindigits)
   {
      mindigits=mymindigits;
      datasize=0;contnum=1;isrejecting=1;canoverlap=1;isfixedlen=0;
   }

// Compression functions

   char ParseString(char *str,unsigned len,char *dataptr)
   {
      return ParseSignedInt(str,len,&val,&isneg);
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      cont->StoreSInt32(isneg,val);
   }
};


class SignedIntUncompressor : public UserUncompressor
{
   unsigned long mindigits;
public:

   SignedIntUncompressor(unsigned long mymindigits=0)
   {
      mindigits=mymindigits;
      datasize=0;contnum=1;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      char isneg;
      unsigned long val=cont->LoadSInt32(&isneg);

      PrintInteger(val,isneg,mindigits,output);
   }
};


class SignedIntCompressorFactory : public UserCompressorFactory
{

   SignedIntUncompressor   uncompressor;

public:
   char *GetName()         {  return "i"; }
   char *GetDescription()  {  return "Compressor for signed integers"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         if(mindigits<=0)
         {
            Error("Invalid parameter '");
            ErrorCont(paramstr,len);
            ErrorCont("' for compressor 'i'!");
            Exit();
         }
      }
      return new SignedIntCompressor(mindigits);
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         return new SignedIntUncompressor(mindigits);
      }
      else
         return &uncompressor;
   }

};

//***********************************************************************
//***********************************************************************

// The delta compressor 'di'

struct DeltaCompressorState
   // The state of the delta compressor
{
   long  prevvalue;
};


class DeltaCompressor : public UserCompressor
{
   long  val;                 // The temporarily parsed value
   unsigned long mindigits;

public:

   DeltaCompressor(unsigned long mymindigits)
   {
      mindigits=mymindigits;
      datasize=sizeof(DeltaCompressorState);
      contnum=1;isrejecting=1;canoverlap=1;
   }

   void InitCompress(CompressContainer *cont,char *dataptr)
   {
      ((DeltaCompressorState *)dataptr)->prevvalue=0;
   }

   char ParseString(char *str,unsigned len,char *dataptr)
   {
      DeltaCompressorState *state=(DeltaCompressorState *)dataptr;

      if(ParseSignedInt(str,len,&val)==0)
         return 0;

      // We should check that (val-state->prevvalue) fits into a compressed integer !!
      return 1;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
      long                 dval;
      DeltaCompressorState *state=(DeltaCompressorState *)dataptr;

      dval=val-state->prevvalue;

      if(dval>=0)
         cont->StoreCompressedSInt(0,dval);
      else
         cont->StoreCompressedSInt(1,0xFFFFFFFFL-dval+1L);

      state->prevvalue=val;
   }
};


class DeltaUncompressor : public UserUncompressor
{
   unsigned long mindigits;
public:
   DeltaUncompressor(unsigned long mymindigits=0)
   {
      mindigits=mymindigits;

      datasize=sizeof(DeltaCompressorState);
      contnum=1;
   }

   void InitUncompress(UncompressContainer *cont,char *dataptr)
   {
      ((DeltaCompressorState *)dataptr)->prevvalue=0;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      DeltaCompressorState *state=(DeltaCompressorState *)dataptr;

      state->prevvalue+=cont->LoadSInt32();

      if(state->prevvalue&0x80000000L)
         PrintInteger((unsigned long)-state->prevvalue,1,mindigits,output);
      else
         PrintInteger((unsigned long) state->prevvalue,0,mindigits,output);
   }
};


class DeltaCompressorFactory : public UserCompressorFactory
{

   DeltaUncompressor uncompressor;


public:
   char *GetName()         {  return "di"; }
   char *GetDescription()  {  return "Delta compressor for signed integers"; }


   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         if(mindigits<=0)
         {
            Error("Invalid parameter '");
            ErrorCont(paramstr,len);
            ErrorCont("' for compressor 'di'!");
            Exit();
         }
      }
      return new DeltaCompressor(mindigits);
   }

   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      unsigned long mindigits=0;
      if(paramstr!=NULL)
      {
         char savechar=paramstr[len];
         paramstr[len]=0;
         mindigits=atoi(paramstr);
         paramstr[len]=savechar;
         return new DeltaUncompressor(mindigits);
      }
      else
         return &uncompressor;
   }

};

//***************************************************************************
//***************************************************************************

// The constant compressor


class ConstantCompressor : public UserCompressor
{
   char     *constantstr;     // The constant string that the parsed string must match
   unsigned constantstrlen;

public:
   char IsRejecting()               {  return 1;   }
   char CanOverlap()                {  return 1;   }
   unsigned short GetContainerNum() {  return 0;   }

// Compression functions

   ConstantCompressor(char *str,unsigned len)
   {
      datasize=0;contnum=0;isrejecting=1;canoverlap=1;isfixedlen=0;

      constantstr=str;
      constantstrlen=len;
   }

   char ParseString(char *str,unsigned len,char *dataptr)
   {
      char *ptr=constantstr;

      while(len--)
      {
         if(*ptr!=*str)
            return 0;
         ptr++;
         str++;
      }
      return 1;
   }

   void CompressString(char *str,unsigned len,CompressContainer *cont,char *dataptr)
   {
   }
};

class ConstantUncompressor : public UserUncompressor
{
   char     *constantstr;
   unsigned constantstrlen;

public:
   ConstantUncompressor(char *str,unsigned len)
   {
      datasize=0;contnum=0;

      constantstr=str;
      constantstrlen=len;
   }

   void UncompressItem(UncompressContainer *cont,char *dataptr,XMLOutput *output)
   {
      output->characters(constantstr,constantstrlen);
   }
};


class ConstantCompressorFactory : public UserCompressorFactory
{
public:
   char *GetName()         {  return "\"...\""; }
   char *GetDescription()  {  return "Constant compressor"; }

   UserCompressor *InstantiateCompressor(char *paramstr,int len)
   {
      if(paramstr==NULL)
      {
         Error("Constant text compressor 'c' requires string argument");
         Exit();
      }
      if((*paramstr!='"')||(paramstr[len-1]!='"'))
      {
         Error("Parameter '");
         ErrorCont(paramstr,len);
         ErrorCont("' is not a string");
         Exit();
      }
      return new ConstantCompressor(paramstr+1,len-2);
   }


   UserUncompressor *InstantiateUncompressor(char *paramstr,int len)
   {
      if(paramstr==NULL)
      {
         Error("Constant text compressor 'c' requires string argument");
         Exit();
      }
      if((*paramstr!='"')||(paramstr[len-1]!='"'))
      {
         Error("Parameter '");
         ErrorCont(paramstr,len);
         ErrorCont("' is not a string");
         Exit();
      }
      return new ConstantUncompressor(paramstr+1,len-2);
   }

};

//***************************************************************************
//***************************************************************************

// The actual instantiations of the compressor factories

PlainTextCompressorFactory    plaincompressor;
UnsignedIntCompressorFactory  unsignedintcompressor;
UnsignedInt8CompressorFactory unsignedint8compressor;
SignedIntCompressorFactory    signedintcompressor;
DeltaCompressorFactory        deltacompressor;
ConstantCompressorFactory     constantcompressor;
UserCompressorFactory         *constantcompressfactoryptr=&constantcompressor;
