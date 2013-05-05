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

// This module contains the XML-Parser. Based on class 'FileParser',
// the XML parser implements functions for finding and parsing elements and attributes
// the events are handled through an SAX-like interface called SAXClient

#include "Error.hpp"
#include "FileParser.hpp"
#include "SAXClient.hpp"

extern unsigned long memory_cutoff;
   // The memory cutoff is the maximum amount of memory that should be used
   // If the current memory allocation exceed the limit, then the parser stops
   // and the current data is written to the compressed output file
   // Then, the parser resumes

inline char *TraverseWhiteSpaces(char *ptr,char *endptr)
{
   while((ptr<endptr)&&
         ((*ptr==' ')||(*ptr=='\t')||(*ptr=='\r')||(*ptr=='\n')))
      ptr++;
   return ptr;
}

class XMLParse : public FileParser
{ 
   SAXClient *saxclient;   // the event-receiving client

public:
   void XMLParseError(char *errmsg)
      // Writes a parser error and exits
   {
      char tmpstr[50];
      sprintf(tmpstr,"Parse error in line %lu:\n",GetCurLineNo());
      Error(tmpstr);
      ErrorCont(errmsg);
      Exit();
   }

   void XMLParseError(char *errmsg,int savelineno)
      // Writes a parser error and exits
   {
      char tmpstr[50];
      sprintf(tmpstr,errmsg,savelineno);
      Error(tmpstr);
      Exit();
   }

private:
   char SkipWhiteSpaces()
   {
      char c;
      do
      {
         PeekChar(&c);
         if((c!=' ')&&(c!='\t')&&(c!='\r')&&(c!='\n'))
            break;
         SkipChar();
      }
      while(1);

      return c;
   }

   char ParseAttribs()
   // This function scans the attributes in a given start label
   // The returns as soon as the trailing '>' is reached
   {
      char c;
      char *strptr;
      int  len;

      do
      {
         while(ReadWhiteSpaces(&strptr,&len)==0)
            // We read all white-spaces
            saxclient->HandleAttribWhiteSpaces(strptr,len,1);

         saxclient->HandleAttribWhiteSpaces(strptr,len,0);

         // Now we don't have any more white-spaces and we search
         // for '=' (if there is an attribute) or '>' (for the end of the element)
         PeekChar(&c);
         if((c=='>')||(c=='/'))  // End of label?
         {
            SkipChar();
            return c;
         }
         // Let's find '=' or some white-space
         while(ReadStringUntil(&strptr,&len,1,'=',0)==0)
            // We scan until we reach '='
            saxclient->HandleAttribName(strptr,len,1);

         // We found '='
         saxclient->HandleAttribName(strptr,len-1,0);

         if(strptr[len-1]!='=')
            // We found white-spaces instead?
         {
            c=SkipWhiteSpaces();
            if(c!='=')
               XMLParseError("Symbol '=' expected !");
            SkipChar();
         }
         // We skip all white spaces after '='
         c=SkipWhiteSpaces();

         // The next character should be a '"'
         // If not, then we assume that the value only goes until the
         // next white-space (or '>' or '/')!

         if(c!='"')
         {
            while(ReadStringUntil(&strptr,&len,1,'>','/')==0)
               saxclient->HandleAttribValue(strptr,len,0);

            saxclient->HandleAttribValue(strptr,len-1,1);

            c=strptr[len-1];
            if((c=='/')||(c=='>'))
               return c;
         }
         else
         {
            SkipChar();

            while(ReadStringUntil(&strptr,&len,0,'"','>')==0)
               saxclient->HandleAttribValue(strptr,len,0);

            if(strptr[len-1]=='>')
            {
               char tmpstr[100];
               sprintf(tmpstr,"Line %lu: Missing '\"' at the end of attribute value '",GetCurLineNo());
               Error(tmpstr);
               ErrorCont(strptr,len-1);
               ErrorCont("'!");
               PrintErrorMsg();
               UndoReadChar();
               len--;
            }

            saxclient->HandleAttribValue(strptr,len-1,1);

            PeekChar(&c);
            if((c!='>')&&(c!=' ')&&(c!='\t')&&(c!='\n')&&(c!='\r')&&(c!='/'))
            {
               char tmpstr[50];
               sprintf(tmpstr,"Skip invalid character '%c' in line %lu",c,GetCurLineNo());
               Error(tmpstr);
               PrintErrorMsg();
               SkipChar();
            }
         }
      }
      while(1);
   }

