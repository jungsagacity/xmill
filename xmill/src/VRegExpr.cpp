#include "VRegExpr.hpp"
#include "FSM.hpp"

extern MemStreamer tmpmem;
extern MemStreamer mainmem;

MemStreamer *vregexprmem;

extern TLabelID elementpoundlabelid;
extern TLabelID attribpoundlabelid;

//****************************************************************************
//****************************************************************************

void VRegExpr::Print()
{
   printf("{");
   switch(type)
   {
   case VP_ITEMTYPE_LABEL:    globallabeldict.PrintLabel(labelid);break;
   case VP_ITEMTYPE_DONTCARE: printf("_");break;
   case VP_ITEMTYPE_POUND:    printf("#");break;
   case VP_ITEMTYPE_STAR:     children.child1->Print();printf("*");break;
   case VP_ITEMTYPE_PLUS:     children.child1->Print();printf("+");break;
   case VP_ITEMTYPE_OPT:      children.child1->Print();printf("?");break;
   case VP_ITEMTYPE_NOT:      children.child1->Print();printf("~");break;
   case VP_ITEMTYPE_EMPTY:    printf("()");;
   case VP_ITEMTYPE_GROUP:    printf("(");children.child1->Print();printf(")");break;
   case VP_ITEMTYPE_MINUS:    children.child1->Print();
                              printf("-");
                              children.child2->Print();
                              break;

   case VP_ITEMTYPE_SEQ:      children.child1->Print();
                              printf(",");
                              children.child2->Print();
                              break;

   case VP_ITEMTYPE_ALT:      children.child1->Print();
                              printf("|");
                              children.child2->Print();
                              break;

   case VP_ITEMTYPE_AND:      children.child1->Print();
                              printf("&");
                              children.child2->Print();
                              break;
   }
   printf("}");
}

//****************************************************************************
//****************************************************************************

FSM *VRegExpr::CreateNonDetFSM()
{
   FSM *fsm;
   FSMState *startstate,*finalstate;

   // We create the finite state automaton
   fsm=new(fsmtmpmem) FSM();

   startstate=fsm->CreateState();
   finalstate=fsm->CreateState(1);

   fsm->SetStartState(startstate);

   CreateFSMEdges(fsm,startstate,finalstate);

   return fsm;
}

