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

// This module contains the implementation of finite state machines

#include <stdio.h>

#include "src/LabelDict.hpp"
#include "src/FSM.hpp"
#include "src/Load.hpp"

//#include "Load.hpp"

// We reserve two labels for '#' and '@#'
TLabelID elementpoundlabelid=LABEL_UNDEFINED;
TLabelID attribpoundlabelid=LABEL_UNDEFINED;

void FSMInit()
   // This assigns the first two label IDs to '#' and '@#'
{
   elementpoundlabelid=globallabeldict.CreateLabelOrAttrib("#",1,0);
   attribpoundlabelid=globallabeldict.CreateLabelOrAttrib("#",1,1);
}

MemStreamer *fsmtmpmem,*fsmmem;

inline void *FSMLabel::operator new(size_t size, MemStreamer *mem)
{
   return mem->GetByteBlock(size);
}

inline void *FSMEdge::operator new(size_t size, MemStreamer *mem)
{
   return mem->GetByteBlock(size);
}

//******************************************************************************************

inline char IsInLabelList(FSMLabel *labellist,TLabelID labelid)
   // Checks whether 'labelid' is in 'labellist'
{
   FSMLabel *curitem=labellist;
   while(curitem!=NULL)
   {
      if(curitem->labelid==labelid)
         return 1;
      curitem=curitem->next;
   }
   return 0;
}

inline void RemoveFromLabelList(FSMLabel **listref,int labelid)
   // Removes a certain label from a labellist *listref
{
   while(*listref!=NULL)
   {
      if((*listref)->labelid==labelid)
      {
         *listref=(*listref)->next;
         return;
      }
      listref=&((*listref)->next);
   }
}

inline void DupLabelList(FSMLabel *labellist,FSMLabel **newlabellist)
   // Stores the list of labels in 'labellist' to the list in 'newlabellist
{
   while(labellist!=NULL)
   {
      *newlabellist=new(fsmmem) FSMLabel(labellist->labelid);
      newlabellist=&((*newlabellist)->next);
      labellist=labellist->next;
   }
}

//******************************************************************************************

inline FSMEdge::FSMEdge(FSMState *tostate)
{
   type=EDGETYPE_EMPTY;
   nextstate=tostate;
}

inline FSMEdge::FSMEdge(FSMState *tostate,TLabelID mylabelid)
{
   type=EDGETYPE_LABEL;
   nextstate=tostate;
   labelid=mylabelid;
}

inline FSMEdge::FSMEdge(FSMState *tostate,FSMLabel *mylabels)
{
   type=EDGETYPE_NEGLABELLIST;
   nextstate=tostate;
   labellist=mylabels;
}

inline char FSMEdge::DoesMatchLabel(TLabelID mylabelid)
   // Checks whether some edge matches a given label
{
   if(labelid==LABEL_UNDEFINED)  // This means it matches everything
      return type==EDGETYPE_NEGLABELLIST;

   switch(type)
   {
   case EDGETYPE_LABEL:       return labelid==mylabelid;
   case EDGETYPE_NEGLABELLIST:return !IsInLabelList(labellist,mylabelid);
   case EDGETYPE_EMPTY:       return 0;
   }
   return 0;
}

//******************************************************************************************

inline FSMState::FSMState(unsigned mystateid,char myisfinal)
{
   stateid=mystateid;
   isfinal=myisfinal;
   outedges=NULL;
   next=NULL;
   origstateset=NULL;
}

inline FSMEdge *FSMState::FindNegEdge()
   // Find the negative outgoing edge in this state
   // (if there is one)
{
   FSMEdge *edge=outedges;

   while(edge!=NULL)
   {
      if(edge->GetType()==EDGETYPE_NEGLABELLIST)
         return edge;
      edge=edge->next;
   }
   return NULL;
}

FSMState *FSMState::GetNextState(TLabelID labelid,char *overpoundedge)
   // Returns the next state after label 'labelid' is consumed.
   // isattrib must describe whether the label is an attribute or not.
   // 'overpoundedge' will indicated afterwards whether the FSM moved over a 'pound'-edge
   // This only works for deterministic automata
{
   FSMEdge  *edge=outedges;
   FSMEdge  *defaultedge=NULL;
   char     isattrib=ISATTRIB(labelid);

   if(overpoundedge!=NULL)
      *overpoundedge=0;

   while(edge!=NULL)
   {
      switch(edge->GetType())
      {
      case EDGETYPE_LABEL:
         // If a label edge matches the label exactly, then we can return immediately
         if(edge->labelid==labelid)
            return edge->GetNextState();

         // If we have a pound-edge, we save its pointer
         // (if we don't have a default edge already)
         if((((edge->labelid==elementpoundlabelid)&&(isattrib==0))||
            ((edge->labelid==attribpoundlabelid)&&(isattrib==1)))&&
            (defaultedge==NULL))
         {
            defaultedge=edge;
            if(overpoundedge!=NULL)
               *overpoundedge=1;
         }
         break;

      case EDGETYPE_NEGLABELLIST:
      {
         // A negative edge automatically becomes the default edge,
         // (And we overwrite any previous pound-edge!)
         // if 'labelid' is not contained in the negative list
         
         if(!IsInLabelList(edge->labellist,labelid))
            // 'labelid' is not in the negative list?
         {
            defaultedge=edge;
            if(overpoundedge!=NULL)
               *overpoundedge=0;
         }
         break;
      }
//      case EDGETYPE_EMPTY:
//         return NULL;   // This should never happen !
      }
      edge=edge->next;
   }
   // If we didn't find any exactly matching label edge, then we choose
   // the default edge. If there is no default edge, then there is no match
   if(defaultedge!=NULL)
      return defaultedge->GetNextState();
   else
      return NULL;
}

//**************************************************************************
//**************************************************************************

FSMEdge *FSM::CreateLabelEdge(FSMState *fromstate,FSMState *tostate,TLabelID labelid)
   // Creates an EDGETYPE_LABEL edge
{
   FSMEdge *edge=new(fsmmem) FSMEdge(tostate,labelid);

   edge->next=fromstate->outedges;
   fromstate->outedges=edge;
   return edge;
}

FSMEdge *FSM::CreateNegEdge(FSMState *fromstate,FSMState *tostate,FSMLabel *labellist)
   // Creates an EDGETYPE_NEGLABELLIST edge
{
   FSMEdge *edge=new(fsmmem) FSMEdge(tostate,labellist);

   edge->next=fromstate->outedges;
   fromstate->outedges=edge;
   return edge;
}

FSMEdge *FSM::CreateEmptyEdge(FSMState *fromstate,FSMState *tostate)
   // Creates an EDGETYPE_EMPTY edge
{
   FSMEdge *edge=new(fsmmem) FSMEdge(tostate);

   edge->next=fromstate->outedges;
   fromstate->outedges=edge;
   return edge;
}

FSMState *FSM::CreateState(char isfinal)
{
   FSMState *newstate=new(fsmmem) FSMState(curidx,isfinal);

   curidx++;

   *laststatelistref=newstate;
   laststatelistref=&((*laststatelistref)->next);
   return newstate;
}

