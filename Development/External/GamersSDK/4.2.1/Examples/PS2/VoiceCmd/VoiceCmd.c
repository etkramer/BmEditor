/*

  voicecmd.c

  Sample which demonstrates a push-to-talk scenario with voice recognition

  Description:
        This sample shows how to implement a quick and
    dirty push to talk scenario. It demonstrates recognition 
    and records the user's voice and playing it back. 

  Instructions:
        - Push the SELECT button to display device capabilities
        - Push the EKS button to record
        - Release the EKS button to stop recording
        - Push the CIRCLE button to play back what you recorded
        - Release the CIRCLE button to stop playback
        - Use L1 to increase the input gain
        - Use L2 to decrease the input gain
        - Use R1 to increase the playback volume
        - Use R2 to decrease the playback volume
        
  This Fonix example is based on the Logitech USB Audio SDK ppt example.

*/

#define DISPLAY_TIMING

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

#include <eekernel.h>
#include <eeregs.h>
#include <libgraph.h>
#include <stdio.h>
#include <sifdev.h>
#include <sifrpc.h>
#include <libpad.h>
#include <libcdvd.h>
#include <stdlib.h>
#include <string.h>

#include <liblgaud.h>

#include "VoiceCmds.h"

#include <errno.h>

//////////////////////////////////////////////////////////////////////
// Fonix Ugly Globals
//////////////////////////////////////////////////////////////////////
// The path in the debugger needs to be set to the executable directory to load the files.
# define DEFAULT_PATH "/Program Files/fonix/GamersSDK.3.3/Examples/data/VoiceCmd/Data/PS2/"
#ifdef TASTY_70
#define XVOCAB_FILE      "host0:" DEFAULT_PATH "Tasty.xvocab"
#define VOICE_NNET_FILE  "host0:" DEFAULT_PATH "Tasty.vnn"
#define VOICE_USER_FILE  "host0:" DEFAULT_PATH "Tasty.usr"
#define VOICE_TEXT_FILE  "host0:" DEFAULT_PATH "Tasty.txt"
#define SINGLE_USER      /* Flag for whether to limit display to first user's speech.  
                             (Required for tasty demo, or stuff won't fit on the screen).*/
#endif

#ifdef MONSTERS_10
#define XVOCAB_FILE      "host0:" DEFAULT_PATH "Monsters.xvocab"
#define VOICE_NNET_FILE  "host0:" DEFAULT_PATH "Monsters.vnn"
#define VOICE_USER_FILE  "host0:" DEFAULT_PATH "Monsters.usr"
#define VOICE_TEXT_FILE  "host0:" DEFAULT_PATH "Monsters.txt"
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
    unsigned int   *pdwWordID;       /* Word ID of each word (or phrase) */


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
int ReadWordList(char *sWordList, char ***ppsWords, DWORD **ppiWordIDs);
void *ReadBinaryFile1(char *wsFilename, int *piSize);

enum {VOICE_NOT_CONNECTED, // No communicator on this port
      VOICE_IDLE,          // Not doing any speech recognition [waiting for push-to-talk button]
      VOICE_LISTENING,     // Currently accepting wave input into the recognizer
      VOICE_FINISHING,     // Hit end-of-speech, and finishing processing on buffered data [don't send more wave samples]
      VOICE_DISPLAYING,    // Finished processing speech, got result, now displaying/animating results.
      VOICE_UNAVAILABLE};  // Voice recognition could not be initialized.

const float NO_CONFIDENCE=-9999.0f;



//////////////////////////////////////////////////////////////////////
// Ugly Globals (PADMAN)
//////////////////////////////////////////////////////////////////////
unsigned char g_cPadData[32];
unsigned short g_wButtonData = 0;
u_long128 pad_dma_buf[scePadDmaBufferMax] __attribute__ ((aligned(64)));

//////////////////////////////////////////////////////////////////////
// Even Uglier Globals (USBAUDIO)
//////////////////////////////////////////////////////////////////////

#define IN_SAMPLING_FREQUENCY 11025
#define IN_SAMPLING_BITS 16
#define IN_SAMPLING_CHANNELS 1
#define IN_SPOOLERBUF 10 * IN_SAMPLING_FREQUENCY * IN_SAMPLING_CHANNELS * (IN_SAMPLING_BITS/8)

#define OUT_SAMPLING_FREQUENCY 11025
#define OUT_SAMPLING_BITS 16
#define OUT_SAMPLING_CHANNELS 1

#define PLAYBACKLENGTH 10   // milliseconds
#define RECORDLENGTH 10     // milliseconds  
#define PLAYCHUNK 9000      // enough to hold more than a few frames of audio
#define READCHUNK 9000      // enough to hold more than a few frames of audio

#define min(a, b) (a) < (b) ? (a) : (b)
#define max(a, b) (a) > (b) ? (a) : (b)

u_char* g_Spooler = NULL;
int g_isRecording = 0;
int g_isPlaying = 0;
int g_usb_device = LGAUD_INVALID_DEVICE;
u_char* g_pReadPtr = NULL;
u_char* g_pWritePtr = NULL;
lgAudDeviceDesc g_usb_device_desc;
int g_playback_volume = 100;
int g_record_volume = 100;
u_char* g_pPlaybackEnd = NULL;


//////////////////////////////////////////////////////////////////////
// Prototypes
//////////////////////////////////////////////////////////////////////

// Fonix
int LoadRecognitionSupport();


// Logitechs
int LoadModules();
void ReadPad(int pad_device);
void InitAudioDevice(int usb_device, int mode);
int FindAndOpenHeadsetDevice();
int CloseDevice();
int StartRecording();
int StopRecording();
int StartPlayback();
int StopPlayback();
void Usage();
void ShowDeviceSupport();
void SifLoadModule(char *module, char *arg, int arglen);

//////////////////////////////////////////////////////////////////////
// Main Loop
//////////////////////////////////////////////////////////////////////
int main()
{
    int pad_device = 0;

    if (!LoadModules())
        return 0;
    
    // Init Padman
    scePadInit(0);
    if (scePadPortOpen( pad_device, 0, pad_dma_buf ) == 0)
    {
        printf( "ERROR: scePadPortOpen\n" );
        return 0;
    }   

    // Init USB Audio
    lgAudInit(NULL, NULL);

    // Create our internal global buffers
    g_Spooler = (u_char*)malloc(IN_SPOOLERBUF);
    printf("malloc: spooler = %d\n", IN_SPOOLERBUF);
    memset(g_Spooler, 0, IN_SPOOLERBUF);
    g_pReadPtr = g_Spooler;
    g_pWritePtr = g_Spooler;
    g_pPlaybackEnd = g_Spooler;


    // Init Fonix Support files
    LoadRecognitionSupport();

    printf("\nPress X to recognize on a word.\n");


    // Usage 
    Usage();

    printf("Searching for attached USB Headsets...\n");
    
    while (1)
    {
        // If the current device is invalid, search for one
        if (g_usb_device == LGAUD_INVALID_DEVICE)
        {
            g_usb_device = FindAndOpenHeadsetDevice();
        }
        else
        {
            ReadPad(pad_device);
            
            // Are we recording?
            if (g_isRecording)
            {
                // collect our data at the read point
                int bytes_to_read = min(READCHUNK, (int)((g_Spooler + IN_SPOOLERBUF) - g_pReadPtr));
                if (bytes_to_read > 0)
                {
                    if (LGAUD_SUCCEEDED(lgAudRead(g_usb_device, LGAUD_BLOCKMODE_NOT_BLOCKING, g_pReadPtr, &bytes_to_read)))
                    {
                       
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
                             // Send in some wave data and do some processing 
                             err = FnxVoiceRecognize(pFnxVoice, pVoiceUser, 
                                  (short *)(g_pReadPtr), bytes_to_read/BYTES_PER_SAMPLE, &uiResult);

                             if (uiResult & END_OF_SPEECH)
                             {
                                eState = VOICE_FINISHING;
                                StopRecording();
                             }

                             if (uiResult & SPEECH_DETECTED)
                             {
                                bSpeechDetected = TRUE;
                             }
                             else
                             {
                                bSpeechDetected = FALSE;
                             }
                          }
                          if (uiResult & RESULTS_AVAILABLE)
                          {
                             int iNBest, n, iWord;
                             int *pdwTempWordID;
                             float *pfTempConfidence;


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
                                   if (pdwTempWordID[n] == pdwWordID[iWord])
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
//enable the next line for continious    
//                                StartRecording();
                             }

                          }

                        }  /* Do ASR */

                        // update the read ptr
                        g_pReadPtr += bytes_to_read;

                    }
                    else
                    {
                        printf("Lost connection to device. Closing it..\n");
                        CloseDevice();
                    }
                }
            }
            
            // Are we writing?
            if (g_isPlaying)
            {
                // write in chunks of 10ms of data (or whatever is remaining if its smaller) 
                int bytes_left_to_write = g_pPlaybackEnd - g_pWritePtr;
                int bytes_to_write = min(PLAYCHUNK, bytes_left_to_write);
                
                if (bytes_to_write > 0)
                {
                    if (LGAUD_SUCCEEDED(lgAudWrite(g_usb_device, LGAUD_BLOCKMODE_NOT_BLOCKING, g_pWritePtr, &bytes_to_write)))
                    {
                        //printf("Wrote %d bytes of data\n", bytes_to_write);
                        
                        // adjust our write ptr
                        g_pWritePtr += bytes_to_write;
                        
                        // checkf if we have played all our recorded data
                        if (g_pWritePtr >= g_pPlaybackEnd)
                        {
                            printf("** All recorded data has been played ***\n");
                            StopPlayback();
                        }
                    }
                    else
                    {
                        printf("Lost connection to device. Closing it..\n");
                        CloseDevice();
                    }
                }
            }
        }

        // wait for next frame...
    	sceGsSyncV(0);
    }
    
    return 0;
}


