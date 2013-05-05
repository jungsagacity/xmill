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

//*****************************************************************
//*****************************************************************

// This module implements the error handling. An error message
// is one line and several error messages can be stored together
// Then, an exception can be raised and all error messages
// are printed through 'PrintErrorMsg()'.

#include <stdio.h>
#include <string.h>

#include "Error.hpp"

#define ERRMSG_MAXLEN   512   // The maximum length of all error
                              // messages together

struct ErrLine
{
   ErrLine  *next;
   char     line[1];
};

ErrLine  *curerrline=NULL;

char  errmsg[ERRMSG_MAXLEN+1];
char  *curptr=errmsg;
int   errstrlen;
char  *msgptr=errmsg+ERRMSG_MAXLEN-1;

void Error(char *str,int len)
   // Starts a new error msg
{
   if((curptr-errmsg)+sizeof(ErrLine)+len+1>ERRMSG_MAXLEN)
      return;

   ((ErrLine *)curptr)->next=curerrline;
   curerrline=(ErrLine *)curptr;

   memcpy(curerrline+1,str,len);
   ((char *)(curerrline+1))[len]=0;

   curptr+=sizeof(ErrLine)+len+1;
}

void Error(char *str)
   // Starts a new error msg (with '\0' at the end)
{
   Error(str,strlen(str));
}

void ErrorCont(char *str,int len)
   // Continues the current error msg
{
   curptr--;
   if((curptr-errmsg)+len+1>ERRMSG_MAXLEN)
      return;

   memcpy(curptr,str,len);
   curptr[len]=0;

   curptr+=len+1;
}

void ErrorCont(char *str)
   // Continues the current error msg
{
   ErrorCont(str,strlen(str));
}

void PrintErrorMsg()
   // Prints the current error messsages
{
   while(curerrline!=NULL)
   {
      printf("%s\n",(char *)(curerrline+1));
      curerrline=curerrline->next;
   }

   curptr=errmsg;
   msgptr=errmsg+ERRMSG_MAXLEN-1;
}

// A global exception that we use to exit the program
XMillException e;

void Exit()
{
//   PrintErrorMsg();
   throw &e;
}
