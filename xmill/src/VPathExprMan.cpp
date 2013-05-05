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

// This module implements the management of container path expressions

#include "VPathExprMan.hpp"

#ifdef XMILL
#ifdef FULL_PATHEXPR
#include "VRegExpr.hpp"
#endif
#include "PathTree.hpp"
#include "ContMan.hpp"
#endif

#ifdef XDEMILL
#include "SmallUncompress.hpp"
#endif

#include "Load.hpp"

// The memory used for allocating the path expression and FSM information
extern MemStreamer mainmem;
extern MemStreamer tmpmem;

// The following flags determine how white spaces should be stored
extern char globalleftwhitespacescompress;
extern char globalrightwhitespacescompress;

#ifdef XMILL
extern UserCompressor   *plaincompressorptr; // The plain compressor: 't'

extern CompressContainer *globalwhitespacecont;
   // The global white space container
#endif

inline void VPathExpr::PathParseError(char *errmsg,char *errptr)
   // Prints an error message, if the parsing of some path expression failed.
{
   Error("Error while parsing path expression:\n\n   ");
   ErrorCont(regexprstr,regexprendptr-regexprstr);
   ErrorCont("\n");
   for(int i=0;i<errptr-regexprstr+3;i++)
      ErrorCont(" ",1);
   ErrorCont("^\n");
   ErrorCont(errmsg);
   Exit();
}

inline void VPathExpr::HandlePathExprOption(char * &str,char *endptr)
   // Parses one single option
   // Options are separated by ':'
{
   // The user can specify options of the form l(i|g|t) or r(i|g|t) to
   // influence the handling of left/right white spaces directly for
   // a given path expression
   if(str+2<=endptr)
   {
      switch(*str)
      {
      case 'l':switch(str[1]) // Left white space option?
               {
               case 'i':      leftwhitespacescompress=WHITESPACE_IGNORE;str+=2;return;
               case 'g':      leftwhitespacescompress=WHITESPACE_STOREGLOBAL;str+=2;return;
               case 't':      leftwhitespacescompress=WHITESPACE_STORETEXT;str+=2;return;
               }
               break;
      case 'r':switch(str[1]) // Right white space option?
               {
               case 'i':      rightwhitespacescompress=WHITESPACE_IGNORE;str+=2;return;
               case 'g':      rightwhitespacescompress=WHITESPACE_STOREGLOBAL;str+=2;return;
               case 't':      rightwhitespacescompress=WHITESPACE_STORETEXT;str+=2;return;
               }
               break;
/*
      case 'w':switch(str[1])
               {
               case 'i':      fullwhitespacescompress=WHITESPACE_IGNORE;str+=2;return;
               case 'g':      fullwhitespacescompress=WHITESPACE_STOREGLOBAL;str+=2;return;
               case 't':      fullwhitespacescompress=WHITESPACE_STORETEXT;str+=2;return;
               }
               break;
*/
      }
   }

   // Otherwise, it must be a compressor:

#ifdef XMILL
   usercompressor=compressman.CreateCompressorInstance(str,endptr);
#endif
#ifdef XDEMILL
   useruncompressor=compressman.CreateUncompressorInstance(str,endptr);
#endif
}

inline void VPathExpr::ParseUserCompressorString(char * &str,char *endptr)
   // Parses the user compressor string
   // It parses the options. Note that the actual user compressor *must*
   // come at the end
{
   // We continue parsing and look for ':'
   while(str<endptr)
   {
      // We exit if we find a white-space
      if((*str==0)||(*str==' ')||(*str=='\t')||(*str=='\r')||(*str=='\n'))
      {
         regexprendptr=str;
         return;
      }

      // Let's handle the option or user compressor
      HandlePathExprOption(str,endptr);

      if(str==endptr)
      {
         regexprendptr=str;
         return;
      }

      // We exit, if we find a white-space
      if((*str==0)||(*str==' ')||(*str=='\t')||(*str=='\r')||(*str=='\n'))
      {
         regexprendptr=str;
         return;
      }

      if(*str!=':')
      {
         Error("Character ':' expected at '...'");
         if(endptr-str>5)
         {
            ErrorCont(str,5);
            ErrorCont("...'");
         }
         else
            ErrorCont(str,endptr-str);
         Exit();
      }
      str++;

   }
}

#ifdef XMILL