   void ParseLabel()
      // Scans a label after the '<' has already been parsed.
   {
      char c,*ptr;
      int  len;

      PeekChar(&c);

      if(c=='/') // An ending label ?
      {
         GetChar(&c);

         while(ReadStringUntil(&ptr,&len,0,'>','<')==0)
//         while(ReadStringUntil(&ptr,&len,'>')==0)
            // We didn't find '>'  ?
            saxclient->HandleEndLabel(ptr,len,1);

         if(ptr[len-1]=='<')
         {
            Error("Unfinished end label!");
            PrintErrorMsg();
            UndoReadChar();
         }

         saxclient->HandleEndLabel(ptr,len-1,0);
         return;
      }

      while(ReadStringUntil(&ptr,&len,1,'>','/')==0)
         // We didn't find '>' or '/' or a white-space ?
         saxclient->HandleStartLabel(ptr,len,1);

      switch(ptr[len-1])
      {
      case '>':
         saxclient->HandleStartLabel(ptr,len-1,0);
         return;

      case '/':
         saxclient->HandleStartLabel(ptr,len-1,0);
         GetChar(&c);
         if(c!='>')
            XMLParseError("Symbol '/' in label must be followed by '>' !");

         saxclient->HandleEndLabel(NULL,0,0);
         return;

      default: // Did we find some white space ??
         saxclient->HandleStartLabel(ptr,len,0);
         c=ParseAttribs();
         if(c=='/')
         {
            // I.e. we received an empty label
            saxclient->HandleEndLabel(NULL,0,0);
            GetChar(&c);
         }
         if(c!='>')
            XMLParseError("Symbol '>' expected after '/' in tag!");
      }
   }

   void ParsePI()
      // Parses a processing instruction
   {
      int len,savelineno=GetCurLineNo();
      char *ptr;

      do
      {
         if(ReadStringUntil(&ptr,&len,"?>"))
            break;

         if(len==0)
            XMLParseError("Could not find closing '?>' for processing instruction in line %lu !",savelineno);

         saxclient->HandlePI(ptr,len,1);
      }
      while(1);

      saxclient->HandlePI(ptr,len,0);
   }

   void ParseCDATA()
      // Parses a CDATA section
   {
      int len,savelineno=GetCurLineNo();
      char *ptr;

      while(ReadStringUntil(&ptr,&len,"]]>")==0)
      {
         if(len==0)
            XMLParseError("Could not find closing ']]>' for CDATA section starting in line %lu !",savelineno);

         saxclient->HandleCDATA(ptr,len,1);
      }
    
      saxclient->HandleCDATA(ptr,len,0);
   }

   void ParseComment()
      // Parses a comment section
   {
      int len,savelineno=GetCurLineNo();
      char *ptr;

      while(ReadStringUntil(&ptr,&len,"-->")==0)
      {
         if(len==0)
            XMLParseError("Could not find closing '-->' for comment starting in line %lu !",savelineno);

         saxclient->HandleComment(ptr,len,1);
      }

      saxclient->HandleComment(ptr,len,0);
   }

   void ParseText()
      // Parses some text data
   {
      char err;
      int len;
      char *ptr,*leftwsptr,*rightwsptr,*endptr;

      // We look for the end '<'
      err=ReadStringUntil(&ptr,&len,'<');

      if((err==0)&&(len==0))
         return;

      endptr=ptr+len;

      // Let's traverse over all white spaces at the beginning
      leftwsptr=ptr;

      while((leftwsptr<endptr)&&
            (*leftwsptr==' ')||(*leftwsptr=='\t')||
            (*leftwsptr=='\r')||(*leftwsptr=='\n'))
         leftwsptr++;

      while(err==0)  // We didn't find '<' yet ?
                     // No? => We must handle that text before we can continue
      {
         if(len>0)
         {
            if(IsEndOfFile()&&(len==leftwsptr-ptr))
               // If all remaining characters are white spaces,
               // we send one single sequence
               saxclient->HandleText(ptr,len,0,len,len);
            else
               saxclient->HandleText(ptr,len,1,leftwsptr-ptr,0);
         }

         if(leftwsptr==endptr)   // Everything until now was just white spaces ?
                                 // ==> We compute again the number of left white-spaces
         {
            err=ReadStringUntil(&ptr,&len,'<');

            if((err==0)&&(len==0))  // No more characters? ==> We are done
               return;

            leftwsptr=ptr;

            while((leftwsptr<endptr)&&
                  (*leftwsptr==' ')||(*leftwsptr=='\t')||
                  (*leftwsptr=='\r')||(*leftwsptr=='\n'))
               leftwsptr++;
         }
         else
         {
            err=ReadStringUntil(&ptr,&len,'<');
            if((err==0)&&(len==0))
               return;
         
            leftwsptr=ptr; // i.e. the number of left-white spaces is set to zero
         }
      }

      // We found the character '<'

      // We take the '<' back
      UndoReadChar();
      len--;

      endptr=ptr+len;

      // Let's find the number of white spaces at the end of the string
      rightwsptr=endptr-1;

      while((rightwsptr>=ptr)&&
            (*rightwsptr==' ')||(*rightwsptr=='\t')||
            (*rightwsptr=='\r')||(*rightwsptr=='\n'))
         rightwsptr--;

      if(len>0)
         saxclient->HandleText(ptr,len,0,leftwsptr-ptr,endptr-rightwsptr-1);
   }