//////////////////////////////////////////////////////////////////////
// LoadModules
//
// Loads the irx modules
//////////////////////////////////////////////////////////////////////
int LoadModules()
{
    
    char usbd_argv[] = "conf=2048\0";
    char lgaud_argv[32];
    // we'll be limiting the stream to the largest we'll ever read in one
    // single call to lgAudWrite()/lgAudRead(). This saves space on the IOP!
    sprintf(lgaud_argv, "maxstream=%d", max(PLAYCHUNK, READCHUNK));
    strcat(lgaud_argv, "\0");

    sceSifInitRpc(0);

    while(!sceSifRebootIop("host0:/usr/local/sce/iop/modules/" IOP_IMAGE_file));    while(!sceSifSyncIop());

    sceSifInitRpc(0);
    sceSifInitIopHeap();
    sceSifLoadFileReset();
    
    SifLoadModule("sio2man.irx", 0, 0);
    SifLoadModule("padman.irx", 0, 0);
    SifLoadModule("usbd.irx", usbd_argv, sizeof(usbd_argv));
    SifLoadModule(LGAUD_IRXNAME, lgaud_argv, strlen(lgaud_argv)+2);
    
    return 1;
}


//////////////////////////////////////////////////////////////////////
// InitAudioDevice
//
// Initializes the attached device
//////////////////////////////////////////////////////////////////////
void InitAudioDevice(int usb_device, int mode)
{

    switch(mode)
    {
    case LGAUD_MODE_RECORDING:
        lgAudSetRecordingVolume(g_usb_device, LGAUD_CH_BOTH, g_record_volume);
        break;
    case LGAUD_MODE_PLAYBACK:
        lgAudSetPlaybackVolume(usb_device, LGAUD_CH_BOTH, g_playback_volume);
        break;
    default:
        break;
    }
}

