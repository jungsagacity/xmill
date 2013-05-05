/*
 * Copyright 1999 AT&T Labs -- Research
 * By receiving and using the code that follows, the user acknowledges
 * and agrees to keep the code confidential and use the code solely for
 * non-commercial research. The user further acknowledges and agrees that
 * the  code is not an official product of AT&T Corp., and is provided
 * on an "as is"  basis with no warranty, either express or implied, including
 * but not limited to implied warranties of merchantability and fitness
 * for a particular purpose, and any warranties of noninfringement.
 * The user shall use reasonable efforts to provide AT&T Laboratories
 * with reports of any errors, defects or suggestions for change and
 * improvements to the code.
 */


#ifndef XMLOUTPUT_HPP
#define XMLOUTPUT_HPP

#include "Output.hpp"
/*
#define XMLOUTPUT_DEFAULT              0
#define XMLOUTPUT_LABELSTARTED         1
#define XMLOUTPUT_ATTRIBNAMESTARTED    2
#define XMLOUTPUT_ATTRIBVALUESTARTED   3
#define XMLOUTPUT_TEXTSTARTED          4
#define XMLOUTPUT_INITIALIZED          5
*/

#define XMLOUTPUT_INIT           0
#define XMLOUTPUT_OPENLABEL      1
#define XMLOUTPUT_OPENATTRIB     2
#define XMLOUTPUT_AFTERDATA      3
#define XMLOUTPUT_AFTERENDLABEL  4

#define XMLINTENT_NONE        0
#define XMLINTENT_WRAP        1
#define XMLINTENT_SPACES      3
#define XMLINTENT_TABS        4

class XMLOutput : public Output
{
   OUTPUT_STATIC int curcol,coldelta;

   OUTPUT_STATIC struct ABC {
   unsigned char  status:3;
   unsigned char  content:2;
   unsigned char  isinattrib:1;
   unsigned char  intentation:5;
   unsigned char  valuespacing:1;
   unsigned char  attribwhitespace:1;
   } x;

   OUTPUT_STATIC void GotoNextLine(char moveright)
   {
      switch(x.intentation)
      {
      case XMLINTENT_SPACES:
      case XMLINTENT_TABS:
      {
         StoreNewline();

         if(moveright==0)
            curcol-=coldelta;

         if(x.intentation==XMLINTENT_SPACES)
            mymemset(GetDataPtr(curcol),' ',curcol);
         else
            mymemset(GetDataPtr(curcol),'\t',curcol);

         if(moveright)
            curcol+=coldelta;
         break;
      }
/*
      case XMLINTENT_WRAP:
         if(GetColPos()>=coldelta)
            StoreNewline();
*/
      }
   }

public:

   void OUTPUT_STATIC Init(unsigned char myintentation,unsigned char myvaluespacing=0,unsigned char mycoldelta=1)
   {
      curcol=0;
      coldelta=mycoldelta;
      x.intentation=myintentation;

      x.status=XMLOUTPUT_INIT;
      x.isinattrib=0;

      x.valuespacing=myvaluespacing;
   }

   XMLOutput()
   {
      Init(XMLINTENT_NONE);
   }

//***********************************************************************

#ifndef XDEMILL_NOOUTPUT

   void OUTPUT_STATIC startElement(char *str,int len)
   {
      switch(x.status)
      {
      case XMLOUTPUT_OPENLABEL:
         StoreChar('>');
         GotoNextLine(1);
         break;

      case XMLOUTPUT_OPENATTRIB:
         Error("Cannot start element within attribute!");
         Exit();

      case XMLOUTPUT_AFTERENDLABEL:
      case XMLOUTPUT_AFTERDATA:
         GotoNextLine(1);
         break;

      case XMLOUTPUT_INIT:
         curcol+=coldelta;
         break;
      }
      StoreChar('<');

      x.status=XMLOUTPUT_OPENLABEL;
      StoreData(str,len);

      x.attribwhitespace=0;
   }