void VRegExpr::CreateFSMEdges(FSM *newfsm,FSMState *fromstate,FSMState *tostate)
{
   FSMState       *newstate;

   switch(type)
   {
   case VP_ITEMTYPE_LABEL:
      newfsm->CreateLabelEdge(fromstate,tostate,labelid);
      return;

   case VP_ITEMTYPE_DONTCARE:
      newfsm->CreateNegEdge(fromstate,tostate);
      return;

   case VP_ITEMTYPE_POUND:
      newfsm->CreateLabelEdge(fromstate,tostate,elementpoundlabelid);
      newfsm->CreateLabelEdge(fromstate,tostate,attribpoundlabelid);
      return;

   case VP_ITEMTYPE_STAR:
      newstate=newfsm->CreateState();

      newfsm->CreateEmptyEdge(fromstate,newstate);

      children.child1->CreateFSMEdges(newfsm,newstate,newstate);

      newfsm->CreateEmptyEdge(newstate,tostate);
      return;

   case VP_ITEMTYPE_PLUS:
      newstate=newfsm->CreateState();

      children.child1->CreateFSMEdges(newfsm,fromstate,newstate);
      children.child1->CreateFSMEdges(newfsm,newstate,newstate);

      newfsm->CreateEmptyEdge(newstate,tostate);
      return;

   case VP_ITEMTYPE_OPT:
      children.child1->CreateFSMEdges(newfsm,fromstate,tostate);
      newfsm->CreateEmptyEdge(fromstate,tostate);
      return;

   case VP_ITEMTYPE_NOT:
   {
      FSM *fsm1,*negfsm1;
      fsm1=children.child1->CreateNonDetFSM();

      negfsm1=fsm1->CreateNegateFSM();
      newfsm->AddFSM(fromstate,tostate,negfsm1);
      return;
   }

   case VP_ITEMTYPE_EMPTY:
      newfsm->CreateEmptyEdge(fromstate,tostate);
      return;

   case VP_ITEMTYPE_GROUP:
      children.child1->CreateFSMEdges(newfsm,fromstate,tostate);
      return;

   case VP_ITEMTYPE_MINUS:
   {
      FSM *fsm1,*fsm2,*negfsm1,*combinefsm;
      FSMState *startstate,*finalstate;
      
      fsm1=children.child1->CreateNonDetFSM();
      fsm2=children.child2->CreateNonDetFSM();

      negfsm1=fsm1->CreateNegateFSM();

      combinefsm=new(fsmtmpmem) FSM();

      startstate=combinefsm->CreateState();
      finalstate=combinefsm->CreateState(1);

      combinefsm->SetStartState(startstate);

      combinefsm->AddFSM(startstate,finalstate,negfsm1);
      combinefsm->AddFSM(startstate,finalstate,fsm2);

      combinefsm=combinefsm->CreateNegateFSM();

      newfsm->AddFSM(fromstate,tostate,combinefsm);
      return;
   }

   case VP_ITEMTYPE_SEQ:
   {
      FSMState *middlestate=newfsm->CreateState();
      
      children.child1->CreateFSMEdges(newfsm,fromstate,middlestate);
      children.child2->CreateFSMEdges(newfsm,middlestate,tostate);
      return;
   }
   case VP_ITEMTYPE_ALT:
      children.child1->CreateFSMEdges(newfsm,fromstate,tostate);
      children.child2->CreateFSMEdges(newfsm,fromstate,tostate);
      return;

   case VP_ITEMTYPE_AND:
   {
      FSM *fsm1,*fsm2,*negfsm1,*negfsm2,*combinefsm;
      FSMState *startstate,*finalstate;
      
      fsm1=children.child1->CreateNonDetFSM();
      fsm2=children.child2->CreateNonDetFSM();

      negfsm1=fsm1->CreateNegateFSM();
      negfsm2=fsm2->CreateNegateFSM();

      combinefsm=new(fsmtmpmem) FSM();

      startstate=combinefsm->CreateState();
      finalstate=combinefsm->CreateState(1);

      combinefsm->AddFSM(startstate,finalstate,negfsm1);
      combinefsm->AddFSM(startstate,finalstate,negfsm2);

      combinefsm=combinefsm->CreateNegateFSM();

      newfsm->AddFSM(fromstate,tostate,combinefsm);
      return;
   }
   }
}

//****************************************************************************
//****************************************************************************
//****************************************************************************

// Saves the pointer to position where the error occurred
char *vregexprerrptr;   
char *vregexprerrstr;

TLabelID VRegExpr::ParseLabel(char **str,char *endptr)
   // Parses a single label - either of the form
   // <label> (for elements) or @label@ (for attributes)
{
   char  isattrib=0;

   if(**str=='<')
      isattrib=0;
   else
   {
      if(**str=='@')
         isattrib=1;
   }
   (*str)++;

   char *saveptr=*str;

   while(*str<endptr)
   {
      if(((isattrib==0)&&(**str=='>'))||
         ((isattrib==1)&&(**str=='@')))
      {
         (*str)++;
         return globallabeldict.GetLabelOrAttrib(saveptr,*str-saveptr-1,isattrib);
      }
      (*str)++;
   }

   Error("Could not find ending '>' in '");
   ErrorCont(saveptr,10);  // !!!
   Exit();
   return 0;
}

//*************************************************************************************
//*************************************************************************************
//*************************************************************************************
//*************************************************************************************

