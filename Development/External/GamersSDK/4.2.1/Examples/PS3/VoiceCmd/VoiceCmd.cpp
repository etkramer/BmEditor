/*****************************************************************
 * SCE CONFIDENTIAL
 PLAYSTATION(R)3 Programmer Tool Runtime Library 110.006
 * Copyright (c) 2006 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 *
 * R & D Division, Sony Computer Entertainment Inc.
 *****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <types.h>
#include <sys/return_code.h>
#include <sys/timer.h>
#include <sys/paths.h>
#include <sys/spu_initialize.h>
#include <sysutil/sysutil_sysparam.h>

#include <cell/audio.h>
#include <cell/mic.h>
#include <cell/sysmodule.h>

#include <ringbuf.h>

#include <cell/pad.h>
#include <cell/cell_fs.h>

//#define WRITE_AUDIO_DATA	// Write audio from mic to files.
							// This includes down sampled as well
							// raw audio from the mic.

static CellMicInputFormatI rawfmt;

// Fonix Defines
// Fonix Demos
#define TASTY_70
//#define MONSTERS_10

#ifdef MONSTERS_10
char *items[] = 
{
"1	Unicorn",
"2	Dragon",
"3	Centaur",
"4	Sprite",
"5	Dwarf",
"6	Goblin",
"7	Mummy",
"8	Phoenix",
"9   Ghost",
"10  Toddler",
"",
};
#define NUMBER_OF_WORDS_IN_LIST 10
#endif //MONSTERS_10

#ifdef TASTY_70
//#Tasty vocab
char *items[] = 
{
"1	Hamburger",
"2	Hot dog",
"3	Cheeseburger",
"4	Salad",
"5	Taco",
"6	Burrito",
"7	Lasagna",
"8	Enchilada",
"9	Waffle",
"10	Pancake",
"11	Fried chicken",
"12	Spicy Dragon",
"13	Pasta",
"14	Calzone",
"15	French fries",
"16	Root beer",
"17	Soft drink",
"18	Almond",
"19	Apple",
"20	Artichoke",
"21	Asparagus",
"22	Banana",
"23	Beet",
"24	Broccoli",
"25	Buckwheat",
"26	Cabbage",
"27	Carrot",
"28	Cashew",
"29	Cattle ",
"30	Cauliflower ",
"31	Celery ",
"32	Cherry ",
"33	Chicken ",
"34	Chives ",
"35	Cinnamon ",
"36	Ice cream ",
"37	Cucumber ",
"38	Date ",
"39	Dairy ",
"40	Dill ",
"41	Eggs ",
"42	Twist cone ",
"43	Fig",
"44	Fish ",
"45	Garlic ",
"46	Ginger ",
"47	Goat ",
"48	Grape",
"49	Kelp ",
"50	Lemon ",
"51	Lettuce ",
"52	Mango ",
"53	Melon ",
"54	Mushroom ",
"55	Mustard ",
"56	Oats ",
"57	Olive ",
"58	Onion ",
"59	Orange ",
"60	Peach ",
"61	Pear ",
"62	Pepper ",
"63	Pistachio ",
"64	Plum ",
"65	Ribs ",
"66	Rice ",
"67	Rosemary ",
"68	Sheep ",
"69	Soybean ",
"70	Spinach",
"",
};
#define NUMBER_OF_WORDS_IN_LIST 70
#endif //TASTY_70

#include "VoiceCmds.h"
//////////////////////////////////////////////////////////////////////
// Fonix Globals
//////////////////////////////////////////////////////////////////////
// The path in the debugger needs to be set to the executable directory to load the files.
//# define DEFAULT_PATH "/Program Files/fonix/GamersSDK/Examples/data/VoiceCmd/Data/PS2/"
# define DEFAULT_PATH ""
#ifdef TASTY_70
#define XVOCAB_FILE      SYS_APP_HOME DEFAULT_PATH "/Tasty.xvocab"
#define VOICE_NNET_FILE  SYS_APP_HOME DEFAULT_PATH "/Tasty.vnn"
#define VOICE_USER_FILE  SYS_APP_HOME DEFAULT_PATH "/Tasty.usr"
#define VOICE_TEXT_FILE  SYS_APP_HOME DEFAULT_PATH "/Tasty.txt"
#define SINGLE_USER      /* Flag for whether to limit display to first user's speech.  
                             (Required for tasty demo, or stuff won't fit on the screen).*/
