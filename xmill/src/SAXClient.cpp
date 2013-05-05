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

// This module contains the SAX-Client
// The interface used is very similar to SAX.

#include <stdio.h>

#include "Error.hpp"
#include "Output.hpp"
#include "SAXClient.hpp"
#include "ContMan.hpp"
#include "CurPath.hpp"
#include "XMLParse.hpp"

extern CurPath  curpath;

extern char globalfullwhitespacescompress;
extern char globalattribwhitespacescompress;

extern CompressContainer *globalwhitespacecont;

// These flags tell us whether to ignore comments, cdata, etc. or not.
extern char ignore_comment;
extern char ignore_cdata;
extern char ignore_doctype;
extern char ignore_pi;

// The XML Parser
XMLParse *xmlparser;


#ifdef USE_FORWARD_DATAGUIDE
PathTreeNode   *curpathtreenode;

void InitForwardDataGuide()
{
   curpathtreenode=pathtree.GetRootNode();
}
#endif

//**************************************************************************
//**************************************************************************

// First some auxiliary functions for storing start/end labels

inline void StoreEndLabel()
   // We store an end label by simply storing the TREETOKEN_ENDLABEL token
{
   globaltreecont->StoreCompressedSInt(0,TREETOKEN_ENDLABEL);

#ifdef USE_FORWARD_DATAGUIDE
   curpathtreenode=curpathtreenode->parent;

#ifdef USE_NO_DATAGUIDE
   pathtreemem.RemoveLastMemBlock();
#endif
#endif
}

inline void StoreEmptyEndLabel()
   // We store an end label by simply storing the TREETOKEN_ENDLABEL token
{
   globaltreecont->StoreCompressedSInt(0,TREETOKEN_EMPTYENDLABEL);

#ifdef USE_FORWARD_DATAGUIDE
   curpathtreenode=curpathtreenode->parent;

#ifdef USE_NO_DATAGUIDE
   pathtreemem.RemoveLastMemBlock();
#endif
#endif
}

inline void StoreStartLabel(TLabelID labelid)
   // We store the start label by simply storing the label id and an LABELIDX_TOKENOFFS
   // The LABELIDX_TOKENOFFS is used since the first labels (0 and 1) are used
   // to denote white spaces and special strings (DOCTYPE, ...)
{
   globaltreecont->StoreCompressedSInt(0,GET_LABELID(labelid)+LABELIDX_TOKENOFFS);

#ifdef USE_FORWARD_DATAGUIDE
#ifdef USE_NO_DATAGUIDE
   pathtreemem.StartNewMemBlock();
#endif

   curpathtreenode=pathtree.ExtendCurPath(curpathtreenode,labelid);
#endif
}

inline void StoreTextToken(unsigned blockid)
   // A text token is stored by simply storing the block ID
{
   globaltreecont->StoreCompressedSInt(1,blockid);
/*
#ifdef USE_FORWARD_DATAGUIDE
   CurPathIterator it;
   TLabelID labelid;
   PathTreeNode *mycurnode=reversedataguide.GetRootNode();

   curpath.InitIterator(&it);
   while((labelid=it.GotoPrev())!=LABEL_UNDEFINED)
      mycurnode=reversedataguide.ExtendCurPath(mycurnode,labelid);
#endif
*/
}

//**************************************************************************
//**************************************************************************

void CompressTextItem(char *str,int len,int leftwslen,int rightwslen);

void SAXClient::HandleAttribName(char *str,int len,char iscont)
   // Handles a single attribute name
{
   // We simply create a new attribute, if it does not already exist
   TLabelID labelid=globallabeldict.FindLabelOrAttrib(str,len,1);

   if(labelid==LABEL_UNDEFINED)
      labelid=globallabeldict.CreateLabelOrAttrib(str,len,1);

   // We add it to the current path
   curpath.AddLabel(labelid);

   // We store the label ID
   StoreStartLabel(labelid);
}

void SAXClient::HandleAttribValue(char *str,int len,char iscont)
   // Handles the attribute value
{
   // We simply compress and store the text
   CompressTextItem(str,len,0,0);

   // We remove the attribute label from the path stack
   curpath.RemoveLabel();

   // We store the end label token
   StoreEndLabel();
}

