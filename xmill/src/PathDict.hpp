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

#ifndef PATHDICT_HPP
#define PATHDICT_HPP

#include "Types.hpp"
#include "MemStreamer.hpp"
#include "LabelDict.hpp"
#include "ContMan.hpp"

// The memory used for allocating nodes in the path dictionary tree
extern MemStreamer *pathdictmem;

class VPathExpr;
class PathDict;
class CompressContainerBlock;
class UncompressContainerBlock;

// The size of the hash table
#define PATHDICT_HASHSIZE  512
#define PATHDICT_HASHMASK  511


class PathDictNode
   // Each node is represented by this structure
{
   friend PathDict;

   PathDictNode   *parent; // The parent node.
                           // Is NULL for the super root node
   union
   {
      TLabelID       labelid;    // For non-root nodes, we store the label
                                 // for the edge from the parent
      VPathExpr      *pathexpr;  // For root nodes, we store the FSM info
   };

   // For compressor/decompressor, we need to keep track of the actual
   // container block used. The container block can be NULL at the beginning
   // and be allocated later.
   union
   {
      CompressContainerBlock     *compresscontblock;
      UncompressContainerBlock   *uncompresscontblock;
   };

   // The next node with the same hash value
   PathDictNode   *nextsamehash;

public:

   void *operator new(size_t size)
   {
      return pathdictmem->GetByteBlock(size);
   }

   CompressContainerBlock *GetCompressContainerBlock()
   {
      return compresscontblock;
   }

   CompressContainerBlock *AssignCompressContainerBlock(unsigned contnum,unsigned userdatasize,VPathExpr *pathexpr)
      // If there is no container block yet, then this function allocated
      // the container block.
   {
      compresscontblock=compresscontman.CreateNewContainerBlock(contnum,userdatasize,this,pathexpr);
      return compresscontblock;
   }

   void PrintInfo(); // Prints the information about the node's container block

   VPathExpr *GetPathExpr()   {  return pathexpr;}
};

inline unsigned long ComputePathDictHashIdx(PathDictNode *parent,TLabelID labelid)
   // Computes the hash index based on the parent node and the label ID
   // - This is for non-root nodes
{
   return (((unsigned long)parent)+labelid)&PATHDICT_HASHMASK;
}

inline unsigned long ComputePathDictHashIdx(VPathExpr *pathexpr)
   // Computes the hash index based on the path expression
   // - This is for root nodes
{
   return (((unsigned long)pathexpr)-1)&PATHDICT_HASHMASK;
}

//***************************************************************************************

class PathDict
   // The actual path dictionary class
{
   PathDictNode   *hashtable[PATHDICT_HASHSIZE];   // The hash table
   PathDictNode   rootnode;   // The super root node

#ifdef PROFILE
   // Some profiling information
   unsigned long  nodecount,lookupcount,hashitercount;
#endif

public:
   void Init()
      // Initializes the path dictionary
   {
      int i;
      for(i=0;i<PATHDICT_HASHSIZE;i++)
         hashtable[i]=NULL;
#ifdef PROFILE
      nodecount=0;
      lookupcount=0;
      hashitercount=0;
#endif
   }

#ifdef USE_FORWARD_DATAGUIDE
   void ResetContBlockPtrs()
      // We reset the container block pointers to NULL for all
      // nodes
   {
      for(int i=0;i<PATHDICT_HASHSIZE;i++)
      {
         PathDictNode *node=hashtable[i];
         while(node!=NULL)
         {
            node->compresscontblock=NULL;
            node=node->nextsamehash;
         }
      }
   }
#endif

   PathDict()  {  Init();  }

   PathDictNode *FindOrCreateRootPath(VPathExpr *pathexpr)
      // Finds or creates the root node for a specific FSM (i.e. the path expression)
   {
      unsigned long  hashidx=ComputePathDictHashIdx(pathexpr);
      PathDictNode   *node=hashtable[hashidx];
#ifdef PROFILE
      lookupcount++;
#endif

      while(node!=NULL)
      {
#ifdef PROFILE
         hashitercount++;
#endif
         if(node->pathexpr==pathexpr)
            return node;

         node=node->nextsamehash;
      }

      // We must create a new node
      node=new PathDictNode();

#ifdef PROFILE
      nodecount++;
#endif

      node->parent=NULL;
      node->pathexpr=pathexpr;
      node->compresscontblock=NULL;
//      node->labelid=LABEL_UNDEFINED;

      node->nextsamehash=hashtable[hashidx];
      hashtable[hashidx]=node;

      return node;
   }

   PathDictNode *FindOrCreatePath(PathDictNode *parent,TLabelID labelid)
      // Finds or creates a non-root node with parent 'parent' and an
      // edge label 'labelid'.
   {
      unsigned long hashidx=ComputePathDictHashIdx(parent,labelid);
      PathDictNode   *node=hashtable[hashidx];

#ifdef PROFILE
      lookupcount++;
#endif

      while(node!=NULL)
      {
#ifdef PROFILE
         hashitercount++;
#endif

         if((node->parent==parent)&&(node->labelid==labelid))
            return node;

         node=node->nextsamehash;
      }

      // We must create a new node
      node=new PathDictNode();

#ifdef PROFILE
      nodecount++;
#endif

      node->parent=parent;
      node->labelid=labelid;
      node->compresscontblock=NULL;

      node->nextsamehash=hashtable[hashidx];
      hashtable[hashidx]=node;

      return node;
   }

#ifdef PROFILE
   void PrintProfile()
   {
      printf("PathDict: nodecount=%lu  lookupcount=%lu   hashitercount=%lu\n",
         nodecount,lookupcount,hashitercount);
   }
#endif

//*******************************************************************************
};

extern PathDict pathdict;

#endif
