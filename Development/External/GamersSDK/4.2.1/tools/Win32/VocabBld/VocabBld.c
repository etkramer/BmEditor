/*C_HEADER_FILE****************************************************************
FILE			:	VocabBld.c
DESC			:	Build voice recognition binary files
TABS			:	3
OWNER			:	Fonix
DATE CREATED:	19 April 2002

(C) Copyright 2002 by Fonix Corporation.
This source code to be used only for Game applications.

  $Date: 12/10/02 9:43a $
  $Revision: 14 $

*END_HEADER*******************************************************************/
#ifndef RELEASED
char sBuildRecogVersion[] = __FILE__ ", "  __TIMESTAMP__;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <wchar.h>
#include "VocabLib.h"

/*FUNCTION_HEADER**********************
 * NAME:		;Message
 * DESC: 	Print messages to the appropriate place.
 * IN:		The same as fprintf
 * OUT:		If the global handle for a list box is provided the message
				is printed there.  If not a message is printed to the appropriate 
				stream if provided (stdout, stderr, fp).  If no stream is provided
				then output is directed to stdout.
				
 * RETURN:	
 * NOTES:	This routine is called by various library functions to report errors.
            You can define a GUI version for GUI applications (just display sMessage
            to a popup window), or create a stub that does nothing, if you prefer.
            However, there must be a function of this name in order for the
            XVocab library to link.
 *END_HEADER***************************/
void Message(void *pVoid, char *fmt, ...)
{
   va_list argptr;
   char sMessage[1024];

   /* Convert printf-like arguments into a string 'sMessage'. */
   va_start(argptr, fmt);
   vsprintf(sMessage, fmt, argptr);
   va_end(argptr);

   printf("%s\n", sMessage);
}	/*  Message  */


/*FUNCTION_HEADER*******************
 * NAME:	  ;PrintUsage
 * DESC:	  Print the usage information for this program and then exit.
 * IN:     wsCommand - Name of the command (i.e., argv[0])
 * OUT:    Usage information is printed to stdout, and the program terminates.
 * RETURN: n/a
 * NOTES:
 *END_HEADER***************************/
static void PrintUsage(wchar_t *wsCommand)
{
   printf("   %s\n", sBuildRecogVersion);
   printf("   Copyright (C) 2002, by Fonix Corporation\n");
  
   printf("Usage: %S <Word list.txt> <Dictionary.pdc> <AsrNNet.pni> <Output.xvocab> <Adapted vocab Data>\n", wsCommand);
  
   printf("             [-n <N-Best>] [-p <pruneVal>]\n");
   printf("  Utility to create a speech recognition file.\n");
   printf("    <Word list.txt> has one line per word to recognize,\n");
   printf("                    with each line containing:\n");
   printf("                    <ID# to return><tab><word or phrase>\n");
   printf("                    (lines beginning with non-numeric characters are ignored)\n");
   printf("    <Dictionary.pdc> is a dictionary file provided by Fonix corporation\n");
   printf("    <AsrNNet.pni>    is a 'packed net file' from Fonix corporation\n");
   printf("    <Output.xvocab>  is the name of the output file to create.\n");
   printf("    <Input Adapted Vocab Data>  is the name of an input file with adapted vocab data.\n");

   printf("    <NBest>          is the number of top answers to get for this vocabulary (default=1)\n");
   printf("                       Larger values are slightly bigger and slower, so only use if needed.\n");
   printf("    <pruneVal>       is a number from 0-100 indicating the desired trade-off\n");
   printf("                       between speed and accuracy.  Usually you'll want values from 10-50 or so.\n");
   printf("                       0=>fastest (and smallest), but accuracy may suffer.\n");
   printf("                       50=>default; pretty fast (and small) and about as accurate possible\n");
   printf("                       100=>slowest and biggest; may be slightly more accurate than 50, but not much\n");
   exit(-1);
}