//////////////////////////////////////////////////////////////////////
// ReadPad
//
// Standard read pad
//////////////////////////////////////////////////////////////////////
void ReadPad(int pad_device)
{

    unsigned short buttondata;
    
    if ( scePadRead(0, 0, g_cPadData) > 0 )
    {
        buttondata = 0xffff ^ ((g_cPadData[2] << 8) | g_cPadData[3]);
    }
    else
    {
        buttondata = 0;
        return;
    }
            
    if( g_cPadData[0] == 0 )
    {
        if( (buttondata & SCE_PADRdown) &&  !(g_wButtonData & SCE_PADRdown) )
        {
            // user has pressed the EKS button.
            //printf("EKS button was pressed\n");
            StartRecording();
        }
        else
        if( !(buttondata & SCE_PADRdown) && (g_wButtonData & SCE_PADRdown) )
        {
            // user has released the EKS button.
            //printf("EKS button was released\n");
//            StopRecording();
        }
        else
        if( (buttondata & SCE_PADRright))
        {
            // user has pressed the CIRCLE button
            //printf("CIRCLE button was pressed\n");
            if (g_isPlaying == 0)
                StartPlayback();
        }
        else
        if( !(buttondata & SCE_PADRright) && (g_wButtonData & SCE_PADRright))
        {
            // user has released the CIRCLE button
            //printf("CIRCLE button was released\n");
//            StopPlayback();
        }

        if( (buttondata & SCE_PADL1) && !(g_wButtonData & SCE_PADL1))
        {
            // user has pressed the L1 button
            //printf("L1 button was pressed\n");

            g_record_volume += 5;
            if (g_record_volume > 100)
                g_record_volume = 100;
                        
            if (lgAudSetRecordingVolume(g_usb_device, LGAUD_CH_BOTH, g_record_volume) == LGAUD_SUCCESS)
            {
                printf("Record volume is now at %d\n", g_record_volume);
            }
        }
        else
        if( (buttondata & SCE_PADL2) && !(g_wButtonData & SCE_PADL2))
        {
            // user has pressed the L2 button
            //printf("L2 button was pressed\n");
            
            g_record_volume -= 5;
            if (g_record_volume < 0)
                g_record_volume = 0;

            if (lgAudSetRecordingVolume(g_usb_device, LGAUD_CH_BOTH, g_record_volume) == LGAUD_SUCCESS)
            {
                printf("Record volume is now at %d\n", g_record_volume);
            }
        }
        else
        if( (buttondata & SCE_PADR1) && !(g_wButtonData & SCE_PADR1))
        {
            // user has pressed the R1 button
            //printf("R1 button was pressed\n");
            
            g_playback_volume += 5;
            if (g_playback_volume > 100)
                g_playback_volume = 100;
            
            if (lgAudSetPlaybackVolume(g_usb_device, LGAUD_CH_BOTH, g_playback_volume) == LGAUD_SUCCESS)
            {
                printf("Playback volume is now at %d\n", g_playback_volume);
            }
            
        }
        else
        if( (buttondata & SCE_PADR2) && !(g_wButtonData & SCE_PADR2))
        {
            // user has pressed the R2 button
            //printf("R2 button was pressed\n");
            
            g_playback_volume -= 5;
            if (g_playback_volume < 0)
                g_playback_volume = 0;
            
            if (lgAudSetPlaybackVolume(g_usb_device, LGAUD_CH_BOTH, g_playback_volume) == LGAUD_SUCCESS)
            {
                printf("Playback volume is now at %d\n", g_playback_volume);
            }
        }

        g_wButtonData = buttondata;
        
    }
    else
        printf("ERROR: g_cPadData[0] has error\n");
    
}