void SAXClient::HandleAttribWhiteSpaces(char *str,int len,char iscont)
{
   if(globalattribwhitespacescompress!=WHITESPACE_IGNORE)
   {
      if(len>0)
      {
         globalwhitespacecont->StoreUInt32(len);
         globalwhitespacecont->StoreData(str,len);
         globaltreecont->StoreCompressedSInt(0,TREETOKEN_ATTRIBWHITESPACE);
      }
   }
}

void SAXClient::HandleStartLabel(char *str,int len,char iscont)
   // Handles a start element tag
{
   // Find or create the attribute
   TLabelID labelid=globallabeldict.FindLabelOrAttrib(str,len,0);

   if((len==9)&&(memcmp(str,"CARBOHYD ",9)==0))
      labelid=labelid;

   if(labelid==LABEL_UNDEFINED)
   {
      labelid=globallabeldict.CreateLabelOrAttrib(str,len,0);
      if(labelid==LABEL_UNDEFINED)
         labelid=LABEL_UNDEFINED;
   }

   // Add the label to the path
   curpath.AddLabel(labelid);

   // Store the start label in the schema container
   StoreStartLabel(labelid);
}

void SAXClient::HandleEndLabel(char *str,int len,char iscont)
   // Stores the end label
{
   TLabelID labelid=curpath.RemoveLabel();
   TLabelID endlabelid;

   // Let's check that the end label doesn't have any trailing white spaces
   while((len>0)&&
         ((str[len-1]=='\n')||(str[len-1]=='\r')||(str[len-1]=='\t')||(str[len-1]==' ')))
   {
      Error("End label has trailing white spaces!");
      PrintErrorMsg();
      len--;
   }

   // Was the current path empty? I.e. we didn't have any corresponding starting label?
   // ==> Exit
   if(labelid==LABEL_UNDEFINED)
   {
      Error("Unexpected end label '");
      ErrorCont(str,len);
      ErrorCont("' !");
      xmlparser->XMLParseError("");
   }

   if(str==NULL)  // Did we have an empty element of the form <label/> ?
      StoreEmptyEndLabel();
   else
   {
      // Otherwise, let's check whether the end label is the same as the start label
      endlabelid=globallabeldict.FindLabelOrAttrib(str,len,0);

      if(endlabelid!=labelid) // Not the same?
                              // We look at the previous label in the path
                              // If this is not equal either, then we exit
      {
         char *ptr;
         unsigned long startlen=globallabeldict.LookupCompressLabel(labelid,&ptr);

         TLabelID prevlabelid=curpath.RemoveLabel();
         if(prevlabelid!=endlabelid)
         {
            Error("End label '");
            ErrorCont(str,len);
            ErrorCont("' does not match start label '");
            ErrorCont(ptr,startlen);
            ErrorCont("' !");
            xmlparser->XMLParseError("");
         }

         // The previous label was equal,
         char tmpstr[100];

         Error("Warning: End label '");
         ErrorCont(str,len);
         sprintf(tmpstr,"' in line %lu does not match start label '",xmlparser->GetCurLineNo());
         ErrorCont(tmpstr);
         ErrorCont(ptr,startlen);
         ErrorCont("'!\n => Additional end label inserted!");
         PrintErrorMsg();

         // We store one additional end tag token
         StoreEndLabel();
      }
      StoreEndLabel();
   }
}

void SAXClient::HandleText(char *str,int len,char iscont,int leftwslen,int rightwslen)
   // This function handles text
{
   if((leftwslen==len)&&(rightwslen==len))
      // Is the entire text block only containing white spaces ?
   {
      // Depending on the flag for full white space text, we either
      // ignore, store them in a global container or treat them as text.
      switch(globalfullwhitespacescompress)
      {
      case WHITESPACE_IGNORE:
         return;

      case WHITESPACE_STOREGLOBAL:
         globaltreecont->StoreCompressedSInt(0,TREETOKEN_WHITESPACE);
         globalwhitespacecont->StoreUInt32(len);
         globalwhitespacecont->StoreData(str,len);
         return;

      case WHITESPACE_STORETEXT:
         CompressTextItem(str,len,0,0);
         return;
      }
   }
   // If there is some real text in the text block, then we use
   // the following function, which distributes the text to the
   // appropriate user compressor
   CompressTextItem(str,len,leftwslen,rightwslen);
}

void SAXClient::HandleComment(char *str,int len,char iscont)
   // Handles comment sections
{
   if(!ignore_comment)
   {
      globaltreecont->StoreCompressedSInt(0,TREETOKEN_SPECIAL);
      globalspecialcont->StoreUInt32(len);
      globalspecialcont->StoreData(str,len);
   }
}