   void OUTPUT_STATIC endElement(char *str,int len)
   {
      char *ptr;
      switch(x.status)
      {
      case XMLOUTPUT_OPENLABEL:
         ptr=GetDataPtr(len+4);
         *(ptr++)='>';
         *(ptr++)='<';
         *(ptr++)='/';
         mymemcpy(ptr,str,len);
         ptr+=len;
         *(ptr++)='>';
         curcol-=coldelta;
         x.status=XMLOUTPUT_AFTERENDLABEL;
         return;

      case XMLOUTPUT_AFTERENDLABEL:
         GotoNextLine(0);
         break;

      case XMLOUTPUT_AFTERDATA:
         curcol-=coldelta;
         break;

      default:
         Error("Invalid end tag");
         Exit();
      }

      ptr=GetDataPtr(len+3);

      *(ptr++)='<';
      *(ptr++)='/';
      mymemcpy(ptr,str,len);
      ptr+=len;
      *(ptr++)='>';
      
      x.status=XMLOUTPUT_AFTERENDLABEL;
   }

   void OUTPUT_STATIC endEmptyElement()
   {
      char *ptr;
      if(x.status!=XMLOUTPUT_OPENLABEL)
         // Something is wrong !!
         return;

      ptr=GetDataPtr(2);
      *(ptr++)='/';
      *(ptr++)='>';

      x.status=XMLOUTPUT_AFTERENDLABEL;
      curcol-=coldelta;
   }

//***********************

   void OUTPUT_STATIC startAttribute(char *str,int len)
   {
      register char *ptr;

      if(x.status==XMLOUTPUT_OPENATTRIB)
      {
         ptr=GetDataPtr(len+(x.attribwhitespace ? 3 : 4));
         *(ptr++)='"';
      }
      else
      {
         if(x.status!=XMLOUTPUT_OPENLABEL)
         {
            Error("Cannot print attribute outside of start element!");
            Exit();
         }
         ptr=GetDataPtr(len+(x.attribwhitespace ? 2 : 3));
      }
      if(x.attribwhitespace==0)
      {
         *(ptr++)=' ';
         x.attribwhitespace=0;
      }

      mymemcpy(ptr,str,len);
      ptr+=len;
      *(ptr++)='=';
      *(ptr++)='"';

      x.status=XMLOUTPUT_OPENATTRIB;
   }

   void OUTPUT_STATIC endAttribute(char *str=NULL,int len=0)
   {
      if(x.status!=XMLOUTPUT_OPENATTRIB)
      {
         Error("Could not finish attribute outside of start element!");
         Exit();
      }
      StoreChar('"');
      x.status=XMLOUTPUT_OPENLABEL;
   }

   void OUTPUT_STATIC characters(char *str,int len)
   {
      switch(x.status)
      {
      case XMLOUTPUT_OPENATTRIB:
         StoreData(str,len);
         return;

      case XMLOUTPUT_OPENLABEL:
         StoreChar('>');

      case XMLOUTPUT_AFTERDATA:
      case XMLOUTPUT_AFTERENDLABEL:
      case XMLOUTPUT_INIT:
         StoreData(str,len);
      }
      x.status=XMLOUTPUT_AFTERDATA;
   }

   void OUTPUT_STATIC whitespaces(char *str,int len)
   {
      characters(str,len);
   }

   void OUTPUT_STATIC attribWhitespaces(char *str,int len)
   {
      char *ptr=GetDataPtr(len);
      mymemcpy(ptr,str,len);
      x.attribwhitespace=1;
   }

#else
   void OUTPUT_STATIC startElement(char *str,int len);// {}
   void OUTPUT_STATIC endElement(char *str,int len);// {}
   void OUTPUT_STATIC endEmptyElement();  // {}
   void OUTPUT_STATIC startAttribute(char *str,int len);// {}
   void OUTPUT_STATIC endAttribute(char *str=NULL,int len=0);// {}
   void OUTPUT_STATIC characters(char *str,int len);// {}
   void OUTPUT_STATIC whitespaces(char *str,int len);// {}
   void OUTPUT_STATIC attribWhitespaces(char *str,int len); // {}
#endif

};

#endif