//****************************************************************************
//****************************************************************************
//****************************************************************************
//****************************************************************************

//******** These are functions for making an FSM deterministic ********

struct FSMStateSetItem
   // This structure is used for representing a set of (non-deterministic states)
{
   FSMStateSetItem   *next;   // The next non-deterministic state
   FSMState          *state;  // The actual state

   FSMStateSetItem(FSMState *mystate,FSMStateSetItem *mynext)
   {
      state=mystate;
      next=mynext;
   }

   void *operator new(size_t size)     {  return fsmtmpmem->GetByteBlock(size); }
   void operator delete(void *ptr)     {}
   void *operator new[] (size_t size)  {  return fsmtmpmem->GetByteBlock(size); }
   void operator delete[] (void *ptr)  {}
};

inline FSMState::FSMState(unsigned mystateid,FSMStateSetItem *origset)
   // Creates a new (deterministic) state based on the set of (nondeterministic)
   // states in 'origset'. We also compute whether the new state is final or not
{
   outedges    =NULL;
   origstateset=origset;
   stateid     =mystateid;
   next        =NULL;
   isfinal     =0;

   while(origset!=NULL)
   {
      if(origset->state->IsFinal())
      {
         isfinal=1;
         break;
      }
      origset=origset->next;
   }
}

inline FSMState *FSM::CreateState(FSMStateSetItem *list)
// Creates a determinsitic state with a set of corresponding nondet. states
{
  FSMState *newstate=new(fsmmem) FSMState(curidx,list);

  curidx++;
  *laststatelistref=newstate;
  laststatelistref=&((*laststatelistref)->next);

  return newstate;
}

inline char IsInStateSet(FSMStateSetItem *set,FSMState *state)
   // Checks whether a given state is already in the state set
{
   FSMStateSetItem   *curitem=set;

   // Check if the state is not already in the set
   while(curitem!=NULL)
   {
      if(curitem->state==state)
         return 1;
      curitem=curitem->next;
   }
   return 0;
}

inline FSMStateSetItem *AddToStateSet(FSMStateSetItem *set,FSMState *state)
   // Adds a new state to the current set of states, if the state is
   // not already in the set.
   // If the state is inserted, then we also insert all states that
   // are reachable over empty edges

   // This function is static
{
   // Check if the state is already in the set
   // In this case, we don't have to do anything
   if(IsInStateSet(set,state))
      return set;

   set=new FSMStateSetItem(state,set);

   // We also add the states that are reachable by empty paths

   FSMEdge *outedges=state->GetOutEdges();
   while(outedges!=NULL)
   {
      if(outedges->GetType()==EDGETYPE_EMPTY)
         set=AddToStateSet(set,outedges->GetNextState());
      outedges=outedges->next;
   }
   return set;
}

inline void CreateStateSet(FSMStateSetItem *fromstateset,int labelid,FSMStateSetItem **newset)
   // Starting from a set of states, this function produces a new set of states
   // that are reachable from the current set of states via label 'labelid'
   // If 'labelid' is -1, it means that we are looking for everything except the
   // given labels
{
   FSMEdge           *curedge;

   *newset=NULL;

   while(fromstateset!=NULL)
   {
      curedge=fromstateset->state->GetOutEdges();

      while(curedge!=NULL)
      {
         if(curedge->DoesMatchLabel(labelid))
            *newset=AddToStateSet(*newset,curedge->GetNextState());

         curedge=curedge->next;
      }
      fromstateset=fromstateset->next;
   }
}

inline char CompareStateSets(FSMStateSetItem *set1,FSMStateSetItem *set2)
   // Compares two sets of states
   // If they are equal, the result is 1, otherwise 0
{
   int               count1=0;
   FSMStateSetItem   *curitem=set2;

   // For every element of 'set1', we check whether it is in 'set2'
   // If not, we return 0
   // We also count the number of elements in 'count1'

  while(set1!=NULL)
  {
      if(!IsInStateSet(set2,set1->state)) // Not in set?
         return 0;

      count1++;
      set1=set1->next;
   }

   // Now we check whether the two sets have the same number of elements
   curitem=set2;
   while(curitem!=NULL)
   {
      count1--;
      if(count1<0)
         return 0;
      curitem=curitem->next;
   }
   return 1;
}

inline void FindAllOutgoingLabels(FSMState *state,FSMLabel **dest)
   // This function enumerates all outgoing labels from state 'state'
   // - this includes all labels mentioned in EDGETYPE_LABEL
   // and EDGETYPE_NEGLABELLIST edges.
   // The accepted labels are added to label list 'dest'
{
   FSMLabel *curlabel;
   FSMEdge  *outedge;

   outedge=state->GetOutEdges();
   while(outedge!=NULL)
   {
      switch(outedge->GetType())
      {
      case EDGETYPE_LABEL:
         if(!IsInLabelList(*dest,outedge->GetLabelID()))
            // We only insert if the label is not already in the list
            *dest=new(fsmtmpmem) FSMLabel(outedge->GetLabelID(),*dest);

         break;

      case EDGETYPE_NEGLABELLIST:
         curlabel=outedge->GetLabelList();
         while(curlabel!=NULL)
         {
            if(!IsInLabelList(*dest,curlabel->labelid))
               // We only insert if the label is not already in the list
               *dest=new(fsmtmpmem) FSMLabel(curlabel->labelid,*dest);

            curlabel=curlabel->next;
         }
      }
      outedge=outedge->next;
   }
}

inline void FindAllOutgoingLabels(FSMStateSetItem *set,FSMLabel **dest)
   // This function enumerates all outgoing labels from a given set
   // of states.
{
   // We iterate over all states and all outgoing edges
   while(set!=NULL)
   {
      // We enumerate for a specific state
      FindAllOutgoingLabels(set->state,dest);

      set=set->next;
   }
}

inline char ConsiderNegEdgeAtState(FSM *newfsm,FSMState *curstate,FSMLabel *mylabellist,FSMState *curstatelist,FSMState **newstate)
   // Creates an negedge that leads from 'curstate' to some potential new state
   // The function stores the new state in *newstate
   // Returns 0, if an existing state can be used
   //         1, if a new next state is created
{
   FSMStateSetItem   *nextstateset;

   // First, we find all nextstates that are reachable from origstateset over 'labelid'
   CreateStateSet(curstate->GetOrigStateSet(),LABEL_UNDEFINED,&nextstateset);

   // If the new state set is empty, then we do not have to do anything
   if(nextstateset==NULL)
      return 0;

   // Now, we check whether we already have a state with the same statelist
   while(curstatelist!=NULL)
   {
      if(CompareStateSets(nextstateset,curstatelist->GetOrigStateSet()))
         // If checkstate has the same stateset, then we use that existing one
      {
         newfsm->CreateNegEdge(curstate,curstatelist,mylabellist);

         *newstate=curstatelist;
         return 0;
      }
      curstatelist=curstatelist->GetNextStateInList();
   }

   // We didn't find any state with the same state set
   // ==> then we must create one

   *newstate=newfsm->CreateState(nextstateset);
   newfsm->CreateNegEdge(curstate,*newstate,mylabellist);

   return 1;
}