//////////////////////////////////////////////////////////////////////
// StartRecording
//
// Initiates a recording session
//////////////////////////////////////////////////////////////////////
int StartRecording()
{

    if(g_usb_device == LGAUD_INVALID_DEVICE)
        return LGAUD_ERROR;

    if (g_isRecording == 1)
        return LGAUD_SUCCESS;
    
    if (g_isPlaying)
        StopPlayback();

    if (LGAUD_SUCCEEDED(lgAudStartRecording(g_usb_device)))
    {        
        printf("\nStart Record\n");

        // test data.
        memset(g_Spooler, 0, IN_SPOOLERBUF);
        g_pReadPtr = g_Spooler;
        g_isRecording = 1;
        eState = VOICE_LISTENING;

        return LGAUD_SUCCESS;
    }
    else
    {
        // we must have lost our device
        CloseDevice();
    }

    return LGAUD_ERROR;
}

//////////////////////////////////////////////////////////////////////
// ReadData
//
// Leaves a recording session
//////////////////////////////////////////////////////////////////////
unsigned char g_buffer[2206]; 

int ReadData(int device) 
{ 
   int read_bytes = sizeof(g_buffer); 
   // now we attempt to read up our full buffer size, 2206 bytes. 
   // specifying the NOT_BLOCKING parameter means that the call should 
   // return with whatever amount of data is available, rather than 
   // wait for the specified amount to become available 
   if(LGAUD_SUCCESS == lgAudRead(device, LGAUD_BLOCKMODE_NOT_BLOCKING, g_buffer, &read_bytes)) 
   { 
      printf("lgAudRead: read %d bytes\n", read_bytes); 
      // process the read bytes as you wish. 
      // you can write to a file, run through a speech recognizer, etc return read_bytes; 
   } 
   return 0; 
}

