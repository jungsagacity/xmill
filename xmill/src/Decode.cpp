
#include "UnCompCont.hpp"
#include "XMLOutput.hpp"
#include "LabelDict.hpp"
#include "CurPath.hpp"

#undef LoadString

extern UncompressContainerMan  uncomprcont;

void DecodeTreeBlock(UncompressContainer *treecont,UncompressContainer *whitespacecont,UncompressContainer *specialcont,XMLOutput *output)
{
   char              *strptr;
   unsigned char     isattrib;
   int               mystrlen;
   static char       tmpstr[20];

   unsigned char     *curptr,*endptr;
   long              id;
   char              isneg;

   curptr=treecont->GetDataPtr();
   endptr=curptr+treecont->GetSize();

   while(curptr<endptr)
   {
      id=LoadSInt32(curptr,&isneg);

      if(isneg==0)   // Do we have a label ID ?
      {
         if(id>=32768L)
         {
            Error("Error while decompressing file!");
            Exit();
         }

         switch(id)
         {
         case TREETOKEN_ENDLABEL:  // An end-of-label token (i.e. id==0) ?
            mystrlen=globallabeldict.LookupLabel(curpath.RemoveLabel(),&strptr,&isattrib);

            if(isattrib==0)
               output->endElement(strptr,mystrlen);
            else
               output->endAttribute(strptr,mystrlen);
            break;

         case TREETOKEN_EMPTYENDLABEL:  // An end-of-label token for an empty element
            curpath.RemoveLabel();
            output->endEmptyElement();
            break;
         
         case TREETOKEN_WHITESPACE:  // A white-space token ?
            mystrlen=whitespacecont->LoadUInt32();
            output->whitespaces((char *)whitespacecont->GetDataPtr(mystrlen),mystrlen);
            break;

         case TREETOKEN_ATTRIBWHITESPACE:  // A attrib white-space token ?
            mystrlen=whitespacecont->LoadUInt32();
            output->attribWhitespaces((char *)whitespacecont->GetDataPtr(mystrlen),mystrlen);
            break;

         case TREETOKEN_SPECIAL:    // A special token
            strptr=(char *)(specialcont->LoadString((unsigned *)&mystrlen));
            output->characters(strptr,mystrlen);
            break;

         default: // Do we have a start label token?
            id-=LABELIDX_TOKENOFFS;
            mystrlen=globallabeldict.LookupLabel((TLabelID)id,&strptr,&isattrib);

            if(isattrib==0)
               output->startElement(strptr,mystrlen);
            else
               output->startAttribute(strptr,mystrlen);

            curpath.AddLabel((TLabelID)id);
         }
      }
      else  // We have a block ID ==> I.e. we have some text
         uncomprcont.GetContBlock(id)->UncompressText(output);
   }
}