inline char ConsiderLabelIDAtState(FSM *newfsm,FSMState *curstate,int labelid,FSMState *curstatelist,FSMState **newstate)
   // Creates an edge that leads from 'curstate' to some potential new state
   // The function stores the new state in *newstate
   // Returns 0, if an existing state can be used
   //         1, if a new next state is created
{
   FSMStateSetItem   *nextstateset;
   FSMEdge           *edge;

   // First, we find all nextstates that are reachable from origstateset over 'labelid'
   CreateStateSet(curstate->GetOrigStateSet(),labelid,&nextstateset);

   // If the new state set is empty, then we do not have to do anything
   if(nextstateset==NULL)
      return 0;

   // Now, we check whether we already have a state with the same statelist
   while(curstatelist!=NULL)
   {
      if(CompareStateSets(nextstateset,curstatelist->GetOrigStateSet()))
         // If checkstate has the same stateset, then we use that existing one
      {
         *newstate=curstatelist;

         // We check if we already have a negedge between the two
         // existing states
         edge=curstate->FindNegEdge();
         if((edge!=NULL)&&(edge->GetNextState()==curstatelist))
            // If yes, then we only need to remove the label from the negative list
            RemoveFromLabelList(edge->GetLabelListRef(),labelid);
         else
            newfsm->CreateLabelEdge(curstate,curstatelist,labelid);

         return 0;
      }
      curstatelist=curstatelist->GetNextStateInList();
   }

   // We didn't find any state with the same state set
   // ==> then we must create one

   *newstate=newfsm->CreateState(nextstateset);

   newfsm->CreateLabelEdge(curstate,*newstate,labelid);
   return 1;
}

FSM *FSM::MakeDeterministic()
   // This is the main function for making an FSM deterministic
{
   FSMStateSetItem   *todolist=NULL,**todoref=&todolist;
      // The 'todolist' contains the list of deterministic states that still
      // have to be considered
   FSMLabel          *tmplabellist,*curlabel;
   FSMState          *curstate,*newnextstate;
   FSM               *newfsm=new(fsmmem) FSM();

   FSMStateSetItem   *startstateset=NULL;

   // The start state set only contains the start state
   startstateset=AddToStateSet(startstateset,startstate);

   // We create a corresponding deterministic start state
   curstate=newfsm->CreateState(startstateset);

   newfsm->SetStartState(curstate);

   // We now always consider 'curstate' - the previously created deterministic state
   do
   {
      // First, we find all distinct outgoing labels for 'curstate'
      tmplabellist=NULL;
      FindAllOutgoingLabels(curstate->GetOrigStateSet(),&tmplabellist);

      // We consider the negedge for the current state

      if(ConsiderNegEdgeAtState(newfsm,curstate,tmplabellist,newfsm->GetStateList(),&newnextstate)==1)
         // A new state has been created?
         // ==> We add the state to the 'todo'-list
         todolist=new FSMStateSetItem(newnextstate,todolist);

      // Now, we also consider the label edges of the current state

      curlabel=tmplabellist;
      while(curlabel!=NULL)
      {
         if(ConsiderLabelIDAtState(newfsm,curstate,curlabel->labelid,newfsm->GetStateList(),&newnextstate)==1)
            // A new state has been created?
            // ==> We add the state to the 'todo'-list
            todolist=new FSMStateSetItem(newnextstate,todolist);

         curlabel=curlabel->next;
      }

      if(todolist==NULL)
         break;

      // We go to next element of todo-list
      curstate=todolist->state;
      todolist=todolist->next;
   }
   while(1);

   return newfsm;
}

//*****************************************************************************
//*****************************************************************************

// The following functions are for the minimization of deterministic FSMs

struct StateEqualPairListItem
   // This structure is used to represent a list of state-pairs
{
   StateEqualPair          *statepair; // The state-pair
   StateEqualPairListItem  *next;      // The next state-pair

   StateEqualPairListItem(StateEqualPair *mystatepair,StateEqualPairListItem *mynext)
   {
      statepair=mystatepair;
      next=mynext;
   }

   void *operator new(size_t size)  {  return fsmtmpmem->GetByteBlock(size); }
   void operator delete(void *ptr)  {}
};

struct StateEqualPair
   // This structure represents a pair of two states. We keep information about
   // whether the two states are not equal and we also keep a list of
   // state pairs, which become 'not-equal', if this state pair becomes 'not-equal'.
{
   unsigned long           isnotequal:1;     // Tells us if those two states are definitely not equal
   FSMState                *state1,*state2;  // The two states in this pair
   StateEqualPairListItem  *dependlist;      // If the two states are not equal, then all the
                                             // state-pairs in the 'dependlist' become not equal

   void *operator new[] (size_t size)  {  return fsmtmpmem->GetByteBlock(size); }
   void operator delete[] (void *ptr)  {}
};

inline char CheckEqual(FSMState *nextstate1,FSMState *nextstate2,StateEqualPair **array)
   // Checks whether the two states 'nextstate1' and 'nextstate2' could be equal
   // We also return '1', if both states are NULL, i.e. the label 'curlabel'
   // was not accepted by any of the two states
{
   if(nextstate1!=NULL)
   {
      if(nextstate2==NULL) // 'curlabel' is accepted in one state, but not
                           // in the other ? ==> states are not equal !
         return 0;

      unsigned sid1=nextstate1->GetStateID();
      unsigned sid2=nextstate2->GetStateID();
      if(sid1==sid2)
         return 1;   // Are the two states identical?

      if(sid2<sid1)
         return !(array[sid1][sid2].isnotequal);
      else
         return !(array[sid2][sid1].isnotequal);
   }
   else
      return nextstate2==NULL;
}

inline void AddDependency(FSMState *nextstate1,FSMState *nextstate2,StateEqualPair *curpair,StateEqualPair **array)
   // Adds the state pair 'curpair' into the dependency list for pair 'nextstate1' and 'nextstate2'
{
   StateEqualPair *nextpair;

   if((nextstate1==NULL)||(nextstate2==NULL))
      return;

   unsigned sid1=nextstate1->GetStateID();
   unsigned sid2=nextstate2->GetStateID();
   if(sid1==sid2)
      return;

   nextpair= (sid2<sid1) ? (array[sid1]+sid2) : (array[sid2]+sid1);

   if((nextpair->dependlist==NULL)||
      (nextpair->dependlist->statepair!=curpair))
      // We only insert if 'curpair' is not already at the beginning of the list
      nextpair->dependlist=new StateEqualPairListItem(curpair,nextpair->dependlist);
}

void MarkDependentNotEqual(StateEqualPairListItem *dependlist)
   // This recursive functions marks the dependents in 'dependlist' and
   // their dependents as 'not equal'
{
   while(dependlist!=NULL)
   {
      if(dependlist->statepair->isnotequal==0)
      {
         dependlist->statepair->isnotequal=1;
         if(dependlist->statepair->dependlist!=NULL)
            MarkDependentNotEqual(dependlist->statepair->dependlist);
      }
      dependlist=dependlist->next;
   }
}