   void ParseDOCTYPE()
      // Parses a DOCTYPE section.
      // A DOCTYPE has format <!DOCTYPE ... >  or  <!DOCTTYPE ... [ ... ] >
   {
      int   len,savelineno=GetCurLineNo(); // We save the line
      char  *ptr;
      char  *myendptr,*curptr;

      // Let's get the current piece of buffer
      len=GetCurBlockPtr(&ptr);
      if(len==0)
         RefillAndGetCurBlockPtr(&ptr,&len);

      myendptr=ptr+len;
      curptr=ptr;

      do
      {
         if(*curptr=='[')
         {
            do
            {
               curptr++;
               if(curptr==myendptr)
               {
                  saxclient->HandleDOCTYPE(ptr,len,1);
                  FastSkipData(len);
                  RefillAndGetCurBlockPtr(&ptr,&len);
                  if(len==0)
                     XMLParseError("Could not find closing ']>' for DOCTYPE section starting in line %lu !",savelineno);

                  myendptr=ptr+len;
                  curptr=ptr;
               }
            }
            while(*curptr!=']');
         }
         if(*curptr=='>')
            break;

         curptr++;
         if(curptr==myendptr)
         {
            saxclient->HandleDOCTYPE(ptr,len,1);
            FastSkipData(len);
            RefillAndGetCurBlockPtr(&ptr,&len);
            if(len==0)
               XMLParseError("Could not find closing ']>' for DOCTYPE section starting in line %lu !",savelineno);
            myendptr=ptr+len;
            curptr=ptr;
         }
      }
      while(1);

      saxclient->HandleDOCTYPE(ptr,curptr+1-ptr,0);
      FastSkipData(curptr+1-ptr);
   }

//******************************************************************************

public:

   char DoParsing(SAXClient *myclient)
      // This is the main parse function
   {
      saxclient=myclient;

      xmlparser=this;

      char c[9];

      do
      {
         // Let's start parsing text
         ParseText();

         // If have reached the end of the file, we exit
         if(IsEndOfFile())
            return 1;

         // The next character must be an '<' character
         PeekChar(c);
         if(*c!='<') // This should actually be never true
         {
            Error("Character '<' expected !");
            XMLParseError("");
         }

         // let's look at the next three characters
         PeekData(c,3);

         switch(c[1])
         {
            case '?': // Processing Instruction ?
               if(c[2]=='>')
               {
                  SkipChar();
                  ParseLabel();
               }
               else
                  ParsePI();
               break;

            case '!':
               switch(c[2])
               {
               case '[': // We have <![CDATA[... ]]>
                  PeekData(c,9);
                  if(memcmp(c,"<![CDATA[",9)!=0)
                  {
                     Error("Invalid tag '");
                     ErrorCont(c,9);
                     ErrorCont("...' should probably be '<![CDATA ...' !");
                     XMLParseError("");
                  }
                  ParseCDATA();
                  break;
      
               case 'D': // We must have <!DOCTYPE ... [ ... ] >
               {
                  PeekData(c,9);
                  if(memcmp(c,"<!DOCTYPE",9)!=0)
                  {
                     Error("Invalid tag '");
                     ErrorCont(c,9);
                     ErrorCont("...' should probably be '<!DOCTYPE ...' !");
                     XMLParseError("");
                  }
                  ParseDOCTYPE();
               }
               break;

               case '-': // We (probably) have a comment <!-- ... -->
                  PeekData(c,4);

                  if(c[3]!='-')
                  {
                     Error("Invalid tag '");
                     ErrorCont(c,4);
                     ErrorCont("...' should probably be '<!-- ...' !");
                     XMLParseError("");
                  }
                  ParseComment();
                  break;

               default:
                  Error("Invalid tag '");
                  ErrorCont(c,3);
                  ErrorCont("...' !");
                  XMLParseError("");
               }
               break;

         case '=':
            Error("Invalid label '<=...'!");
            PrintErrorMsg();
            SkipChar();
            saxclient->HandleText("<",1,0,0,0);
            break;

         default: // If we only have a simple '<', we skip the character and
                  // parse the following label
            SkipChar();
            ParseLabel();
         }
      }
      while(allocatedmemory<memory_cutoff);
         // We perform the parsing as long as the allocated memory is smaller than the
         // memory cut off

      return 0;
   }
};
