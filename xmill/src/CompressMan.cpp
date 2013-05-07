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

// This module contains the compressor manager, which manages the set
// of compressor factories and the (de)compressor instantiation
// process

#include "CompressMan.hpp"


// The global user-compressor manager
CompressMan compressman;


//***********************************************************************

inline void CompressMan::AddCompressFactory(UserCompressorFactory *compressor)
   // Adds a new user compressor factory
{
   // We need to be a little careful here
   // It is possible that the AddCompressFactory is called *before*
   // the constructore CompressMan::CompressMan is called
   // Therefore, we do some potential initalization of lastref,
   // if nexessary:
   if(lastref==NULL)
      lastref=&compressorlist;

   *lastref=compressor;
   lastref=&(compressor->next);
   *lastref=NULL;
}

UserCompressorFactory::UserCompressorFactory()
   // The default constructor for adding a new user-compressor factory
{
   compressman.AddCompressFactory(this);
}

//***********************************************************************
//***********************************************************************

char *FindEndOfParamString(char *str,char *endptr)
   // This function analyses the parameter string between 'str' and 'endptr' and finds the
   // ending ')' of the parameter string. most importantly, '(' and ')' can occur
   // in a nested fashion - thus we keep a parenthesis count.
{
   int   parenthesiscount=0;
   char  *startptr=str;
   char  *saveptr;

   while(str<endptr)
   {
      switch(*str)
      {
      case '"':
         saveptr=str;
         str++;
         while(str<endptr)
         {
            if(*str=='\\')
            {
               str++;
               continue;
            }
            if(*str=='"')
               break;
            str++;
         }
         if(str==endptr)
         {
            Error("Could not find closing '\"' in string '");
            ErrorCont(saveptr,endptr-saveptr);
            ErrorCont("'");
            Exit();
         }
         break;

      case '(':
         parenthesiscount++;
         break;

      case ')':
         if(parenthesiscount==0) // We found the closing bracket ?
            return str;
         parenthesiscount--;
         break;
      }
      str++;
   }
   Error("Could not find closing parenthesis for parameter '");
   ErrorCont(startptr,endptr-startptr);
   ErrorCont("'");
   Exit();
   return NULL;
}

// We need to keep an external reference to the constant compressor factory,
// so that we can access it directly.
extern UserCompressorFactory *constantcompressfactoryptr;

//**********************************************************************



UserCompressor *CompressMan::CreateCompressorInstance(char * &compressorname,char *endptr)
   // Finds a specific user compressor factory that is specified by
   // 'compressorname' (the length is given by 'endptr-compressorname')
   // The compressor name might have parameters and after the operation,
   // the 'compressorname' is set to the next character following the
   // compressor name.
{
   char  *ptr1=compressorname,
         *ptr2;

   if(*compressorname=='"')
      // Do we have a string ??
   {
      ptr2=ParseString(ptr1,endptr);

      compressorname=ptr2+1;

      return constantcompressfactoryptr->InstantiateCompressor(ptr1,ptr2-ptr1+1);
   }

   // First, let's find the end of the compressor name
   while(ptr1<endptr)
   {
      if((*ptr1=='(')||(*ptr1==')')||(*ptr1==',')||
         (*ptr1==' ')||(*ptr1=='\n')||(*ptr1=='\r')||(*ptr1=='\t'))
         break;
      ptr1++;
   }

   UserCompressorFactory *compressor=FindCompressorFactory(compressorname,ptr1-compressorname);

   if(compressor==NULL)
   {
      Error("Compressor '");
      ErrorCont(compressorname,ptr1-compressorname);
      ErrorCont("' is not defined");
      Exit();
   }

   // Do we have a parameter coming afterwards ?
   if(*ptr1=='(')
   {
      // Let's find the end of the parameter string
      ptr2=FindEndOfParamString(ptr1+1,endptr);

      compressorname=ptr2+1;

      return compressor->InstantiateCompressor(ptr1+1,ptr2-1-ptr1);
   }
   else
   {
      compressorname=ptr1;
      return compressor->InstantiateCompressor(NULL,0);
   }
}


//**********************************************************************


UserUncompressor *CompressMan::CreateUncompressorInstance(char * &compressorname,char *endptr)
   // Finds a specific user decompressor factory that is specified by
   // 'compressorname' (the length is given by 'endptr-compressorname')
   // The compressor name might have parameters and after the operation,
   // the 'compressorname' is set to the next character following the
   // compressor name.
{
   char  *ptr1=compressorname,
         *ptr2;

   if(*compressorname=='"')
      // Do we have a string ? ==> We must use the constant compressor
   {
      // Let's find the end of the string
      ptr2=ParseString(ptr1,endptr);

      compressorname=ptr2+1;

      return constantcompressfactoryptr->InstantiateUncompressor(ptr1,ptr2-ptr1+1);
   }

   // First, let's find the end of the compressor name
   while(ptr1<endptr)
   {
      if((*ptr1=='(')||(*ptr1==')')||(*ptr1==',')||
         (*ptr1==' ')||(*ptr1=='\n')||(*ptr1=='\r')||(*ptr1=='\t'))
         break;
      ptr1++;
   }

   // Let's find the compressor factory
   UserCompressorFactory *compressor=FindCompressorFactory(compressorname,ptr1-compressorname);

   if(compressor==NULL)
   {
      Error("Compressor '");
      ErrorCont(compressorname,ptr1-compressorname);
      ErrorCont("' is not defined");
      Exit();
   }

   // Do we have a parameter coming afterwards ?
   if(*ptr1=='(')
   {
      ptr2=FindEndOfParamString(ptr1+1,endptr);
      
      compressorname=ptr2+1;

      return compressor->InstantiateUncompressor(ptr1+1,ptr2-1-ptr1);
   }
   else
   {
      compressorname=ptr1;
      return compressor->InstantiateUncompressor(NULL,0);
   }
}


