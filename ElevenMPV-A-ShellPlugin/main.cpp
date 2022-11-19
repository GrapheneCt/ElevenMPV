#include <stddef.h>
#include <scetypes.h>
#include <kernel.h>
#include <libdbg.h>
#include <appmgr.h>
#include <taihen.h>
#include <paf.h>

#include "ipc.h"
#include "main.h"

#define IPC_PIPE_ATTR (SCE_KERNEL_MSG_PIPE_MODE_FULL | SCE_KERNEL_ATTR_OPENABLE)

using namespace paf;

static ui::Widget *imposeRoot = SCE_NULL;
static ImposeThread *mainThread = SCE_NULL;
static RxThread *rxThread = SCE_NULL;

static SceUID ipcPipeRX = SCE_UID_INVALID_UID;
static SceUID ipcPipeTX = SCE_UID_INVALID_UID;

static ui::Widget *buttonREW = SCE_NULL;
static ui::Widget *buttonPLAY = SCE_NULL;
static ui::Widget *buttonFF = SCE_NULL;
static ui::Widget *textTop = SCE_NULL;
static ui::Widget *textBottom = SCE_NULL;

static tai_hook_ref_t hookRef[1];
static SceUID hookId[1];

static wstring *topText;
static wstring *bottomText;

static SceBool impose = SCE_FALSE;

static SceBool imposeIpcActive = SCE_FALSE;

int setup_stage1()
{
	Plugin *imposePlugin = SCE_NULL;
	ScePVoid powerRoot = SCE_NULL;

	//Get power manage plugin object
	imposePlugin = Plugin::Find("power_manage_plugin");
	if (imposePlugin == NULL) {
		SCE_DBG_LOG_ERROR("Power manage plugin not found\n");
		goto setup_error_return;	
	}

	//Power manage plugin -> power manage root
	powerRoot = imposePlugin->GetInterface(1);
	if (powerRoot == NULL) {
		SCE_DBG_LOG_ERROR("Power root not found\n");
		goto setup_error_return;
	}

	//Power manage root -> impose root (some virtual function)
	ui::Widget *(*getImposeRoot)();
	getImposeRoot = (ui::Widget *(*)()) *(int *)((void *)powerRoot + 0x54);
	imposeRoot = getImposeRoot();
	if (imposeRoot == NULL) {
		SCE_DBG_LOG_ERROR("Impose root not found\n");
		goto setup_error_return;
	}

	return SCE_KERNEL_START_SUCCESS;

setup_error_return:

	return SCE_KERNEL_START_NO_RESIDENT;

}

void startMainThread()
{
	if (mainThread == SCE_NULL) {
		mainThread = new ImposeThread(SCE_KERNEL_LOWEST_PRIORITY_USER, SCE_KERNEL_4KiB, "ElevenMPVA::ShellControl");
		mainThread->Start();
	}
}

void stopMainThread()
{
	if (mainThread != SCE_NULL) {
		mainThread->Cancel();
		mainThread->Join();
		delete mainThread;
		mainThread = SCE_NULL;
	}
}

void setup_stage2()
{
	topText = new wstring();
	bottomText = new wstring();

	ipcPipeRX = sceKernelCreateMsgPipe("ElevenMPVA::ShellIPC_RX", SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN, IPC_PIPE_ATTR, sizeof(IpcDataRX), SCE_NULL);
	ipcPipeTX = sceKernelCreateMsgPipe("ElevenMPVA::ShellIPC_TX", SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN, IPC_PIPE_ATTR, sizeof(IpcDataTX), SCE_NULL);

	startMainThread();

	rxThread = new RxThread(SCE_KERNEL_LOWEST_PRIORITY_USER, SCE_KERNEL_4KiB, "ElevenMPVA::ShellRx");
	rxThread->Start();
}

/*
void cleanup()
{
	IpcDataRX ipcDataRX;

	if (mainThread != SCE_NULL) {
		mainThread->Cancel();
		mainThread->Join();
		delete mainThread;
	}

	if (rxThread != SCE_NULL) {
		ipcDataRX.cmd = EMPVA_TERMINATE_SHELL_RX;
		sceKernelSendMsgPipe(ipcPipeRX, &ipcDataRX, sizeof(IpcDataRX), SCE_KERNEL_MSG_PIPE_MODE_WAIT | SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL, SCE_NULL);
		rxThread->Join();
		delete rxThread;
	}

	if (ipcPipeRX > 0)
		sceKernelDeleteMsgPipe(ipcPipeRX);

	if (ipcPipeTX > 0)
		sceKernelDeleteMsgPipe(ipcPipeTX);

	if (hookId[0] > 0)
		taiHookRelease(hookId[0], hookRef[0]);
}
*/