/*FUNCTION_HEADER*******************
 * NAME:	  ;main [BuildRecog]
 * DESC:	  Build a recognizer file
 * IN:     argc   - Number of command-line arguments
           argv[] - <Command name> <WordList.txt> <Dictionary.dcc> <AsrNNet.pni> <Output.xvocab> <Adapted vocab data file>
            where: 
               <WordList> contains lines with "<ID#><tab><word or phrase>\n"
                          (ignore lines beginning with non-numeric characters).
               <Dictionary> is a binary dictionary file from Fonix corporation
               <AsrNNet.pni> is a "packed net file" from Fonix corporation
			   <Adapted vocab data file> is the name of an input adapted vocab data file.
           argv[2] - Dictionary containing information needed to build pronunciations for each word.
           argv[3] - Packed Net file containing information about phoneme categories.
           argv[4] - Filename of the recognizer file to write out.
		   argv[5] - Filename of an input adapted vocab data file.
 * OUT:    argv[4] is written
 * RETURN: 0 on success, -1 on error.
 * NOTES:  
 *END_HEADER***************************/
int wmain( int argc, wchar_t *argv[ ])
{
   wchar_t *wsWordFile       = NULL;
   wchar_t *wsDictionaryFile = NULL;
   wchar_t *wsNNetFile       = NULL;
   wchar_t *wsXVocabFile     = NULL;
   wchar_t *wsAdaptedVocabData   = NULL;   // The name of an input adapted vocab data file.
   int   iNBest=1;       // N-best value to use for this vocabulary (e.g., N=3 => always find the best three matching words)
   int   iPruneLevel=50; // Value from 0..100 indicating how several to prune.  0=>small, fast, less accurate;
                         //    50 => medium size and speed, good accuracy; 100=>big, slow, probably not more accurate.
   int   iMainParam=1;   // Which non-flagged parameter we're on (starting with #1)
   int   iArg;           // Which argument in argv[] we're on
   HRESULT err;
   
  	  
   /* Make sure we have enough command-line arguments */
   if (argc<4)
      PrintUsage(argv[0]);

   /* Parse the command-line arguments */
   for (iArg=1; iArg < argc; iArg++)
   {
      if (argv[iArg][0]=='-')
      {
         /* Handle flags in whatever order they happen to come in */
         switch(argv[iArg][1])
         {
            case 'n': iNBest = _wtoi(argv[++iArg]); break;
            case 'p': iPruneLevel = _wtoi(argv[++iArg]); break;
            // other arguments may go here if needed
            default: printf("Unknown argument '%s'\n", argv[iArg]);
                     PrintUsage(argv[0]); // exits the program.
         }
      }
      else
      {
         /* Handle non-flagged arguments in the order expected */
		  // This depends on case strings 2,3,4 to be convertable to 
		  // multibyte strings.
         switch(iMainParam++)
         {
            case 1: 
               wsWordFile       = argv[iArg];
               break;
            case 2: 
               wsDictionaryFile = argv[iArg];
               break;
            case 3: 
               wsNNetFile       = argv[iArg];
               break;
            case 4: 
               wsXVocabFile     = argv[iArg];
			   break;
			case 5:
			   wsAdaptedVocabData    = argv[iArg];  
               break;
            default: PrintUsage(argv[0]);
         }
      }
   }

   if (iMainParam<=4 || wsWordFile==NULL || wsDictionaryFile==NULL || wsNNetFile==NULL || wsXVocabFile==NULL)
      PrintUsage(argv[0]); // exit if there is a parameter missing

   /* Create the output file 'wsXVocabFile' and return the result (0=>ok, -1=>error). */
   err = FnxVocabBld(wsWordFile, wsDictionaryFile, wsNNetFile,  wsXVocabFile, wsAdaptedVocabData, iNBest, (FLOAT)iPruneLevel);

   if (err)
   {
      wprintf(L"Error in trying to create recognition vocabulary '%s'.\n", wsXVocabFile);
      PrintUsage(argv[0]);
   }
   else wprintf(L"Wrote recognition vocabulary file '%s'.\n", wsXVocabFile);
      
   return 0;
}  /* main */