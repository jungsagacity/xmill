#include "LabelDict.hpp"
#include "MemStreamer.hpp"

class FSMState;
class FSM;

extern MemStreamer *vregexprmem; // Identifiers the global memory
                                 // that is used to storing all regular expressions
                                 // In the current version, this is simply 'tmpmem'

//*************************************************************************************
//*************************************************************************************

#define VP_ITEMTYPE_LABEL     0
#define VP_ITEMTYPE_DONTCARE  1
#define VP_ITEMTYPE_POUND     12
#define VP_ITEMTYPE_STAR      2
#define VP_ITEMTYPE_PLUS      3
#define VP_ITEMTYPE_OPT       4
#define VP_ITEMTYPE_NOT       5
#define VP_ITEMTYPE_EMPTY     6
#define VP_ITEMTYPE_GROUP     7

#define VP_ITEMTYPE_MINUS     8
#define VP_ITEMTYPE_SEQ       9
#define VP_ITEMTYPE_ALT       10
#define VP_ITEMTYPE_AND       11


#define MAX_LEN   ((unsigned short)65535)

class VRegExpr
{
   unsigned       type:5;           // The type of regular expression

   union{
      TLabelID       labelid;       // The regular expression could be a label
      struct                        // Or a pair of two children
      {
         VRegExpr    *child1;
         VRegExpr    *child2;
      }  children;
   };

   void CreateFSMEdges(FSM *fsm,FSMState *fromstate,FSMState *tostate);

public:
   void *operator new(size_t size)  {  return vregexprmem->GetByteBlock(size);  }
   void operator delete(void *ptr)  {}

   VRegExpr(TLabelID mylabelid)
   {
      type=VP_ITEMTYPE_LABEL;
      labelid=mylabelid;
   }

   VRegExpr(int mytype,VRegExpr *child1=NULL,VRegExpr *child2=NULL)
   {
      type=mytype;
      children.child1=child1;
      children.child2=child2;
   }

   void PrintLabelList();
   void Print();

   FSM *CreateNonDetFSM();
   FSM *CreateFSM();

   static TLabelID ParseLabel(char **str,char *endptr);
   static VRegExpr *ParseVRegExpr(char * &str,char *endptr);
};

//************************************************************************************
//************************************************************************************