//**********************************

FSM *FSM::Minimize() // This is the main function for minimizing a deterministic FSM
{
   int            statenum,i,j;
   FSMState       *curstate1,*curstate2;
   StateEqualPair *pairptr;
   FSMLabel       *tmplabellist,*curlabel;

   // Let's get rid of redundant states first
   PruneRedundantStates();

   // We create a NxN array where N is the number of states in the FSM
   // Each array field represents a state-pair and we keep track of when
   // two states could be equivalent.

   statenum=GetStateCount();

   StateEqualPair **array=(StateEqualPair **)fsmtmpmem->GetByteBlock(statenum*sizeof(StateEqualPair *));

   curstate1=statelist;

   // We initialize the array of states
   // Two states can definitely not be equal, if one of them is a final state and the other not
   // Note that we only need to consider array fields [i,j] with j<i
   for(i=0;i<statenum;i++)
   {
      array[i]=new StateEqualPair[i];

      pairptr=array[i];

      curstate2=statelist;

      for(j=0;j<i;j++)
      {
         pairptr->state1      =curstate1;
         pairptr->state2      =curstate2;
         pairptr->dependlist  =NULL;
         pairptr->isnotequal  =(curstate1->IsFinal() != curstate2->IsFinal());

         pairptr++;
         curstate2=curstate2->next;
      }
      curstate1=curstate1->next;
   }

   curstate1=statelist;

   // Now we look at each state-pair separately
   // We only consider array fields [i,j] with j<i
   for(i=0;i<statenum;i++)
   {
      pairptr=array[i];

      curstate2=statelist;

      for(j=0;j<i;j++)
      {
         // We first find the labels on all outgoing edges

         tmplabellist=NULL;
         FindAllOutgoingLabels(curstate1,&tmplabellist);
         FindAllOutgoingLabels(curstate2,&tmplabellist);

         // Now, for each label in 'tmplabellist', we check whether
         // the states that would be reached for that label are equal.
         // If for some label they are not equal, then we need to add a dependency
         curlabel=tmplabellist;
         while(curlabel!=NULL)
         {
            if(CheckEqual( curstate1->GetNextState(curlabel->labelid),
                           curstate2->GetNextState(curlabel->labelid),array)==0)
               break;
            curlabel=curlabel->next;
         }


         if(curlabel==NULL)   // Only if all labels passed well, we try to
                              // look at the "EVERYTHING_ELSE" case
         {
            if(CheckEqual( curstate1->GetNextState(LABEL_UNDEFINED),
                           curstate2->GetNextState(LABEL_UNDEFINED),array)==0)
               curlabel=(FSMLabel *)1; // We set curlabel to a value!=NULL
         }

         if(curlabel!=NULL)   // Did one of the labels lead to non-equal states?
                              // Then the current state pair is also marked "non-equal"
         {
            // We mark the pair (and all dependent pairs) as not-equal 
            pairptr->isnotequal=1;

            if(pairptr->dependlist!=NULL)
               MarkDependentNotEqual(pairptr->dependlist);
         }
         else  // We don't know whether the states are equivalent
               // ==> We build dependency chains to each of the successor states
               // If the successor states become non-equal, then we change this statepair
         {
            curlabel=tmplabellist;
            while(curlabel!=NULL)
            {
               AddDependency( curstate1->GetNextState(curlabel->labelid),
                              curstate2->GetNextState(curlabel->labelid),
                              pairptr,
                              array);

               curlabel=curlabel->next;
            }
            AddDependency( curstate1->GetNextState(LABEL_UNDEFINED),
                           curstate2->GetNextState(LABEL_UNDEFINED),
                           pairptr,
                           array);
         }
         pairptr++;
         curstate2=curstate2->next;
      }
      curstate1=curstate1->next;
   }

   // In the final phase, we create a new automaton and copy the states

   FSM *newfsm=new(fsmmem) FSM();
   FSMEdge  *edge,*negedge;

   // First, we create all states

   curstate1=statelist;

   for(i=0;i<statenum;i++)
   {
      pairptr=array[i];
      curstate2=statelist;

      for(j=0;j<i;j++)
      {
         if(pairptr->isnotequal==0)
            // We found a previous state that is equal? ==> We store a pointer
            // to the corresponding state and we set array[i]=NULL
         {
            curstate1->data=curstate2->data;
            array[i]=NULL;
            break;
         }
         curstate2=curstate2->next;
         pairptr++;
      }

      if(i==j) // We didn't find any previous state that is equal
               // ==> We create a new state
         curstate1->data=newfsm->CreateState(curstate1->IsFinal());

      curstate1=curstate1->next;
   }

   // We also need to take care of the start-state
   newfsm->SetStartState((FSMState *)startstate->data);

   // Now, we need to take care of the edges
   curstate1=statelist;
   for(i=0;i<statenum;i++)
   {
      if(array[i]!=NULL)
      {
         // Let's take care of the negedge first

         edge=curstate1->FindNegEdge();
         if(edge!=NULL)
         {
            tmplabellist=NULL;
            DupLabelList(edge->GetLabelList(),&tmplabellist);

            negedge=newfsm->CreateNegEdge((FSMState *)curstate1->data,
                                          (FSMState *)edge->GetNextState()->data,
                                          tmplabellist);
         }
         else
            negedge=NULL;

         // Let's consider all outgoing edges
         edge=curstate1->GetOutEdges();
         while(edge!=NULL)
         {
            if(edge->GetType()==EDGETYPE_LABEL)
            {
               if((negedge!=NULL)&&
                  (negedge->GetNextState()==(FSMState *)edge->nextstate->data))
                  // Is the target state the same as the one of the existing negedge ?
                  // ==> We simply remove the element from the negedge labellist
                  RemoveFromLabelList(&(negedge->labellist),edge->labelid);
               else
                  newfsm->CreateLabelEdge((FSMState *)curstate1->data,
                                          (FSMState *)edge->GetNextState()->data,
                                          edge->labelid);
            }
            edge=edge->next;
         }
      }
      curstate1=curstate1->next;
   }
   return newfsm;
}

//**********************************************************************
//**********************************************************************

char FSMState::PruneRedundantStatesRecurs()
   // Eliminates states that are either not reachable
   // from the start state or states that never lead
   // to final states
   // The function returns a value !=0, if the state leads to
   // a final state
{
   char reachesfinalstate=0;

   // We use 'tmpval' to represent the status information.
   // If bit 2 is set, then it means that the
   // state reaches a final state or is a final state itself.
   // If bit 1 is set, then the state can be reached from
   // the start state. Each state that is reached over this
   // recursive function is reachable from the start state
   if(isfinal)
      tmpval=3;
   else
      tmpval=1;

   FSMEdge *outedge=outedges;

   while(outedge!=NULL)
   {
      if((outedge->GetNextState()->tmpval&1)==0)
         // We haven't visited the next state so far ?
      {
         if(outedge->GetNextState()->PruneRedundantStatesRecurs())
            tmpval=3;
      }
      else
      {
         if(outedge->GetNextState()->tmpval==3)
            tmpval=3;
      }
      outedge=outedge->next;
   }
   return (tmpval==3);
}

