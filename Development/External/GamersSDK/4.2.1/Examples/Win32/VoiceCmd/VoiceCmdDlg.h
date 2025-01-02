// FnxVoiceAppDlg.h : header file
//

#include "afxwin.h"
#include "afxcmn.h"
#include <D3DX9.h>
#include <mmsystem.h>
#include <dsound.h>
#include "VoiceCmds.h"
#pragma once

# define  VOICE_SAMPLE_RATE  16000
# define  BYTES_PER_SAMPLE   2
# define  NUg_PACKETS        1000
# define  PACKET_SIZE        (VOICE_SAMPLE_RATE * BYTES_PER_SAMPLE / 25)
# define DEFAULT_PATH	L"..\\..\\Data\\VoiceCmd\\Win32\\"
# define DEFAULT_PATH_FRE	L"..\\..\\..\\Languages\\USEnglish\\"
# define XVOCAB_FILE      DEFAULT_PATH L"Tasty.xvocab"
# define VOICE_NNET_FILE  DEFAULT_PATH L"Tasty.vnn"
# define VOICE_USER_FILE  DEFAULT_PATH L"Tasty.usr"
# define VOICE_TEXT_FILE  DEFAULT_PATH L"Tasty.txt"
# define DICTIONARY_PDC   DEFAULT_PATH_FRE L"USEnglish.pdc"
# define NEURAL_NET_PSI   DEFAULT_PATH_FRE L"USEnglish.psi"

#define NUg_REC_NOTIFICATIONS  16
#define MAX(a,b)        ( (a) > (b) ? (a) : (b) )

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#define NO_CONFIDENCE -9999.0f

enum {VOICE_NOT_CONNECTED, // No communicator on this port
	VOICE_IDLE,          // Not doing any speech recognition [waiting for push-to-talk button]
	VOICE_LISTENING,     // Currently accepting wave input into the recognizer
	VOICE_FINISHING,     // Hit end-of-speech, and finishing processing on buffered data [don't send more wave samples]
	VOICE_DISPLAYING,    // Finished processing speech, got result, now displaying/animating results.
	VOICE_UNAVAILABLE};  // Voice recognition could not be initialized.

// CFnxVoiceAppDlg dialog
class CFnxVoiceAppDlg : public CDialog
{
// Construction
public:
	CFnxVoiceAppDlg(CWnd* pParent = NULL);	// standard constructor
	~CFnxVoiceAppDlg();
	HRESULT InitDirectSound( HWND hDlg, GUID* pDeviceGuid );
	HRESULT FreeDirectSound();


	HRESULT CreateCaptureBuffer( WAVEFORMATEX* pwfxInput );
	HRESULT InitNotifications();
	HRESULT StartOrStopRecord( BOOL bStartRecording );
	HRESULT RecordCapturedData();
	HRESULT LoadRecognitionSupport();
	void *ReadBinaryFile(wchar_t *wsFilename, int *piSize);
	HRESULT Process(VOID* pbCaptureData, DWORD  dwCaptureLength, bool bFinish);
	//static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);


// Dialog Data
	enum { IDD = IDD_FNXVOICEAPP_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CButton m_Speak;
	CListCtrl m_WordList;
	afx_msg void OnBnClickedOk();

	FnxVoicePtr     m_pFnxVoice;       /* Main (common) recognition structure. */
	FnxVocabPtr     m_pFnxVocab;       /* Recognition vocabulary */
	int             m_iNumWords;       /* Number of words (or phrases) in the recognition vocabulary */
	char          **m_psWords;         /* Strings containing the words (or phrases) to display */
	unsigned long  *m_pdwWordID;       /* Word ID of each word (or phrase) */
	FnxVoiceUserPtr m_pVoiceUser;
	int             m_eState;          /* Current state (see below) */
	int             m_bSpeechDetected; /* Flag for whether speech has been detected in the current utterance */


	LPDIRECTSOUNDCAPTURE       m_pDSCapture /*        = NULL*/;
	LPDIRECTSOUNDCAPTUREBUFFER m_pDSBCapture/*        = NULL*/;
	LPDIRECTSOUNDNOTIFY        m_pDSNotify/*          = NULL*/;
	BOOL                       m_bRecording;
	WAVEFORMATEX               m_wfxInput;
	DSBPOSITIONNOTIFY          m_aPosNotify[ NUg_REC_NOTIFICATIONS + 1 ];  
	HANDLE                     m_hNotificationEvent; 
	DWORD                      m_dwCaptureBufferSize;
	DWORD                      m_dwNextCaptureOffset;
	DWORD                      m_dwNotifySize;
};