void SAXClient::HandlePI(char *str,int len,char iscont)
   // Handles processing instruction sections
{
   if(!ignore_pi)
   {
      globaltreecont->StoreCompressedSInt(0,TREETOKEN_SPECIAL);
      globalspecialcont->StoreUInt32(len);
      globalspecialcont->StoreData(str,len);
   }
}

void SAXClient::HandleDOCTYPE(char *str,int len,char iscont)
   // Handles DOCTYPE sections
{
   if(!ignore_doctype)
   {
      globaltreecont->StoreCompressedSInt(0,TREETOKEN_SPECIAL);
      globalspecialcont->StoreUInt32(len);
      globalspecialcont->StoreData(str,len);
   }
}

void SAXClient::HandleCDATA(char *str,int len,char iscont)
   // Handles CDATA sections
{
   if(!ignore_cdata)
   {
      globaltreecont->StoreCompressedSInt(0,TREETOKEN_SPECIAL);
      globalspecialcont->StoreUInt32(len);
      globalspecialcont->StoreData(str,len);
   }
}

//**************************************************************************************
//**************************************************************************************

inline char VPathExpr::CompressTextItem(char *str,int len,PathDictNode *pathdictnode,int wsleftlen,int wsrightlen)
   // Attempts to compress the string (str,len) by the user compressor of
   // this path expression
   // wsleftlen and wsrightlen describe the length of left and right white spaces
   // at the beginning/end of the string
{
   CompressContainerBlock  *contblock=pathdictnode->GetCompressContainerBlock();
   CompressContainer       *cont;
   char                    *dataptr;

   // If we haven't created container block yet, then let's do that now
   if(contblock==NULL)
   {
      contblock=pathdictnode->AssignCompressContainerBlock(
         GetUserContNum(),
         GetUserDataSize(),this);

      cont=contblock->GetContainer(0);
      dataptr=contblock->GetUserDataPtr();

      // We initialize the state of the container block
      // by invoking InitCompress on the user compressor
      InitCompress(cont,dataptr);
   }
   else
   {
      cont=contblock->GetContainer(0);
      dataptr=contblock->GetUserDataPtr();
   }

   if(wsleftlen>0)   // Do we have left white spaces?
   {
      // If the left white spaces should be  part of the text, then we simply
      // set wsleftlen=0. Otherwise, 'str' and 'len' are adjusted to exclude
      // those white spaces
      switch(leftwhitespacescompress)
      {
      case WHITESPACE_IGNORE:
      case WHITESPACE_STOREGLOBAL:
         str+=wsleftlen;
         len-=wsleftlen;
         break;
      case WHITESPACE_STORETEXT:
         wsleftlen=0;
      }
   }
   if(wsrightlen>0)
   {
      // If the right white spaces should be part of the text, then we simply
      // set wsrightlen=0. Otherwise, 'str' and 'len' are adjusted to exclude
      // those white spaces
      switch(rightwhitespacescompress)
      {
      case WHITESPACE_IGNORE:
      case WHITESPACE_STOREGLOBAL:
         len-=wsrightlen;
         break;
      case WHITESPACE_STORETEXT:
         wsrightlen=0;
      }
   }

   char *savedataptr=dataptr;

   // Let's try to parse and compress the string using the user compressor
   // If it doesn't work, then we exit
   if(len>0)
   {
      if(usercompressor->ParseString(str,len,dataptr)==0)
         return 0;

      usercompressor->CompressString(str,len,cont,savedataptr);
   }

   // The compression of the text was successful !

   // Let's globally store the left white spaces (if there are some)
   if((wsleftlen>0)&&(leftwhitespacescompress==WHITESPACE_STOREGLOBAL))
   {
      globaltreecont->StoreCompressedSInt(0,TREETOKEN_WHITESPACE);
      globalwhitespacecont->StoreUInt32(wsleftlen);
      globalwhitespacecont->StoreData(str-wsleftlen,wsleftlen);
   }

   // We store the text token. Note that this happens *after* storing the
   // left white space token, but *before* storing the right white space token!

   if(len>0)
      StoreTextToken(contblock->GetID());

   // Let's globally store the right white spaces (if there are some)
   if((wsrightlen>0)&&(rightwhitespacescompress==WHITESPACE_STOREGLOBAL))
   {
      globaltreecont->StoreCompressedSInt(0,TREETOKEN_WHITESPACE);
      globalwhitespacecont->StoreUInt32(wsrightlen);
      globalwhitespacecont->StoreData(str+len,wsrightlen);
   }
   return 1;
}