void FSM::PruneRedundantStates()
// Eliminates states that will never lead to final states
// and states that are not reachable from the start state
{
   FSMState *curstate=statelist;
   FSMEdge  **outedgeref;

   // Let's set the 'tmpval' of each state first
   while(curstate!=NULL)
   {
      curstate->tmpval=0;
      curstate=curstate->next;
   }
   startstate->PruneRedundantStatesRecurs();

   FSMState **curstateref=&statelist;

   curidx=0;

   while(*curstateref!=NULL)
   {
      if((*curstateref)->tmpval!=3)
         // The state is either not reachable from the startstate or
         // it does not lead to a final state?
      {
         *curstateref=(*curstateref)->next;
      }
      else
      {
         (*curstateref)->stateid=curidx;

         outedgeref=&((*curstateref)->outedges);
         while(*outedgeref!=NULL)
         {
            if((*outedgeref)->GetNextState()->tmpval!=3)
               // Edge leads to a redundant state? ==> Delete edge
               *outedgeref=(*outedgeref)->next;
            else
               outedgeref=&((*outedgeref)->next);
         }
         curidx++;
         curstateref=&((*curstateref)->next);
      }
   }
   *curstateref=NULL;
}

//**********************************************************************
//**********************************************************************

void FSMState::EliminateRedundantPoundEdges()
   // This function eliminate redundant outgoing pound-edges
   // Pound-edges are not needed, if all cases are captured
   // by an outgoing negative edge
{
   FSMEdge  *outedge=GetOutEdges();
   FSMLabel *curlabel;

   FSMEdge  *elementpoundedge=NULL;
   FSMEdge  *attribpoundedge=NULL;
   FSMEdge  *negedge=NULL;

   // First, we find out whether there are pound edges
   // and a negative edge
   // we store this information in 'elementpoundedge', 'attribpoundedge'
   // and 'negedge'

   outedge=GetOutEdges();

   while(outedge!=NULL)
   {
      switch(outedge->GetType())
      {
      case EDGETYPE_LABEL:
         if(outedge->GetLabelID()==elementpoundlabelid)
            elementpoundedge=outedge;
         else
         {
            if(outedge->GetLabelID()==attribpoundlabelid)
               attribpoundedge=outedge;
         }
         break;

      case EDGETYPE_NEGLABELLIST:
         negedge=outedge;
         break;
      }
      outedge=outedge->next;
   }

   // We only need to consider the case when there is a negative edge
   if(negedge==NULL)
      return;

   // Nothing to delete ?
   if((attribpoundedge==NULL)&&
      (elementpoundedge==NULL))
      return;  

   // We traverse the list of labels in the negative list
   // and check whether there is a corresponding positive label
   // If not, then the label is not captured by negative or positive labels
   // but will be captured by the pound!
   curlabel=negedge->labellist;
   while(curlabel==NULL)
   {
      outedge=GetOutEdges();
      while(outedge!=NULL)
      {
         if(outedge->GetType()==EDGETYPE_LABEL)
         {
            if(outedge->GetLabelID()==curlabel->labelid)
               break;
         }
         outedge=outedge->next;
      }
      if(outedge==NULL) // We didn't find the label ?
      {
         if(ISATTRIB(curlabel->labelid))  // ==> We don't have to remove the
                                          // the corresponding pound edge, since
                                          // it captures that label
            attribpoundedge=NULL;
         else
            elementpoundedge=NULL;
      }
      curlabel=curlabel->next;
   }

   // Now, we need to remove the pound-edges

   if((attribpoundedge==NULL)&&
      (elementpoundedge==NULL))
      return;  // Nothing to do ?

   FSMEdge **edgeref=&outedges;

   while(*edgeref!=NULL)
   {
      if((*edgeref==attribpoundedge)||
         (*edgeref==elementpoundedge))
         *edgeref=(*edgeref)->next;
      else
         edgeref=&((*edgeref)->next);
   }
}

void FSM::EliminateRedundantPoundEdges()
   // This function eliminate redundant outgoing pound-edges
   // Pound-edges are not needed, if all cases are captured
   // by an outgoing negative edge
{
   FSMState *state=statelist;
   while(state!=NULL)
   {
      state->EliminateRedundantPoundEdges();
      state=state->next;
   }
}
//**********************************************************************
//**********************************************************************

void FSMState::ComputeOutCompleteness()
   // Computes for the state whether it is out-complete,
   // i.e. whether there exists an edge for each possible label
{
   FSMEdge  *outedge=GetOutEdges();
   FSMLabel *curlabel;
   FSMEdge  *elementpoundedge=NULL,
            *attribpoundedge=NULL,
            *negedge=NULL;

   while(outedge!=NULL)
   {
      switch(outedge->GetType())
      {
      case EDGETYPE_LABEL:
         if(outedge->GetLabelID()==elementpoundlabelid)  // Do we have an element-pound sign? => The state is out-complete
         {
            if(attribpoundedge!=NULL)  // Do we also have an attribute-pound edge ?
                                       // ==> We are complete
            {
               isoutcomplete=1;
               return;
            }
            elementpoundedge=outedge;
         }
         else
         {
            if(outedge->GetLabelID()==attribpoundlabelid)  // Do we have a attribute-pound sign? => The state is out-complete
            {
               if(elementpoundedge!=NULL)  // Do we also have an element-pound edge ?
                                          // ==> We are complete
               {
                  isoutcomplete=1;
                  return;
               }
               attribpoundedge=outedge;
            }
         }
         break;

      case EDGETYPE_NEGLABELLIST:
         negedge=outedge;
         break;
      }
      outedge=outedge->next;
   }

   if(negedge==NULL) // We didn't have a negative edge
                     // (And we didn't find *two* pound edges ? ==> Incomplete
   {
      isoutcomplete=0;
      return;
   }

   // We test whether each negative label occurs as a positive one
   curlabel=negedge->GetLabelList();
   while(curlabel!=NULL)
   {
      outedge=GetOutEdges();
      while(outedge!=NULL)
      {
         if(outedge->GetType()==EDGETYPE_LABEL)
         {
            if(outedge->GetLabelID()==curlabel->labelid)
               break;
         }
         outedge=outedge->next;
      }
      if(outedge==NULL)
         // We didn't find a corresponding positive label?
         // We check if we have a corresponding pound-edge
      {
         // There is no corresponding pound-edge ? ==> Incomplete
         if((ISATTRIB(curlabel->labelid)&&(attribpoundedge==NULL))||
            ((!ISATTRIB(curlabel->labelid))&&(elementpoundedge==NULL)))
         { 
            isoutcomplete=0;
            return;
         }
      }
      curlabel=curlabel->next;
   }
   // Each of the negative labels either occurred as a positive label
   // or there exists a corresponding pound-edge
   // I.e. the state is out-complete

   isoutcomplete=1;
}

