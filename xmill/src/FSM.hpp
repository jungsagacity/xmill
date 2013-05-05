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

#ifndef FSM_HPP
#define FSM_HPP

#include "Types.hpp"
#include "MemStreamer.hpp"

//#define FSM_NEGATE    // Enables Negation of FSMs
//#define FSM_PRINT     // Enables Print-functions for FSMs
//#define FSM_STORE     // Enables Store/Load functions for FSMs

// We distinguish three types of edges:

#define EDGETYPE_LABEL        0     // A normal label edge
                                    // A particular instance of label edges are
                                    // pound-edges - i.e. edges with the '#' symbol!
#define EDGETYPE_NEGLABELLIST 1     // An edge that captures everything minus certain labels
#define EDGETYPE_EMPTY        2     // An empty edge. This is only used for
                                    // non-deterministic FSMs

class MemStreamer;
class Input;

// We use the following two MemStreamers
extern MemStreamer *fsmtmpmem;   // For temporary transformations -
                                 // used for example for making FSMs deterministic or minimal
extern MemStreamer *fsmmem;      // For usual objects, such as FSM edges, states


struct FSMLabel
   // A label in the FSM within the 'EDGETYPE_NEGLABELLIST' edge
   // Labels are concatenated in a single-chained list
{
   TLabelID labelid; // The label ID
   FSMLabel *next;   // The next label

   FSMLabel(TLabelID mylabelid,FSMLabel *mynext=NULL)
   {
      labelid=mylabelid;
      next=mynext;
   }

   void *operator new(size_t size, MemStreamer *mem);
#ifdef SPECIAL_DELETE
   void operator delete(void *ptr,MemStreamer *mem) {}
#endif
   void operator delete(void *ptr) {}
};

//**************************************************************************
//**************************************************************************

struct FSMStateSetItem;
class FSMEdge;
class FSM;

class FSMState
   // Represents a state in the FSM
{
   friend FSM;
   friend FSMEdge;
   unsigned isfinal:1;        // Final state?
   unsigned isaccepting:1;    // Determines whether the FSM in that (final) state
                              // will definitely accept
                              // the word, no matter what comes afterwards
                              // In other words: all following states are final states
   unsigned isoutcomplete:1;  // Describes whether the outgoing edges capture *all*
                              // cases of labels
   unsigned haspoundsahead:1; // Is true, iff there are pound-edge that can be
                              // reached from this state
   unsigned stateid:10;       // The identifier of the state
   FSMEdge  *outedges;        // The list of outgoing edges
   FSMState *next;            // The next state (all states in the FSM are concatenated in a list)

   // Depending on the current type of state, we keep some additional info
   union
   {
      FSMStateSetItem   *origstateset; // This is used for the conversion from nondet=>det
                                       // Contains a reference to the set of nondet. states
                                       // represented by this det. state
      void              *data;
      long              tmpval;
   };

   char PruneRedundantStatesRecurs();  // Markes states that are either not reachable
                                       // from the start state or states that never lead
                                       // to final states
   char FindAcceptingStatesRecurs();   // Sets attribute 'isaccepting' for the states appropriately
   char HasPoundsAheadRecurs();        // Determines for the state whether there are pound-edges
                                       // reachable from that state

   void EliminateRedundantPoundEdges();// This function eliminate redundant outgoing pound-edges
                                       // Pound-edges are not needed, if all cases are captured
                                       // by an outgoing negative edge

   void ComputeOutCompleteness();      // Determines whether the state is 'out-complete'
                                       // and sets 'outcomplete' accordingly

public:

   FSMState()  {}
   FSMState(unsigned mystateid,char myisfinal=0);
   FSMState(unsigned mystateid,FSMStateSetItem *myorigdataset);

   void *operator new(size_t size, MemStreamer *mem)  {  return mem->GetByteBlock(size);}
   void operator delete(void *ptr)  {}
   void *operator new[](size_t size, MemStreamer *mem)   {  return mem->GetByteBlock(size);}
   void operator delete[](void *ptr)   {}
#ifdef SPECIAL_DELETE
   void operator delete(void *ptr,MemStreamer *mem)  {}
   void operator delete[](void *ptr,MemStreamer *mem)  {}
#endif

   FSMEdge *GetOutEdges()              {  return outedges;  }
   FSMStateSetItem *GetOrigStateSet()  {  return origstateset; }

   void SetFinal(char myisfinal=1)     {  isfinal=myisfinal;   }
   char IsFinal()                      {  return isfinal;   }
   char IsOutComplete()                {  return isoutcomplete;   }
   char IsAccepting()                  {  return isaccepting;   }
   char HasPoundsAhead()               {  return haspoundsahead;  }
   int GetStateID()                    {  return stateid; }
   FSMState *GetNextStateInList()      {  return next;   }

   FSMEdge *FindNegEdge();    // Find the one negative outgoing edge for this state
                              // if there is one

   FSMState *GetNextState(TLabelID labelid,char *overpoundedge=NULL);
      // Determines the next state (in an det. FSM!) that would be reached
      // by reading label 'labelid'. '*overpoundedge' is set to 1, if the
      // corresponding transition edge is a pound-edge.
#ifdef FSM_PRINT
   void Print();  // Prints the state and the outgoing edges
#endif
#ifdef FSM_STORE
   void Store(MemStreamer *mem); // Stores the state and the outgoing edges
                                 // in memstreamer 'mem'

   void Load(unsigned char * &ptr,FSMState *statearray,MemStreamer *fsmmem);
      // Initializes the state and loads the outgoing edges from the data in 'ptr'
#endif
};

//********************************************************************************

