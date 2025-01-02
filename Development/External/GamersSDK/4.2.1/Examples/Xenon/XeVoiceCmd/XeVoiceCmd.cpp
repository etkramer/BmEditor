// XeVoiceCmd.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <xtl.h>
#include <winsockx.h>
#include <xonline.h>
//#include <d3d9.h>
#include <xaudio.h>
#include <xhv.h>
//#include <xhvpv.h>
#include <algorithm>
#include <AtgApp.h>
#include <AtgUtil.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <cassert>
#include "VoiceCmds.h"
#include <xauddefs.h>

//-----------------------------------------------------------------------------
// Display switches
//-----------------------------------------------------------------------------
#define DISPLAY_TIMING       // Flag for whether to display timing information at the bottom
#define DISPLAY_CONFIDENCE   // Flag for whether to display confidence scores on the left

/* Define one of the following, depending on whether you want the tasty, monsters or Japanese demo */
#define TASTY_DEMO
//#define TASTY_DEMO_5
//#define MONSTERS_DEMO
//#define JAPANESE_DEMO

#define DICTIONARY_PDC     L"game:\\data\\USEnglish.pdc"
#define NEURAL_NET_PSI     L"game:\\data\\USEnglish.psi"

#ifdef TASTY_DEMO
# define XVOCAB_FILE      L"game:\\data\\Tasty.xvocab"
# define VOICE_NNET_FILE  L"game:\\data\\Tasty.vnn"
# define VOICE_USER_FILE  L"game:\\data\\Tasty.usr"
# define VOICE_TEXT_FILE  L"game:\\data\\Tasty.txt"
# define SINGLE_USER      /* Flag for whether to limit display to first user's speech.  
                             (Required for tasty demo, or stuff won't fit on the screen).*/
#elif defined(TASTY_DEMO_5)
# define XVOCAB_FILE      L"game:\\data\\Tasty5.xvocab"
# define VOICE_NNET_FILE  L"game:\\data\\Tasty5.vnn"
# define VOICE_USER_FILE  L"game:\\data\\Tasty5.usr"
# define VOICE_TEXT_FILE  L"game:\\data\\Tasty5.txt"
# define SINGLE_USER 

#elif defined(MONSTERS_DEMO)
# define XVOCAB_FILE      L"game:\\samples\\data\\Monsters.xvocab"
# define VOICE_NNET_FILE  L"game:\\samples\\data\\Monsters.vnn"
# define VOICE_TEXT_FILE  L"game:\\samples\\data\\Monsters.txt"
# define VOICE_USER_FILE  L"game:\\samples\\data\\Monsters.usr"

#elif defined(JAPANESE_DEMO)
# define XVOCAB_FILE      L"game:\\samples\\data\\Japanese.xvocab"
# define VOICE_NNET_FILE  L"game:\\samples\\data\\Japanese.vnn"
# define VOICE_TEXT_FILE  L"game:\\samples\\data\\Japanese.txt"
# define VOICE_USER_FILE  L"game:\\samples\\data\\Japanese.usr"

#else
#  error "Must define one of Tasty/Monsters/Japanese demo!"
#endif

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD COLOR_HIGHLIGHT     = 0xffffff00;     // Yellow
const DWORD COLOR_GREEN         = 0xff00ff00;     // Green
const DWORD COLOR_RED           = 0xffff5050;     // Red
const DWORD COLOR_NORMAL        = 0xffffffff;     // White
const DWORD MAX_ERROR_STR       = 64;
const DWORD MAX_STATUS_STR      = 128;


// Voice processing: 16kHz, mono, 16 bit samples, 1000 packets, 10ms each 
const DWORD VOICE_SAMPLE_RATE   = 16000;
const DWORD BYTES_PER_SAMPLE    = 2;
const DWORD NUM_PACKETS         = 1000;
const DWORD PACKET_SIZE         = VOICE_SAMPLE_RATE * BYTES_PER_SAMPLE / 25;

// Forward declaration
class CAtgVoiceCmd;

CAtgVoiceCmd *pCAtaVoiceCmd = NULL;
//-----------------------------------------------------------------------------
// Name: class CASRCommunicator
// Desc: This class represents one instance of a Xbox Communicator performing
//          speech recognition from the microphone
//-----------------------------------------------------------------------------
class CASRCommunicator
{
public:
    CASRCommunicator();
    ~CASRCommunicator();

    HRESULT Initialize( DWORD dwPort, CAtgVoiceCmd *pParent );
    HRESULT Inserted();
    HRESULT Removed();

// Fonix voice recognition information specific to a particular headset user.
    HRESULT PushToTalk();
    FnxVoiceUserPtr m_pVoiceUser;      /* Voice User structure for this headset */
    int            *m_piWordPos;       /* Current position in the display list of the given word */
    int            *m_piOldPos;        /* Previous position in the display list of the given word */
    int             m_eState;          /* Current state (see below) */
    int             m_bSpeechDetected; /* Flag for whether speech has been detected in the current utterance */
    HRESULT         m_err;             /* Error code */
    FLOAT          *m_pfConfidence;    /* Confidence of each word (NO_CONFIDENCE => don't display a confidence) */
    int             m_iAnimationSteps; /* Number of frames it takes to move words from their old position to new */
    int             m_iAnimationStep;  /* Current animation step */
    CAtgVoiceCmd      *m_pParent;         /* Pointer to the object that has this communicator as its child */

private:
    DWORD           m_dwControllerPort;

    // Microphone-related data
    BYTE*           m_pMicrophoneBuffer;
    DWORD           m_adwMicrophoneStatus[NUM_PACKETS];
    DWORD           m_dwMicrophonePacket;
//    XMediaObject*   m_pMicrophoneXMO;
    
    // Headphone-related data
    BYTE*           m_pHeadphoneBuffer;
    DWORD           m_adwHeadphoneStatus[NUM_PACKETS];
    DWORD           m_dwHeadphonePacket;

    short*          m_pwWaveSamples;
    int             m_iMaxWaveSamples; // Size that pwWaveData is allocated to.
    int             m_iNumWaveSamples; // Number of samples currently in pwWaveData.

};

extern "C" {
   // Helper function to read a word list text file and associated ID numbers.
int FnxReadWordList(wchar_t *sWordList, char ***ppsWords, unsigned long **ppiWordIDs);
void BlockByteSwap16(unsigned short * pvData, int dwSize);

/*FUNCTION_HEADER**********************
 * NAME:    ;BlockByteSwap16
 * DESC:    Swap the bytes of each 2-byte integer in an array
 * IN:      pi - Array of 16-bit (2-byte) integers to swap bytes in
            iSize - Number of elements in pi[]
 * OUT:     pi[0..iSize-1] each have their bytes swapped.
 * RETURN:  n/a
 * NOTES:   
 *END_HEADER***************************/
void BlockByteSwap16(unsigned short * pw, int iSize)
{
   int i;
   for (i=0; i< iSize; i++)
   {
      *pw = ((unsigned short) 0x00FFU & *pw >> 8) | ((unsigned short) 0xFF00U & *pw << 8);
      pw++; // some compilers don't handle the *pw++ = F(*pw) well.
   }
}


};

// Forward declaration of function defined at the bottom, which is a
// helper function to read a binary file into a newly-allocated block of memory
// (XBox games will typically have their own way of getting blocks into memory).
void *ReadBinaryFile(wchar_t *wsFilename, int *piSize);

//-----------------------------------------------------------------------------
// Name: class CAtgVoiceCmd
// Desc: Main class to run this application. Most functionality is inherited
//       from the CAtgApplication base class.
//-----------------------------------------------------------------------------
class CAtgVoiceCmd : public ATG::Application
{
	ATG::Font             m_Font;                         // game font

	enum Action
    {
        EV_BUTTON_A,
        EV_BUTTON_B,
        EV_BUTTON_X,
        EV_BUTTON_Y,
        EV_BUTTON_BLACK,
        EV_BUTTON_WHITE,
        EV_TRIGGER_LEFT,
        EV_TRIGGER_RIGHT,
        EV_UP,
        EV_DOWN,
        EV_LEFT,
        EV_RIGHT,
        EV_DISCONNECT,
        EV_NULL
    };

    struct Event
    {
        Event() {}
        Event( DWORD p, Action a ) { dwPort = p; action = a; }
        DWORD dwPort;
        Action action;
    };

    enum
    {
        // Main menu
        MAIN_MENU_START_GAME = 0,
        MAIN_MENU_JOIN_GAME  = 1,
        MAIN_MENU_MAX,

        // Game menu
        GAME_MENU_WAVE       = 0,
        GAME_MENU_LOOPBACK   = 1,
        GAME_MENU_LEAVE_GAME = 2,
        GAME_MENU_MAX
    };

    enum InitStatus
    {
        Success,
        NotConnected,
        InitFailed
    };

    enum VoiceLevel
    {
        NoPlayer,
        NotAllowed,
        NoCommunicator,
        Everything,
    };

	 HRESULT UpdateComunicators(Event ev);

	CASRCommunicator m_aCommunicators[ XHV_MAX_LOCAL_TALKERS ];
    DWORD               m_dwMicrophoneState;
//    DWORD               m_dwHeadphoneState;
    DWORD               m_dwConnectedCommunicators;

    BOOL                m_bLoopback[ XUSER_MAX_COUNT];   // Loopback toggle
    BOOL                m_bXHVInitialized;              // TRUE if we've initialized XHV

public:
    PIXHVENGINE         XHVEngine; 
    HANDLE              m_hWorkerThread;
    mutable HANDLE      m_hLogFile;      // Log file

    // Fonix speech recognition information, common to all users:
    FnxVoicePtr     m_pFnxVoice;       /* Main (common) recognition structure. */
    FnxVocabPtr     m_pFnxVocab;       /* Recognition vocabulary */
    int             m_iNumWords;       /* Number of words (or phrases) in the recognition vocabulary */
    char          **m_psWords;         /* Strings containing the words (or phrases) to display */
    WCHAR         **m_pwsWords;        /* Wide-character versions of the same words */
    DWORD          *m_pdwWordID;       /* Word ID of each word (or phrase) */
    HRESULT         m_err;             /* Error code returned from initialization */

    void	Process(DWORD dwPort, PVOID pvData, DWORD dwSize, PBOOL bVoiceDetected);
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    HRESULT CheckCommunicatorStatus();  // Handle device insertion/removal

private:
private:
    Event GetEvent();

    VOID UpdateMenu( Event );
    VOID Wave();
    VOID StartVoice();
    VOID Heartbeat();

    VOID Init();
    HRESULT InitXHV();

};

enum {VOICE_NOT_CONNECTED, // No communicator on this port
      VOICE_IDLE,          // Not doing any speech recognition [waiting for push-to-talk button]
      VOICE_LISTENING,     // Currently accepting wave input into the recognizer
      VOICE_FINISHING,     // Hit end-of-speech, and finishing processing on buffered data [don't send more wave samples]
      VOICE_DISPLAYING,    // Finished processing speech, got result, now displaying/animating results.
      VOICE_UNAVAILABLE};  // Voice recognition could not be initialized.

const FLOAT NO_CONFIDENCE=-9999.0f;


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
void __cdecl main()
{
    CAtgVoiceCmd xbApp;
    xbApp.m_hLogFile = INVALID_HANDLE_VALUE;
    pCAtaVoiceCmd = &xbApp;
    ATG::GetVideoSettings( &xbApp.m_d3dpp.BackBufferWidth, &xbApp.m_d3dpp.BackBufferHeight );

#if 0
    HRESULT         err;             /* Error code returned from initialization */
    FnxVoiceUserPtr pVoiceUser;      /* Voice User structure for this headset */
    FnxVoicePtr     pFnxVoice;       /* Main (common) recognition structure. */
    FnxVocabPtr     pFnxVocab;       /* Recognition vocabulary */
    wchar_t *pVocab =  L"TastyTest.xvocab";
    int *piNumSearchPaths=NULL;  // Maximum number of search paths for each vocabulary.
    int *piNumSearchNodes=NULL;  // Number of nodes for each vocabulary.

	 // Test for the reading and writing of xvocab, vnn, and user files
	 err = FnxVocabBld(L"game:\\data\\Tasty.txt", L"game:\\data\\USEnglish.pdc", L"game:\\data\\USEnglish.psi", 
					L"game:\\data\\TastyTest.xvocab", NULL, 10, 0);


	 err = FnxBuildVoice(VOICE_NNET_FILE, L"game:\\data\\TastyTest.usr", L"game:\\data\\USEnglish.psi", 
                &pVocab, 1, 1, 0, &piNumSearchPaths, 0, &piNumSearchNodes);


    pFnxVoice = (FnxVoicePtr)ReadBinaryFile(VOICE_NNET_FILE, NULL/*&iVoiceSize*/);
    
    // Initialize the common voice recognition block (or set m_err to FNX_NULL_POINTER if the read failed)
    err  = FnxVoiceInit(pFnxVoice);

    pFnxVocab = (FnxVocabPtr)ReadBinaryFile(pVocab, NULL/*&iVocabSize*/);
    err = FnxVocabInit(pFnxVocab);

    pVoiceUser = (FnxVoiceUserPtr)ReadBinaryFile(VOICE_USER_FILE, NULL);
    if (pVoiceUser)
		err = FnxVoiceUserInit(pFnxVoice, pVoiceUser);
#endif

	xbApp.Run();

}


VOID MyAudioCallback(DWORD dwPort, PVOID pvData, DWORD dwSize, PBOOL pbVoiceDetected)
{
    // pvData points to dwSize bytes worth of 16-bit, 16kHz PCM.  
	pCAtaVoiceCmd->Process(dwPort, pvData, dwSize, pbVoiceDetected);

}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT CAtgVoiceCmd::Initialize()
{
	HRESULT hr;
	XAUDIOENGINEINIT    XAudioInit = {0};
	XHV_INIT_PARAMS     XHVInitParams = {0};
   wchar_t *pVocab = XVOCAB_FILE;
   int *piNumSearchPaths=NULL;  // Maximum number of search paths for each vocabulary.
   int *piNumSearchNodes=NULL;  // Number of nodes for each vocabulary.

   // This is a test for the dynamic loading
	// Test for the reading and writing of xvocab, vnn, and user files
//	m_err = FnxVocabBld(VOICE_TEXT_FILE, DICTIONARY_PDC, NEURAL_NET_PSI, 
//					XVOCAB_FILE, NULL, 10, 0);

//	m_err = FnxBuildVoice(VOICE_NNET_FILE, VOICE_USER_FILE, NEURAL_NET_PSI, 
//             &pVocab, 1, 1, 0, &piNumSearchPaths, 0, &piNumSearchNodes);


    /***************************************************/
    // Load the common voice recognition information.
    m_pFnxVoice = (FnxVoicePtr)ReadBinaryFile(VOICE_NNET_FILE, NULL/*&iVoiceSize*/);
    
    // Initialize the common voice recognition block (or set m_err to FNX_NULL_POINTER if the read failed)
    m_err  = FnxVoiceInit(m_pFnxVoice);

    if (!m_err)
    {
       // Load vocabulary
       m_pFnxVocab = (FnxVocabPtr)ReadBinaryFile(XVOCAB_FILE, NULL/*&iVocabSize*/);
       m_err = FnxVocabInit(m_pFnxVocab);
       if (!m_err)
       {
          // Load corresponding text file, and build an array of words to display.
          m_iNumWords = FnxReadWordList(VOICE_TEXT_FILE, &m_psWords, &m_pdwWordID);
          if (m_psWords && m_iNumWords>0)
          {
             
             m_pwsWords  = new WCHAR *[m_iNumWords];
             for (int i = 0; i < m_iNumWords; i++)
             {
                int iLength = MultiByteToWideChar(CP_ACP, 0, m_psWords[i], -1, NULL, 0);
                m_pwsWords[i] = new WCHAR[iLength+2];  // Convert strings to wide characters
                MultiByteToWideChar(CP_ACP, 0, m_psWords[i], -1, m_pwsWords[i], iLength);
             }
          }
       }
    }
 
    // Create a font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // In order to handle an Xbox Communicator being inserted between the two
    // calls to XGetDevices (or XGetDeviceChanges), we track the state of
    // microphone devices and headphone devices separately.
    m_dwConnectedCommunicators = 0;

	 //Initialize the microphone at this point.
//    ZeroMemory(&XAudioInit, sizeof(XAUDIOENGINEINIT));
    XAudioInit.pEffectTable = &XAudioDefaultEffectTable;
 
    hr = XAudioInitialize(&XAudioInit);

    if ( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling XAudioInitialize\n", hr );

//    ZeroMemory(&XHVInitParams, sizeof(XHV_INIT_PARAMS));

	// If you want it to play back the audio along with high quality. 
//    XHV_PROCESSING_MODE LocalModes[]  = { XHV_LOOPBACK_MODE, XHV_HIGH_QUALITY_VOICECHAT_MODE }; 

	// Use this if you want to hear in the headset what you have said.
//    XHV_PROCESSING_MODE LocalModes[]  = { XHV_LOOPBACK_MODE, XHV_VOICECHAT_MODE };
    XHV_PROCESSING_MODE LocalModes[]  = { XHV_VOICECHAT_MODE };

    XHVInitParams.localTalkerEnabledModes       = LocalModes;
    XHVInitParams.dwNumLocalTalkerEnabledModes  = ARRAYSIZE( LocalModes );


	XHVInitParams.dwMaxLocalTalkers = 1; //XDEVICE_GAMEPAD_PORT_COUNT;
	XHVInitParams.pfnMicrophoneRawDataReady = MyAudioCallback;

    hr = XHVCreateEngine(&XHVInitParams, &m_hWorkerThread, &XHVEngine);
    if(FAILED(hr))
    {
        __debugbreak();
    }
 

    for( int i = 0; i < XHV_MAX_LOCAL_TALKERS; i++ )
    {
        // Tell the CASRCommunicator which port it owns.  This doesn't
        // mean that a Communicator is inserted there - we'll call Inserted() 
        // when that happens in CheckCommunicatorStatus

        m_aCommunicators[i].Initialize( i, this );
    }

    // Initialize the users
    m_dwConnectedCommunicators = 0;
    ATG::Input::GetInput( NULL );
    for( DWORD i = 0; i < XHV_MAX_LOCAL_TALKERS; i++ )
    {
        if( ATG::Input::m_Gamepads[ i ].bConnected )
        {
			m_dwConnectedCommunicators++;
			m_aCommunicators[i].Inserted();
        }
    }

	 m_bXHVInitialized = TRUE;
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Return the state of the controller
//-----------------------------------------------------------------------------
CAtgVoiceCmd::Event CAtgVoiceCmd::GetEvent()
{
	ATG::Input::GetInput( NULL );
    ATG::GAMEPAD* rgGamePads = ATG::Input::m_Gamepads;

    for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        // "A" or "Start"
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_A ||
            rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_START )
        {
            return Event( i, EV_BUTTON_A );
        }

        // "B" or "back"
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_B ||
            rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_BACK )
            return Event( i, EV_BUTTON_B );

        // "X"
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_X  )
            return Event( i, EV_BUTTON_X );

        // "Y"
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_Y )
            return Event( i, EV_BUTTON_Y );

        // "Black"