#endif

#ifdef MONSTERS_10
#define XVOCAB_FILE      SYS_APP_HOME DEFAULT_PATH "/Monsters.xvocab"
#define VOICE_NNET_FILE  SYS_APP_HOME DEFAULT_PATH "/Monsters.vnn"
#define VOICE_USER_FILE  SYS_APP_HOME DEFAULT_PATH "/Monsters.usr"
#define VOICE_TEXT_FILE  SYS_APP_HOME DEFAULT_PATH "/Monsters.txt"
#define SINGLE_USER      /* Flag for whether to limit display to first user's speech.  
                             (Required for tasty demo, or stuff won't fit on the screen).*/
#endif

// Voice processing: 11025Hz, mono, 16 bit samples, 1000 packets, 10ms each 
DWORD BYTES_PER_SAMPLE    = 2;


// Fonix voice recognition information.
// Recognition information, common to all users:
FnxVoicePtr     pFnxVoice;       /* Main (common) recognition structure. */
FnxVocabPtr     pFnxVocab;       /* Recognition vocabulary */
int             iNumWords;       /* Number of words (or phrases) in the recognition vocabulary */
char          **psWords;         /* Strings containing the words (or phrases) to display */
char          **pwsWords;        /* Wide-character versions of the same words */
int			   *pdwWordID;       /* Word ID of each word (or phrase) */


// This data can be used per device that is on the system.
HRESULT PushToTalk();
FnxVoiceUserPtr pVoiceUser;      /* Voice User structure for this headset */
int            *piWordPos;       /* Current position in the display list of the given word */
int            *piOldPos;        /* Previous position in the display list of the given word */
int             eState;          /* Current state (see below) */
int             bSpeechDetected; /* Flag for whether speech has been detected in the current utterance */
HRESULT         err;             /* Error code */
float          *pfConfidence;    /* Confidence of each word (NO_CONFIDENCE => don't display a confidence) */

// Helper function to read a word list text file and associated ID numbers.
extern "C" int FnxReadWordsFromList(char **psBuf, char ***ppsWords, int **ppiWordIDs, char **psGrammar );
void *ReadBinaryFile1(char *wsFilename, int *piSize);

enum {VOICE_NOT_CONNECTED, // No communicator on this port
      VOICE_IDLE,          // Not doing any speech recognition [waiting for push-to-talk button]
      VOICE_LISTENING,     // Currently accepting wave input into the recognizer
      VOICE_FINISHING,     // Hit end-of-speech, and finishing processing on buffered data [don't send more wave samples]
      VOICE_DISPLAYING,    // Finished processing speech, got result, now displaying/animating results.
      VOICE_UNAVAILABLE};  // Voice recognition could not be initialized.

const float NO_CONFIDENCE=-9999.0f;
// Pad Variables
/*E  Variables */

#define MAX_PAD 2
//#define CELL_PAD_AUTO_PARSE

/* pad read */
#define PAD_NO_CHANGE  0
#define PAD_LEFT       0x8000
#define PAD_DOWN       0x4000
#define PAD_RIGHT      0x2000
#define PAD_UP         0x1000
#define PAD_START      0x0800
#define PAD_R3         0x0400
#define PAD_L3         0x0200
#define PAD_SELECT     0x0100
#define PAD_SQUARE     0x0080
#define PAD_CROSS      0x0040
#define PAD_CIRCLE     0x0020
#define PAD_TRIANGLE   0x0010
#define PAD_R1         0x0008
#define PAD_L1         0x0004
#define PAD_R2         0x0002
#define PAD_L2         0x0001

static bool padPressed;

// Test data
#ifdef WRITE_AUDIO_DATA
int		gfp16;
int		gfp48;
#endif

//-----------------------------------------------------------------------------
// Usage
//-----------------------------------------------------------------------------
void Usage()
{
    int i;
    printf("\n\nFonix Recognition Test\n");
    printf("\nSample: VoiceCmd\n\n");   
    printf("Instructions\n");
    printf("- Push the CROSS button to start recognition\n");
    printf("\n");

    printf("Recognition Words:\n");
    for (i = 1; i <= NUMBER_OF_WORDS_IN_LIST; i++)
    {
      printf("%-20s", pwsWords[i-1]);
      if ((i % 5) == 0)
      {
         printf("\n");
      }
    }
    printf("\n");
    printf("\n");
}

//-----------------------------------------------------------------------------
// Name: LoadRecognitionSupport()
// Desc: Called during device initialization, this code loads the support
//       xvocab, vnn, and user files for recogntion.
//-----------------------------------------------------------------------------
int LoadRecognitionSupport()
{
   // Load the common voice recognition information.
   pFnxVocab = (FnxVocabPtr)ReadBinaryFile1(XVOCAB_FILE, NULL/*&iVocabSize*/);
   pFnxVoice = (FnxVoicePtr)ReadBinaryFile1(VOICE_NNET_FILE, NULL/*&iVoiceSize*/);
   // Initialize the common voice recognition block (or set m_err to FNX_NULL_POINTER if the read failed)
   err  = FnxVoiceInit(pFnxVoice);

   if (!err)
   {
      // Load vocabulary
      err = FnxVocabInit(pFnxVocab);
      if (!err)
      {
         // Load corresponding text file, and build an array of words to display.
         iNumWords = FnxReadWordsFromList(items, &psWords, &pdwWordID, NULL);
         if (psWords && iNumWords>0)
         {
            int i;
            pwsWords  = (char**)malloc( sizeof(char*) * iNumWords);
            printf("malloc: pwsWords = %d\n", sizeof(char*) * iNumWords);
            for (i = 0; i < iNumWords; i++)
            {
               size_t iLength = strlen(psWords[i]);
               pwsWords[i] = (char*)malloc((sizeof( char ) * iLength) + 2);
               printf("malloc: pwsWords[%d] = %d\n", i, (sizeof( char ) * iLength) + 2);
               memset(pwsWords[i], 0, iLength+2);
               strncpy(pwsWords[i], psWords[i], iLength);
            }
         }
      }
   }

   /*******************************************/
   // Initialize user-specific ASR information
   eState = VOICE_UNAVAILABLE; // unless initialization is successful.
   if (pFnxVoice)
   {
       pVoiceUser = (FnxVoiceUserPtr)ReadBinaryFile1(VOICE_USER_FILE, NULL);
       if (pVoiceUser)
       {
          int i;
          err = FnxVoiceUserInit(pFnxVoice, pVoiceUser);
          if (err)
          {
             free(pVoiceUser);
             pVoiceUser=NULL;
          }
          else
          {
             // Select the one and only vocabulary
             err = FnxSelectXVocabs(pVoiceUser, &pFnxVocab, 1);
          }

          // Start all words out in the order they came from.
          piWordPos    = (int*)malloc (sizeof(int) * iNumWords);
          piOldPos     = (int*)malloc (sizeof(int) * iNumWords);
          pfConfidence = (float*)malloc (sizeof(float) * iNumWords);

          for (i = 0; i < iNumWords; i++)
          {
             piWordPos[i] = piOldPos[i] = i;
             pfConfidence[i] = NO_CONFIDENCE;
          }

          // Start out in the idle state
          eState = VOICE_IDLE;
          bSpeechDetected = false;
       }

   }
   return err;
}

//-----------------------------------------------------------------------------
// Name: ProcessRecognition()
// Desc: Called during device activation, the data is sent to the
//       recogntion engine.
//-----------------------------------------------------------------------------
int ProcessRecognition(int devnum, void *clean_samples, int len)
{
	uint16_t waveData[0x150] = {0};
	uint32_t recognBuffLength = 0;  // Take every third sample
    uint16_t* pusBuf  = (uint16_t*)clean_samples;

	// IMPORTANT: We need to down sample the audio from 48000 to 16000
	// to match the vnn and neural net sample rate.
    for (int32_t i = 0; i < len; i++) 
    {
		if (!(i%3))
		{
			waveData[recognBuffLength++] = pusBuf[i];
		}
    }

    //************************************************/
    if (eState == VOICE_LISTENING || eState == VOICE_FINISHING)
    {
      DWORD uiResult;

      // Submit the copied data to the speech recognizer
      if (eState == VOICE_FINISHING)
      {
         printf("finished recording needing more process time\n");

         // Provide time to finish doing recognition, but don't send more data
         err = FnxVoiceRecognize(pFnxVoice, pVoiceUser, NULL, 0, &uiResult);
      }
      else 
      {
#ifdef WRITE_AUDIO_DATA
		  if (gfp16 && gfp48)
		  {
			cellFsWrite(gfp16, waveData, recognBuffLength*2, NULL);
			cellFsWrite(gfp48, clean_samples, len*2, NULL);
		  }
#endif
         // Send in some wave data and do some processing 
         err = FnxVoiceRecognize(pFnxVoice, pVoiceUser, 
              (short *)(waveData), recognBuffLength - 1, &uiResult);

         if (uiResult & END_OF_SPEECH)
         {
			eState = VOICE_FINISHING;
         }

         if (uiResult & SPEECH_DETECTED)
         {
            bSpeechDetected = true;
         }
         else
         {
            bSpeechDetected = false;
         }
      }
      if (uiResult & RESULTS_AVAILABLE)
      {
         int iNBest, n, iWord;
         unsigned int *pdwTempWordID;
         float *pfTempConfidence;

		 padPressed = false;
#ifdef WRITE_AUDIO_DATA
		 cellFsClose(gfp16);
		 cellFsClose(gfp48);
		 gfp16 = 0;
		 gfp48 = 0;
#endif
		 eState = VOICE_UNAVAILABLE;
		 cellMicStop(0);

         // Processing has completed on the speech, so get the results
         err = FnxVoiceGetResults(pFnxVoice, pVoiceUser, &pdwTempWordID, &pfTempConfidence, &iNBest);
         if (err)
         {
            printf("error in getting results\n");
         }
         else
         {
         // Prepare to display animation
            eState = VOICE_DISPLAYING;
            printf("Recognized words\n");
         }

         // Clear the confidence score of every word
         for (iWord=0; iWord < iNumWords; iWord++)
         {
            pfConfidence[iWord] = NO_CONFIDENCE;
            piOldPos[iWord]     = piWordPos[iWord];
         }

         // Find each recognized word in the list
         for (n=0; n < iNBest; n++)
         {
            for (iWord = 0; iWord < iNumWords; iWord++)
            {
               if (pdwTempWordID[n] == (unsigned int) pdwWordID[iWord])
                  break;
            }
            if (iWord < iNumWords)
            {
               pfConfidence[iWord] = pfTempConfidence[n];
               //m_pfConfidence[iWord] = (FLOAT)n+1; // use rank instead of confidence
               piWordPos[iWord] = n;
            }
         }
         for (iWord=0; iWord < iNumWords; iWord++)
         {
            if (pfConfidence[iWord] == NO_CONFIDENCE)
               piWordPos[iWord] = n++;
         }
         // at this point, n should be equal to m_pParent->m_iNumWords.

         {
            int col = 1;
            for (iWord=0; iWord < iNumWords; iWord++)
            {
               int i;
               for ( i = 0; i < iNumWords; i++)
               {
                   if ( piWordPos[i] == iWord)
                   {
                      if (pfConfidence[i] != NO_CONFIDENCE)
                      {
                         printf("%2d %-20s %3.5f\n", piWordPos[i]+1, pwsWords[i], pfConfidence[i]);
                         if (iWord == 9)
                            printf("\n");
                         break;
                      }
                      else 
                      {
                         printf("%-20s",pwsWords[i]);
                         if ((col % 6) == 0)
                         {
                            printf("\n");
                         }
                         col++;
                         break;
                      }
                   }
               }
            }
			printf("\n");
//enable the next line for continious    
//       StartRecording();
         }
      }
    }  /* Do ASR */
	return 0;
}


/************************************
 * NAME:	  ReadBinaryFile1
 * DESC:	  Read a binary file into a newly-allocated block of memory.
 * IN:     wsFilename - Name of the file to read.
           piSize     - Address in which to return the size of the file. (NULL=>ignore).
 * OUT:    'wsFilename' is read into a newly-allocated block of memory.
           *piSize contains the size.
 * RETURN: Pointer to the block of memory
 * NOTES:  Simple helper utility to get binary speech blobs into memory.
           (Games should typically have their own way of getting blocks into memory).
 *END_HEADER***************************/
void *ReadBinaryFile1(char *wsFilename, int *piSize)
{
   int fp;
   void *pvBuffer;
   uint64_t lSize=0;
   uint64_t nread=0;
   uint64_t offset=0;
   int	ret;

   ret = cellFsOpen( wsFilename, CELL_FS_O_RDONLY, &fp, NULL, 0);
   if (fp>=0)
   {
	  ret = cellFsLseek(fp, 0, CELL_FS_SEEK_END, &lSize);

      pvBuffer = (void *)malloc(lSize);
      printf("malloc: %s = %d\n", wsFilename, (int)lSize);
      if (pvBuffer!=NULL)
      {
		 ret = cellFsLseek(fp, 0, CELL_FS_SEEK_SET, &offset);
		 nread = lSize;
         ret = cellFsRead(fp, pvBuffer, lSize, &nread);
		 if (nread != lSize)
		 {
            free(pvBuffer);
            pvBuffer=NULL; // couldn't read full amount, strangely.
         }
         ret = cellFsClose(fp);
      }
   }
   else pvBuffer=NULL;

   if (piSize)
      *piSize = (int)lSize;
   return pvBuffer;
}  /* ReadBinaryFile1 */

// Pad Functions  -------------------------------------------------------------------------
int padRead(void)
{
	CellPadData padData;
	
	cellPadGetData (0, &padData);
	
	if (padData.len == 0){
		return PAD_NO_CHANGE;
	}
	
	return (padData.button[2]<<8 | padData.button[3]);
}


static int32_t initializePadModule(void)
{
	int32_t ret;

	ret = cellPadInit (MAX_PAD);
    if (ret != CELL_OK){
        printf ("cellPadInit failed 0x%08x\n", ret);
		return(ret);
    }
	return(ret);
}

static int32_t finalizeModule(void)
{
	int32_t ret;
	ret = cellPadEnd ();
    if (ret != CELL_OK){
        printf ("cellPadEnd failed 0x%08x\n", ret);
    }

	return(ret);
}

// -------------------------------------------------------------------------


#define USE_LIBMIC_CALLBACK

#if 0
//----------------------------------------------------------------------------------------------------------------------------------
static const CellSurMixerConfig         sSurMixerConfig =
{
    priority:333,
    chStrips1:1,
    chStrips2:0,
    chStrips6:0,
    chStrips8:0,
};

static CellAANHandle                    hSurMixerHandle;
static uint32_t                         iSurMixerPortNr;

static float                            fPlayingBuffer[32*1024] __attribute__ ((aligned(128))); 
static class RingBuffer                 playRingBuffer;  //the far-end voice playback buffer

//----------------------------------------------------------------------------------------------------------------------------------
static int MixerCallback(void *pArg, uint32_t iCounter, uint32_t iSamples)
{
    static float fSamples[400];

    int iSamplesFull = (playRingBuffer.Read((char*)(&fSamples[0]), iSamples*sizeof(float)))/sizeof(float);
    if (iSamples > (uint32_t)iSamplesFull)
    {
        memset(&fSamples[iSamplesFull], 0, (iSamples-iSamplesFull)*sizeof(float));
    }

	// Process the audio data for recognition here.


    cellAANAddData(hSurMixerHandle, iSurMixerPortNr, 0, &fSamples[0], iSamples);

    return 0;
}

inline void _up_samprate_3(float x_n_1, float x_n, float* y)
{
    const float fFactor1 = (1.0f/3.0f);
    const float fFactor2 = (2.0f/3.0f);

    y[0] = x_n_1;
    y[1] = x_n_1*fFactor2+x_n*fFactor1;
    y[2] = x_n_1*fFactor1+x_n*fFactor2;
}
#endif

static int ProcessLocalVoice(int devnum)
{
	int asrError;
    static uint8_t clean_samples[2048]   __attribute__ ((aligned(128))); 
    int len = cellMicReadRaw(devnum,&clean_samples[0],(int)sizeof(clean_samples));
    if (len <= 0) 
		return len;

    int numSamples = len/sizeof(uint16_t);

	asrError = ProcessRecognition(devnum, clean_samples, numSamples);

/*
	playRingBuffer.Write((char*)(&fSamples[0]), numSamples*3*sizeof(float));
*/    
	return len;

}

#ifndef USE_LIBMIC_CALLBACK
static int WaitForDeviceAttach(int iDevNum)
{
	int i=0;
	printf("waitattach enter\n");
    while ( !cellMicIsAttached(iDevNum) )
    {
		sys_timer_usleep(1000*64);
		if ((i++&0xff)==0) printf("waiting for microphone to attach\n");
    }
    int err = cellMicOpen(iDevNum, 16000);
    if (err != CELL_OK)
    {
        printf("cellMicOpen fails: 0x%x\n", err);
        return err;
    }

    //Below shows example of how to use set/get attribute libmic api
    float fBackgroundNoiseGain = 5;  //5db noise reduction gain
//    int iAGCLevel = 13000;  //agc target level
    int iAGCLevel = 33;  //agc target level

    cellMicSetSignalAttr(iDevNum,CELLMIC_SIGATTR_BKNGAIN,&fBackgroundNoiseGain);
    cellMicSetSignalAttr(iDevNum,CELLMIC_SIGATTR_AGCLEVEL,&iAGCLevel);

    cellMicGetSignalAttr(iDevNum,CELLMIC_SIGATTR_BKNGAIN,&fBackgroundNoiseGain);
    cellMicGetSignalAttr(iDevNum,CELLMIC_SIGATTR_AGCLEVEL,&iAGCLevel);
    printf("set background noise reduction gain at %2.1f decibels\n", fBackgroundNoiseGain);
    printf("set target voice volume at: %d\n", iAGCLevel);
    
    //err = cellMicStart(iDevNum);
    //if (err != CELL_OK)
    //{
    //    printf("cellMicStart fails: 0x%x\n", err);
    //    return err;
    //}

    return CELL_OK;
}
#endif

//----------------------------------------------------------------------------------------------------------------------------------
int main(int argc, const char **argv)
{
    int     err;
	int     ret;  // Use with pad

    err = cellSysmoduleInitialize();
	printf("cellSysmoduleInitialize() : %d\n", err);
	if (err != CELL_OK) return -1;
 
	err = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
	if (err != CELL_OK) 
    {
        printf("cellSysmoduleLoadModule(CELL_SYSMODULE_FS) fails: 0x%x\n", err);
        return -1;
    }
 
	err = cellSysmoduleLoadModule(CELL_SYSMODULE_AUDIO);
	if (err != CELL_OK) 
    {
        printf("cellSysmoduleLoadModule(CELL_SYSMODULE_AUDIO) fails: 0x%x\n", err);
        return -1;
    }
 
    err = cellSysmoduleLoadModule(CELL_SYSMODULE_MIC);
	if (err != CELL_OK) 
    {
        printf("cellSysmoduleLoadModule(CELL_SYSMODULE_MIC) fails: 0x%x\n", err);
        return -1;
    }

    // Initialize cell audio.
    printf("Initialize cell audio.\n");
    err = cellAudioInit();
    if (err != CELL_OK)
    {
        printf("cellAudioInit() returned %i\n", err);
        return -1;
    }

    err = cellMicInit();
    if (err != CELL_OK)
    {
        printf("cellMicInit() failed with result = %i\n", err);
        return -1;
    }
//    playRingBuffer.Init((char*)(&fPlayingBuffer[0]), sizeof(fPlayingBuffer));


    printf("Set mixer callback.\n");
	err = initializePadModule();
	if(err != CELL_OK){
		printf("initializePadModule failed 0x%08x", err);
		return -1;
	}

    // Init Fonix Support files
    LoadRecognitionSupport();

    printf("\nPress X to recognize on a word.\n");

    printf("Searching for attached USB Headsets...\n");
    

#ifdef USE_LIBMIC_CALLBACK
    sys_event_t         callback_event;
    sys_event_queue_t   callback_queue;
    uint64_t            equeue_key;
    const uint64_t      equeue_key_base = 0x0000000072110700UL;

    //create event queue to recv "MicIn" callback from MIOS
    sys_event_queue_attribute_t  equeue_attr = {SYS_SYNC_FIFO, SYS_PPU_QUEUE};
    equeue_key = equeue_key_base;

    int tryCount = 0;
    while ( (tryCount++) < 100 )
    {
        err = sys_event_queue_create(&callback_queue, &equeue_attr, equeue_key, 32);
        if (err == CELL_OK) break;
        equeue_key =  equeue_key_base | ( rand() & 0xffff);
    }
    if (err != CELL_OK)
    {
        printf("sys_event_queue_create failure:(0x%x)\n", err);
        return -1;
    }
    printf("created callback event que(0x%08X%08X)\n",(uint32_t)(equeue_key>>32), (uint32_t)equeue_key);

    //install "MicIn" system-callback(with devnum == -1) to recv attach/detach event
    err = cellMicSetNotifyEventQueue(equeue_key);
    if (err != CELL_OK)
    {
        printf("cellMicSetNotifyEventQueue failure:(0x%x)\n", err);
        return err;
    }
#endif

	Usage();

    while (1)
    {
		ret = padRead();
		if (ret & PAD_CROSS)
		{
			if (!padPressed)
			{
				padPressed = true;
#ifdef WRITE_AUDIO_DATA
				ret = cellFsOpen( SYS_APP_HOME DEFAULT_PATH "/Tasty16.raw", 
					CELL_FS_O_RDWR | CELL_FS_O_CREAT | CELL_FS_O_TRUNC, &gfp16, NULL, 0);
				ret = cellFsOpen( SYS_APP_HOME DEFAULT_PATH "/Tasty48.raw", 
					CELL_FS_O_RDWR | CELL_FS_O_CREAT | CELL_FS_O_TRUNC, &gfp48, NULL, 0);
#endif
				eState = VOICE_LISTENING;
				cellMicStart(0);
			}
		}

    #ifdef USE_LIBMIC_CALLBACK
        err = sys_event_queue_receive(callback_queue, &callback_event, 32*1000);
        if(err == ETIMEDOUT) continue;

        int msg = (int)callback_event.data1;
        int iDevNum = (int)callback_event.data2;

        switch (msg)
        {
        case CELLMIC_ATTACH:
            printf("recv CELLMIC_ATTACH message: dev_num(%d)\n", iDevNum);
			err = cellMicOpenEx( 0,     /* dev_num = 0 */
                      16000, /* open device raw data at 48000 hz sample rate */
                      1,     /* choose maximum device channel */
                      16000, /* open the DSP data at 16000 hz sample rate */
                      1000,   /* set the audio input stream ring buffer size at 1000 milli-seconds */
                      CELLMIC_SIGTYPE_RAW    /* open only raw data type */
                    );  
			if (err != CELL_OK)
			{
				printf("unsupported format\n");
				exit(0);
			}

			cellMicGetFormatRaw(0, &rawfmt);
			printf("the device RAW data format: bNrChannels(%d channels), bSubframeSize(%d bytes), bBitResolution(%d bits), bDataType(%d), uiSampRate(%d hz)\n",
               rawfmt.bNrChannels, rawfmt.bSubframeSize, rawfmt.bBitResolution, rawfmt.bDataType, rawfmt.uiSampRate);

            break;
        case CELLMIC_DETACH:
            printf("recv CELLMIC_DETACH message: dev_num(%d)\n", iDevNum);
            cellMicClose(iDevNum);
            break;
        case CELLMIC_DATA:
            ProcessLocalVoice(iDevNum);
            break;
        default:
            break;
        }

    #else
        int retcode = WaitForDeviceAttach(0);
        if (retcode != CELL_OK)
        {
            return retcode;
        }
        static int counter = 0;
        int err = 0;
        while (cellMicIsAttached(0))
        {
            sys_timer_usleep(32*1000);
            err = ProcessLocalVoice(0);
            if ( (++counter%2000) == 0)
            {
                cellMicReset(0);
            }
        }
        err = cellMicClose(0);
    #endif
    }

	finalizeModule();
    return 0;
}



