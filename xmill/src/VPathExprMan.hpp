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

#ifndef REGEXPRMAN_HPP
#define REGEXPRMAN_HPP

#include "FSM.hpp"
#include "CompressMan.hpp"

class VPathExprMan;
struct FSMManStateItem;
class SmallBlockUncompressor;


class VPathExpr
   // Represents a single container path expression
{
   friend VPathExprMan;

   VPathExpr      *next;   // The next container path expression in the path manager



   // For forward dataguides, we also keep the forward FSM
#ifdef USE_FORWARD_DATAGUIDE
   FSM            *forwardfsm;   // The forward FSM
#endif
   FSM            *reversefsm;   // The reverse FSM


   // We also keep the original path expression string
   char           *regexprstr,*regexprendptr,   // The entire string
                  *regexprusercompressptr;      // A pointer to the user compressor string

   unsigned long  idx:20;  // The index of this path expression

   // Information about the compressor

   unsigned long  leftwhitespacescompress:2; // Describes how left white spaces should be compressed
   unsigned long  rightwhitespacescompress:2;// Describes how right white spaces should be compressed

//   unsigned long  compresscontnum:10;        // Number of containers that are needed by the compressor
//   unsigned long  compressuserdatasize:10;   // Number of containers that are needed by the compressor


   UserCompressor    *usercompressor;     // The user compressor


   UserUncompressor  *useruncompressor;   // The user decompressor


   void HandlePathExprOption(char * &str,char *endptr);
      // Parses the specific path expression option and constructs the user compressor object

   void ParseUserCompressorString(char * &str,char *endptr);
      // Parses the user compressor string + options and constructs the user compressor object

   void PathParseError(char *errmsg,char *errptr);
      // Outputs an path error message and exits

   void CreateXPathEdge(char *from,char *to,FSM *fsm,FSMState *fromstate,FSMState *tostate,char ignore_pounds);
      // Reads the atomic symbol betwen 'from' and 'to' and generates
      // the corresponding edge between 'fromstate' and 'tostate' in 'fsm.
      // If ignore_pound is 1, then pound symbols are simply treated as '*' symbols.
      
   void ParseXPathItem(char * &startptr,char *endptr,FSM *fsm,FSMState *fromstate,FSMState *tostate,char ignore_pounds);
   FSM *ParseXPath(char * &str,char *endptr,char ignore_pounds);

   void InitWhitespaceHandling();
      // If the default white space handling for the path expression
      // is the global setting, then we replace that reference
      // by the global default value
      // This function is called after the parsing

public:

   void *operator new(size_t size, MemStreamer *mem)  {  return mem->GetByteBlock(size); }
   void operator delete(void *ptr)  {}
#ifdef SPECIAL_DELETE
   void operator delete(void *ptr,MemStreamer *mem)  {}
#endif

   VPathExpr()
   {


#ifdef USE_FORWARD_DATAGUIDE
      forwardfsm=NULL;
#endif
      reversefsm=NULL;
      next=NULL;

   }

   void CreateFromString(char * &str,char *endptr);
      // This function initializes the object with the path expression
      // found between 'str' and 'endptr.
      // It creates the forward and backward FSM and parses the
      // user compressor string

   void PrintRegExpr(); // Prints the container path expression

   unsigned long GetIdx()  {  return idx; }
   VPathExpr *GetNext() {  return next;   }


   FSMState *GetReverseFSMStartState() {  return reversefsm->GetStartState();}
#ifdef USE_FORWARD_DATAGUIDE
   FSMState *GetForwardFSMStartState() {  return forwardfsm->GetStartState();}
#endif

   void Store(MemStreamer *output);    // Stores the FSM
      // Stores the path expression in 'output'



   void Load(SmallBlockUncompressor *uncompressor);
      // Loads the user compressor string from 'uncompress'
      // It parses the user compressor string and creates the corresponding user compressor



   unsigned long GetUserContNum()   {  return usercompressor->GetUserContNum(); }
   unsigned long GetUserDataSize()  {  return usercompressor->GetUserDataSize();  }


   unsigned long UnGetUserContNum()   {  return useruncompressor->GetUserContNum(); }
   unsigned long UnGetUserDataSize()  {  return useruncompressor->GetUserDataSize();  }



   void InitCompress(CompressContainer *cont,char *dataptr)
   {
      usercompressor->InitCompress(cont,dataptr);
   }

   void FinishCompress(CompressContainer *cont,char *dataptr)
   {
      usercompressor->FinishCompress(cont,dataptr);
   }

   char CompressTextItem(char *str,int len,PathDictNode *pathdictnode,int wsleftlen,int wsrightlen);
      // This is the main function for compressing a text item
      // The function returns 1 if the user compressor accepted the string and
      // thecompression was successful
      // Otherwise, the function returns 0

   UserCompressor *GetUserCompressor()
   {
      return usercompressor;
   }
   



   UserUncompressor *GetUserUncompressor()
   {
      return useruncompressor;
   }

};


//**********************************************************
//**********************************************************
//**********************************************************

struct PathTreeNode;

class VPathExprMan
   // The path expression manager
{
   unsigned       pathexprnum;      // The number of paths
   VPathExpr      *pathexprs;       // The list of paths
   VPathExpr      *lastpathexpr;    // The pointer to the last path

public:

   VPathExprMan()
   {
      pathexprnum=0;
      pathexprs=lastpathexpr=NULL;
   }

   VPathExpr *GetPathExpr(unsigned long idx)
      // Returns the path expression with index 'idx'
   {
      VPathExpr *curpathexpr=pathexprs;
      while(idx--)
         curpathexpr=curpathexpr->next;
      return curpathexpr;
   }

   void AddNewVPathExpr(char * &str,char *endptr);
      // Adds a new path expression to the set of paths

   void Store(MemStreamer *memstream);
      // Stores all path expressions

   void Load(SmallBlockUncompressor *uncompressor);
      // Load the set of path expressions from 'uncompressor'


   VPathExpr *GetVPathExprs() {  return pathexprs; }

   void InitWhitespaceHandling();
      // If the default white space handling for the path expression
      // is the global setting, then we replace that reference
      // by the global default value
      // This function is called after all path expressions
      // habe been inserted
};



class PathDictNode;

extern MemStreamer *pathtreemem;

struct FSMManStateItem
   // This structure is used to represent a state within a set of states
   // for a specific path in the XML document
{
   FSMManStateItem   *next;            // The state of the next FSM in the list

   FSMState          *curstate;        // The current state
   VPathExpr         *pathexpr;        // The path expression that this state belongs to
   PathDictNode      *pathdictnode;    // The node in the path dictionary
                                       // representing the pounds that already occurred
#ifndef USE_FORWARD_DATAGUIDE
   unsigned long     overpoundedge:1;  // State has been reached over a pound-edge?
   unsigned long     poundcount:7;     // How many pound-edges have been passed?
#endif

public:

   void *operator new(size_t size)  {  return pathtreemem->GetByteBlock(size); }
   void operator delete(void *ptr)  {}

   PathDictNode *GetPathDictNode()
   {
      return pathdictnode;
   }
};



#endif
