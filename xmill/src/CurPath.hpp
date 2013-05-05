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

// This module contains class CurPath for storing the current path
// within the XML document.

// The current path is stored as a sequence of labels -
// They are kept stored in blocks of 32 labels in 'CurPathLabelBlock'

// The number of labels per block
#define CURPATH_LABELBLOCKSIZE   32

struct CurPathLabelBlock
{
   CurPathLabelBlock *prev,*next;   // Previous and next label block
   TLabelID labels[CURPATH_LABELBLOCKSIZE];
};

class CurPath;

class CurPathIterator
   // The iterator is used to iterate forward and backward within the
   // the current path
{
   friend CurPath;
   CurPathLabelBlock *curblock;  // The current block
   TLabelID          *curlabel;  // The current label (points into the current block)

public:

   TLabelID GotoNext()
      // Go forward and return the next label
      // Returns LABEL_UNDEFINED, if the iterator is at the end of the path
   {
      if(curlabel==curblock->labels+CURPATH_LABELBLOCKSIZE)
      {
         curblock=curblock->next;
         if(curblock==NULL)
            return LABEL_UNDEFINED;
         curlabel=curblock->labels;
      }
      return *(curlabel++);
   }

   TLabelID GotoPrev()
      // Go backward and return the next label
      // Returns LABEL_UNDEFINED, if the iterator is at the beginning of the path
   {
      curlabel--;
      if(curlabel==curblock->labels-1)
      {
         curblock=curblock->prev;
         if(curblock==NULL)
            return LABEL_UNDEFINED;

         curlabel=curblock->labels+CURPATH_LABELBLOCKSIZE-1;
      }
      return *curlabel;
   }
};

class CurPath
   // Implements the label stack
{
   CurPathLabelBlock firstblock; // The first block
   CurPathLabelBlock *curblock;  // The current (last) block
   TLabelID          *curlabel;  // The current label (within the current block)
                                 // The current label is always the *next free* pointer

#ifdef PROFILE
   unsigned        curdepth,maxdepth;
#endif

public:
   CurPath()
   {
      firstblock.prev=NULL;
      firstblock.next=NULL;
      curblock=&firstblock;
      curlabel=curblock->labels;

#ifdef PROFILE
     curdepth=maxdepth=0;
#endif
   }

   void AddLabel(TLabelID labelid)
      // Add a label at the end of the path
   {
#ifdef PROFILE
      curdepth++;
         if(curdepth>maxdepth)
            maxdepth=curdepth;
#endif

      if(curlabel==curblock->labels+CURPATH_LABELBLOCKSIZE)
         // Is there not enough space in the current block?
         // ==> Create new block, if there is no next block
         // (We never delete blocks)
      {
         if(curblock->next==NULL)
         {
            curblock->next=new CurPathLabelBlock();
            curblock->next->prev=curblock;
            curblock->next->next=NULL;
         }
         curblock=curblock->next;

         // We set the new current label
         curlabel=curblock->labels;
      }
      *curlabel=labelid;
      curlabel++;
   }

   TLabelID RemoveLabel()
      // Removes the last label from the stack
   {
#ifdef PROFILE
      curdepth--;
#endif

      curlabel--;
      if(curlabel==curblock->labels-1)
         // If the label in the previou block?
         // Go one block back
      {
         curblock=curblock->prev;
         if(curblock==NULL)   // No previous block? => Exit
            return LABEL_UNDEFINED;

         curlabel=curblock->labels+CURPATH_LABELBLOCKSIZE-1;
      }
      return *curlabel;
   }

   void InitIterator(CurPathIterator *it)
      // Initializes an iterator for the path to the last label in the path
   {
      it->curblock=curblock;
      it->curlabel=curlabel;
   }

#ifdef PROFILE
   void PrintProfile()
   {
      printf("Maximal Depth=%lu\n",maxdepth);
   }
#endif
};

// There is one global path
extern CurPath curpath;