//**************************************************************************************

#ifndef USE_FORWARD_DATAGUIDE

void CompressTextItem(char *str,int len,int leftwslen,int rightwslen)
   // Compresses a given piece of text where 'leftwslen' and 'rightwslen'
   // are the number of white space on the left and right and of
   // the string

   // This function distributes the text pieces depending on the current
   // path.
{
   CurPathIterator         it,saveit;
   FSMManStateItem         *fsmstate;
   TLabelID                labelid;
   char                    overpoundedge;
   FSMState                *curstate;
   PathDictNode            *pathdictnode;

   // We iterate over the current path
   curpath.InitIterator(&it);

   PathTreeNode *curpathtreenode=pathtree.GetRootNode();

   // We start at the root-node of the reverse data guide
   // and traverse the path backward as long as no accepting state
   // has been reached
   while(curpathtreenode->IsAccepting()==0)
   {
      labelid=it.GotoPrev();
      if(labelid==LABEL_UNDEFINED)
         break;

      curpathtreenode=pathtree.ExtendCurPath(curpathtreenode,labelid);
   }

   // After we reached an accepting state, we look at each
   // single regular expression and the corresponding state of the FSM
   // Note that each of the states already accepted the word!
   // Therefore, we only check whether there are additional pound-signs that
   // come afterwards

   fsmstate=curpathtreenode->GetFSMStates();

   // Did we reach the end of the path?
   if(labelid==LABEL_UNDEFINED)
   {
      // We look for an FSM whose state is final for that path
      while(fsmstate!=NULL)
      {
         if(fsmstate->curstate->IsFinal())
            // Did we find a final state => We send the text to the
            // corresponding path expression
         {
            if(fsmstate->pathexpr->CompressTextItem(
                  str,len,fsmstate->GetPathDictNode(),
                  leftwslen,rightwslen))
               return;
         }
         fsmstate=fsmstate->next;
      }
   }
   else
   {
      // We haven't reached the end of the path, but we found
      // an accepting state?

      // Let's save the iterator
      saveit=it;

      // Let's find the FSMs whose states are accepting
      while(fsmstate!=NULL)
      {
         if(fsmstate->curstate->IsAccepting()==0)
         {
            fsmstate=fsmstate->next;
            continue;
         }

         // For each state, go over the rest of the path and
         // traverse the rest of the FSM and we instantiate
         // the # symbols.

         pathdictnode=fsmstate->GetPathDictNode();

         curstate=fsmstate->curstate;

         // Let's go the starting point in the iterator
         saveit=it;

         // Let's instantiate the #'s as long as we still have
         // #'s ahead
         while(curstate->HasPoundsAhead())
         {
            labelid=it.GotoPrev();
            if(labelid==LABEL_UNDEFINED)  // We reached the beginning of the path?
               break;
            curstate=curstate->GetNextState(labelid,&overpoundedge);

            // Did we jump over a pound-edge ?
            // ==> We must advance the 'pathdictnode' item
            if(overpoundedge)
               pathdictnode=pathdict.FindOrCreatePath(pathdictnode,labelid);
         }

         // Let's now try to compress the text with the compressor
         if(fsmstate->pathexpr->CompressTextItem(str,len,pathdictnode,leftwslen,rightwslen))
            return;
      
         fsmstate=fsmstate->next;
      }
   }
   // No FSM accepts the path? ==> Something is wrong
   Error("Fatal error: no automaton accepts current path !\n");
   Exit();
}

//****************************************************************************************
//****************************************************************************************

// The forward guide implementation shouldn't be used

#else // Should we use a forward data guide ?

void CompressTextItem(char *str,int len,int leftwslen,int rightwslen)
{
   FSMManStateItem         *fsmstate;

   // We look at each single regular expression and the
   // corresponding state of the FSM

   fsmstate=curpathtreenode->GetFSMStates();

   while(fsmstate!=NULL)
   {
      if(fsmstate->curstate->IsFinal())
      {
         if(fsmstate->pathexpr->CompressTextItem(
               str,len,fsmstate->GetPathDictNode(),
               leftwslen,rightwslen))
            return;
      }
      fsmstate=fsmstate->next;
   }
   // No FSM accepts the path? ==> Something is wrong
   Error("No automaton accepts string !\n");
   Exit();
}

#endif