void FSM::ComputeOutCompleteness()
   // Computes for each state whether it is out-complete,
   // i.e. whether there exists an edge for each possible label
{
   FSMState *curstate=statelist;

   while(curstate!=NULL)
   {
      curstate->ComputeOutCompleteness();
      curstate=curstate->next;
   }
}

char FSMState::FindAcceptingStatesRecurs()
   // Sets attribute 'isaccepting' depending on whether
   // all reachable states for this state are final
{
   tmpval=1;

   isaccepting=(isfinal && IsOutComplete());
      // For now, we simply assume that the state is accepting
      // (Only final, out-complete states can be accepting!)
      // This is important for cycles in the recursion !

   FSMEdge *outedge=outedges;

   while(outedge!=NULL)
   {
      if((outedge->GetNextState()->tmpval&1)==0)
         // We haven't visited the next state so far ?
      {
         if(outedge->GetNextState()->FindAcceptingStatesRecurs()==0)
            // If the next state is not accepting, then this one is not too
            isaccepting=0;
      }
      else
      {
         if(outedge->GetNextState()->isaccepting==0)
            // If the next state is not accepting, then this one is not too
            isaccepting=0;
      }
      outedge=outedge->next;
   }

   // All outgoing edges lead to accepting states ==> We are also accepting
   return isaccepting;
}

void FSM::FindAcceptingStates()
   // Determines all (final) states that will definitely lead to an acceptance of
   // the word, no matter what comes afterwards
   // In other words: all following states are final states
{
   FSMState *curstate=statelist;

   // First, let's compute the out-completeness of all states
   // States that are not out-complete can never be accepting
   ComputeOutCompleteness();

   // We initialize the 'tmpval' elements to '0'
   while(curstate!=NULL)
   {
      curstate->tmpval=0;
      curstate=curstate->next;
   }
   // We compute the accepting status, starting with 'startstate'
   startstate->FindAcceptingStatesRecurs();
}

//**********************************************************************
//**********************************************************************

char FSMState::HasPoundsAheadRecurs()
   // Determines for the state whether there are pound-edges
   // reachable from that state
{
   tmpval=1;

   haspoundsahead=0;

   FSMEdge *outedge=outedges;

   while(outedge!=NULL)
   {
      if((outedge->GetNextState()->tmpval&1)==0)
         // We haven't visited the next state so far ?
      {
         if(outedge->GetNextState()->HasPoundsAheadRecurs()!=0)
            // If the next state has a pound connection, then this one has too
            haspoundsahead=1;
      }
      else
      {
         if(outedge->GetNextState()->haspoundsahead)
            // If the next state is a pound connection, then this one has too
            haspoundsahead=1;
      }

      if(outedge->GetType()==EDGETYPE_LABEL)
      {
         if((outedge->GetLabelID()==elementpoundlabelid)||
            (outedge->GetLabelID()==attribpoundlabelid))

            haspoundsahead=1;
      }
      outedge=outedge->next;
   }

   // No outgoing edges has pounds ahead
   return haspoundsahead;
}

void FSM::ComputeStatesHasPoundsAhead()
{
   FSMState *curstate=statelist;

   // We initialize the 'tmpval' elements to '0'
   while(curstate!=NULL)
   {
      curstate->tmpval=0;
      curstate=curstate->next;
   }
   // We compute the accepting status, starting with 'startstate'
   startstate->HasPoundsAheadRecurs();
}

//**********************************************************************
//**********************************************************************

#ifdef FSM_NEGATE

void FSM::CompleteDetState(FSMState *state,FSMState *deadstate)
   // Completes the deterministic state by adding edges for the missing
   // labels. The edges lead to 'deadstate'
{
   FSMEdge *outedge=state->GetOutEdges();
   FSMLabel *labellist=NULL,**ref;
   FSMLabel *curlabel;
   char     isneglist=0,deleted;

   while(outedge!=NULL)
   {
      switch(outedge->GetType())
      {
      case EDGETYPE_LABEL:
         if(isneglist==0)  // We have a list of positive labels?
         {
            if(!IsInLabelList(labellist,outedge->labelid))
               // We only insert if the label is not already in the list
               labellist=new(fsmtmpmem) FSMLabel(outedge->labelid,labellist);
         }
         else
         {
            // In this case, the label *must* be in 'labellist'
            // and we remove it from the list
            deleted=0;

            ref=&labellist;
            while(*ref!=NULL)
            {
               if((*ref)->labelid==outedge->labelid)
               {
                  *ref=(*ref)->next;   // We simply eliminate the item
                  deleted=1;
                  break;
               }
               ref=&((*ref)->next);
            }
            if(deleted==0) // If the label did not occur in the list, then
                           // we have a problem !
            {
               Error("Fatal error in FSM::CompleteDetState()");
               Exit();
            }
         }
         break;

      case EDGETYPE_NEGLABELLIST:
         if(isneglist)  // If we already have a negative list, then something
                        // is severely wrong !!!
         {
            Error("Fatal error in FSM::CompleteDetState()");
            Exit();
         }
         isneglist=1;

         curlabel=outedge->GetLabelList();
         while(curlabel!=NULL)
         {
            deleted=0;

            ref=&labellist;
            while(*ref!=NULL)
            {
               if((*ref)->labelid==curlabel->labelid)
               {
                  *ref=(*ref)->next;   // We simply eliminate the item
                  deleted=1;
                  break;
               }
               ref=&((*ref)->next);
            }
            if(deleted==0) // If the label did not occur in the list, then
                           // we add it to the list
               labellist=new (fsmtmpmem) FSMLabel(curlabel->labelid,labellist);

            curlabel=curlabel->next;
         }
      }
      outedge=outedge->next;
   }

   if(isneglist==1)
      // Negative list? => We create set of edges to 'deadstate'
   {
      ref=&labellist;
      while(*ref!=NULL)
      {
         CreateLabelEdge(state,deadstate,(*ref)->labelid);
         ref=&((*ref)->next);
      }
   }
   else
      CreateNegEdge(state,deadstate,labellist);
}

void FSM::CompleteDetFSM()
   // This function adds edges (and one state) to an FSM so that it
   // becomes complete -- i.e. there is an outgoing edge for every state
   // and every possible label
{
   // We create a non-final state, which is a deadend-state
   FSMState *deadstate,*curstate;
   FSMEdge  *loopedge;

   deadstate=CreateState((char)0);

   // We create a loop edge for deadstate, so that we
   // stay in 'deadstate' forever.
   loopedge=CreateNegEdge(deadstate,deadstate,NULL);

   // For every state, we must add another edge to 'deadstate'
   // which takes care of all labels that are not mentioned in
   // already existing edges
   curstate=statelist;

   while(curstate!=NULL)
   {
      // We complete each of the states
      CompleteDetState(curstate,deadstate);
      curstate=curstate->next;
   }
}