#define STACKITEM_REGEXPR     0
#define STACKITEM_OPENPARENC  1
#define STACKITEM_BINOPSTART  2
#define STACKITEM_SEQ         2
#define STACKITEM_MINUS       3
#define STACKITEM_ALT         4
#define STACKITEM_AND         5

struct RegExprStackItem
   // This data structure is used to represent a single item on
   // the operator stack. An item is either a regular expression
   // or an operator (see STACKITEM_...)
{
   RegExprStackItem  *prev;
   unsigned long     type;
   VRegExpr          *regexpr;

   void *operator new(size_t size)  {  return vregexprmem->GetByteBlock(size);  }
   void operator delete(void *ptr)  {}

   RegExprStackItem(VRegExpr *myregexpr,RegExprStackItem *myprev)
   {
      prev=myprev;
      type=STACKITEM_REGEXPR;
      regexpr=myregexpr;
   }

   RegExprStackItem(unsigned long mytype,RegExprStackItem *myprev)
   {
      prev=myprev;
      type=mytype;
      regexpr=NULL;
   }
};

inline char ConvertOpCode(unsigned long type)
{
   switch(type)
   {
   case STACKITEM_SEQ:  return VP_ITEMTYPE_SEQ;
   case STACKITEM_MINUS:return VP_ITEMTYPE_MINUS;
   case STACKITEM_ALT:  return VP_ITEMTYPE_ALT;
   case STACKITEM_AND:  return VP_ITEMTYPE_AND;
   }
   return 0;
}

inline void ReduceAll(RegExprStackItem **stackptr)
{
   VRegExpr *regexpr;

   while(((*stackptr)->prev!=NULL)&&
         ((*stackptr)->prev->type>=STACKITEM_BINOPSTART))
   {
      regexpr=new VRegExpr(ConvertOpCode((*stackptr)->prev->type),
                           (*stackptr)->prev->prev->regexpr,
                           (*stackptr)->regexpr);
      *stackptr=new RegExprStackItem(regexpr,(*stackptr)->prev->prev->prev);
   }
}

inline void HandleBinOp(RegExprStackItem  **stackptr,unsigned opcode)
{
   VRegExpr *regexpr;

   if((*stackptr==NULL)||((*stackptr)->type!=STACKITEM_REGEXPR))
   {
      Error("Binary operation !!\n");
      Exit();
   }

   while(((*stackptr)->prev!=NULL)&&
         ((*stackptr)->prev->type>=opcode))
   {
      regexpr=new VRegExpr(ConvertOpCode((*stackptr)->prev->type),
                           (*stackptr)->prev->prev->regexpr,
                           (*stackptr)->regexpr);
      *stackptr=new RegExprStackItem(regexpr,(*stackptr)->prev->prev->prev);
   }
   *stackptr=new RegExprStackItem(opcode,*stackptr);
}

inline void HandleRegExpr(RegExprStackItem  **stackptr,VRegExpr *regexpr)
   // Adds a new expression to the stack
{
   if((*stackptr!=NULL)&&
      ((*stackptr)->type==STACKITEM_REGEXPR))
      HandleBinOp(stackptr,STACKITEM_SEQ);

   *stackptr=new RegExprStackItem(regexpr,*stackptr);
}

inline void HandleUnaryOp(RegExprStackItem  **stackptr,char opcode,char *curptr)
{
   if((*stackptr==NULL)||
      ((*stackptr)->type!=STACKITEM_REGEXPR))
   {
      Error("Unexpected unary operator");
      Exit();
   }

   VRegExpr *regexpr=new VRegExpr(opcode,(*stackptr)->regexpr);
   *stackptr=new RegExprStackItem(regexpr,(*stackptr)->prev);
}

inline void HandleOpenParenc(RegExprStackItem  **stackptr)
{
   if((*stackptr!=NULL)&&
      ((*stackptr)->type==STACKITEM_REGEXPR))
      HandleBinOp(stackptr,STACKITEM_SEQ);

   *stackptr=new RegExprStackItem(STACKITEM_OPENPARENC,*stackptr);
}

