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

// This module contains the backward data guide
// The backward data guide maps paths into path dictionary (see PathDict.hpp)
// nodes. Each path in the backward data guide represents a
// path in the XML document that is *backward* - i.e. from
// the leaf (i.e. a leaf node with some string value) toward
// the root of the XML document.

// The backward dataguide is implemented as a tree.

#ifndef PATHTREE_HPP
#define PATHTREE_HPP

#include "LabelDict.hpp"
#include "PathDict.hpp"
#include "MemStreamer.hpp"
#include "VPathExprMan.hpp"

extern MemStreamer   *pathtreemem;
   // The memory for the dataguide nodes

extern VPathExprMan  pathexprman;
   // The path expression manager

class UnpackContainer;

struct PathTreeNode
   // Represents a single dataguide node
{
   PathTreeNode      *parent;       // The parent
   PathTreeNode      *nextsamehash; // The next node with the same hash value

   TLabelID          labelid;       // The label that leads to this node
   unsigned char     isaccepting:1; // Describes whether each of the states in
                                    // fsmstatelist is accepting

   FSMManStateItem   *fsmstatelist; // This contains the states of the
                                    // automata
#ifdef USE_FORWARD_DATAGUIDE
   void ComputePathDictNode(FSMManStateItem *stateitem);
#endif

public:
   void *operator new(size_t size)  {  return pathtreemem->GetByteBlock(size); }
   void operator delete(void *ptr) {}

   void PrintPath()
   {
      if(parent==NULL)
      {
         printf("<>");
         return;
      }
      if(parent->parent!=NULL)
      {
         parent->PrintPath();
         printf(".");
      }
      globallabeldict.PrintLabel(labelid);
   }

   char IsAccepting()               {  return isaccepting; }
   FSMManStateItem *GetFSMStates()  {  return fsmstatelist; }

   void ComputeInitStateList(unsigned char *isaccepting);
      // Computes the list of initial FSM states for this node
      // The state must be the root node!

   void ComputeNextStateList(TLabelID labelid,unsigned char *isaccepting);
      // Computes the list of FSM states  for this non-root node
      // The function looks up the states of the parent node and
      // computes the next states for each FSM that is reachable over 'labelid'
};

// The hash table size for the dataguide
#define PATHTREE_HASHSIZE  65536
#define PATHTREE_HASHMASK  65535

class PathTree
{
   // The hash table
   PathTreeNode   *pathtreehashtable[PATHTREE_HASHSIZE];

   PathTreeNode   rootnode;   // The root node

public:
   void CreateRootNode();
   PathTreeNode *GetRootNode()   {  return &rootnode; }

   PathTreeNode *ExtendCurPath(PathTreeNode *curnode,TLabelID labelid);
      // This important function extends the dataguide from the
      // current node 'curnode' by adding an edge with label 'labelid'
      // It also computes the corresponding FSM state annotations
      // If such an edge already exists, the function simply returns
      // the existing child node

   void ReleaseMemory();

#ifdef PROFILE
   unsigned long  nodecount,lookupcount,hashitercount,fsmstatecount;

   void PrintProfile()
   {
      printf("PathTree: nodecount=%lu  FSMStatecount=%lu  lookupcount=%lu   hashitercount=%lu\n",
         nodecount,fsmstatecount,lookupcount,hashitercount);
   }
#endif
};

   // The global dataguide dictionary
extern PathTree pathtree;

#endif
