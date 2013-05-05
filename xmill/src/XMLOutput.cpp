#include <stdio.h>
#include "XMLOutput.hpp"

#ifdef SET_OUTPUT_STATIC

int XMLOutput::curcol;
int XMLOutput::coldelta;

struct XMLOutput::ABC XMLOutput::x;

#endif


#ifdef XDEMILL_NOOUTPUT

void OUTPUT_STATIC XMLOutput::startElement(char *str,int len)
{
}

void OUTPUT_STATIC XMLOutput::endElement(char *str,int len)
{
}

void OUTPUT_STATIC XMLOutput::startAttribute(char *str,int len)
{
}

void OUTPUT_STATIC XMLOutput::endAttribute(char *str,int len)
{
}

void OUTPUT_STATIC XMLOutput::characters(char *str,int len)
{
}

void OUTPUT_STATIC XMLOutput::whitespaces(char *str,int len)
{
}

#endif