int findWidgets()
{
	rco::Element widgetSearchResult;

	widgetSearchResult.hash = PlayerButtonCB::ButtonHash_Rew;

	buttonREW = SCE_NULL;
	while (buttonREW == NULL) {
		buttonREW = imposeRoot->GetChild(&widgetSearchResult, 0);
		thread::Sleep(100);
	}

	widgetSearchResult.hash = PlayerButtonCB::ButtonHash_Ff;
	buttonFF = imposeRoot->GetChild(&widgetSearchResult, 0);
	if (buttonFF == NULL) {
		SCE_DBG_LOG_ERROR("buttonFF not found\n");
		goto findButton_error_return;
	}

	widgetSearchResult.hash = PlayerButtonCB::ButtonHash_Play;
	buttonPLAY = imposeRoot->GetChild(&widgetSearchResult, 0);
	if (buttonPLAY == NULL) {
		SCE_DBG_LOG_ERROR("buttonPLAY not found\n");
		goto findButton_error_return;
	}

	widgetSearchResult.hash = 0x66FDAFE3;
	textTop = imposeRoot->GetChild(&widgetSearchResult, 0);
	if (textTop == NULL) {
		SCE_DBG_LOG_ERROR("textTop not found\n");
		goto findButton_error_return;
	}

	widgetSearchResult.hash = 0xF099B450;
	textBottom = imposeRoot->GetChild(&widgetSearchResult, 0);
	if (textBottom == NULL) {
		SCE_DBG_LOG_ERROR("textBottom not found\n");
		goto findButton_error_return;
	}

	return 0;

findButton_error_return:

	return -1;
}

int resetWidgets()
{
	buttonPLAY = SCE_NULL;
	buttonREW = SCE_NULL;
	buttonFF = SCE_NULL;
	textTop = SCE_NULL;
	textBottom = SCE_NULL;

	return 0;
}

void setButtonState()
{
	buttonPLAY->Enable(SCE_FALSE);
	buttonREW->Enable(SCE_FALSE);
	buttonFF->Enable(SCE_FALSE);

	buttonPLAY->UnregisterEventCallback(0x10000008, 0, 0);
	buttonREW->UnregisterEventCallback(0x10000008, 0, 0);
	buttonFF->UnregisterEventCallback(0x10000008, 0, 0);

	PlayerButtonCB *btCb = new PlayerButtonCB();
	buttonPLAY->RegisterEventCallback(0x10000008, btCb, SCE_FALSE);
	buttonREW->RegisterEventCallback(0x10000008, btCb, SCE_FALSE);
	buttonFF->RegisterEventCallback(0x10000008, btCb, SCE_FALSE);
}

void setText()
{
	Rgba col;
	col.r = 1.0f;
	col.g = 1.0f;
	col.b = 1.0f;
	col.a = 1.0f;
	textTop->SetColor(&col);
	textBottom->SetColor(&col);

	textTop->SetLabel(topText);
	textBottom->SetLabel(bottomText);
}

SceVoid PlayerButtonCB::PlayerButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	IpcDataTX ipcDataTX;
	ipcDataTX.cmd = 0;

	switch (self->elem.hash) {
	case ButtonHash_Play:

		ipcDataTX.cmd = EMPVA_IPC_PLAY;

		break;
	case ButtonHash_Rew:

		ipcDataTX.cmd = EMPVA_IPC_REW;

		break;
	case ButtonHash_Ff:

		ipcDataTX.cmd = EMPVA_IPC_FF;

		break;
	}

	sceKernelSendMsgPipe(ipcPipeTX, &ipcDataTX, sizeof(IpcDataTX), SCE_KERNEL_MSG_PIPE_MODE_WAIT | SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL, SCE_NULL);
}