//////////////////////////////////////////////////////////////////////
// StopRecording
//
// Leaves a recording session
//////////////////////////////////////////////////////////////////////
int StopRecording()
{

//    int bytes_recorded = g_pReadPtr - g_Spooler;
    
    if (g_isRecording == 0)
        return LGAUD_SUCCESS;
    
    if(g_usb_device == LGAUD_INVALID_DEVICE)
        return LGAUD_ERROR;
    
    lgAudStopRecording(g_usb_device);
        
    
    g_isRecording = 0;

    return LGAUD_SUCCESS;
}


//////////////////////////////////////////////////////////////////////
// StartPlayback
//
// Initiates a playback session
//////////////////////////////////////////////////////////////////////
int StartPlayback()
{

    if(g_usb_device == LGAUD_INVALID_DEVICE)
        return LGAUD_ERROR;

    if (g_isPlaying == 1)
        return LGAUD_SUCCESS;
    
    if (g_isRecording)
        StopRecording();
    
    if (lgAudStartPlayback(g_usb_device) == LGAUD_SUCCESS)
    {
        // store how many bytes we recorded
        int bytes_recorded = g_pReadPtr - g_Spooler;
 
        printf("Start playback with %d bytes recorded\n", bytes_recorded);

        g_pPlaybackEnd = g_Spooler + bytes_recorded;
        g_pWritePtr = g_Spooler;
        g_isPlaying = 1;

        return LGAUD_SUCCESS;
    }
    else
    {
        // we must have lost our device
        CloseDevice();
    }

    return LGAUD_ERROR;

}


//////////////////////////////////////////////////////////////////////
// StopPlayback
//
// Leaves a playback session
//////////////////////////////////////////////////////////////////////
int StopPlayback()
{
   // Uncomment the lines to dump a wave.
//   int fp;

   if (g_isPlaying == 0)
       return LGAUD_SUCCESS;
    
   lgAudStopPlayback(g_usb_device);
    
//   fp = sceOpen("host0:/data/dump.wav", SCE_CREAT | SCE_RDWR, 0444);
//    if (fp>=0)
//    {
//        sceWrite(fp, g_Spooler, bytes_recorded);
//    }
    g_pWritePtr = g_Spooler;
    g_isPlaying = 0;
    
    return LGAUD_SUCCESS;
}