class FSMEdge
   // Represents an edge in the FSM
{
   friend FSMState;
   friend FSM;

   unsigned type;          // Type of the edge (EDGETYPE_...)
   FSMState *nextstate;    // The state that is reached by this edge

   // Depending on the type, we store different info
   union{
      FSMLabel *labellist; // EDGETYPE_NEGLABELLIST ==> We store a list of labels
      TLabelID labelid;    // EDGETYPE_LABEL ==> We store a single label
   };

public:
   FSMEdge  *next;   // The next outgoing edge of the same state

   void *operator new(size_t size, MemStreamer *mem);
   void operator delete(void *ptr) {}
#ifdef SPECIAL_DELETE
   void operator delete(void *ptr,MemStreamer *mem)  {}
#endif

   FSMEdge()   {}

   FSMEdge(FSMState *tostate,TLabelID mylabel);
   FSMEdge(FSMState *tostate);
   FSMEdge(FSMState *tostate,FSMLabel *mylabels);

   unsigned GetType()            {  return type;   }
   FSMState *GetNextState()      {  return nextstate; }
   FSMLabel *GetLabelList()      {  return labellist; }
   FSMLabel **GetLabelListRef()  {  return &labellist; }
   TLabelID GetLabelID()         {  return labelid; }

   char DoesMatchLabel(TLabelID labelid);
      // Checks whether the edge matches label 'labelid'

#ifdef FSM_PRINT
   void PrintLabelList();  // Prints the label list, if the type is EDGETYPE_NEGLABELLIST
   void Print();           // Prints the edge information
#endif
#ifdef FSM_STORE
   void Store(MemStreamer *mem); // Stores the edge in memstreamer 'mem'
   void Load(unsigned char * &ptr,FSMState *statearray,MemStreamer *fsmmem);
      // Loads the edge information from the data in 'ptr'
#endif
};

//**************************************************************************
//**************************************************************************

struct StateEqualPair;
struct StateEqualPairListItem;

class FSM
   // The representation of a (deterministic or non-deterministic) FSM
{
   FSMState       *statelist,          // The list of FSM states
                  **laststatelistref;  // The reference to the last FSM state
   FSMState       *startstate;         // The start state
   unsigned long  isdeterministic:1;   // Is the automaton deterministic?
   unsigned       curidx:30;           // The number of states and at the same time
                                       // the index for the next state

   void PruneRedundantStates();  // Eliminates states that are not reachable from the
                                 // start state and states that never lead to final states

//**********
#ifdef FSM_NEGATE
   // Functions for negating a given FSM
   void CompleteDetState(FSMState *state,FSMState *deadstate);
      // Completes the deterministic state by adding edges for the missing
      // labels. The edges lead to 'deadstate'

   void CompleteDetFSM();
      // Completes all deterministic states in the FSM
#endif

public:

   inline void *operator new(size_t size, MemStreamer *mem)
   {
      return mem->GetByteBlock(size);
   }

   inline void operator delete(void *ptr) {}
#ifdef SPECIAL_DELETE
   inline void operator delete(void *ptr,MemStreamer *mem) {}
#endif

   FSM()
   {
      isdeterministic=0;
      curidx=0;
      statelist=NULL;
      laststatelistref=&statelist;
   }

   unsigned GetStateCount()   {  return curidx; }

   FSMState *CreateState(char isfinal=0);          // Creates a state
   FSMState *CreateState(FSMStateSetItem *list);   // Creates a deterministic state
                                                   // with a set of corresponding nondet. states

   FSMEdge *CreateLabelEdge(FSMState *fromstate,FSMState *tostate,TLabelID labelid);
      // Creates an EDGETYPE_LABEL edge
   FSMEdge *CreateNegEdge(FSMState *fromstate,FSMState *tostate,FSMLabel *labellist=NULL);
      // Creates an EDGETYPE_NEGLABELLIST edge
   FSMEdge *CreateEmptyEdge(FSMState *fromstate,FSMState *tostate);
      // Creates an EDGETYPE_EMPTY edge

   void SetStartState(FSMState *mystartstate)   {  startstate=mystartstate;   }
   FSMState *GetStartState()  {  return startstate;   }
   FSMState *GetStateList()   {  return statelist; }

   FSM *MakeDeterministic();  // The main function for making an FSM deterministic
   FSM *Minimize();           // The main function to minimize a deterministic FSM

   void AddFSM(FSMState *fromstate,FSMState *tostate,FSM *fsm);
      // Adds the FSM 'fsm' into the current FSM between 'fromstate' and 'tostate'

   void EliminateRedundantPoundEdges();
      // Eliminates states that will never lead to final states
      // and states that are not reachable from the start state

   void ComputeOutCompleteness();
      // Computes for the state whether it is out-complete,
      // i.e. whether there exists an edge for each possible label

   void FindAcceptingStates();
      // Determines all (final) states that will definitely lead to an acceptance of
      // the word, no matter what comes afterwards
      // In other words: all following states are final states

   void ComputeStatesHasPoundsAhead();
      // Determines all states that have 'pound'-edges ahead

#ifdef FSM_NEGATE
   FSM *CreateNegateFSM();    // Computes the negated FSM
#endif

   FSM *CreateReverseFSM();   // Computes the reverse FSM

#ifdef FSM_PRINT
   void Print();
#endif
#ifdef FSM_STORE
   void Store(MemStreamer *mem);
   void Load(unsigned char * &ptr,MemStreamer *fsmmem);
#endif
//************
/*
   FSMState *CreateIntersectState(FSMState *origstate1,FSMState *origstate2);
   void IntersectFSMS(FSM *fsm1,FSM *fsm2);
*/
};

extern TLabelID elementpoundlabelid;
extern TLabelID attribpoundlabelid;

#endif
