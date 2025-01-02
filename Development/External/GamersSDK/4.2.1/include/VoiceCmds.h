/*H_HEADER_FILE***************************************************************
FILE			: VoiceCmds.h
DESC			: Information needed for running Fonix speech recognition 

(C) Copyright 2002 Fonix Corporation.

*END_HEADER******************************************************************/
#ifndef	VOICECMDS_H
#define	VOICECMDS_H

#if defined( __EE__) || defined(__CELLOS_LV2__) || defined(_Wii)
#include <stddef.h>
#  define HRESULT int
#  define DWORD   unsigned int
#  define FLOAT   float
#  define LPWSTR  wchar_t *
#elif _XBOX
#include <xtl.h>
#else
#include "windows.h"
#endif

/* Return codes possible from Recognize() [one or more can be or-ed together]*/
#define RESULTS_AVAILABLE 0x01  // Recognition complete: Call GetResults() before calling Recognize() again.
#define SPEECH_DETECTED   0x02  // Beginning of speech has been detected (in case you wanted to know)
#define END_OF_SPEECH     0x04  // End-of-speech has been detected: Any further wave data will be ignored.

/* Main types needed by a voice-enabled application */
typedef void *FnxVoicePtr;      // Single structure shared by all users and vocabularies
typedef void *FnxVocabPtr;      // One of potentially many recognition vocabularies
typedef void *FnxVoiceUserPtr;  // One of up to 4 "users", i.e., one per attached headset communicator.

