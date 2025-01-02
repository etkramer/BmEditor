/*C_HEADER_FILE****************************************************************
FILE			:	BuildVoice
DESC			:	Build a binary neural network file
TABS			:	3
OWNER			:	Fonix
DATE CREATED:	1 May 2002

(C) Copyright 2001 Fonix corporation.  All rights reserved.
This is an unpublished work, and is confidential and proprietary: 
technology and information of fonix corporation.  No part of this
code may be reproduced, used or disclosed without written consent of 
fonix corporation in each and every instance.

*END_HEADER*******************************************************************/
#ifndef RELEASED
char sBuildVoiceVersion[] = __FILE__ ", "  __TIMESTAMP__;
#else
char sBuildVoiceVersion[] = "BuildVoice.EXE , "  __TIMESTAMP__;
#endif


#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
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
            to a popup window).
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
 * IN:     sCommand - Name of the command (i.e., argv[0])
 * OUT:    Usage information is printed to stdout, and the program terminates.
 * RETURN: n/a
 * NOTES:
 *END_HEADER***************************/
static void PrintUsage(wchar_t *wsCommand)
{
   printf("\n   %s\n", sBuildVoiceVersion);
   printf("   Copyright (C) 2002, by Fonix Corporation\n\n");
   wprintf(L"Usage: %s <OutputFnxVoice.vnn> <OutputFnxVoiceUser.usr> <inputNNet.pni>\n <vocab1.xvocab> [vocab2.xvocab...]\n", wsCommand);
   printf("         [-s MaxSimultaneousVocabs] [-p MaxTotalSearchPaths] [-n MaxTotalNodes]\n\n");
   printf("Utility to create a binary neural network file.\n");
   printf(" <FnxVoice.vnn>     is the Voice Neural Network output file suitable for use in FnxVoiceInit().\n");
   printf(" <FnxVoiceUser.usr> is the Voice User output file for use in FnxVoiceUserInit().\n");
   printf(" <inputNNet.pni>    a 'packed net file' from Fonix corporation\n");
   printf("                     (The same one used in VocabBld.exe)\n");
   printf(" <vocab1.xvocab> a list of one or more 'xvocab' files (created by VocabBld.exe)\n");
   printf("                 (Be sure to include all of the xvocab files you will be using)\n");
   printf(" -s MaxSimulVocabs is the maximum number of the vocabularies in the list that\n");
   printf("                   you want to be able to do recognition on simultaneously,\n");
   printf("                   i.e.,so that you can get the best matching word from ANY\n");
   printf("                   of the simultaneous vocabularies from the same spoken \n");
   printf("                   waveform. [Default=allow all to be used at once, so if \n");
   printf("                   you know how many will ever be used together at once, you\n");
   printf("                   can decrease memory requirements.]\n");
   printf(" -p MaxSearchPaths is the maximum total number of search paths possible from\n");
   printf("                   any of the combinations of vocabularies that are expected\n");
   printf("                   to be used together.  You can use the numbers output by\n");
   printf("                   VocabBld.exe to find the number of search paths for each\n");
   printf("                   vocabulary. [Default=use the largest 'MaxSimultaneousVocabs'\n");
   printf("                   vocabularies to determine how many search paths are \n");
   printf("                   possible. If you know which vocabularies will be used \n");
   printf("                   together, you can decrease memory requirements and \n");
   printf("                   CPU usage]\n");
   printf(" -n MaxTotalNodes  is the maximum total number of search nodes possible from\n");
   printf("                   any of the combinations of vocabularies that are expected\n");
   printf("                   to be used together.  You can use the numbers output by\n");
   printf("                   VocabBld.exe to find the number of search nodes for each \n");
   printf("                   vocabulary. [Default=use the largest 'MaxSimultaneousVocabs'\n");
   printf("                   vocabularies to determine how many search paths are \n");
   printf("                   possible. If you know which vocabularies will be used \n");
   printf("                   together, you can decrease memory requirements]\n");
   exit(-1);
}  /* PrintUsage */

/*FUNCTION_HEADER*******************
 * NAME:	  ;main [BuildVoice]
 * DESC:	  Build a binary neural network file
 * IN:     argc   - Number of command-line arguments
           argv[] - <output.vnn> <inputNNet.pni> <vocab1.xvocab> [vocab2.xvocab...]
                    [-n MaxSimultaneousVocabs] [-t MaxTotalSearchPaths] [-prune]
 * OUT:    output.vnn is written, and search path and search node numbers are displayed.
 * RETURN: 0 on success, -1 on error.
 * NOTES:  
 *END_HEADER***************************/