inline void VPathExpr::CreateXPathEdge(char *from,char *to,FSM *fsm,FSMState *fromstate,FSMState *tostate,char ignore_pounds)
   // Reads the atomic symbol betwen 'from' and 'to' and generates
   // the corresponding edge between 'fromstate' and 'tostate' in 'fsm.
   // If ignore_pound is 1, then pound symbols are simply treated as '*' symbols.
{
   if(from==to)   // Empty string?
      PathParseError("Unexpected character",from);

   switch(*from)
   {
   case '@':   // Do we have '@#' or '@name' ?
               // ==> Create a corresponding
      if((from+2==to)&&(from[1]=='#'))
         fsm->CreateLabelEdge(fromstate,tostate,attribpoundlabelid);
      else
         fsm->CreateLabelEdge(fromstate,tostate,globallabeldict.GetLabelOrAttrib(from+1,to-from-1,1));
      return;

   case '#':   // Do we have '#' or '##'
      if((from+2==to)&&(from[1]=='#'))
         // Do we have a double-pound '##' ?
      {
         FSMState *middlestate=fsm->CreateState();
         fsm->CreateEmptyEdge(fromstate,middlestate);
         fsm->CreateEmptyEdge(middlestate,tostate);
         if(ignore_pounds)
            fsm->CreateNegEdge(middlestate,middlestate);
         else
         {
            fsm->CreateLabelEdge(middlestate,middlestate,elementpoundlabelid);
            fsm->CreateLabelEdge(middlestate,middlestate,attribpoundlabelid);
         }
      }
      else  // we have '#'
      {
         if(from+1!=to)
            PathParseError("Symbol '/' or '|' expected after '#'",from+1);

         if(ignore_pounds)
            fsm->CreateNegEdge(fromstate,tostate);
         else
         {
            fsm->CreateLabelEdge(fromstate,tostate,elementpoundlabelid);
            fsm->CreateLabelEdge(fromstate,tostate,attribpoundlabelid);
         }
      }
      return;

   case '*':   // We have '*'
      if(from+1!=to)
         PathParseError("Symbol '/' or '|' expected after '*'",from+1);
  
      fsm->CreateNegEdge(fromstate,tostate);
      return;

   default:
         fsm->CreateLabelEdge(fromstate,tostate,globallabeldict.GetLabelOrAttrib(from,to-from,0));
   }
}

inline void VPathExpr::ParseXPathItem(char * &startptr,char *endptr,FSM *fsm,FSMState *fromstate,FSMState *tostate,char ignore_pounds)
   // Reads the path expression betwen 'from' and 'to' and generates
   // the corresponding edges and states between 'fromstate' and 'tostate' in 'fsm.
   // If ignore_pound is 1, then pound symbols are simply treated as '*' symbols.
{
   FSMState *curfromstate=fromstate;
   FSMState *curtostate=fsm->CreateState();
   char     *to;

   do
   {
      if(*startptr=='(')
         // We try to consume a label or an expression enclosed in  '(...)'
      {
         startptr++;
         ParseXPathItem(startptr,endptr,fsm,curfromstate,curtostate,ignore_pounds);

         // Afterwards, there must be a symbol ')'
         if((startptr==endptr)||(*startptr!=')'))
            PathParseError("Missing closed parenthesis ')'",startptr);
         startptr++;
      }
      else
      {
         // First, we find the end of the label
         to=startptr;
         while((to<endptr)&&(*to!='/')&&(*to!='=')&&(*to!='|')&&(*to!=')'))
            to++;

         // We create the actual edge
         CreateXPathEdge(startptr,to,fsm,curfromstate,curtostate,ignore_pounds);

         startptr=to;
      }

      if(startptr==endptr) // The path expression is finished after the label
         break;

      // We look at the character coming after the label
      switch(*startptr)
      {
      case '/':   // We have a separator
         startptr++;

         if((startptr<endptr)&&(*startptr=='/'))   // Do we have another '/' following ?
                                                   // i.e. we have '//'
         {
            // Let's create a middle state with a self-loop
            // and an empty edge from 'curfromstate' to 'middlestate'
            // and an empty edge from 'middlestate' to 'curtostate'
            FSMState *middlestate=fsm->CreateState();
            fsm->CreateEmptyEdge(curtostate,middlestate);
            fsm->CreateNegEdge(middlestate,middlestate,NULL);

            curtostate=fsm->CreateState();
            fsm->CreateEmptyEdge(middlestate,curtostate);
            startptr++;
         }
         if((startptr==endptr)||(*startptr=='=')||(*startptr==')'))
            // Is '/' the last character in the expression ? ==> We are done
         {
            fsm->CreateEmptyEdge(curtostate,tostate);
            return;
         }

         curfromstate=curtostate;
         curtostate=fsm->CreateState();
         break;

      case '|':   
         startptr++;
         break;

      case ')':
      case '=':
         fsm->CreateEmptyEdge(curtostate,tostate);
         return;

      default:
         PathParseError("Invalid symbol",startptr);
      }
   }
   while(1);

   fsm->CreateEmptyEdge(curtostate,tostate);
}

