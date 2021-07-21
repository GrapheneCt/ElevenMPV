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

static widget::Widget *imposeRoot = SCE_NULL;
static ImposeThread *mainThread = SCE_NULL;
static RxThread *rxThread = SCE_NULL;

static SceUID ipcPipeRX = SCE_UID_INVALID_UID;
static SceUID ipcPipeTX = SCE_UID_INVALID_UID;

static widget::Widget *buttonREW = SCE_NULL;
static widget::Widget *buttonPLAY = SCE_NULL;
static widget::Widget *buttonFF = SCE_NULL;
static widget::Widget *textTop = SCE_NULL;
static widget::Widget *textBottom = SCE_NULL;

static tai_hook_ref_t hookRef[1];
static SceUID hookId[1];

static WString *topText;
static WString *bottomText;

static SceBool impose = SCE_FALSE;

static SceBool imposeIpcActive = SCE_FALSE;

int setup_stage1()
{
	//Get power manage plugin object
	Plugin *imposePlugin = Plugin::Find("power_manage_plugin");
	if (imposePlugin == NULL) {
		SCE_DBG_LOG_ERROR("Power manage plugin not found\n");
		goto setup_error_return;	
	}

	//Power manage plugin -> power manage root
	widget::Widget *powerRoot = imposePlugin->GetInterface(1);
	if (powerRoot == NULL) {
		SCE_DBG_LOG_ERROR("Power root not found\n");
		goto setup_error_return;
	}

	//Power manage root -> impose root (some virtual function)
	widget::Widget *(*getImposeRoot)();
	getImposeRoot = (widget::Widget *(*)()) *(int *)((void *)powerRoot + 0x54);
	imposeRoot = getImposeRoot();
	if (imposeRoot == NULL) {
		SCE_DBG_LOG_ERROR("Impose root not found\n");
		goto setup_error_return;
	}

	return SCE_KERNEL_START_SUCCESS;

setup_error_return:

	return SCE_KERNEL_START_NO_RESIDENT;

}

void setup_stage2()
{
	topText = new WString();
	bottomText = new WString();

	ipcPipeRX = sceKernelCreateMsgPipe("ElevenMPVA::ShellIPC_RX", SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN, IPC_PIPE_ATTR, sizeof(IpcDataRX), SCE_NULL);
	ipcPipeTX = sceKernelCreateMsgPipe("ElevenMPVA::ShellIPC_TX", SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN, IPC_PIPE_ATTR, sizeof(IpcDataTX), SCE_NULL);

	mainThread = new ImposeThread(250, 0x1000, "ElevenMPVA::ShellControl");
	mainThread->done = SCE_FALSE;
	mainThread->Start();

	rxThread = new RxThread(249, 0x1000, "ElevenMPVA::ShellRx");
	rxThread->Start();
}

void cleanup()
{
	IpcDataRX ipcDataRX;

	if (mainThread != SCE_NULL) {
		mainThread->done = SCE_TRUE;
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
}

int findWidgets()
{
	Resource::Element widgetSearchResult;

	widgetSearchResult.hash = PlayerButtonCB::ButtonHash_Rew;

	buttonREW = SCE_NULL;
	while (buttonREW == NULL) {
		buttonREW = imposeRoot->GetChildByHash(&widgetSearchResult, 0);
		thread::Thread::Sleep(100);
	}

	widgetSearchResult.hash = PlayerButtonCB::ButtonHash_Ff;
	buttonFF = imposeRoot->GetChildByHash(&widgetSearchResult, 0);
	if (buttonFF == NULL) {
		SCE_DBG_LOG_ERROR("buttonFF not found\n");
		goto findButton_error_return;
	}

	widgetSearchResult.hash = PlayerButtonCB::ButtonHash_Play;
	buttonPLAY = imposeRoot->GetChildByHash(&widgetSearchResult, 0);
	if (buttonPLAY == NULL) {
		SCE_DBG_LOG_ERROR("buttonPLAY not found\n");
		goto findButton_error_return;
	}

	widgetSearchResult.hash = 0x66FDAFE3;
	textTop = imposeRoot->GetChildByHash(&widgetSearchResult, 0);
	if (textTop == NULL) {
		SCE_DBG_LOG_ERROR("textTop not found\n");
		goto findButton_error_return;
	}

	widgetSearchResult.hash = 0xF099B450;
	textBottom = imposeRoot->GetChildByHash(&widgetSearchResult, 0);
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
	widget::Widget::Color col;
	col.r = 1.0f;
	col.g = 1.0f;
	col.b = 1.0f;
	col.a = 1.0f;
	textTop->SetFilterColor(&col);
	textBottom->SetFilterColor(&col);

	textTop->SetLabel(topText);
	textBottom->SetLabel(bottomText);
}

SceVoid PlayerButtonCB::PlayerButtonCBFun(SceInt32 eventId, widget::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	IpcDataTX ipcDataTX;
	ipcDataTX.cmd = 0;

	switch (self->hash) {
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
	WString text16;
	SceUInt32 artLen, albLen;
	widget::Widget::Color col;
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
		case EMPVA_IPC_INFO:

			if ((ipcDataRX.flags & EMPVA_IPC_REFRESH_PBBT) == EMPVA_IPC_REFRESH_PBBT) {

			}

			if ((ipcDataRX.flags & EMPVA_IPC_REFRESH_TEXT) == EMPVA_IPC_REFRESH_TEXT) {

				text16.Set(ipcDataRX.title);

				if (textTop != SCE_NULL && impose) {
					textTop->SetLabel(&text16);
					textTop->SetFilterColor(&col);
				}

				topText->Clear();
				topText->Set(text16.data, text16.length);
				text16.Clear();

				text16.Set(ipcDataRX.artist);
				artLen = sce_paf_wcslen((wchar_t *)ipcDataRX.artist);
				albLen = sce_paf_wcslen((wchar_t *)ipcDataRX.album);
				if (artLen != 0 && albLen != 0)
					text16.Append((SceWChar16 *)L" / ", 4);
				text16.Append(ipcDataRX.album, albLen);

				if (textBottom != SCE_NULL && impose) {
					textBottom->SetLabel(&text16);
					textBottom->SetFilterColor(&col);
				}

				bottomText->Clear();
				bottomText->Set(text16.data, text16.length);
				text16.Clear();
			}

			break;
		}
	}

endRxThrd:

	sceKernelExitDeleteThread(0);
}

SceVoid ImposeThread::EntryFunction()
{
	SceAppMgrAppState appState;

	while (!done) {
		sceAppMgrGetAppState(&appState);
		if (impose != appState.isSystemUiOverlaid && appState.isSystemUiOverlaid == SCE_TRUE) {
			SCE_DBG_LOG_INFO("Impose detected\n");
			if (imposeIpcActive) {
				thread::Thread::Sleep(100);
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
		thread::Thread::Sleep(100);
	}

	sceKernelExitDeleteThread(0);
}

extern "C" {

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
#else
		sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_ERROR);
#endif

		int ret = setup_stage1();
		if (ret != SCE_KERNEL_START_SUCCESS)
			return ret;

		setup_stage2();

		hookId[0] = taiHookFunctionImport(&hookRef[0], "SceShell", 0xA6605D6F, 0x62BEBD65, sceAppMgrGetCurrentBgmState2_patched);
	}

	int module_stop(SceSize args, const void * argp)
	{
		cleanup();
		if (hookId[0] > 0)
			taiHookRelease(hookId[0], hookRef[0]);
		return SCE_KERNEL_STOP_SUCCESS;
	}

}