FSM *FSM::CreateNegateFSM()
   // This function negates the given FSM
   // I.e. it creates an FSM that accepts exactly the words
   // that are not accepted by 'this'
{
   FSM      *detfsm;
   FSMState *curstate;

   // First, we make the FSM deterministic
   detfsm=MakeDeterministic();

   // We make the FSM complete
   detfsm->CompleteDetFSM();

   // We consider each single state and toggle the final status

   curstate=detfsm->GetStateList();

   while(curstate!=NULL)
   {
      if(curstate->IsFinal())
         curstate->isfinal=0;
      else
         curstate->isfinal=1;

      curstate=curstate->next;
   }

   detfsm=detfsm->Minimize();

   return detfsm;
}

#endif

//**********************************************************************
//**********************************************************************

void FSM::AddFSM(FSMState *fromstate,FSMState *tostate,FSM *fsm)
   // Adds the FSM 'fsm' into the current FSM between 'fromstate' and 'tostate'
{
   FSMState *curstate=fsm->GetStateList(),*newstate;

   // First, we create all the states and
   // we set 'curstate->data' to the new state
   while(curstate!=NULL)
   {
      // For the start state, we simply use the 'fromstate' state
      if(curstate==fsm->GetStartState())
         curstate=fromstate;
      else
      {
         newstate=CreateState((char)0);
         curstate->data=newstate;
      }
      if(curstate->IsFinal())
         // For final states, we create an empty edge to 'tostate'
         CreateEmptyEdge((FSMState *)(curstate->data),tostate);

      curstate=curstate->next;
   }

   // Now, we can take care of the edges
   curstate=fsm->GetStateList();

   FSMEdge  *outedge;

   // We only need to copy the edges

   while(curstate!=NULL)
   {
      outedge=curstate->GetOutEdges();
      while(outedge!=NULL)
      {
         switch(outedge->GetType())
         {
         case EDGETYPE_LABEL:
            CreateLabelEdge((FSMState *)(curstate->data),
                            (FSMState *)(outedge->GetNextState()->data),
                            outedge->labelid);
            break;

         case EDGETYPE_NEGLABELLIST:
         {
            FSMLabel *newlabellist=NULL;

            DupLabelList(outedge->labellist,&newlabellist);

            CreateNegEdge((FSMState *)(curstate->data),
                              (FSMState *)(outedge->GetNextState()->data),
                              newlabellist);
            break;
         }

         case EDGETYPE_EMPTY:
            CreateEmptyEdge((FSMState *)(curstate->data),
                              (FSMState *)(outedge->GetNextState()->data));
            break;
         }

         outedge=outedge->next;
      }
      curstate=curstate->next;
   }
}

//**********************************************************************
//**********************************************************************
//**********************************************************************

FSM *FSM::CreateReverseFSM()
   // Reverses a non-deterministic FSM - i.e. the resulting FSM
   // accepts the reverse language L^R, if and only if FSM accepts L.
{
   FSMState *newstate;
   FSMEdge  *edge;

   FSM *newfsm=new(fsmmem) FSM();

   // We create a new start state
   // This start state will be connected to all final states 
   // of the original FSM
   FSMState *newstartstate=newfsm->CreateState();

   newfsm->SetStartState(newstartstate);

   FSMState *curstate=statelist;

   // First, we copy all the states
   // The start state becomes the new final state
   // Furthermore, if the original state is final, then it becomes
   // a new start state - we simulate this by creating an empty edge
   // between 'newstartstate' and 'newstate'
   while(curstate!=NULL)
   {
      newstate=newfsm->CreateState(curstate==startstate);

      if(curstate->IsFinal())
         newfsm->CreateEmptyEdge(newstartstate,newstate);

      curstate->data=newstate;
      curstate=curstate->next;
   }

   // Now, we consider all states and we create the edges
   curstate=statelist;
   
   while(curstate!=NULL)
   {
      edge=curstate->GetOutEdges();
      while(edge!=NULL)
      {
         switch(edge->GetType())
         {
         case EDGETYPE_LABEL:
            newfsm->CreateLabelEdge((FSMState *)(edge->GetNextState()->data),
                                    (FSMState *)(curstate->data),
                                    edge->GetLabelID());
            break;

         case EDGETYPE_NEGLABELLIST:
         {
            FSMLabel *newlabellist=NULL;

            DupLabelList(edge->GetLabelList(),&newlabellist);

            newfsm->CreateNegEdge(  (FSMState *)(edge->GetNextState()->data),
                                    (FSMState *)(curstate->data),
                                    newlabellist);
            break;
         }
         case EDGETYPE_EMPTY:
            newfsm->CreateEmptyEdge((FSMState *)(edge->GetNextState()->data),
                                    (FSMState *)(curstate->data));
            break;
         }
         edge=edge->next;
      }
      curstate=curstate->next;
   }
   return newfsm;
}

//****************************************************************************
//****************************************************************************
//****************************************************************************
//****************************************************************************

#ifdef FSM_PRINT

void FSMState::Print()
   // Prints information about the state
{
   printf("%lu",stateid);
   if(isfinal)
      printf(":F");

   if(isoutcomplete)
      printf(":C");

   if(isaccepting)
      printf(":A");

   if(haspoundsahead)
      printf(":P");

/*
   if(origstateset!=NULL)
   {
      FSMStateSetItem *state=origstateset;
      printf("  {");

      while(state!=NULL)
      {
         state->state->Print();
         state=state->next;
         if(state!=NULL)
            printf(",");
      }
      printf("}");
   }
*/
}

void FSMEdge::PrintLabelList()
   // Prints the labellist of an EDGETYPE_LABELLIST edge
{
   FSMLabel *curlabel=labellist;

   while(curlabel!=NULL)
   {
         globallabeldict.PrintLabel(curlabel->labelid);

      curlabel=curlabel->next;
      if(curlabel!=NULL)
         printf("|");
   }
}

void FSMEdge::Print()
   // Prints the information about an edge
{
   switch(type)
   {
   case EDGETYPE_LABEL:
         globallabeldict.PrintLabel(labelid);
      break;
      
   case EDGETYPE_NEGLABELLIST:
      printf("ALL BUT ");
      PrintLabelList();
      break;

   case EDGETYPE_EMPTY:
      break;
   }
}

//*********************************************************************
//*********************************************************************

void FSM::Print()
   // Prints the structure of the FSM
{
   FSMState *curstate=statelist;
   FSMEdge  *edge;

   while(curstate!=NULL)
   {
      edge=curstate->GetOutEdges();

      while(edge!=NULL)
      {
         curstate->Print();
         printf(" -- ");
         edge->Print();
         printf(" --> ");
         edge->GetNextState()->Print();
         printf("\n");

         edge=edge->next;
      }
      curstate=curstate->next;
   }
}

#endif // FSM_PRINT

//*********************************************************************
//*********************************************************************
//*********************************************************************

#ifdef FSM_STORE