/* Function prototypes */
#if defined(__cplusplus)
extern "C" {
#endif

/* Error return codes: */
#define FNX_TOO_MANY_VOCABS  -11  /* iNumVocabs is more than allocated for.
                                     Re-run BuildVoice with parameters to allow
                                     all of the vocabularies being selected to be used together. */
#define FNX_NODE_OVERFLOW    -12  /* Number of nodes used by this set of vocabularies is more
                                       than that allowed for when building FnxVoiceUser block.
                                     Re-run BuildVoice with parameters to allow
                                     all of the vocabularies being selected to be used together. */
#define FNX_NULL_POINTER     -13  /* pFnxVoice, pFnxVoiceUser, or pFnxVocab is NULL */
#define FNX_NOT_INITIALIZED  -14  /* pFnxVoice, pFnxVoiceUser, or pFnxVocab is not initialized */
#define FNX_WRONG_TYPE       -15  /* pFnxVoice, pFnxVoiceUser, or pFnxVocab is not the right
                                     type of block (e.g., perhaps a pFnxVocab was passed in
                                     where a pFnxVoice should have been, etc., or maybe a
                                     completely bogus pointer was used for one of these). */
#define FNX_INCOMPLETE       -16  /* pFnxVoice, pFnxVoiceUser or pFnxVocab did not contain the
                                      proper footer, indicating that the block may be incomplete
                                      or otherwise corrupted. */
#define FNX_SPEECH_ERROR     -17  /* Could not reset or initialize speech recognizer for some unexpected reason. */
#define FNX_VERSION_MISMATCH -18  /* pFnxVoice, pFnxVoiceUser, or pFnxVocab are a different 
                                      version than that supported by the code in use.
                                      (Rerun XVocabBld and BuildVoice to create compatible files). */
#define FNX_TAG_MISMATCH     -19  /* pFnxVoice, pFnxVoiceUser, and pFnxVocab must all have the same
                                      internal tag, which happens automatically when BuildVoice
                                      is run.  Probably you've mixed files built with one run of
                                      BuildVoice with those built in another run.  Rerun the tools
                                      to build all of the files at once that you want to use with
                                      each other. */
#define FNX_UNALIGNED_BLOCK  -20  /* pFnxVoice, pFnxVoiceUser or pFnxVocab did not begin at an
                                      even 4-byte-aligned memory address. */
#define FNX_PSI_READ_ERROR        -21 /* Could not read .pni file, or there was a problem with it */
#define FNX_VOCAB_READ_ERROR      -22 /* Could not read an .xvocab file */
#define FNX_DICTIONARY_READ_ERROR -23 /* Could not read a .pdc dictionary file, or there was
                                         an error getting a pronunciation from it. */
#define FNX_WRITE_ERROR           -24 /* Error in writing a file */
#define FNX_WORDLIST_READ_ERROR   -25 /* Could not read a wordlist text file */
#define FNX_INIT_ERROR            -26 /* Could not initialize (create) recognizer */

#define FNX_TRAIN_VOCAB_ERROR     -27



//Initialization====================================================================
/*FUNCTION_HEADER**********************
 * NAME:   ;FnxVoiceInit
 * DESC:   Initialize the main voice info (nnet, etc.).
 * IN:     pFnxVoice - Pointer to a block of memory read directly from a binary neural network (NNX) file.
 * OUT:    pFnxVoice's internal pointers are updated so that they are ready for use.
 * RETURN: 0 on success, or one of the following errors:
             FNX_NULL_POINTER     - pFnxVoice was NULL. 
             FNX_WRONG_TYPE       - pFnxVoice did not point to a valid FnxVoice block.
             FNX_INCOMPLETE       - The footer on the FnxVoice block was incorrect.
             FNX_VERSION_MISMATCH - the FnxVoice block is not the correct version (e.g., built by obsolete code).
             FNX_UNALIGNED_BLOCK  - pFnxVoice was not an an even 4-byte boundary.
 * NOTES:  Call FnxVoiceInit BEFORE using pFnxVoice in any other calls.
 *END_HEADER***************************/
HRESULT FnxVoiceInit(FnxVoicePtr pFnxVoice);

/*FUNCTION_HEADER**********************
 * NAME:   ;FnxVoiceUserInit
 * DESC:   Initialize a block of memory to be used for a voice user.
 * IN:     pFnxVoice     - Voice pointer with information required for initializing the user block.
           pFnxVoiceUser - Pointer to a block of memory containing an uninitialized FnxVoiceUser structure
                           (e.g., read from a *.usr file).
 * OUT:    pFnxVoiceUser is ready to be used.
 * RETURN: 0 on success, or one of the following errors:
             FNX_NULL_POINTER     - pFnxVoice or pFnxVoiceUser was NULL. 
             FNX_WRONG_TYPE       - pFnxVoice or pFnxVoiceUser did not point to a valid
                                     FnxVoice or FnxVoiceUser block, respectively.
             FNX_INCOMPLETE       - The footer on the pFnxVoiceUser block was incorrect.
             FNX_VERSION_MISMATCH - the FnxVoice block is not the correct version (e.g., built by obsolete tools).
             FNX_TAG_MISMATCH     - pFnxVoice and pFnxVoiceUser were not created at the same time,
                                      so they are not allowed to be used together.
             FNX_NOT_INITIALIZED  - pFnxVoice was not initialized (and could not be initialized).
             FNX_UNALIGNED_BLOCK  - pFnxVoice or pFnxVoiceUser was not an an even 4-byte boundary.
 * NOTES:  Call FnxVoiceInit on pFnxVoice BEFORE using it in FnxVoiceUserInit().
           Call FnxVoiceUserInit before using pFnxVoiceUser in any other calls.
 *END_HEADER***************************/
HRESULT FnxVoiceUserInit(FnxVoicePtr pFnxVoice, FnxVoiceUserPtr pFnxVoiceUser);

/*FUNCTION_HEADER**********************
 * NAME:   ;FnxVocabInit
 * DESC:   Initialize a recognition vocabulary block of memory for use.
 * IN:     pFnxVocab - Pointer to the block of memory containing the recognition
                       vocabulary (e.g., read from a *.xvocab file).
 * OUT:    pFnxVocab is ready to be used.
 * RETURN: 0 on success, or one of the following errors:
             FNX_NULL_POINTER     - pFnxVoice was NULL. 
             FNX_WRONG_TYPE       - pFnxVoice did not point to a valid FnxVoice block.
             FNX_INCOMPLETE       - The footer on the FnxVoice block was incorrect.
             FNX_VERSION_MISMATCH - the FnxVoice block is not the correct version (e.g., built by obsolete code).
             FNX_UNALIGNED_BLOCK  - pXVocab was not an an even 4-byte boundary.
 * NOTES:  
 *END_HEADER***************************/
HRESULT FnxVocabInit(FnxVocabPtr pXVocab);

//Choosing/switching vocabs ==============================================================
/*FUNCTION_HEADER**********************
 * NAME:   ;FnxSelectXVocabs
 * DESC:   Choose vocabularies for a user to do recognition on.
 * IN:     pFnxVoiceUser - User to select vocabularies for.
           ppFnxVocabs   - Array of pointers to FnxVocab recognition vocabularies to use.
           iNumVocabs    - Number of elements in ppFnxVocabs[].
 * OUT:    pFnxVoiceUser has the given set of vocabularies selected and reset so
             as to be available for recognition.
 * RETURN: 0 on success, or else one of the following error codes:
              FNX_TOO_MANY_VOCABS: iNumVocabs is more than space was allocated for.
              FNX_NODE_OVERFLOW:   Number of nodes used by this set of vocabularies is more
                                     than that allowed for when building FnxVoiceUser block.
              FNX_NULL_POINTER:    pFnxVoiceUser or one of the elements in ppFnxVocabs[] was NULL.
              FNX_WRONG_TYPE:      pFnxVoiceUser or one of ppFnxVocabs[] was the wrong type of block.
              FNX_NOT_INITIALIZED: pFnxVoiceUser or one of ppFnxVocabs[] was not initialized;
              FNX_TAG_MISMATCH:    At least one vocab did not have the same tag as pFnxVoiceUser,
                                      indicating that they were not created at the same time, and
                                      thus are not allowed to be used together.  Rerun the BuildVoice tool.
 * NOTES:  The BuildVoice tool has parameters that determine the maximum number of
              vocabularies and total nodes that can be used at once, in order to allow
              game designers to use less space when they know they will only be using
              certain vocabularies together.  If you get errors from this routine,
              then you probably need to re-run the tool with values that would allow
              the set of vocabularies that is causing the error to be chosen.
 *END_HEADER***************************/
HRESULT FnxSelectXVocabs(FnxVoiceUserPtr pFnxVoiceUser, FnxVocabPtr *ppFnxVocabs, int iNumVocabs);

//Doing recognition ======================================================================
/*FUNCTION_HEADER**********************
 * NAME:   ;FnxVoiceRecognize
 * DESC:   Do some recognition on some .wav data
 * IN:     pFnxVoice     - Main speech recognition structure (used by all users on all vocabularies)
           pFnxVoiceUser - Voice user to do recognition for
           pwWaveData    - Array of 16-bit (16kHz mono) wave samples from the microphone
           iNumSamples   - Number of elements in pwWaveData[]
           puiResult     - Address in which to return flags indicating status.
 * OUT:    pFnxVoiceUser has its internal state updated to reflect the recognition
             done on the input samples.
           *puiResult receives zero or more of the following flags (or-ed together):
              RESULTS_AVAILABLE   - Recognition is finished, and the results can be gotten via GetResults().
              SPEECH_DETECTED     - The beginning of speech was detected (either this time or earlier)
              END_OF_SPEECH       - The end of speech has been detected (either this time or earlier),
                                      so any further wave data will be ignored until GetResults() is called.
 * RETURN: 0 on success, or else one of the following error codes:
              FNX_NULL_POINTER    - pFnxVoice or pFnxVoiceUser was NULL.
              FNX_WRONG_TYPE      - pFnxVoice or pFnxVoiceUser was not the right type of memory block.
              FNX_NOT_INITIALIZED - pFnxVoice or pFnxVoiceUser was not initialized (via InitVoice() or InitVoiceUser()).
              FNX_SPEECH_ERROR    - Unexpected error during speech recognition.
 * NOTES:  If the entire recognition process has been finished (i.e., end of speech detected
             and all data processed through the decoder), then RESULTS_AVAILABLE will be returned
             in *puiResult without any other work being done.
           Uses "frames" of 10ms (=160 samples at 16kHz), so to keep the amount of time
             spent per call consistent, pass in only that amount.  Passing in more is
             fine, too, but it will all be processed before returning.
           The first several (e.g., 6) frames will likely be much quicker than later frames.
 *END_HEADER***************************/
HRESULT FnxVoiceRecognize(FnxVoicePtr pFnxVoice, FnxVoiceUserPtr pFnxVoiceUser, 
                          short *pwWaveData, int iNumSamples, DWORD *puiResult);

/*FUNCTION_HEADER**********************
 * NAME:   ;FnxVoiceGetResults
 * DESC:   Get the results of speech recognition
 * IN:     pFnxVoice     - Main speech recognition structure
           pFnxVoiceUser - Voice user to get results for.
           ppdwWordID    - Address in which to return a pointer to the array of (nbest) word IDs
           ppfConfidence - Address in which to return a pointer to the array of (nbest) confidence levels
           *piNBest      - Address in which to return the number of results 
                             actually returned in the arrays (i.e., the 'n' of the nbest).
                             (0=>nothing recognized).
 * OUT:    *ppdwWordID and *ppfConfidence are updated.
           pFnxVoice is ready for recognition on the next new utterance.
 * RETURN: 0 on success, or one of the following error codes:
              FNX_NOT_INITIALIZED - if pFnxVoice or pFnxVoiceUser is not initialized.
              FNX_NULL_POINTER    - pFnxVoice or pFnxVoiceUser was NULL.
              FNX_WRONG_TYPE      - pFnxVoice or pFnxVoiceUser was not the right type of memory block.
 * NOTES:  Call this routine once after FnxVoiceRecognize() returns a RESULTS_AVAILBLE flag.
           Negative confidence values indicate that rejection is recommended.
           The returned arrays point to static arrays within the pFnxVoiceUser structure,
             so do not free them, and be sure to use any information you need before
             calling this routine again because they will be overwritten.
           This routine can also be called to cancel recognition and prepare for a new utterance.
 *END_HEADER***************************/
HRESULT FnxVoiceGetResults(FnxVoicePtr pFnxVoice, FnxVoiceUserPtr pFnxVoiceUser, 
                           DWORD **ppdwWordID, FLOAT **ppfConfidence, int *piNBest);

HRESULT FnxVoiceGetResultsGrammar(FnxVoicePtr pFnxVoice, FnxVoiceUserPtr pFnxVoiceUser, 
                           DWORD ***pppdwWordIDList, DWORD **ppdwNumWords,
                           FLOAT **ppfConfidence, int *piNBest);


/*FUNCTION_HEADER**********************
 * NAME:   ;FnxVoiceTiming
 * DESC:   Retrieve statistics on time spent in the recognition 
 * IN:     pFnxVoiceUser  - Voice user to check timing on.
           //Return parameters:
           piFeatureTicks - Number of CPU ticks spent per 10ms frame on feature extraction
           piNNetTicks    - Number of CPU ticks spent per 10ms frame on neural network calculations
           piSearchTicks  - Number of CPU ticks spent per 10ms frame on searching (decoding)
           piTotalTicks   - Number of CPU ticks spent per 10ms frame in entire recognition code
                             (NOT counting the start-up frames when only feature extraction is going.)
           pfSecondsPerTick-Number of seconds per CPU tick (usually about 1/733000000 on an XBox),
                              to use in computing ms, % of CPU, etc.
           piNumFrames    - Number of frames that *piTotalTicks was calculated over.
 * OUT:    The return parameters receive values.
 * RETURN: 0 on success, or else one of the following error codes:
              FNX_NULL_POINTER    - pFnxVoiceUser was NULL.
              FNX_WRONG_TYPE      - pFnxVoiceUser was not the right type of memory block.
              FNX_NOT_INITIALIZED - pFnxVoiceUser was not yet initialized.
 * NOTES:  During the first 11 or so frames of recognition, context information is being
             gathered, so only feature extraction is done.  After that, full processing
             is done, so the piTotalTicks includes the total frames spent per frame 
             only after full processing begins.
           Returns 0 in a parameter when there are no frames of data to compute a value
             (e.g., when recognition has not started yet, or nnet calculations have not
             yet begun).
           Values are reset at the beginning of each recognition utterance.
           When choosing a new set of vocabularies for a voice user, piTotalTicks
             will return 0 until recognition is performed on the next utterance.
           Any of the return parameters that are NULL will be ignored (i.e., you can pass
             in NULL for any values you are not interested in).
 *END_HEADER***************************/
HRESULT FnxVoiceTiming(FnxVoiceUserPtr pFnxVoiceUser, int *piFeatureTicks,
                       int *piNNetTicks, int *piSearchTicks, int *piTotalTicks,
                       float *pfSecondsPerTick, int *piNumFrames);

/************************************
 * NAME:	  FnxVocabBld
 * DESC:	  Build a recognition vocabulary file.
 * IN:     wsWordFile       - Filename of word list file containing lines with 
                              "<ID#><tab><word or phrase>\n" (Ignore any lines beginning with non-numeric characters).
           wsDictionaryFile - File with information needed to generate pronunciations.
           wsNNetFile       - Integer-point "packed net" neural network file (*.psi)
           wsXVocabFile     - Filename to write the new recognition vocabulary file to.
		   wsReserved	    - Reserved set to NULL.
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
HRESULT FnxVocabBld(LPWSTR wsWordFile, LPWSTR wsDictionaryFile, LPWSTR wsNNetFile, 
					LPWSTR wsXVocabFile, LPWSTR wsReserved, int iNBest, FLOAT fPruneLevel);


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
           ppiNumSearchPaths - Needs to be set to NULL.
           iMaxTotalSearchNodes - Total number of search graph nodes allowed at once.
                                  (0=>use max of any combination of 'iMaxSimultaneous' vocabs).
           ppiNumSearchNodes - Needs to be set to NULL.
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
