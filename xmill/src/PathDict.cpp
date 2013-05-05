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

// This module contains the path dictionary class
// The path dictionary maps paths into container blocks
// The paths are not actual paths in the XML document, but
// instantiations of the '#' symbols within the FSM for a given
// path in the XML document

// The path dictionary is implemented as a tree with roots
// for each possible FSM. The roots are children of a node called
// the 'super root node'.
// Edges are labeled with XML label IDs. Each outgoing edge of a parent
// has a distinct ID (i.e. the tree is deterministic)
// To go from one node in the tree to a subnode using a specific label,
// we use a hash table


#include "PathDict.hpp"
#include "VPathExprMan.hpp"

extern MemStreamer blockmem;

#if !defined(USE_FORWARD_DATAGUIDE) || defined(USE_NO_DATAGUIDE)
MemStreamer *pathdictmem=&blockmem;
#else
MemStreamer *pathdictmem=&pathtreemem;
#endif


PathDict pathdict;

void PathDictNode::PrintInfo()
   // Prints the information about the node's container block
{
   PathDictNode   *curnode=this,
                  *parentnod=parent;
   unsigned long  depth=0;

   // We print the regular expression
   compresscontblock->GetPathExpr()->PrintRegExpr();

   printf(" <- ");
   
   curnode=this;

   // Then, we print the path that instantiates the '#'-symbols
   while(curnode->parent!=NULL)
   {
      globallabeldict.PrintLabel(curnode->labelid);
      curnode=curnode->parent;
      if(curnode->parent!=NULL)
         printf("/");
   }
}