inline FSM *VPathExpr::ParseXPath(char * &str,char *endptr,char ignore_pounds)
   // Generates the actual FSM for a given string
   // If ignore_pound is 1, then pound symbols are simply treated as '*' symbols.
{
   FSM *fsm=new(fsmmem) FSM();
   FSMState *startstate=fsm->CreateState();

   fsm->SetStartState(startstate);

   str++;   // We skip the starting '/'

   if(str==endptr)   // We have the single XPath expression '/'
   {
      startstate->SetFinal();
      return fsm;
   }

   if(str[0]=='/')   // Do we have '//...'
   {
      // We loop in start state
      fsm->CreateNegEdge(startstate,startstate,NULL);

      if(str+1==endptr)
         // We have path expression '//'
      {
         str++;
         startstate->SetFinal();
         return fsm;
      }
      FSMState *middlestate=fsm->CreateState();
      fsm->CreateEmptyEdge(startstate,middlestate);
      startstate=middlestate;
      str++;
   }

   // Let's create the final state
   FSMState *finalstate=fsm->CreateState();
   finalstate->SetFinal();

   // We can now parse the path expression and create states/edges between
   // 'startstate' and 'finalstate'
   // If there '=', then we simply have '//=>...' or '/=>...'
   if(*str!='=')
      ParseXPathItem(str,endptr,fsm,startstate,finalstate,ignore_pounds);
   else
      fsm->CreateEmptyEdge(startstate,finalstate);

   // If we are not at the end, then we must have '=>' at the end
   if(str<endptr)
   {
      if((str+1==endptr)||(*str!='=')||(str[1]!='>'))
         PathParseError("Unexpected character",str);

      str+=2;  // We move the 'str'-pointer to the string coming
               // after '=>'
   }
   return fsm;
}

void VPathExpr::CreateFromString(char * &str,char *endptr)
   // This function initializes the object with the path expression
   // found between 'str' and 'endptr.
   // It creates the forward and backward FSM and parses the
   // user compressor string
{
#ifdef FULL_PATHEXPR
   VRegExpr       *regexpr;
#endif
   FSM            *tmpforwardfsm;
   char           *savestr=str;

   regexprstr=str;
   regexprendptr=endptr;   // The end ptr will be set later

   // We start a new block of temporary data
   tmpmem.StartNewMemBlock();

   // The forward FSM is only generated in temporary memory
   fsmmem=&tmpmem;
   fsmtmpmem=&tmpmem;

   // For now, it is required that paths start with '/'
   if(*str=='/')
      tmpforwardfsm=ParseXPath(str,endptr,0);
   else
   {
#ifdef FULL_PATHEXPR
      // Let's firstly take care of the main expression

      vregexprmem=&tmpmem;
      regexpr=VRegExpr::ParseVRegExpr(str,endptr);

      // Let's convert the regular expression into an automaton
      // Let's create an FSM
      tmpforwardfsm=regexpr->CreateNonDetFSM();
#else
      PathParseError("Character '/' expected",str);
#endif
   }

   // Let's make the FSM deterministic
   tmpforwardfsm=tmpforwardfsm->MakeDeterministic();

   // Let's minimize
   tmpforwardfsm=tmpforwardfsm->Minimize();

   // We compute which states are accepting
   //tmpforwardfsm->FindAcceptingStates();

   // We store the following automata in the temporary memory
   fsmmem=&tmpmem;

   // Now we reverse the FSM
   reversefsm=tmpforwardfsm->CreateReverseFSM();

   reversefsm=reversefsm->MakeDeterministic();

   // We store the following automaton in the main memory
   fsmmem=&mainmem;
   mainmem.WordAlign();

   // Only now we create the FSM in main memory
   reversefsm=reversefsm->Minimize();

   // We compute which states are accepting
   reversefsm->FindAcceptingStates();

   // For each state, we also determine whether there
   // are pounds coming afterwards
   reversefsm->ComputeStatesHasPoundsAhead();

#ifdef USE_FORWARD_DATAGUIDE
   if(*savestr=='/')
      forwardfsm=ParseXPath(savestr,endptr,1);
   else
   {
      Error("Fatal Error in VPathExpr::CreateFromString\n");
      Exit();
   }

   // Let's make the FSM deterministic
   
   forwardfsm=forwardfsm->MakeDeterministic();

   fsmmem=&mainmem;
   mainmem.WordAlign();

   // Let's minimize
   forwardfsm=forwardfsm->Minimize();

#endif

   // We remove all the temporary data
   tmpmem.RemoveLastMemBlock();

//*************************************************************************

   // We use the global setting as default for the white space handling
   // for the specific path expression. This might be overwritten by
   // function 'ParseUserCompressorString' below, which parses the user compressor
   // string
   leftwhitespacescompress=WHITESPACE_DEFAULT;
   rightwhitespacescompress=WHITESPACE_DEFAULT;

   // As the default compressor, we set the plain text compressor
   char *textstring="t";
   usercompressor=compressman.CreateCompressorInstance(textstring,textstring+1);

   regexprusercompressptr=str;

   // Let's parse the compressor string now
   ParseUserCompressorString(str,endptr);

   regexprendptr=str;
}

