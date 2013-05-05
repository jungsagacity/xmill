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

// This module contains the backward data guide
// The backward data guide maps paths into path dictionary (see PathDict.hpp)
// nodes. Each path in the backward data guide represents a
// path in the XML document that is *backward* - i.e. from
// the leaf (i.e. a leaf node with some string value) toward
// the root of the XML document.

// The backward dataguide is implemented as a tree.


#include "PathTree.hpp"
#include "ContMan.hpp"

#ifdef USE_FORWARD_DATAGUIDE
MemStreamer    mypathtreemem(5);
MemStreamer    *pathtreemem=&mypathtreemem;
#else
MemStreamer    *pathtreemem=&blockmem;
#endif


//*****************************************************************************
//*****************************************************************************

#ifdef USE_FORWARD_DATAGUIDE
inline void PathTreeNode::ComputePathDictNode(FSMManStateItem *stateitem)
{
   PathDictNode   *mypathdictnode=pathdict.FindOrCreateRootPath(stateitem->pathexpr);
   FSMState       *curstate=stateitem->pathexpr->GetReverseFSMStartState();
   char           overpoundedge;

   PathTreeNode *curpathtreenode=this;

   while((curpathtreenode->labelid!=LABEL_UNDEFINED)&&
         (curstate->HasPoundsAhead()))

   {
      curstate=curstate->GetNextState(curpathtreenode->labelid,&overpoundedge);

      // Did we jump over a pound-edge ?
      // ==> We must advance the 'pathdictnode' item
      if(overpoundedge)
         mypathdictnode=pathdict.FindOrCreatePath(mypathdictnode,curpathtreenode->labelid);

      curpathtreenode=curpathtreenode->parent;
   }
   stateitem->pathdictnode=mypathdictnode;
}
#endif

inline void PathTreeNode::ComputeInitStateList(unsigned char *isaccepting)
   // Computes the list of initial FSM states for this node
   // The state must be the root node!
{
   VPathExpr         *pathexpr=pathexprman.GetVPathExprs();
   FSMManStateItem   **statelistref=&fsmstatelist;

   *isaccepting=1;

   // We consider all path expressions
   while(pathexpr!=NULL)
   {
#ifdef PROFILE
      pathtree.fsmstatecount++;
#endif

      // Let's create a new FSM state reference for each FSM
      *statelistref=new FSMManStateItem();

      (*statelistref)->pathexpr     =pathexpr;

#ifndef USE_FORWARD_DATAGUIDE
      // We annotate the state with the start state of the reverse FSM
      (*statelistref)->curstate    =pathexpr->GetReverseFSMStartState();
      // We also find (or create) the corresponding root node from the
      // path dictionary
      (*statelistref)->pathdictnode=pathdict.FindOrCreateRootPath(pathexpr);
#else
      (*statelistref)->curstate    =pathexpr->GetForwardFSMStartState();
      ComputePathDictNode(*statelistref);
#endif

      // If one of the FSM states is not accepting, then the *entire*
      // dataguide state is not accepting.
      if((*statelistref)->curstate->IsAccepting()==0)
         *isaccepting=0;

      statelistref=&((*statelistref)->next);
      pathexpr=pathexpr->GetNext();
   }
   *statelistref=NULL;
}

