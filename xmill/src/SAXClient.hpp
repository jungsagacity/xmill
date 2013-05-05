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

// This module contains the SAX-Client. The interface used is very similar to SAX.

#ifndef SAX_CLIENT
#define SAX_CLIENT

#include "MemStreamer.hpp"
#include "PathTree.hpp"
#include "LabelDict.hpp"
#include "Compress.hpp"

void PrintLabel(char *str,int len);

// XMLParse contains the XML parser that calls the SAX client.
class XMLParse;
extern XMLParse *xmlparser;

class SAXClient
{
public:

   static void HandleAttribName(char *str,int len,char iscont);
      // Handles a given attribute name
      // 'iscont' is one, if the string is not complete and more is to follow

   static void HandleAttribValue(char *str,int len,char iscont);
      // Handles an attribute value
      // 'iscont' is one, if the string is not complete and more is to follow

   static void HandleAttribWhiteSpaces(char *str,int len,char iscont);
      // Handles white spaces between attribute-value pairs
      // 'iscont' is one, if the string is not complete and more is to follow

   static void HandleStartLabel(char *str,int len,char iscont);
      // Handles a start tag
      // 'iscont' is one, if the string is not complete and more is to follow

   static void HandleEndLabel(char *str,int len,char iscont);
      // Handles an end tag
      // 'iscont' is one, if the string is not complete and more is to follow

   static void HandleText(char *str,int len,char iscont,int leftwslen,int rightwslen);
      // Handles a piece of text.
      // 'leftwslen' and 'rightwslen' specify how many white spaces (' ', '\n', ...)
      // are on the left and right end of the text.
      // If 'iscont=1', then 'rightwslen' must be zero and for the text piece
      // coming afterwards, 'leftwslen' must be zero.
      // It is also possible that 'len=leftwslen=rightwslen'

   static void HandleComment(char *str,int len,char iscont);
      // Handles a piece of comment data

   static void HandlePI(char *str,int len,char iscont);
      // Handles a piece of processing instruction data

   static void HandleDOCTYPE(char *str,int len,char iscont);
      // Handles a piece of DOCTYPE data

   static void HandleCDATA(char *str,int len,char iscont);
      // Handles a piece of CDATA data
};

#endif
