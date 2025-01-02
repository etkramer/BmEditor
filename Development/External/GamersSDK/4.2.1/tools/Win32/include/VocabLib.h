/*H_HEADER_FILE***************************************************************
FILE			: VocabLib.h
DESC			: Header file for tools to build binary files needed for speech
				recognition.

(C) Copyright 2002 Fonix Corporation.

*END_HEADER******************************************************************/
#ifndef	XVOCABLIB_H
#define	XVOCABLIB_H

//#include <windows.h>

#if defined(__cplusplus)
extern "C" {
#endif

//#ifndef FNX_GRAMMAR
//	#define FNX_GRAMMAR
//#endif

#define FNX_PNI_READ_ERROR        -21 /* Could not read .pni file, or there was a problem with it */
#define FNX_VOCAB_READ_ERROR      -22 /* Could not read an .xvocab file */
#define FNX_DICTIONARY_READ_ERROR -23 /* Could not read a .pdc dictionary file, or there was
                                         an error getting a pronunciation from it. */
#define FNX_WRITE_ERROR           -24 /* Error in writing a file */
#define FNX_WORDLIST_READ_ERROR   -25 /* Could not read a wordlist text file */
#define FNX_INIT_ERROR            -26 /* Could not initialize (create) recognizer */

#ifdef TRAIN_PHONEME_WEIGHT
#	define FNX_TRAIN_VOCAB_ERROR  -27
#endif


/************************************
 * NAME:	  FnxVocabBld
 * DESC:	  Build a recognition vocabulary file.
 * IN:     wsWordFile       - Filename of word list file containing lines with 
                              "<ID#><tab><word or phrase>\n" (Ignore any lines beginning with non-numeric characters).
           wsDictionaryFile - File with information needed to generate pronunciations.
           wsNNetFile       - Floating-point "packed net" neural network file (*.pni)
           wsXVocabFile     - Filename to write the new recognition vocabulary file to.
		   wsPhonemeData    - File name containing trained phoneme data 
							  (Set it to "NULL" if USE_PHONEME_WEIGHT is not defined)
							  (Set it to "NULL" if not using it even if USE_PHONEME_WEIGHT is defined).
           iNBest           - NBest value to use (Default=1).
           fPruneLevel      - Pruning level (0=fastest/smallest, but might lower accuracy; 
                                             50=default=medium, good accuracy;
                                             100=slow/big, might have slightly higher accuracy).
 * OUT:    'wsXVocabFile' is written.
 * RETURN: 0 on success, or one of the following errors:
             FNX_WORDLIST_READ_ERROR   - Can't read wsWordFile, or there's a problem with it.
             FNX_DICTIONARY_READ_ERROR - Can't read wsDictionaryFile, or there's a problem with a pronunciation.
             FNX_PNI_READ_ERROR        - Can't read wsNNetFile, or there's a problem with it.
             FNX_INIT_ERROR            - Can't initialize recognizer.
             FNX_WRITE_ERROR           - Can't write wsXVocabFile.
 * NOTES:  Usually use iNBest=1 and fPruneLevel=50.0.  
 *END_HEADER***************************/
HRESULT FnxVocabBld(LPWSTR wsWordFile, LPWSTR wsDictionaryFile, LPWSTR wsNNetFile, LPWSTR wsXVocabFile, LPWSTR wsPhonemeData, int iNBest, FLOAT fPruneLevel);


/************************************
 * NAME:	  FnxAdaptVocab
 * DESC:	  Train phonemes for words in a vocabulary for use on the XBox
 * IN:     wsWordFile       - Filename of word list file containing lines with 
                              "<ID#><tab><word or phrase>\n" (Ignore any lines beginning with non-numeric characters).
           wsDictionaryFile - File with information needed to generate pronunciations.
           wsNNetFile       - Floating-point "packed net" neural network file (*.pni)
           wsTrainingData   - Filename of a file with a list of wave file paths and transcription file paths.
		   wsPhonemeData    - File name to write trained adapted vocab data.
		   wsTrainingDir		- Name of a directory in which temperary files will be stored during training.

 * OUT:    'wsPhonemeData' is written.
 * RETURN: 0 on success, or one of the following errors:
             FNX_WORDLIST_READ_ERROR   - Can't read wsWordFile, or there's a problem with it.
             FNX_DICTIONARY_READ_ERROR - Can't read wsDictionaryFile, or there's a problem with a pronunciation.
             FNX_PNI_READ_ERROR        - Can't read wsNNetFile, or there's a problem with it.
             FNX_INIT_ERROR            - Can't initialize recognizer.
             FNX_WRITE_ERROR           - Can't write wsXVocabFile.
			 FNX_TRAIN_VOCAB_ERROR     - Error in training recognizer.
 * NOTES:  
 *END_HEADER***************************/
HRESULT FnxAdaptVocab(LPWSTR wsWordFile, LPWSTR wsDictionaryFile, LPWSTR wsNNetFile, 
					LPWSTR wsTrainingData, LPWSTR wsPhonemeData, LPWSTR wsTrainingDir);



/************************************
 * NAME:	  FnxBuildVoice
 * DESC:	  Build a neural network file pruned to a list of vocabularies.
 * IN:     wsOutputVNN      - Output: "Voice Neural Network" binary file. (NULL=>just get counts for ppiSearchPaths)
           wsInputPNI       - Input: Floating-point "packed net" neural network file (*.pnf)
           pwsXVocabFiles   - Filename to write the new recognition vocabulary file to.
           iNumVocabFiles   - Number of elements in pwsXVocabFile[].
           iMaxSimultaneous - Max number of vocabularies expected at once (1=>only one allowed at a time, etc.)
           iMaxTotalSearchPaths - Total number of tokens expected at once (i.e., from using multiple vocabs)
                                  (0=>use max of any combination of 'iMaxSimultaneous' vocabs).
           ppiNumSearchPaths - Address in which to return an array of the max search paths 
                               for each vocabulary (NULL=>don't bother).
           iMaxTotalSearchNodes - Total number of search graph nodes allowed at once.
                                  (0=>use max of any combination of 'iMaxSimultaneous' vocabs).
           ppiNumSearchNodes - Address in which to return an array of the number of search nodes 
                               for each vocabulary (NULL=>don't bother).
 * OUT:    'wsOutputVNN' and 'wsVoiceUser' are written, and
           pwsXVocabFiles[iNumVocabFiles] are rewritten with a unique tag showing that
           all of these files are compatible with each other.
 * RETURN: 0 on success, or else one of the following error codes:
              FNX_PNI_READ_ERROR   - Can't read wsInputPNI.
              FNX_VOCAB_READ_ERROR - Can't read an xvocab file.
              FNX_WRITE_ERROR      - Can't write wsOutputVoice or wsVoiceUser.
 * NOTES:  The returned array ppiSearchPaths[] contains the max search paths allowed
              for each of the vocabs, PLUS one more element that receives the
              max total search paths allowed in the output recognizer, as determined
              from the input parameters and vocabs themselves.  Similarly, ppiSearchNodes[]
              contains an additional element indicating the value used in the output.
 *END_HEADER***************************/
HRESULT FnxBuildVoice(LPWSTR wsOutputVNN, LPWSTR wsVoiceUser, LPWSTR wsInputPNI, 
                LPWSTR *pwsXVocabFiles, int iNumVocabFiles, int iMaxSimultaneous, 
                int iMaxTotalSearchPaths, int **ppiNumSearchPaths, 
                int iMaxTotalSearchNodes, int **ppiNumSearchNodes);

#if defined(__cplusplus)
}
#endif

#endif