inline void PathTreeNode::ComputeNextStateList(TLabelID labelid,unsigned char *isaccepting)
   // Given a new label and a list of existing states,
   // this function produces a list of successor states
{
   FSMManStateItem   *newstatelist=NULL,**newstatelistref=&fsmstatelist,
                     *newfsmstate;
   FSMState          *nextstate;
   char              overpoundedge;

   // Let's find the previous set of FSM states
   FSMManStateItem   *prevstatelist=parent->fsmstatelist;
   
   *isaccepting=1;

   // We conside all FSM states of the parent node
   while(prevstatelist!=NULL)
   {
      // Can we find a successor state over 'labelid'?
      nextstate=prevstatelist->curstate->GetNextState(labelid,&overpoundedge);
      if(nextstate==NULL)
         // There is no following state, i.e. the automaton does not
         // accept that word
      {
         prevstatelist=prevstatelist->next;
         continue;
      }

#ifdef PROFILE
      pathtree.fsmstatecount++;
#endif

      newfsmstate=new FSMManStateItem();

      newfsmstate->pathexpr      =prevstatelist->pathexpr;
      newfsmstate->curstate      =nextstate;

#ifndef USE_FORWARD_DATAGUIDE
      // If we have a pound edge, we need to maintain this information,
      // since pound edges must be instantiated to labels 
      newfsmstate->overpoundedge =(unsigned long)overpoundedge;

      // If we have a '#' edge, then we need to go to some child node
      // in the path dictionary that is reachable of the labelid
      if(overpoundedge)
         newfsmstate->pathdictnode=pathdict.FindOrCreatePath(prevstatelist->pathdictnode,labelid);
      else
         // Otherwise, we stay in the same path dictionary node
         newfsmstate->pathdictnode=prevstatelist->pathdictnode;
#else
      if(nextstate->IsFinal())
         ComputePathDictNode(newfsmstate);
      else
         newfsmstate->pathdictnode=NULL;
#endif

      // If one of the next states is not accepting,
      // then the entire state is not accepting
      if(nextstate->IsAccepting()==0)
         *isaccepting=0;

      *newstatelistref=newfsmstate;
      newstatelistref=&(newfsmstate->next);

      prevstatelist=prevstatelist->next;
   }
   *newstatelistref=NULL;
}

//*****************************************************************************

void PathTree::CreateRootNode()
   // Creates the root node
{
   unsigned char isaccepting;

   rootnode.parent=NULL;
   rootnode.labelid=LABEL_UNDEFINED;
#ifdef PROFILE
   nodecount=0;
   lookupcount=0;
   hashitercount=0;
   fsmstatecount=0;
#endif

//   rootnode.fsmstatelist=pathexprman.CreateInitStateList(&isaccepting);
   rootnode.ComputeInitStateList(&isaccepting);

   rootnode.isaccepting=isaccepting;
}

// Computes the hash index for a parent node and a label id
#define ComputePathTreeHashIdx(node,labelid) ((((unsigned long)(node))+(labelid))&PATHTREE_HASHMASK)

PathTreeNode *PathTree::ExtendCurPath(PathTreeNode *curnode,TLabelID labelid)
   // This important function extends the dataguide from the
   // current node 'curnode' by adding an edge with label 'labelid'
   // It also computes the corresponding FSM state annotations
   // If such an edge already exists, the function simply returns
   // the existing child node
{
   unsigned char  isaccepting;
   PathTreeNode   *node;

#ifndef USE_NO_DATAGUIDE
   unsigned long  hashidx=ComputePathTreeHashIdx(curnode,labelid);

#ifdef PROFILE
   lookupcount++;
#endif

   // First, we check whether we already have an edge with the
   // same label.
   node=pathtreehashtable[hashidx];

   while(node!=NULL)
   {
#ifdef PROFILE
      hashitercount++;
#endif

      if((node->parent==curnode)&&
         (node->labelid==labelid))
         break;
      node=node->nextsamehash;
   }

   if(node!=NULL)
      // if an edge with the label already exists ? ==> We return it
      return node;

#ifdef PROFILE
   nodecount++;
#endif

#endif // USE_NO_DATAGUIDE

   // If we don't have an edge with the label, we create a new dataguide node
   node=new PathTreeNode();
   node->parent=curnode;
   node->labelid=labelid;

   // ... and compute the set of FSM states for that node
   node->ComputeNextStateList(labelid,&isaccepting);

   node->isaccepting=isaccepting;

#ifndef USE_NO_DATAGUIDE
   node->nextsamehash=pathtreehashtable[hashidx];
   pathtreehashtable[hashidx]=node;
#endif

   return node;
}

void PathTree::ReleaseMemory()
{
//   pathtreemem->ReleaseMemory();
//   pathtreemem.Initialize(5);
//   Init();

   for(int i=0;i<PATHTREE_HASHSIZE;i++)
      pathtreehashtable[i]=NULL;
}
