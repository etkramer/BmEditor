// FnxVoiceAppDlg.cpp : implementation file
//

#include "stdafx.h"
#include "VoiceCmd.h"
#include "VoiceCmdDlg.h"

#include <commdlg.h>
#include <string.h>
#include <stdio.h>
#include "resource.h"
extern "C"
{
#include "VoiceCmds.h"
}

extern "C"
{
int FnxReadWordList(wchar_t *sWordList, char ***ppsWords, unsigned long **ppiWordIDs);
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CFnxVoiceAppDlg dialog



CFnxVoiceAppDlg::CFnxVoiceAppDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFnxVoiceAppDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
   m_pdwWordID = NULL;
}

extern "C" void Fnx_Gamers_MemCheck ( char *message );
CFnxVoiceAppDlg::~CFnxVoiceAppDlg()
{
   char messageFnx[512] = {0};

	for (int i=0; i<m_iNumWords; i++)
	{
		delete [] m_psWords[i];
	}

   if (m_psWords)
	{
		delete [] m_pdwWordID;
		delete [] m_psWords;
	}

    CloseHandle( m_hNotificationEvent );

    SAFE_RELEASE( m_pDSNotify );
    SAFE_RELEASE( m_pDSBCapture );
    SAFE_RELEASE( m_pDSCapture ); 

	if (m_pVoiceUser)
		free (m_pVoiceUser);
	if (m_pVoiceUser)
		free (m_pFnxVoice);
	if (m_pVoiceUser)
		free (m_pFnxVocab);

    // Release COM
    CoUninitialize();

    Fnx_Gamers_MemCheck(NULL);
}

void CFnxVoiceAppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDOK, m_Speak);
	DDX_Control(pDX, IDC_LIST1, m_WordList);
}

BEGIN_MESSAGE_MAP(CFnxVoiceAppDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()



// CFnxVoiceAppDlg message handlers

BOOL CFnxVoiceAppDlg::OnInitDialog()
{
	HRESULT hr;
//	WAVEFORMATEX wfxInput;
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	m_WordList.InsertColumn (0, "Score", LVCFMT_LEFT, 80);
	m_WordList.InsertColumn (1, "Index", LVCFMT_LEFT, 40);
	m_WordList.InsertColumn (2, "Word", LVCFMT_LEFT, 135);
	ZeroMemory( &m_aPosNotify, sizeof(DSBPOSITIONNOTIFY) * 
							(NUg_REC_NOTIFICATIONS + 1) );
	m_pVoiceUser = NULL;
	m_pFnxVoice = NULL;
	m_pFnxVocab = NULL;
	m_dwCaptureBufferSize = 0;
	m_dwNotifySize        = 0;
	m_iNumWords = 0;

	m_pDSCapture         = NULL;
	m_pDSBCapture        = NULL;
	m_pDSNotify          = NULL;
	m_psWords            = NULL;
// Initialize COM
	if( !FAILED( hr = CoInitialize(NULL) ) )
	{
		// Create IDirectSoundCapture using the preferred capture device
		m_pDSCapture = NULL;
		hr = DirectSoundCaptureCreate(NULL, &m_pDSCapture, NULL);
	}	
	if (FAILED( hr))\
	{
		MessageBox ("Error initializing DirectSound.  Sample will now exit.", 
					"DirectSound Sample", MB_OK | MB_ICONERROR );
		PostQuitMessage( 0 );
	}
	ZeroMemory( &m_wfxInput, sizeof(m_wfxInput));
    m_wfxInput.nSamplesPerSec = 16000;
    m_wfxInput.wBitsPerSample = 16;
	m_wfxInput.nChannels = 1;

    m_wfxInput.nBlockAlign = m_wfxInput.nChannels * ( m_wfxInput.wBitsPerSample / 8 );
    m_wfxInput.nAvgBytesPerSec = m_wfxInput.nBlockAlign * m_wfxInput.nSamplesPerSec;
    m_wfxInput.wFormatTag = WAVE_FORMAT_PCM;

	m_hNotificationEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

	CreateCaptureBuffer (&m_wfxInput );
	InitNotifications();
	m_pDSBCapture->GetFormat( &m_wfxInput, sizeof(m_wfxInput), NULL );

//	SetWindowText( strInputFormat );

	m_bRecording = FALSE;
	hr = LoadRecognitionSupport();
    m_Speak.EnableWindow (TRUE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}


void CFnxVoiceAppDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFnxVoiceAppDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFnxVoiceAppDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CFnxVoiceAppDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	//OnOK();
    HRESULT hr;
    DWORD dwResult;
    BOOL  bDone=FALSE;
    MSG   msg;
	m_bRecording = !m_bRecording;
	m_Speak.EnableWindow (FALSE);
    if( FAILED( hr = StartOrStopRecord( m_bRecording ) ) )
    {
        MessageBox( "Error with DirectSoundCapture buffer."                            
                    "Sample will now exit.", "DirectSound Sample", 
                    MB_OK | MB_ICONERROR );
        PostQuitMessage( 0 );
        EndDialog( IDABORT );
    }

    while( !bDone ) 
    { 
        dwResult = MsgWaitForMultipleObjects( 1, &m_hNotificationEvent, 
                                              FALSE, INFINITE, QS_ALLEVENTS );
        switch( dwResult )
        {
            case WAIT_OBJECT_0 + 0:
                // g_hNotificationEvents[0] is signaled

                // This means that DirectSound just finished playing 
                // a piece of the buffer, so we need to fill the circular 
                // buffer with new sound from the wav file

                if( FAILED( hr = RecordCapturedData() ) )
                {
                    bDone = TRUE;
                }
                break;

            case WAIT_OBJECT_0 + 1:
                while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) 
                { 
                    if( !IsDialogMessage( &msg ) )  
                    {
                        TranslateMessage( &msg ); 
                        DispatchMessage( &msg ); 
                    }

                    if( msg.message == WM_QUIT )
					{
                        bDone = TRUE;
					}
                }
                break;
        }
    }

    if( !m_bRecording )
	{
       m_Speak.EnableWindow(TRUE);
   	   m_WordList.EnsureVisible (0, false);
	}
}