inline void HandleCloseParenc(RegExprStackItem  **stackptr,char *curptr)
{
   ReduceAll(stackptr);

   if(((*stackptr)->prev==NULL)||
      ((*stackptr)->prev->type!=STACKITEM_OPENPARENC))
   {
      Error("Unexpected closing bracket");
      Exit();
   }
   VRegExpr *regexpr=new VRegExpr(VP_ITEMTYPE_GROUP,(*stackptr)->regexpr,NULL);

   *stackptr=new RegExprStackItem(regexpr,(*stackptr)->prev->prev);
}

VRegExpr *VRegExpr::ParseVRegExpr(char * &str,char *endptr)
{
   char              reachedend=false;
   RegExprStackItem  *stack=NULL;

   vregexprerrstr=NULL;
   vregexprerrptr=NULL;

   do
   {
      switch(*str)
      {
      case '|':   HandleBinOp(&stack,STACKITEM_ALT);
                  str++;
                  break;
      case '&':   HandleBinOp(&stack,STACKITEM_AND);
                  str++;
                  break;
      case '-':   HandleBinOp(&stack,STACKITEM_MINUS);
                  str++;
                  break;
      case '<':
      case '@':{
                  TLabelID labelid=ParseLabel(&str,endptr);
                  VRegExpr *regexpr=new VRegExpr(labelid);
                  HandleRegExpr(&stack,regexpr);
                  break;
               }

      case '*':   HandleUnaryOp(&stack,VP_ITEMTYPE_STAR,str);
                  str++;
                  break;

      case '+':   HandleUnaryOp(&stack,VP_ITEMTYPE_PLUS,str);
                  str++;
                  break;

      case '?':   HandleUnaryOp(&stack,VP_ITEMTYPE_OPT,str);
                  str++;
                  break;

      case '~':   HandleUnaryOp(&stack,VP_ITEMTYPE_NOT,str);
                  str++;
                  break;

      case '(':   if(str[1]==')')
                  {
                     VRegExpr *regexpr=new VRegExpr(VP_ITEMTYPE_EMPTY);
                     HandleRegExpr(&stack,regexpr);
                     str+=2;
                  }
                  else
                  {
                     HandleOpenParenc(&stack);
                     str++;
                  }
                  break;

      case ')':   HandleCloseParenc(&stack,str);
                  str++;
                  break;

      case '_':{  VRegExpr *regexpr=new VRegExpr(VP_ITEMTYPE_DONTCARE);
                  HandleRegExpr(&stack,regexpr);
                  str++;
                  break;
               }

      case '#':{  VRegExpr *regexpr=new VRegExpr(VP_ITEMTYPE_POUND);
                  HandleRegExpr(&stack,regexpr);
                  str++;
                  break;
               }
      default:    reachedend=1;
                  break;
      }
      if(reachedend==1)
         break;
   }
   while(str<endptr);

   if(stack==NULL)
   {
      Error("Invalid path expression");
      Exit();
   }

   ReduceAll(&stack);

   if(stack->prev!=NULL)
   {
      // stackptr->prev is a open parenthesis !

      Error("Closing parenthesis expected");
      Exit();

      vregexprerrstr="Closing parenthesis expected";
      vregexprerrptr=str;
      return NULL;
   }
   return stack->regexpr;
}

//******************************************************************************
//******************************************************************************

FSM *VRegExpr::CreateFSM()
{
   FSM      *fsm;

   fsmtmpmem=&tmpmem;
   fsmmem=&tmpmem;

   fsm=CreateNonDetFSM();

//   fsm->Print();

   // Let's make the FSM deterministic
   fsm=fsm->MakeDeterministic();

//   fsm->Print();

   // Now, we store the FSM in the main memory
   fsmmem=&mainmem;
   mainmem.WordAlign();

   // Let's minimize
   fsm=fsm->Minimize();

//   fsm->Print();

   return fsm;
}

//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************