SceVoid RxThread::EntryFunction()
{
	IpcDataRX ipcDataRX;
	wstring text16;
	SceUInt32 artLen, albLen;
	Rgba col;
	col.r = 1.0f;
	col.g = 1.0f;
	col.b = 1.0f;
	col.a = 1.0f;

	while (1) {
		sceKernelReceiveMsgPipe(ipcPipeRX, &ipcDataRX, sizeof(IpcDataRX), SCE_KERNEL_MSG_PIPE_MODE_WAIT | SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL, SCE_NULL);

		SCE_DBG_LOG_INFO("IPC RX: %u\n", ipcDataRX.cmd);

		switch (ipcDataRX.cmd) {
		case EMPVA_TERMINATE_SHELL_RX:
			goto endRxThrd;
			break;
		case EMPVA_IPC_ACTIVATE:
			imposeIpcActive = SCE_TRUE;
			break;
		case EMPVA_IPC_DEACTIVATE:
			imposeIpcActive = SCE_FALSE;
			break;
		case EMPVA_IPC_APP_STOP:
			stopMainThread();
			break;
		case EMPVA_IPC_APP_START:
			startMainThread();
			break;
		case EMPVA_IPC_INFO:

			if ((ipcDataRX.flags & EMPVA_IPC_REFRESH_PBBT) == EMPVA_IPC_REFRESH_PBBT) {

			}

			if ((ipcDataRX.flags & EMPVA_IPC_REFRESH_TEXT) == EMPVA_IPC_REFRESH_TEXT) {

				text16 = (wchar_t *)ipcDataRX.title;

				if (textTop != SCE_NULL && impose) {
					textTop->SetLabel(&text16);
					textTop->SetColor(&col);
				}

				topText->clear();
				topText->append(text16.c_str(), text16.length());

				text16 = (wchar_t *)ipcDataRX.artist;
				artLen = sce_paf_wcslen((wchar_t *)ipcDataRX.artist);
				albLen = sce_paf_wcslen((wchar_t *)ipcDataRX.album);
				if (artLen != 0 && albLen != 0)
					text16.append(L" / ", 4);
				text16.append((wchar_t *)ipcDataRX.album, albLen);

				if (textBottom != SCE_NULL && impose) {
					textBottom->SetLabel(&text16);
					textBottom->SetColor(&col);
				}

				bottomText->clear();
				bottomText->append(text16.c_str(), text16.length());
			}

			break;
		}
	}

endRxThrd:

	Cancel();
}

SceVoid ImposeThread::EntryFunction()
{
	SceAppMgrAppState appState;

	while (!IsCanceled()) {
		sceAppMgrGetAppState(&appState);
		if (impose != appState.isSystemUiOverlaid && appState.isSystemUiOverlaid == SCE_TRUE) {
			SCE_DBG_LOG_INFO("Impose detected\n");
			if (imposeIpcActive) {
				thread::Sleep(100);
				findWidgets();
				setButtonState();
				setText();
			}
		}
		else if (impose != appState.isSystemUiOverlaid) {
			SCE_DBG_LOG_INFO("Impose done\n");
			resetWidgets();
		}
		impose = appState.isSystemUiOverlaid;
		thread::Sleep(100);
	}

	Cancel();
}

extern "C" {

	#include <moduleinfo.h>

	SCE_MODULE_INFO(ElevenMPV_A_ShellPlugin, 2, 1, 1)

	typedef struct SceShellAudioBGMState {
		int bgmPortOwnerId;
		int bgmPortPriority;
		int someStatus1;
		int currentState;
		int someStatus2;
	} SceShellAudioBGMState;

	int sceAppMgrGetCurrentBgmState2_patched(SceShellAudioBGMState *state)
	{
		int ret = TAI_NEXT(sceAppMgrGetCurrentBgmState2_patched, hookRef[0], state);
		if (state->bgmPortPriority > 0x80)
			state->bgmPortPriority = 0x80;
		return ret;
	}

	int module_start(SceSize args, const void * argp)
	{
#ifdef _DEBUG
		sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_TRACE);
#endif

		int ret = setup_stage1();
		if (ret != SCE_KERNEL_START_SUCCESS)
			return ret;

		setup_stage2();

		hookId[0] = taiHookFunctionImport(&hookRef[0], "SceShell", 0xA6605D6F, 0x62BEBD65, (const void *)sceAppMgrGetCurrentBgmState2_patched);

		return ret;
	}

	int module_stop(SceSize args, const void * argp)
	{
		//cleanup();
		return SCE_KERNEL_STOP_SUCCESS;
	}

}