//////////////////////////////////////////////////////////////////////
// FindAndOpenHeadsetDevice
//
// Returns the first attached headset device
//////////////////////////////////////////////////////////////////////
int FindAndOpenHeadsetDevice()
{
    
    int ret = LGAUD_SUCCESS;
    int enumhint;

    g_usb_device = LGAUD_INVALID_DEVICE;

    if(LGAUD_SUCCEEDED(lgAudEnumHint(&enumhint)))
    {
        // something changed, try enumerating a headset
        // for simplicity, we just pick the first
        if(LGAUD_SUCCESS == lgAudEnumerate(0, &g_usb_device_desc))
        {
            lgAudOpenParam openParam;
            openParam.Mode = LGAUD_MODE_PLAYBACK | LGAUD_MODE_RECORDING;
            
            openParam.PlaybackFormat.Channels = OUT_SAMPLING_CHANNELS;
            openParam.PlaybackFormat.BitResolution = OUT_SAMPLING_BITS;
            openParam.PlaybackFormat.SamplingRate = OUT_SAMPLING_FREQUENCY;
            openParam.PlaybackFormat.BufferMilliseconds = PLAYBACKLENGTH;
            
            openParam.RecordingFormat.Channels = IN_SAMPLING_CHANNELS;
            openParam.RecordingFormat.BitResolution = IN_SAMPLING_BITS;
            openParam.RecordingFormat.SamplingRate = IN_SAMPLING_FREQUENCY;
            openParam.RecordingFormat.BufferMilliseconds = RECORDLENGTH;
            
            if ((ret = lgAudOpen(0, &openParam, &g_usb_device)) == LGAUD_SUCCESS)
            {
                
                printf("Opened USB Audio Device\n");
                ShowDeviceSupport();
                printf("\n\n");

                printf("Playback: opened device %d with format %dHz, %dbit, %dchannel, %dms buffer\n",
                       g_usb_device,
                       openParam.PlaybackFormat.SamplingRate,
                       openParam.PlaybackFormat.BitResolution,
                       openParam.PlaybackFormat.Channels,
                       openParam.PlaybackFormat.BufferMilliseconds);
                printf("Record: opened device %d with format %dHz, %dbit, %dchannel, %dms buffer\n",
                       g_usb_device,
                       openParam.RecordingFormat.SamplingRate,
                       openParam.RecordingFormat.BitResolution,
                       openParam.RecordingFormat.Channels,
                       openParam.RecordingFormat.BufferMilliseconds);
                printf("\n\n");
                
                InitAudioDevice(g_usb_device, LGAUD_MODE_PLAYBACK);
                InitAudioDevice(g_usb_device, LGAUD_MODE_RECORDING);
                
                return g_usb_device;
            }
            else
            {
                printf("ERROR: lgAudOpen failed (0x%x)\n", ret);
            }
        }
    }
    
    return LGAUD_INVALID_DEVICE;
    
}


//////////////////////////////////////////////////////////////////////
// CloseDevice
// 
// Closes handle to attached device
//////////////////////////////////////////////////////////////////////
int CloseDevice()
{
    lgAudClose(g_usb_device);

    g_usb_device = LGAUD_INVALID_DEVICE;

    return LGAUD_SUCCESS;
}