HRESULT CFnxVoiceAppDlg::CreateCaptureBuffer( WAVEFORMATEX* pwfxInput )
{
    HRESULT hr;
    DSCBUFFERDESC dscbd;

    SAFE_RELEASE( m_pDSNotify );
    SAFE_RELEASE( m_pDSBCapture );

    // Set the notification size
    m_dwNotifySize = MAX( 1024, pwfxInput->nAvgBytesPerSec / 8 );
    m_dwNotifySize -= m_dwNotifySize % pwfxInput->nBlockAlign;   

    // Set the buffer sizes 
    m_dwCaptureBufferSize = m_dwNotifySize * NUg_REC_NOTIFICATIONS;

    SAFE_RELEASE( m_pDSNotify );
    SAFE_RELEASE( m_pDSBCapture );

    // Create the capture buffer
    ZeroMemory( &dscbd, sizeof(dscbd) );
    dscbd.dwSize        = sizeof(dscbd);
    dscbd.dwBufferBytes = m_dwCaptureBufferSize;
    dscbd.lpwfxFormat   = pwfxInput; // Set the format during creatation

    if( FAILED( hr = m_pDSCapture->CreateCaptureBuffer( &dscbd, 
                                                        &m_pDSBCapture, 
                                                        NULL ) ) )
        return E_FAIL;

    m_dwNextCaptureOffset = 0;

    if( FAILED( hr = InitNotifications() ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitNotifications()
// Desc: Inits the notifications on the capture buffer which are handled
//       in WinMain()
//-----------------------------------------------------------------------------
HRESULT CFnxVoiceAppDlg::InitNotifications()
{
    HRESULT hr; 

    if( NULL == m_pDSBCapture )
        return E_FAIL;

    // Create a notification event, for when the sound stops playing
    if( FAILED( hr = m_pDSBCapture->QueryInterface( IID_IDirectSoundNotify, 
                                                    (VOID**)&m_pDSNotify ) ) )
        return E_FAIL;

    // Setup the notification positions
    for( INT i = 0; i < NUg_REC_NOTIFICATIONS; i++ )
    {
        m_aPosNotify[i].dwOffset = (m_dwNotifySize * i) + m_dwNotifySize - 1;
        m_aPosNotify[i].hEventNotify = m_hNotificationEvent;             
    }
    
    // Tell DirectSound when to notify us. the notification will come in the from 
    // of signaled events that are handled in WinMain()
    if( FAILED( hr = m_pDSNotify->SetNotificationPositions( NUg_REC_NOTIFICATIONS, 
                                                            m_aPosNotify ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: StartOrStopRecord()
// Desc: Starts or stops the capture buffer from recording
//-----------------------------------------------------------------------------
HRESULT CFnxVoiceAppDlg::StartOrStopRecord( BOOL bStartRecording )
{
    HRESULT hr;

    if( bStartRecording )
    {
        // Create a capture buffer, and tell the capture 
        // buffer to start recording   
        if( FAILED( hr = CreateCaptureBuffer( &m_wfxInput ) ) )
            return E_FAIL;

        if( FAILED( hr = m_pDSBCapture->Start( DSCBSTART_LOOPING ) ) )
            return E_FAIL;
    }
    else
    {
        // Stop the capture and read any data that 
        // was not caught by a notification
        if( NULL == m_pDSBCapture )
            return S_OK;

        // Stop the buffer, and read any data that was not 
        // caught by a notification
        if( FAILED( hr = m_pDSBCapture->Stop() ) )
            return E_FAIL;

        if( FAILED( hr = RecordCapturedData() ) )
            return E_FAIL;

        // Close the wav file
//        SAFE_DELETE( m_pWaveFile );
    }

    return S_OK;
}




HRESULT CFnxVoiceAppDlg::LoadRecognitionSupport()
{
   HRESULT nRet;
   wchar_t *pVocab = XVOCAB_FILE;
   int *piNumSearchPaths=NULL;  // Maximum number of search paths for each vocabulary.
   int *piNumSearchNodes=NULL;  // Number of nodes for each vocabulary.


   // Uncomment the lines below for the dynamic loading
	// Test for the reading and writing of xvocab, vnn, and user files
//	nRet = FnxVocabBld(VOICE_TEXT_FILE, DICTIONARY_PDC, NEURAL_NET_PSI, 
//					XVOCAB_FILE, NULL, 10, 0);

//	nRet = FnxBuildVoice(VOICE_NNET_FILE, VOICE_USER_FILE, NEURAL_NET_PSI, 
//             &pVocab, 1, 1, 0, NULL, 0, NULL);
   
    // Load the common voice recognition information.
    m_pFnxVocab = (FnxVocabPtr)ReadBinaryFile(XVOCAB_FILE, NULL/*&iVocabSize*/);
    m_pFnxVoice = (FnxVoicePtr)ReadBinaryFile(VOICE_NNET_FILE, NULL/*&iVoiceSize*/);
    
       // Load vocabulary
    nRet = FnxVocabInit(m_pFnxVocab);

    // Initialize the common voice recognition block (or set m_err to FNX_NULL_POINTER if the read failed)
    nRet = FnxVoiceInit(m_pFnxVoice);

	//*******************************************
    // Initialize user-specific ASR information
    m_eState = VOICE_UNAVAILABLE; // unless initialization is successful.
    if (m_pFnxVoice)
    {
       m_pVoiceUser = (FnxVoiceUserPtr)ReadBinaryFile(VOICE_USER_FILE, NULL);
       if (m_pVoiceUser)
       {
          nRet = FnxVoiceUserInit(m_pFnxVoice, m_pVoiceUser);
          if (nRet)
          {
             free(m_pVoiceUser);
             m_pVoiceUser=NULL;
          }
          else
          {
             // Select the one and only vocabulary
             nRet = FnxSelectXVocabs(m_pVoiceUser, &m_pFnxVocab, 1);
          }

			// Load corresponding text file, and build an array of words to display.
          m_iNumWords = FnxReadWordList(VOICE_TEXT_FILE, &m_psWords, &m_pdwWordID);
          // Start all words out in the order they came from.

		  int i=0;
          for (i = 0; i < m_iNumWords; i++)
          {
		     CString sItemNum;
			 sItemNum.Format ("%d", i);
			 int nItem = m_WordList.InsertItem (i, "");
			 m_WordList.SetItemText (nItem, 1, sItemNum);
			 m_WordList.SetItemText (nItem, 2, m_psWords[i]);
          }

          // Start out in the idle state
          m_eState = VOICE_IDLE;
          m_bSpeechDetected = FALSE;
       }
    }


	return nRet;
}

HRESULT CFnxVoiceAppDlg::RecordCapturedData() 
{
    HRESULT hr;
    VOID*   pbCaptureData    = NULL;
    DWORD   dwCaptureLength;
    VOID*   pbCaptureData2   = NULL;
    DWORD   dwCaptureLength2;
    DWORD   dwReadPos;
    DWORD   dwCapturePos;
    LONG lLockSize;

    if( NULL == m_pDSBCapture )
        return S_FALSE;

    if( FAILED( hr = m_pDSBCapture->GetCurrentPosition( &dwCapturePos, &dwReadPos ) ) )
        return E_FAIL;

    lLockSize = dwReadPos - m_dwNextCaptureOffset;
    if( lLockSize < 0 )
        lLockSize += m_dwCaptureBufferSize;

    // Block align lock size so that we are always write on a boundary
    lLockSize -= (lLockSize % m_dwNotifySize);

    if( lLockSize == 0 )
        return S_FALSE;

    // Lock the capture buffer down
    if( FAILED( hr = m_pDSBCapture->Lock( m_dwNextCaptureOffset, lLockSize, 
                                          &pbCaptureData, &dwCaptureLength, 
                                          &pbCaptureData2, &dwCaptureLength2, 0L ) ) )
        return E_FAIL;

	Process(pbCaptureData, dwCaptureLength/BYTES_PER_SAMPLE, false);

    // Move the capture offset along
    m_dwNextCaptureOffset += dwCaptureLength; 
    m_dwNextCaptureOffset %= m_dwCaptureBufferSize; // Circular buffer

    if( pbCaptureData2 != NULL)
    {
        // Write the data into the wav file
		Process(pbCaptureData2, dwCaptureLength2/BYTES_PER_SAMPLE, false);

        // Move the capture offset along
        m_dwNextCaptureOffset += dwCaptureLength2; 
        m_dwNextCaptureOffset %= m_dwCaptureBufferSize; // Circular buffer
    }


    // Unlock the capture buffer
    m_pDSBCapture->Unlock( pbCaptureData,  dwCaptureLength, 
                           pbCaptureData2, dwCaptureLength2 );

	if (m_eState==VOICE_FINISHING)
	{
		StartOrStopRecord( m_bRecording );
		m_eState=VOICE_IDLE;
		return E_FAIL;
	}
	m_bRecording = false;

    return S_OK;
}


HRESULT CFnxVoiceAppDlg::Process(VOID* pbCaptureData, DWORD  dwCaptureLength, bool bFinish)
{
    //************************************************
    DWORD uiResult;
	int nRet;
	unsigned int bytesWritten = 0;

    // Submit the copied data to the speech recognizer
	 if (bFinish)
    {
        // Provide time to finish doing recognition, but don't send more data
        nRet = FnxVoiceRecognize(m_pFnxVoice, m_pVoiceUser, NULL, 0, &uiResult);
    }
    else 
    {
        int iBackoff = 4000; // = 250ms at 16kHz;
        // Send in some wave data and do some processing 
        nRet = FnxVoiceRecognize(m_pFnxVoice, m_pVoiceUser, 
                                    (short *)(pbCaptureData), 
                                    dwCaptureLength, &uiResult);

        if (uiResult & END_OF_SPEECH)
            m_eState = VOICE_FINISHING;

        if (uiResult & SPEECH_DETECTED)
        {
            m_bSpeechDetected = TRUE;
        }
        else
        {
            m_bSpeechDetected = FALSE;
		}
    }
    if (uiResult & RESULTS_AVAILABLE)
    {
        int iNBest, n, iWord;
        DWORD *pdwTempWordID;
        FLOAT *pfTempConfidence;

        /* Processing has completed on the speech, so get the results */

		nRet = FnxVoiceGetResults(m_pFnxVoice, m_pVoiceUser, &pdwTempWordID, &pfTempConfidence, &iNBest);

		for (n=0; n < iNBest; n++)
        {
			m_WordList.SetItemText (n, 0, "");
		}

		for (n=0; n < iNBest; n++)
        {			
			int	iNum = 0;
			char *itemNum[20];
		    for (iWord=m_iNumWords-1; iWord >= 0; iWord--)
		    {
				m_WordList.GetItemText (iWord, 1, (char*)itemNum, 20);
				iNum = atoi ((char*)itemNum);
				if (iNum+1 == pdwTempWordID[n])
					break;
			}
			
            if (iWord < m_iNumWords)
            {
				m_WordList.DeleteItem (iWord);
				CString sConf;
				sConf.Format ("%f", pfTempConfidence[n]);
				int newItem = m_WordList.InsertItem (n, sConf);
				m_WordList.SetItemText (n, 1, (char*)itemNum);
				m_WordList.SetItemText (n, 2, m_psWords[iNum]);
            }
        }
    }  /* Do ASR */

	return S_OK;
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
void *CFnxVoiceAppDlg::ReadBinaryFile(wchar_t *wsFilename, int *piSize)
{
   FILE *fp = NULL;
   void *pvBuffer = NULL;
   size_t lSize=0;
   errno_t err;

   err = _wfopen_s(&fp, wsFilename, L"rb");
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







