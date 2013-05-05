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

// This module implements the error handling. An error message
// is one line and several error messages can be stored together
// Then, an exception can be raised and all error messages
// are printed through 'PrintErrorMsg()'.

#ifndef ERROR_HPP
#define ERROR_HPP

void Error(char *str,int len);   // Starts a new error msg
void Error(char *str);           // Starts a new error msg (with '\0' at the end)

void ErrorCont(char *str,int len);  // Continues the current error msg
void ErrorCont(char *str);          // Continues the current error msg

void PrintErrorMsg();   // Prints the current error messsages

void Exit();   // Exits the program

inline void ExitNoMem()
   // Exits 
{
   Error("Insufficient memory!");
   Exit();
}

inline void ExitCorruptFile()
{
   Error("Corrupt input file!");
   Exit();
}

struct XMillException
{
};

#endif