//////////////////////////////////////////////////////////////////////
// Usage
//////////////////////////////////////////////////////////////////////
void Usage()
{
    int i;
    printf("\n\nFonix Recognition Test\n");
    printf("\nSample: VoiceCmd\n\n");   
    printf("Instructions\n");
    printf("- Push the CROSS button to start recognition\n");
    printf("- Push the CIRCLE button to play back what you recorded\n");
    printf("- Use L1 to increase the input gain\n");
    printf("- Use L2 to decrease the input gain\n");
    printf("- Use R1 to increase the playback volume\n");
    printf("- Use R2 to decrease the playback volume\n");
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


//////////////////////////////////////////////////////////////////////
// ShowDeviceSupport
//
// Displays the capabilities of the selected device 
//////////////////////////////////////////////////////////////////////
void ShowDeviceSupport()
{

    int i;

    if (g_usb_device == LGAUD_INVALID_DEVICE)
        printf("\n\n");
    else
    {
        printf("\n");
        printf("Supported Recording Formats");
        printf("---------------------------");
        printf("\n");
        
        for (i = 0; i < g_usb_device_desc.RecordingFormatsCount; i++)
        {
            printf("%dhz - %dhz %d bit %d channels\n",
                g_usb_device_desc.RecordingFormats[i].LowerSamplingRate,
                g_usb_device_desc.RecordingFormats[i].HigherSamplingRate,
                g_usb_device_desc.RecordingFormats[i].BitResolution,
                g_usb_device_desc.RecordingFormats[i].Channels);
        }           

        printf("\n");
        printf("Supported Playback Formats");
        printf("---------------------------");
        printf("\n");
        
        for (i = 0; i < g_usb_device_desc.PlaybackFormatsCount; i++)
        {
            printf("%dhz - %dhz %d bit %d channels\n",
                g_usb_device_desc.PlaybackFormats[i].LowerSamplingRate,
                g_usb_device_desc.PlaybackFormats[i].HigherSamplingRate,
                g_usb_device_desc.PlaybackFormats[i].BitResolution,
                g_usb_device_desc.PlaybackFormats[i].Channels);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// SifLoadModule
//
// Loads an IRX module
//////////////////////////////////////////////////////////////////////
void SifLoadModule(char *module, char *arg, int arglen)
{
    char hostmod[256];
    int delay = 0;

    while(1)    
    {
        strcpy(hostmod, "host0:/usr/local/sce/iop/modules/");
        strcat(hostmod, module);
        printf("trying to load >%s<\n", hostmod);
        if(sceSifLoadModule(hostmod, arglen, arg) >= 0)
        {
            return;
        }
    
        printf("Failed to load >%s<\n", module);
    
        // this is a really stupid delay loop...
        // hopefully not stupid enough to be optimized out...
        for(delay = 0; delay < 1000*1000; delay++)
        {
            delay = delay;
        }
    }
}

extern int FnxReadWordsFromList(char **psBuf, char ***ppsWords, unsigned int **ppiWordIDs);

//-----------------------------------------------------------------------------
// Name: InitAudioDevice()
// Desc: Called during device initialization, this code checks the device
//       for some minimum set of capabilities
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
         iNumWords = FnxReadWordsFromList(items, &psWords, &pdwWordID);
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
          bSpeechDetected = FALSE;
       }

   }
   return err;
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
           (XBox games will typically have their own way of getting blocks into memory).
 *END_HEADER***************************/
void *ReadBinaryFile1(char *wsFilename, int *piSize)
{
   int fp;
   void *pvBuffer;
   size_t lSize=0;

   fp = sceOpen(wsFilename, SCE_RDONLY, 0444);
   if (fp>=0)
   {
      lSize = sceLseek(fp,0L, SCE_SEEK_END);
      pvBuffer = (void *)malloc(lSize);
      printf("malloc: %s = %d\n", wsFilename, lSize);
      if (pvBuffer!=NULL)
      {
         sceLseek(fp, 0L, SCE_SEEK_SET);
         if ((size_t)sceRead(fp, pvBuffer, lSize) != lSize)
         {
            free(pvBuffer);
            pvBuffer=NULL; // couldn't read full amount, strangely.
         }
         sceClose(fp);
      }
   }
   else pvBuffer=NULL;

   if (piSize)
      *piSize = (int)lSize;
   return pvBuffer;
}  /* ReadBinaryFile1 */