int wmain(int argc, wchar_t *argv[])
{
   wchar_t *wsOutputVoice    = NULL;
   wchar_t *wsVoiceUser      = NULL;
   wchar_t *wsInputPNI       = NULL;
   wchar_t **pwsXVocabFiles  = NULL;
   int  iMainParam=1;           // Which non-flagged parameter we're on (starting with #1)
   int  iArg;                   // Which argument in argv[] we're on
   int  iNumVocabFiles=0;       // Number of vocabularies found so far.
   int  iMaxVocabs=0;           // 0=>use iNumXVocabs, i.e., all vocabularies are possible at once.
   int  iMaxTotalSearchPaths=0; // 0=>use default, i.e., max of 'iMaxVocabs' vocabularies' counts.
   int  iMaxTotalSearchNodes=0; // 0=>use default, i.e., max of 'iMaxVocabs' vocabularies' counts.
   int  iTotalSearchNodes = 0;
   int *piNumSearchPaths=NULL;  // Maximum number of search paths for each vocabulary.
   int *piNumSearchNodes=NULL;  // Number of nodes for each vocabulary.
   int  iTotalSearchPaths;      // Total of all values in piNumSearchPaths[].
   int  iMaxPathIndex;          // Index (in pszXVocabFiles[]) of the vocab with the largest search paths value.
   int  iMaxNodeIndex;          // Index (in pszXVocabFiles[]) of the vocab with the largest search nodes value.
   int  iMaxIndex;
   int  iVocab;
   HRESULT err;

   /* Make sure we have enough command-line arguments */
   if (argc<4)
      PrintUsage(argv[0]);

   /* Allocate an array that is at least large enough to hold all of the XVocab names */
   pwsXVocabFiles = (wchar_t **)calloc(argc-3, sizeof(wchar_t *));

   /* Parse the command-line arguments */
   for (iArg=1; iArg < argc; iArg++)
   {
      if (argv[iArg][0]=='-')
      {
         /* Handle flags in whatever order they happen to come in */
         switch(argv[iArg][1])
         {
            case 's': iMaxVocabs = _wtoi(argv[++iArg]); break;
            case 'p': iMaxTotalSearchPaths = _wtoi(argv[++iArg]); break;
            case 'n': iMaxTotalSearchNodes = _wtoi(argv[++iArg]); break;
            // other arguments may go here if needed
            default: printf("Unknown argument '%s'\n", argv[iArg]);
                     PrintUsage(argv[0]); // exits the program.
         }
      }
      else
      {
         /* Handle non-flagged arguments in the order expected */
         switch(iMainParam++)
         {
            case 1:  wsOutputVoice                = argv[iArg]; break;
            case 2:  wsVoiceUser                  = argv[iArg]; break;
            case 3:  wsInputPNI                   = argv[iArg]; break;
            default: pwsXVocabFiles[iNumVocabFiles++] = argv[iArg]; break;
         }
      }
   }  /* for each argument */

   /* Make sure we got the required arguments */
   if (iMainParam<=3 || wsOutputVoice==NULL || wsInputPNI==NULL || iNumVocabFiles<1)
   {
      printf("Missing arguments.\n");
      PrintUsage(argv[0]); // exit if there is a parameter missing
   }

   /* If not overridden, use the default of allowing all vocabularies at once */
   if (iMaxVocabs<=0)
      iMaxVocabs = iNumVocabFiles;

   /* Create the output file, and allocate storage in piNumSearchPaths[] and piNumSearchNodes[]. */
   err = FnxBuildVoice(wsOutputVoice, wsVoiceUser, wsInputPNI, 
                pwsXVocabFiles, iNumVocabFiles, iMaxVocabs,
                iMaxTotalSearchPaths, &piNumSearchPaths, 
                iMaxTotalSearchNodes, &piNumSearchNodes);
   if (err)
   {
      wprintf(L"Error in trying to create binary nnet file '%s'\n", wsOutputVoice);
      PrintUsage(argv[0]);
   }

   /* Display the vocabularies and number of search paths for each */
   printf("Search Paths  Search Nodes Vocab File\n");
   iTotalSearchPaths = 0;
   iMaxIndex = iMaxTotalSearchPaths = 0;
   for (iVocab=0;iVocab < iNumVocabFiles; iVocab++)
   {
      wprintf(L"%12d  %12d  %s\n", piNumSearchPaths[iVocab], piNumSearchNodes[iVocab], pwsXVocabFiles[iVocab]);
      iTotalSearchPaths += piNumSearchPaths[iVocab];
      if (piNumSearchPaths[iVocab]>iMaxTotalSearchPaths)
      {
         iMaxTotalSearchPaths = piNumSearchPaths[iVocab];
         iMaxPathIndex = iVocab;
      }
      iTotalSearchNodes += piNumSearchNodes[iVocab];
      if (piNumSearchNodes[iVocab]>iMaxTotalSearchNodes)
      {
         iMaxTotalSearchNodes = piNumSearchNodes[iVocab];
         iMaxNodeIndex = iVocab;
      }
   }
   if (iNumVocabFiles>1)
   {
      wprintf(L"%6d total paths in all %d vocabularies; ", iTotalSearchPaths, iNumVocabFiles);
      wprintf(L"%6d paths in largest vocabulary %s\n", iMaxTotalSearchPaths, pwsXVocabFiles[iMaxPathIndex]);
      wprintf(L"%6d total nodes in all %d vocabularies; ", iTotalSearchNodes, iNumVocabFiles);
      wprintf(L"%6d nodes in largest vocabulary %s\n", iMaxTotalSearchNodes, pwsXVocabFiles[iMaxNodeIndex]);
   }
   wprintf(L"%12d  total paths maximum allowed at once in %s\n", piNumSearchPaths[iNumVocabFiles], wsOutputVoice);

   /* Free up memory allocated within FnxBuildVoice() */
   free(piNumSearchPaths);
   free(piNumSearchNodes);
   return 0;
}  /* main */