UserCompressorFactory *CompressMan::FindCompressorFactory(char *name,int len)
   // Finds a certain user compressor factory with a given name
   // If it doesn't find such a compressor, we exit
{
   UserCompressorFactory   *compressor=compressorlist;
   int                     namelen;

   while(compressor!=NULL)
   {
      namelen=strlen(compressor->GetName());
      if((namelen==len)&&
         (memcmp(compressor->GetName(),name,len)==0))
         return compressor;
      compressor=compressor->next;
   }
   Error("Could not find compressor '");
   ErrorCont(name,len);
   ErrorCont("'!");
   Exit();
   return NULL;
}

//**********************************************************************



void CompressMan::PrintCompressorInfo()
   // Prints information about the compressors
{
   UserCompressorFactory   *compressor=compressorlist;
   int                     maxnamelen=0,namelen;

   // First, let's determine the longest compressor name
   while(compressor!=NULL)
   {
      namelen=strlen(compressor->GetName());
      if(maxnamelen<namelen)
         maxnamelen=namelen;

      compressor=compressor->next;
   }

   // Now we print each compressor name and its description

   compressor=compressorlist;

   while(compressor!=NULL)
   {
      printf("    ");
      printf(compressor->GetName());
      namelen=strlen(compressor->GetName());

      while(namelen<maxnamelen)
      {
         printf(" ");
         namelen++;
      }
      printf("  - ");
      printf(compressor->GetDescription());
      printf("\n");

      compressor=compressor->next;
   }
}

//**********************************************************************
//**********************************************************************

unsigned long CompressMan::GetDataSize()
   // Each compressor is allowed to store own data
   // This function determines how much space is needed by all user compressors
{
   UserCompressorFactory   *compressor=compressorlist;
   unsigned long           size=0;

   while(compressor!=NULL)
   {
      size+=compressor->GetGlobalDataSize();
      compressor=compressor->next;
   }
   return size;
}

void CompressMan::CompressSmallGlobalData(Compressor *compress)
   // Stores own data (only the small pieces!) of the user compressor factories
   // in the output stream described by 'compressor'.
{
   UserCompressorFactory *compressor=compressorlist;

   while(compressor!=NULL)
   {
      compressor->CompressSmallGlobalData(compress);
      compressor=compressor->next;
   }
}

void CompressMan::CompressLargeGlobalData(Output *output)
   // Stores own data (only the large pieces!) of the user compressor factories
   // in the output stream described by 'compressor'.
{
   UserCompressorFactory *compressor=compressorlist;

   while(compressor!=NULL)
   {
      compressor->CompressLargeGlobalData(output);
      compressor=compressor->next;
   }
}



//**********************************************************************



void CompressMan::UncompressSmallGlobalData(SmallBlockUncompressor *uncompressor)
   // Reads the own (small) data from the source described by 'uncompressor' and
   // initializes the user compressor factories.
{
   UserCompressorFactory *compressor=compressorlist;

   // Simply iterates over all user compressor factories and
   // invokes 'UncompressSmallGlobalData'.
   while(compressor!=NULL)
   {
      compressor->UncompressSmallGlobalData(uncompressor);
      compressor=compressor->next;
   }
}

void CompressMan::UncompressLargeGlobalData(Input *input)
   // Reads the own (large) data from the source described by 'uncompressor' and
   // initializes the user compressor factories.
{
   UserCompressorFactory *compressor=compressorlist;

   // Simply iterates over all user compressor factories and
   // invokes 'UncompressLargeGlobalData'.

   while(compressor!=NULL)
   {
      compressor->UncompressLargeGlobalData(input);
      compressor=compressor->next;
   }
}

void CompressMan::FinishUncompress()
   // This is called after all user compressor data has been decompressed
{
   UserCompressorFactory *compressor=compressorlist;

   // Simply iterates over all user compressor factories and
   // invokes 'FinishUncompress'.
   while(compressor!=NULL)
   {
      compressor->FinishUncompress();
      compressor=compressor->next;
   }
}

void CompressMan::DebugPrint()
{
   UserCompressorFactory *compressor=compressorlist;
   while(compressor!=NULL)
   {
      printf("%lu =>",compressor);
      printf("%s\n",compressor->GetName());
      compressor=compressor->next;
   }
}