//        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_BLACK )
//            return Event( i, EV_BUTTON_BLACK );

        // "White"
//        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_WHITE )
//            return Event( i, EV_BUTTON_WHITE );

        // Triggers
		if( rgGamePads[i].bPressedLeftTrigger )
            return Event( i, EV_TRIGGER_LEFT );
		if( rgGamePads[i].bPressedRightTrigger )
            return Event( i, EV_TRIGGER_RIGHT );

        // Movement
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            return Event( i, EV_UP );
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            return Event( i, EV_DOWN );
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            return Event( i, EV_LEFT  );
        if( rgGamePads[i].wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            return Event( i, EV_RIGHT );
    }

    return Event( 0, EV_NULL );
}




//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CAtgVoiceCmd::UpdateComunicators(Event ev)
{
	CheckCommunicatorStatus();
    for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        // "A" or "Start"
		 if ( ev.action == EV_BUTTON_A) 
		 {
				if( m_hLogFile == INVALID_HANDLE_VALUE )
				{
					m_hLogFile = CreateFile( "game:\\Wave.raw", GENERIC_WRITE, 0, NULL,
														CREATE_ALWAYS, 0, NULL );
				}
            m_aCommunicators[i].PushToTalk();
       }
	}
    for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        // "A" or "Start"
		 if ( ev.action == EV_BUTTON_A) 
		 {
       }
	}
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CAtgVoiceCmd::Render()
{
   int r=0, g=0, b=0;
   int color;
   FLOAT x, y;
   CASRCommunicator *pCom;
   int iNumCommunicators;
   int iFirstCommunicator=-1;

    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

   // Draw "Fonix ASR" at the top
	m_Font.Begin();
	m_Font.SetScaleFactors( 1.2f, 1.2f );

   r=0; g=255; b=120;
   color = 0xff000000 | (r<<16) | (g<<8) | b;
   m_Font.DrawText( 0, 0, color, L"Fonix ASR" );

   m_Font.SetScaleFactors( .9f, .9f );

   // Count how many communicators are connected.
   for( int i = iNumCommunicators = 0; i < XHV_MAX_LOCAL_TALKERS; i++ )
   {
      if( m_dwConnectedCommunicators & ( 1 << i ) )
      {
         iNumCommunicators++;
         if (iFirstCommunicator<0) // Keep track of which communicator is the first one.
            iFirstCommunicator = i;
      }
   }

   // Display stuff for each connected communicator.
   pCom=NULL;
   for( int i = 0; i < XHV_MAX_LOCAL_TALKERS; i++ )
   {
      if( m_dwConnectedCommunicators & ( 1 << i ) )
      {
         pCom = &m_aCommunicators[i];
         
         if (pCom && pCom->m_eState == VOICE_UNAVAILABLE)
            m_Font.DrawText( 100, 184, COLOR_GREEN, L"Could not load recognition files." );
         if (pCom && pCom->m_err)
         {
            WCHAR *wsError=L"Unknown Error"; // In case it wasn't one of the predefined errors for some reason.
            switch(pCom->m_err)
            {
               case FNX_TOO_MANY_VOCABS: wsError=L"Tried to use too many vocabularies."; break;
               case FNX_NODE_OVERFLOW:   wsError=L"Tried to use too many nodes."; break;
               case FNX_NULL_POINTER:    wsError=L"Null pointer passed to speech function."; break;
               case FNX_NOT_INITIALIZED: wsError=L"Uninitialized block used in speech function."; break;
               case FNX_WRONG_TYPE:      wsError=L"Wrong kind of block passed to speech function."; break;
               case FNX_INCOMPLETE:      wsError=L"Block did not contain proper footer."; break;
               case FNX_SPEECH_ERROR:    wsError=L"Could not reset or initialize speech recognizer."; break;
               case FNX_VERSION_MISMATCH:wsError=L"Version of block does not match code version."; break;
               case FNX_TAG_MISMATCH:    wsError=L"Voice, VoiceUser and XVocab blocks\ndo not have matching tags."; break;
               case FNX_UNALIGNED_BLOCK: wsError=L"Block not aligned to 4-byte address."; break;
            }
            m_Font.DrawText( 100, 214, COLOR_RED, wsError );
         }
         if (pCom && !pCom->m_err && pCom->m_eState != VOICE_UNAVAILABLE)
         {
		    int widthOffset = 5;
            int   iNumRows, iNumColumns;
            FLOAT fRowHeight, fRegionHeight, fRegionWidth;
            FLOAT fLeft, fTop; /* Left and top coordinate of this communicator's word list region */
            FLOAT fMessageHeight = 20; // height of status message.
            WCHAR str[100];
            WCHAR *strMessage;

            /* Split the screen horizontally and vertically if there is more than one communicator
               (or if the one communicator is not plugged into the first port). */
#ifndef SINGLE_USER
            if (iNumCommunicators > 1 || i>0)
            {
               fRegionWidth = 290;
               fRegionHeight = 180 - fMessageHeight;
            }
            else // Only one communicator, plugged into first port, so let it use the whole screen.
#endif
            {
               fRegionWidth = 600;
               fRegionHeight = 380 - fMessageHeight;
            }

            switch (i)
            {
               case 0: 
				   fTop = 20;  fLeft = 0;  
				   break;
               case 1: 
				   fTop = 40;  fLeft = 310; 
				   break;
               case 2: 
				   fTop = 40 + fMessageHeight + fRegionHeight + 20; fLeft = 20;  
				   break;
               case 3: 
				   fTop = 40 + fMessageHeight + fRegionHeight + 20; fLeft = 310; 
				   break;
            }

            /* Calculate how many rows and colums there are, reducing row spacing if necessary */
            fRowHeight = 30;
            iNumRows = (int)(fRegionHeight / fRowHeight);
            iNumColumns = 1 + (pCom->m_pParent->m_iNumWords - 1) / iNumRows;
            if (iNumColumns > 2)
               fRowHeight = 20.0f;
            iNumRows = (int)(fRegionHeight / fRowHeight);
            iNumColumns = 1 + (pCom->m_pParent->m_iNumWords - 1) / iNumRows;
            if (iNumColumns > 2)
               fRowHeight = 15.0f;
            iNumRows = (int)(fRegionHeight / fRowHeight);
            iNumColumns = 1 + (pCom->m_pParent->m_iNumWords - 1) / iNumRows;
            
            /**** Animate word list ****/
            if (pCom->m_eState==VOICE_DISPLAYING)
            {
               pCom->m_iAnimationStep++;
               if (pCom->m_iAnimationStep > pCom->m_iAnimationSteps)
               {
                  pCom->m_eState = VOICE_IDLE;
                  pCom->m_bSpeechDetected = FALSE;
               }
            }
            switch(pCom->m_eState)
            {
               case VOICE_LISTENING:  
                  if (pCom->m_bSpeechDetected)
                  {
                     r=255; g=200; b=200; strMessage = L"Listening...";
                  }
                  else
                  {
                     r=255; g=100; b=100; strMessage = L"Listening";
                  }
                  break;
               case VOICE_FINISHING:  r=255; g=200; b=100; strMessage = L"Finishing up";            break;
               case VOICE_DISPLAYING: r=g=b=255;           strMessage = L"Displaying results";      break;
               case VOICE_IDLE:       r=0; g=255; b=150;   strMessage = L"Press <start> and speak"; break;
            }
            color = 0xff000000 | (r<<16) | (g<<8) | b;
            m_Font.DrawText( fLeft, fTop, color/*0xffc0c0c0*/, strMessage);
            
            /*** Display word list */
            for (int iWord = 0; iWord < pCom->m_pParent->m_iNumWords; iWord++)
            {
               if (pCom->m_eState==VOICE_DISPLAYING)
               {
                  FLOAT fStep, fOld, fNew;
                  fStep = (FLOAT) pCom->m_iAnimationStep / (FLOAT) pCom->m_iAnimationSteps;

                  // Get the old and new row numbers
                  fOld = (FLOAT)(pCom->m_piOldPos[iWord]%iNumRows);
                  fNew = (FLOAT)(pCom->m_piWordPos[iWord]%iNumRows);

                  // Calculate the interpolated vertical position
                  y = fTop + fMessageHeight + fRowHeight * ((fStep * (fNew - fOld)) + fOld);

                  // Get the old and new column offsets
                  fOld = (FLOAT)(pCom->m_piOldPos[iWord] / iNumRows);
                  fNew = (FLOAT)(pCom->m_piWordPos[iWord] / iNumRows);

                  // Calculate the interpolated horizontal position
                  x = fLeft + (fRegionWidth/(FLOAT)iNumColumns) * ((fStep * (fNew - fOld)) + fOld);
               }
               else 
               {
                  y = fTop + fMessageHeight + fRowHeight * (FLOAT)(pCom->m_piWordPos[iWord]%iNumRows);
                  x = fLeft + (fRegionWidth/(FLOAT)iNumColumns) * (FLOAT)(pCom->m_piWordPos[iWord] / iNumRows);
               }
#ifdef DISPLAY_CONFIDENCE 
				/* Draw the confidence (if any) */
				if (pCom->m_pfConfidence[iWord]==NO_CONFIDENCE)
				{
					swprintf(str, L"");
					color = 0xff808080; // grey for unrecognized words
				}
				else 
				{
					swprintf(str, L"%d", (DWORD)pCom->m_pfConfidence[iWord]);
					if (pCom->m_piWordPos[iWord]==0)
					color = 0xffffff00; // Yellow for winner
					else color = 0xffffffff; // white for other NBest words
				}
				m_Font.DrawText( x+widthOffset, y, color, str, ATGFONT_RIGHT);
#endif
               /* Draw the word ID */
               swprintf(str, L"%d", pCom->m_pParent->m_pdwWordID[iWord]);
               m_Font.DrawText( x+widthOffset + 30, y, color, str, ATGFONT_RIGHT);
               /* Draw the word name */
               m_Font.DrawText( x+widthOffset + 40, y, color, pCom->m_pParent->m_pwsWords[iWord]);
            }  // for each word

#ifdef DISPLAY_TIMING
			if (pCom->m_eState == VOICE_IDLE)
			{
				if (i==iFirstCommunicator)
				{
					// Draw timing information
					int iFeatureTicks, iNNetTicks, iSearchTicks, iTotalTicks, iNumFrames;
					float fSecondsPerTick;
		               
					if (FnxVoiceTiming(pCom->m_pVoiceUser, &iFeatureTicks,
						&iNNetTicks, &iSearchTicks, &iTotalTicks,
						&fSecondsPerTick, &iNumFrames) == 0)
					{
						x = 190;
						y = 0;
						color = 0xff804080;
		                  
						/* FnxVoiceRecognize() processes 10ms of speech per "speech frame",
							so the various average tick counts are the number of cycles per 10ms.
							Xenon is 3200MHz, i.e., CPU=3200M cycles/second.  The number of CPU cycles
							per 10ms "speech frame" is thus 3200M cycles/sec * 10ms * (sec/1000ms)
							= 3200M*10/1000=32M cycles, so the Xenon has 32M CPU cycles per 10ms.

							So the percent of the CPU that the speech processing takes up is
							(#ticks/32M)*100% = (#ticks/320,000)%. */
						swprintf(str, L"Ticks/10ms: %dK (Feat: %dK, NNet: %dK, Srch: %dK)",
							iTotalTicks/1000, iFeatureTicks/1000, iNNetTicks/1000, iSearchTicks/1000);

						m_Font.SetScaleFactors( .7f, .7f );
						m_Font.DrawText( x, y, color, str);
					}
				}
			}
#endif

#ifdef SINGLE_USER
            break; // don't bother checking other headsets--they are being ignored.
#endif
         }  // if pCom
      }  // if connected communicator
   }  // for each port

   /************************************************/
   // Display voice recognition results
   if (pCom==NULL) // didn't find any communicators
      m_Font.DrawText( 100, 184, COLOR_GREEN, L"Please insert an Xbox Communicator\ninto one or more controllers." );

   m_Font.End();

		// Present the scene
   m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
   
   return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CheckCommunicatorStatus()
// Desc: Handles any changes in the status of Xbox Communicators.  In order
//          to handle the possibility that a device could be inserted 
//-----------------------------------------------------------------------------
HRESULT CAtgVoiceCmd::CheckCommunicatorStatus()
{

	// Check the microphones
    DWORD dwMicrophoneInsertions = 0;

	if (!m_dwConnectedCommunicators)
	{
		ATG::Input::GetInput( NULL );
		for( DWORD i = 0; i < XHV_MAX_LOCAL_TALKERS; i++ )
		{
			if( ATG::Input::m_Gamepads[ i ].bConnected )
			{
				dwMicrophoneInsertions++;
				m_aCommunicators[i].Inserted();
			}
		}
		m_dwConnectedCommunicators = dwMicrophoneInsertions;
	}

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT CAtgVoiceCmd::Update()
{
    // Process input
    Event ev = GetEvent();

    if(m_bXHVInitialized)
    {
        UpdateComunicators(ev);
    }
    return S_OK;
}





//-----------------------------------------------------------------------------
// Name: CASRCommunicator (ctor)
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CASRCommunicator::CASRCommunicator()
{
    m_dwControllerPort = -1;
    m_pMicrophoneBuffer= NULL;
    m_pHeadphoneBuffer = NULL;
    m_pVoiceUser       = NULL;
    m_piWordPos        = NULL;
    m_piOldPos         = NULL;
    m_pfConfidence     = NULL;
    m_pwWaveSamples    = NULL;
    m_iMaxWaveSamples  = 0;
    m_iNumWaveSamples  = 0;
}




//-----------------------------------------------------------------------------
// Name: ~CASRCommunicator (Dtor)
// Desc: Frees up any resources
//-----------------------------------------------------------------------------
CASRCommunicator::~CASRCommunicator()
{
    Removed();
}



//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the communicator to a specific port
//-----------------------------------------------------------------------------
HRESULT CASRCommunicator::Initialize( DWORD dwPort, CAtgVoiceCmd *pParent)
{
    m_dwControllerPort = dwPort;

    m_eState  = VOICE_NOT_CONNECTED;
    m_bSpeechDetected = FALSE;
    m_err     = pParent->m_err; // hopefully = 0 => no error so far.
    m_pParent = pParent;
    m_pParent->XHVEngine->RegisterLocalTalker(m_dwControllerPort);

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Inserted
// Desc: Handles insertion of a communicator
//-----------------------------------------------------------------------------
HRESULT CASRCommunicator::Inserted()
{
//    HRESULT hr;

//    OUTPUT_DEBUG_STRING( "Detected communicator insertion\n" );

    // Allocate a buffer for PCM sample data
    m_pMicrophoneBuffer = new BYTE[ PACKET_SIZE * NUM_PACKETS ];
    if( !m_pMicrophoneBuffer )
    {
        Removed();
        return E_OUTOFMEMORY;
    }

    // Allocate a buffer for PCM sample data
    m_pHeadphoneBuffer = new BYTE[ PACKET_SIZE * NUM_PACKETS ];
    if( !m_pHeadphoneBuffer )
    {
        Removed();
        return E_OUTOFMEMORY;
    }

    // Allocate a buffer to hold the current utterance for playback.
    m_iNumWaveSamples = 0; // currently empty
    m_iMaxWaveSamples = 160000; // 10 seconds of speech
    m_pwWaveSamples   = new short[m_iMaxWaveSamples];

    // Fill out a waveformat structure
    WAVEFORMATEX wfx;
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.cbSize          = 0;
    wfx.nChannels       = 1;
    wfx.nSamplesPerSec  = VOICE_SAMPLE_RATE;
    wfx.wBitsPerSample  = BYTES_PER_SAMPLE * 8;
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    /*******************************************/
    // Initialize user-specific ASR information
    m_eState = VOICE_UNAVAILABLE; // unless initialization is successful.
    if (m_pParent->m_pFnxVoice)
    {
       m_pVoiceUser = (FnxVoiceUserPtr)ReadBinaryFile(VOICE_USER_FILE, NULL);
       if (m_pVoiceUser)
       {
          m_err = FnxVoiceUserInit(m_pParent->m_pFnxVoice, m_pVoiceUser);
          if (m_err)
          {
             free(m_pVoiceUser);
             m_pVoiceUser=NULL;
          }
          else
          {
             // Select the one and only vocabulary
             m_err = FnxSelectXVocabs(m_pVoiceUser, &m_pParent->m_pFnxVocab, 1);
          }

          // Start all words out in the order they came from.
          m_piWordPos    = new int[m_pParent->m_iNumWords];
          m_piOldPos     = new int[m_pParent->m_iNumWords];
          m_pfConfidence = new FLOAT[m_pParent->m_iNumWords];

          for (int i = 0; i < m_pParent->m_iNumWords; i++)
          {
             m_piWordPos[i] = m_piOldPos[i] = i;
             m_pfConfidence[i] = NO_CONFIDENCE;
          }

          m_iAnimationSteps = 30;

          // Start out in the idle state
          m_eState = VOICE_IDLE;
          m_bSpeechDetected = FALSE;
       }

    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Removed
// Desc: Handles removal (or failed insertion) of a communicator
//-----------------------------------------------------------------------------
HRESULT CASRCommunicator::Removed()
{
//    OUTPUT_DEBUG_STRING( "Detected communicator removal or failed insertion.\n" );

    delete[] m_pMicrophoneBuffer;
    m_pMicrophoneBuffer = NULL;

    delete[] m_pHeadphoneBuffer;
    m_pHeadphoneBuffer = NULL;

    if ( m_pwWaveSamples )
    {
       delete m_pwWaveSamples;
       m_pwWaveSamples = NULL;
       m_iNumWaveSamples = 0;
       m_iMaxWaveSamples = 0;
    }

    if (m_pVoiceUser && m_eState != VOICE_NOT_CONNECTED)
    {
       free(m_pVoiceUser);
       delete m_piWordPos;
       delete m_piOldPos;
       delete m_pfConfidence;

       m_pVoiceUser   = NULL;
       m_piWordPos    = m_piOldPos = NULL;
       m_pfConfidence = NULL;
       m_eState = VOICE_NOT_CONNECTED;
    }
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: PushToTalk
// Desc: Set the state to allow speech recognition to begin.
//-----------------------------------------------------------------------------
HRESULT CASRCommunicator::PushToTalk()
{
   if (!m_err && (m_eState == VOICE_DISPLAYING || m_eState == VOICE_IDLE))
   {
      m_eState = VOICE_LISTENING;
      m_bSpeechDetected = FALSE;
		m_pParent->XHVEngine->StartLocalProcessingModes(m_dwControllerPort, &XHV_VOICECHAT_MODE, 1);
//		m_pParent->XHVEngine->StartLocalProcessingModes(m_dwControllerPort, &XHV_HIGH_QUALITY_VOICECHAT_MODE, 1); //XHV_LOOPBACK_MODE
   }
   return S_OK;
}

//-----------------------------------------------------------------------------
// Name: Process
// Desc: Processes the Microphone and do speech recognition
//-----------------------------------------------------------------------------
void CAtgVoiceCmd::Process(DWORD dwPort, PVOID pvData, DWORD dwSize, PBOOL bVoiceDetected)
{
	PBYTE pvBuffer[512]; // Just to make sure that we have enough space.
    DWORD dwWritten;

   if (m_aCommunicators[dwPort].m_eState == VOICE_LISTENING || m_aCommunicators[dwPort].m_eState == VOICE_FINISHING)
   {
      DWORD uiResult;

      // Submit the copied data to the speech recognizer
      if (m_aCommunicators[dwPort].m_eState == VOICE_FINISHING)
      {
         // Provide time to finish doing recognition, but don't send more data
         m_err = FnxVoiceRecognize(m_pFnxVoice, m_aCommunicators[dwPort].m_pVoiceUser, NULL, 0, &uiResult);
      }
      else 
      {
			int iBackoff = 4000; // = 250ms at 16kHz;
			// Send in some wave data and do some processing 
			m_err = FnxVoiceRecognize(m_pFnxVoice, m_aCommunicators[dwPort].m_pVoiceUser, 
                                       (short *)(pvData), 
                                       dwSize/2, &uiResult);
			// Record the data.
			memmove(pvBuffer, pvData, dwSize);
			BlockByteSwap16((unsigned short *)pvBuffer, dwSize/2);
			WriteFile( m_hLogFile, pvBuffer, dwSize, &dwWritten, NULL );


			if (uiResult & END_OF_SPEECH)
			{
				// Make sure the message makes it to the disk
				if (m_hLogFile != INVALID_HANDLE_VALUE)
				{
					FlushFileBuffers( m_hLogFile );
					CloseHandle(m_hLogFile);
					m_hLogFile = INVALID_HANDLE_VALUE;
				}

				m_aCommunicators[dwPort].m_eState = VOICE_FINISHING;
				XHVEngine->StopLocalProcessingModes(dwPort, &XHV_VOICECHAT_MODE, 1);//XHV_HIGH_QUALITY_VOICECHAT_MODE
//				XHVEngine->StopLocalProcessingModes(dwPort, &XHV_HIGH_QUALITY_VOICECHAT_MODE, 1);//XHV_HIGH_QUALITY_VOICECHAT_MODE
			}

         if (uiResult & SPEECH_DETECTED)
         {
            m_aCommunicators[dwPort].m_bSpeechDetected = TRUE;
         }
         else
         {
            m_aCommunicators[dwPort].m_bSpeechDetected = FALSE;
         }

      }      
      if (uiResult & RESULTS_AVAILABLE)
      {
         int iNBest, n, iWord;
         DWORD *pdwTempWordID;
         FLOAT *pfTempConfidence;

         // Processing has completed on the speech, so get the results
         m_err = FnxVoiceGetResults(m_pFnxVoice, m_aCommunicators[dwPort].m_pVoiceUser, &pdwTempWordID, &pfTempConfidence, &iNBest);

         // Clear the confidence score of every word 
         for (iWord=0; iWord < m_iNumWords; iWord++)
         {
            m_aCommunicators[dwPort].m_pfConfidence[iWord] = NO_CONFIDENCE;
            m_aCommunicators[dwPort].m_piOldPos[iWord]     = m_aCommunicators[dwPort].m_piWordPos[iWord];
         }

         // Find each recognized word in the list 
         for (n=0; n < iNBest; n++)
         {
            for (iWord = 0; iWord < m_iNumWords; iWord++)
            {
               if (pdwTempWordID[n] == m_pdwWordID[iWord])
                  break;
            }
            if (iWord < m_iNumWords)
            {
               m_aCommunicators[dwPort].m_pfConfidence[iWord] = pfTempConfidence[n];
               //m_pfConfidence[iWord] = (FLOAT)n+1; // use rank instead of confidence
               m_aCommunicators[dwPort].m_piWordPos[iWord] = n;
            }
         }
         for (iWord=0; iWord < m_iNumWords; iWord++)
         {
            if (m_aCommunicators[dwPort].m_pfConfidence[iWord] == NO_CONFIDENCE)
               m_aCommunicators[dwPort].m_piWordPos[iWord] = n++;
         }
         // at this point, n should be equal to m_pParent->m_iNumWords.

         // Prepare to display animation
         m_aCommunicators[dwPort].m_eState = VOICE_DISPLAYING;
         m_aCommunicators[dwPort].m_iAnimationStep = 0;

     }

   }  // Do ASR
//    return S_OK;
}


/************************************
 * NAME:	  ReadBinaryFile
 * DESC:	  Read a binary file into a newly-allocated block of memory.
 * IN:     wsFilename - Name of the file to read.
           piSize     - Address in which to return the size of the file. (NULL=>ignore).
 * OUT:    'wsFilename' is read into a newly-allocated block of memory.
           *piSize contains the size.
 * RETURN: Pointer to the block of memory
 * NOTES:  Simple helper utility to get binary speech blobs into memory.
           (XBox games will typically have their own way of getting blocks into memory).
 *END_HEADER***************************/
void *ReadBinaryFile(wchar_t *wsFilename, int *piSize)
{
   FILE *fp;
   void *pvBuffer;
   size_t lSize=0;

   fp = _wfopen(wsFilename, L"rb");
   if (fp!=NULL)
   {
      fseek(fp,0L, SEEK_END);
      lSize = ftell(fp);
      pvBuffer = (void *)malloc(lSize);
      if (pvBuffer!=NULL)
      {
         fseek(fp, 0L, SEEK_SET);
         if (fread(pvBuffer, 1, lSize, fp) != lSize)
         {
            free(pvBuffer);
            pvBuffer=NULL; // couldn't read full amount, strangely.
         }
         fclose(fp);
      }
   }
   else pvBuffer=NULL;

   if (piSize)
      *piSize = (int)lSize;
   return pvBuffer;
}  /* ReadBinaryFile */