void FSMEdge::Store(MemStreamer *mem)
// Stores the information about the edge in 'mem'
{
   // We store the edge type
   // EDGETYPE_LABEL, EDGETYPE_NEGLABELLIST (, or EDGETYPE_EMPTY)

   mem->StoreUInt32(type);
   mem->StoreUInt32(nextstate->GetStateID());

   switch(type)
   {
   case EDGETYPE_LABEL:
      mem->StoreUInt32(labelid);
      break;

   case EDGETYPE_NEGLABELLIST:
   {
      FSMLabel *curlabel=labellist;
      int      labelcount=0;

      // First, let's count the number of labels

      while(curlabel!=NULL)
      {
         labelcount++;
         curlabel=curlabel->next;
      }

      // We store the labelcount
      mem->StoreUInt32(labelcount);

      // Now, we can store the label ID's
      curlabel=labellist;

      while(curlabel!=NULL)
      {
         mem->StoreUInt32(curlabel->labelid);
         curlabel=curlabel->next;
      }
   }
   }
}

void FSMEdge::Load(unsigned char * &ptr,FSMState *statearray,MemStreamer *fsmmem)
{
   type=LoadUInt32(ptr);

   nextstate=statearray+LoadUInt32(ptr);

   switch(type)
   {
   case EDGETYPE_LABEL:
      labelid=(unsigned short)LoadUInt32(ptr);;
      break;

   case EDGETYPE_NEGLABELLIST:
   {
      FSMLabel **labelref=&labellist;
      unsigned long labelcount;
      
      labelcount=LoadUInt32(ptr);

      while(labelcount--)
      {
         *labelref=new(fsmmem) FSMLabel((unsigned short)LoadUInt32(ptr));
         labelref=&((*labelref)->next);
      }
      *labelref=NULL;
   }
   }
}

//*********************************************************************
//*********************************************************************

void FSMState::Store(MemStreamer *mem)
{
   // We store whether the state is final
   mem->StoreUInt32(isfinal);

   // Let's count the number of outgoing edges

   FSMEdge *curedge=outedges;
   unsigned long edgecount=0;

   while(curedge!=NULL)
   {
      edgecount++;
      curedge=curedge->next;
   }

   // We store the number of outgoing edges
   mem->StoreUInt32(edgecount);

   // Now we store the actual edges
   curedge=outedges;
   while(curedge!=NULL)
   {
      curedge->Store(mem);
      curedge=curedge->next;
   }
}

void FSMState::Load(unsigned char * &ptr,FSMState *statearray,MemStreamer *fsmmem)
{
   unsigned long edgecount;

   // We load the status bit
   isfinal=LoadUInt32(ptr);

   FSMEdge  **edgeref =&outedges;

   // We load the number of outgoing edges

   edgecount=LoadUInt32(ptr);

   // We load all edges
   while(edgecount--)
   {
      *edgeref=new(fsmmem) FSMEdge();

      (*edgeref)->Load(ptr,statearray,fsmmem);
      edgeref=&((*edgeref)->next);
   }
   *edgeref=NULL;
}

//*********************************************************************
//*********************************************************************

void FSM::Store(MemStreamer *mem)
// Stores the information in the FSM in 'mem'
{
   mem->StoreUInt32(curidx);

   // We store the ID of the start state
   mem->StoreUInt32(startstate->GetStateID());

   FSMState *curstate=statelist;

   // We store all the states and their outgoing edges
   while(curstate!=NULL)
   {
      curstate->Store(mem);
      curstate=curstate->next;
   }
}

void FSM::Load(unsigned char * &ptr,MemStreamer *fsmmem)
{
   // We load the number of states
   curidx=LoadUInt32(ptr);

   unsigned long startstateidx;

   startstateidx=LoadUInt32(ptr);

   FSMState **stateref=&statelist;
   FSMState *statearray=new(fsmmem) FSMState[curidx];

   for(unsigned i=0;i<curidx;i++)
   {
      *stateref=statearray+i;
      (*stateref)->stateid=i;
      (*stateref)->next=NULL;
      (*stateref)->origstateset=NULL;

      (*stateref)->Load(ptr,statearray,fsmmem);

      stateref=&((*stateref)->next);
   }
   *stateref=NULL;

   startstate=statearray+startstateidx;
}

#endif // FSM_STORE

//**********************************************************************
//**********************************************************************
//**********************************************************************
//**********************************************************************
/*
struct IntersectState
{
   FSMState       *intersectstate;
   FSMState       *state1;
   FSMState       *state2;
   IntersectState *next_todo;
};

IntersectState *intersect_statearray;
IntersectState *todo_list;
unsigned       statecount2;

inline FSMState *FSM::CreateIntersectState(FSMState *origstate1,FSMState *origstate2)
{
   IntersectState *ref=intersect_statearray+origstate1->GetStateID()*statecount2+origstate2->GetStateID();
   if(ref->intersectstate==NULL)
   {
      ref->intersectstate=CreateState();
      ref->next_todo=todo_list;
      todo_list=ref;
   }
   return ref->intersectstate;
}

void FSM::IntersectFSMS(FSM *fsm1,FSM *fsm2)
{
   FSMState *state1,*state2,*newstate;

   unsigned statecount1=fsm1->GetStateCount();

   statecount2=fsm2->GetStateCount();

   intersect_statearray=new IntersectState[statecount1*statecount2];

   todo_list=NULL;

   SetStartState(
      CreateIntersectState(fsm1->GetStartState(),fsm2->GetStartState())
   );

   while(todo_list!=NULL)
   {
      state1=todo_list->state1;
      state2=todo_list->state2;
      newstate=todo_list->intersectstate;

      todo_list=todo_list->next_todo;

      curedge1=state1->GetOutEdges();

      while(curedge1!=NULL)
      {
         switch(curedge1->GetType())
         {
         case EDGETYPE_LABEL:
            curedge2=state2->GetOutEdges();
            while(curedge2!=NULL)
            {
               if(curedge2->DoesMatchLabel(curedge1->GetLabelID())
               {
                  CreateLabelEdge(
                        newstate,
                        CreateIntersectState(curedge1->GetNextState(),curedge2->GetNextState()),
                        curedge1->GetLabelID());
               }
               curedge2=curedge2->next;
            }
            break;

         case EDGETYPE_NEGLABELLIST:
            curedge2=state2->GetOutEdges();
            while(curedge2!=NULL)
            {
               switch(curedge2->type)
               {
               case EDGETYPE_LABEL:
                  if(curedge1->DoesMatchLabel(curedge2->GetLabelID())
                  {
                     CreateLabelEdge(
                        newstate,
                        CreateIntersectState(curedge1->GetNextState(),curedge2->GetNextState()),
                        curedge2->GetLabelID());
                  }
                  break;
               case EDGETYPE_NEGLABELLIST:
                  CreateNegLabelEdge(newstate,
                     CreateIntersectState(curedge1->GetNextState(),curedge2->GetNextState()),
                     UnionNegLabelList(curedge1->GetLabelList(),curedge2->GetLabelList()));
               }
               curedge2=curedge2->next;
            }
         }
         curedge1=curedge1->next
      }
   }

   delete[] intersect_statearray;
}

*/