void VPathExpr::InitWhitespaceHandling()
   // If the default white space handling for the path expression
   // is the global setting, then we replace that reference
   // by the global default value
{
   if(leftwhitespacescompress==WHITESPACE_DEFAULT)
      leftwhitespacescompress=globalleftwhitespacescompress;

   if(rightwhitespacescompress==WHITESPACE_DEFAULT)
      rightwhitespacescompress=globalrightwhitespacescompress;
}

void VPathExprMan::InitWhitespaceHandling()
   // If the default white space handling for some path expressions
   // is the global setting, then we replace that reference
   // by the global default value
{
   VPathExpr *pathexpr=pathexprs;
   while(pathexpr!=NULL)
   {
      pathexpr->InitWhitespaceHandling();
      pathexpr=pathexpr->next;
   }
}

#endif

void VPathExpr::PrintRegExpr()
   // Outputs the path expression string
{
   fwrite(regexprstr,regexprendptr-regexprstr,1,stdout);
}

//*******************************************************************
//*******************************************************************
//*******************************************************************
//*******************************************************************

#ifdef XMILL
void VPathExprMan::AddNewVPathExpr(char * &str,char *endptr)
   // Adds a new path expression to the set of paths
{
   // Create the path expression
   VPathExpr *item=new(&mainmem) VPathExpr();

   item->idx=pathexprnum+1;
   pathexprnum++;

   // Parse the path expression string
   item->CreateFromString(str,endptr);

   // Add the path expression to the list
   if(pathexprs==NULL)
      pathexprs=lastpathexpr=item;
   else
   {
      lastpathexpr->next=item;
      lastpathexpr=item;
   }
}
#endif

//***************************************************************************************
//***************************************************************************************
//***************************************************************************************

#ifdef XMILL
inline void VPathExpr::Store(MemStreamer *output)
   // Stores the path expression in 'output'
{
   // We only store the user compressor expression
   // This can even be the empty string, if there is no user compressor string
   output->StoreUInt32(regexprendptr-regexprusercompressptr);
   output->StoreData(regexprusercompressptr,regexprendptr-regexprusercompressptr);
}
#endif

#ifdef XDEMILL
void VPathExpr::Load(SmallBlockUncompressor *uncompress)
   // Loads the user compressor string from 'uncompress'
   // It parses the user compressor string and creates the corresponding user compressor
{
   char           *ptr;
   unsigned long  len=uncompress->LoadString(&ptr);

   // We allocate some memory for the user compressor string
   regexprusercompressptr=mainmem.GetByteBlock(len);
   mainmem.WordAlign();

   memcpy(regexprusercompressptr,ptr,len);

   regexprendptr=regexprusercompressptr+len;
   regexprstr=NULL;

   char *str=regexprusercompressptr;

   // The default user compressor
   char *textstring="t";
   useruncompressor=compressman.CreateUncompressorInstance(textstring,textstring+1);

   // Let's parse the user compressor
   ParseUserCompressorString(str,regexprendptr);
}
#endif

//************************************************************************
//************************************************************************

#ifdef XMILL
void VPathExprMan::Store(MemStreamer *memstream)
   // Stores all path expressions
{
   // Store the number of path expressions
   memstream->StoreUInt32(pathexprnum);

   VPathExpr   *curpathexpr=pathexprs;

   // We store all paths
   while(curpathexpr!=NULL)
   {
      curpathexpr->Store(memstream);
      curpathexpr=curpathexpr->next;
   }
}
#endif

#ifdef XDEMILL
void VPathExprMan::Load(SmallBlockUncompressor *uncompress)
   // Load the set of path expressions from 'uncompress'
{
   // Load the number
   pathexprnum=uncompress->LoadUInt32();

   VPathExpr **pathexprref=&pathexprs;

   // Load all path expressions

   for(unsigned long i=0;i<pathexprnum;i++)
   {
      *pathexprref=new(&mainmem) VPathExpr();

      (*pathexprref)->idx=i;

      (*pathexprref)->Load(uncompress);

      pathexprref=&((*pathexprref)->next);
   }
}
#endif
