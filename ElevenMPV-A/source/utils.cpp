#include <apputil.h>
#include <kernel.h>
#include <shellsvc.h>
#include <power.h> 
#include <appmgr.h> 
#include <shellaudio.h>
#include <message_dialog.h>
#include <taihen.h>
#include <libdbg.h>
#include <paf.h>
#include <stdlib.h>

#include "utils.h"
#include "touch.h"
#include "audio.h"
#include "common.h"
#include "ipc.h"
#include "yt_utils.h"
#include "vitaaudiolib.h"

static SceBool s_isDeactivated = SCE_FALSE;
static SceBool s_isDeactivatedByPowerCB = SCE_FALSE;

static char s_titleid[12];

static SceUID s_shellPluginUID = SCE_UID_INVALID_UID;
static SceUID s_shellPid = SCE_UID_INVALID_UID;

static SceUID s_ipcPipeRX = SCE_UID_INVALID_UID;
static SceUID s_ipcPipeTX = SCE_UID_INVALID_UID;

static SceInt32 s_memGrown = EMPVAUtils::MemState_Low;
static SceInt32 s_pagemode = menu::settings::Settings::PageMode_Normal;

static EMPVAUtils::PleaseWaitThread *s_pleaseWaitThread = SCE_NULL;

static thread::JobQueue *s_cbJobQueue = SCE_NULL;

SceBool EMPVAUtils::IsSupportedExtension(const char *ext)
{
	for (int i = 0; i < k_supportedExtNum; i++) {
		if (!sce_paf_strncasecmp(ext, k_supportedExtList[i], 4))
			return SCE_TRUE;
	}

	return SCE_FALSE;
}

SceBool EMPVAUtils::IsSupportedCoverExtension(const char *ext)
{
	for (int i = 0; i < k_supportedCoverExtNum; i++) {
		if (!sce_paf_strncasecmp(ext, k_supportedCoverExtList[i], 4))
			return SCE_TRUE;
	}

	return SCE_FALSE;
}

SceBool EMPVAUtils::IsRootDevice(const char *path)
{
	SceInt32 len = sce_paf_strlen(path);
	if (path[len - 1] == '/' && path[len - 2] == ':')
		return SCE_TRUE;

	return SCE_FALSE;
}

const char *EMPVAUtils::GetFileExt(const char *filename)
{
	const char *dot = sce_paf_strrchr(filename, '.');

	if (!dot || dot == filename)
		return "";

	return dot + 1;
}

SceUInt32 EMPVAUtils::GetHash(const char *name)
{
	Resource::Element searchRequest;
	Resource::Element searchResult;

	searchRequest.id = name;
	searchResult.hash = searchResult.GetHashById(&searchRequest);

	return searchResult.hash;
}

wchar_t *EMPVAUtils::GetStringWithNum(const char *name, SceUInt32 num)
{
	Resource::Element searchRequest;
	char fullName[128];

	sce_paf_snprintf(fullName, sizeof(fullName), "%s%u", name, num);

	searchRequest.hash = EMPVAUtils::GetHash(fullName);
	wchar_t *res = (wchar_t *)g_empvaPlugin->GetString(&searchRequest);

	return res;
}

SceUInt32 EMPVAUtils::Downscale(SceInt32 ix, SceInt32 iy, ScePVoid ibuf, SceInt32 ox, SceInt32 oy, ScePVoid obuf)
{
	/*return stbir_resize_uint8_generic((unsigned char *)ibuf, ix, iy, ix * 4, (unsigned char *)obuf, ox, oy, ox * 4, 4, -1, 0,
		STBIR_EDGE_CLAMP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL);*/
	return 0;
}

SceInt32 EMPVAUtils::Alphasort(const void *p1, const void *p2) 
{
	io::Dir::Dirent *entryA = (io::Dir::Dirent *)p1;
	io::Dir::Dirent *entryB = (io::Dir::Dirent *)p2;

	if ((entryA->type == io::Type_Dir) && (entryB->type != io::Type_Dir))
		return -1;
	else if ((entryA->type != io::Type_Dir) && (entryB->type == io::Type_Dir))
		return 1;

	return sce_paf_strcasecmp(entryA->name.data, entryB->name.data);
}

SceBool EMPVAUtils::IsDecoderUsed() 
{
	if (!sceKernelPollEventFlag(g_eventFlagUid, FLAG_ELEVENMPVA_IS_DECODER_USED, SCE_KERNEL_EVF_WAITMODE_AND, SCE_NULL))
		return SCE_TRUE;
	else
		return SCE_FALSE;
}

SceBool EMPVAUtils::IsSleep()
{
	return s_isDeactivatedByPowerCB;
}

SceInt32 EMPVAUtils::GetDecoderType(const char *path)
{
	if (sce_paf_strstr(path, "https://")) {
		return 1000;
	}

	const char *ext = GetFileExt(path);

	for (int i = 0; i < k_supportedExtNum; i++) {
		if (!sce_paf_strncasecmp(ext, k_supportedExtList[i], 4))
			return i;
	}

	return -1;
}

SceVoid EMPVAUtils::PowerTickTask(ScePVoid pUserData)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
}

SceVoid EMPVAUtils::SetPowerTickTask(SceBool enable)
{
	if (enable)
		common::Utils::AddMainThreadTask(EMPVAUtils::PowerTickTask, SCE_NULL);
	else
		common::Utils::RemoveMainThreadTask(EMPVAUtils::PowerTickTask, SCE_NULL);

}

SceInt32 EMPVAUtils::PowerCallback(SceInt32 notifyId, SceInt32 notifyCount, SceInt32 powerInfo, ScePVoid common)
{
	char uri[256];

	if ((powerInfo & SCE_POWER_CALLBACKARG_RESERVED_22) && !s_isDeactivated) { // suspend
		if (g_currentPlayerInstance != SCE_NULL) {
			if (g_currentPlayerInstance->GetCore()) {
				if (g_currentPlayerInstance->GetCore()->GetDecoder() != SCE_NULL) {
					if (!g_currentPlayerInstance->GetCore()->GetDecoder()->IsPaused() && !EMPVAUtils::IsDecoderUsed())
						sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_PLAY, 0);
				}
			}
		}
		sceShellUtilExitToLiveBoard();
		s_isDeactivatedByPowerCB = SCE_TRUE;
	}
	else if ((powerInfo & SCE_POWER_CALLBACKARG_RESERVED_23) && s_isDeactivatedByPowerCB) { // resume
		sce_paf_snprintf(uri, 256, "%s%s", "psgm:play?titleid=", s_titleid);
		sceAppMgrLaunchAppByUri(0x20000, uri);
		s_isDeactivatedByPowerCB = SCE_FALSE;
	}

	return 0;
}

SceVoid EMPVAUtils::AppWatchdogTask(ScePVoid pUserData)
{
	SceAppMgrEvent appEvent;
	SceUInt32 evNum = 0;
	SceUInt32 evNumRecv = 0;
	audio::GenericDecoder *currentDecoder = SCE_NULL;

	if (g_currentPlayerInstance != SCE_NULL) {
		if (g_currentPlayerInstance->GetCore())
			currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
	}

	if (sceKernelPollEventFlag(g_eventFlagUid, FLAG_ELEVENMPVA_IS_FG, SCE_KERNEL_EVF_WAITMODE_AND, SCE_NULL))
		thread::Sleep(100);

	sceAppMgrReceiveEventNum(&evNum);
	if (evNum > 0) {

		do {
			sceAppMgrReceiveEvent(&appEvent);

			switch (appEvent.event) {
			case SCE_APP_EVENT_ON_ACTIVATE:

				EMPVAUtils::Activate();
				s_isDeactivated = SCE_FALSE;

				break;
			case SCE_APP_EVENT_ON_DEACTIVATE:

				EMPVAUtils::Deactivate();
				s_isDeactivated = SCE_TRUE;
				
				break;
			case SCE_APP_EVENT_REQUEST_QUIT:

				sceAppMgrReleaseBgmPort();
				if (!EMPVAUtils::IsDecoderUsed()) {
					sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_STOP, 0);
					sceMusicPlayerServiceTerminate();
				}

				EMPVAUtils::Exit();

				break;
			}

			evNumRecv++;
		} while (evNumRecv < evNum);

	}
}

SceVoid EMPVAUtils::Init()
{
	char pluginPath[256];

	sceAppMgrAppParamGetString(SCE_KERNEL_PROCESS_ID_SELF, 12, s_titleid, 12);
	common::Utils::AddMainThreadTask(EMPVAUtils::AppWatchdogTask, SCE_NULL);

	SceInt32 ret = sceAppMgrGetIdByName(&s_shellPid, "NPXS19999");
	if (ret >= 0) {
		sce_paf_snprintf(pluginPath, 256, "ux0:app/%s/module/shell_plugin.suprx", s_titleid);
		s_shellPluginUID = taiLoadStartModuleForPid(s_shellPid, pluginPath, 0, SCE_NULL, 0);
		sce_paf_memset(pluginPath, 0, sizeof(pluginPath));
		sce_paf_snprintf(pluginPath, 256, "ux0:app/%s/module/download_enabler_empva.suprx", s_titleid);
		taiLoadStartModuleForPid(s_shellPid, pluginPath, 0, SCE_NULL, 0);

		if (s_shellPluginUID > 0) {
			s_ipcPipeRX = sceKernelOpenMsgPipe("ElevenMPVA::ShellIPC_RX");
			s_ipcPipeTX = sceKernelOpenMsgPipe("ElevenMPVA::ShellIPC_TX");
		}
	}

	if (!Misc::IsDolce()) {
		SceUID powerCbid = sceKernelCreateCallback("EMPVA::PowerCb", 0, EMPVAUtils::PowerCallback, SCE_NULL);
		scePowerRegisterCallback(powerCbid);
	}

	thread::JobQueue::Opt queueOpt;
	queueOpt.workerNum = 1;
	queueOpt.workerOpt = SCE_NULL;
	queueOpt.workerPriority = SCE_KERNEL_HIGHEST_PRIORITY_USER + 30;
	queueOpt.workerStackSize = SCE_KERNEL_256KiB;

	s_cbJobQueue = new thread::JobQueue("EMPVA::CallbackJobQueue", &queueOpt);
}

SceVoid EMPVAUtils::Exit()
{
	SceInt32 ret;

	if (s_shellPluginUID > 0)
		taiStopUnloadModuleForPid(s_shellPid, s_shellPluginUID, 0, SCE_NULL, 0, SCE_NULL, &ret);
	YTUtils::Term(SCE_TRUE);
	sceKernelExitProcess(0);
}

SceVoid EMPVAUtils::Activate()
{
	audio::GenericDecoder *currentDecoder = SCE_NULL;
	paf::ui::Widget *scene;

	if (g_currentPlayerInstance != SCE_NULL) {
		if (g_currentPlayerInstance->GetCore())
			currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
	}

	sceKernelSetEventFlag(g_eventFlagUid, FLAG_ELEVENMPVA_IS_FG);

	paf::Misc::ResumeTouchInput(SCE_TOUCH_PORT_FRONT);

	scene = Framework::s_frameworkInstance->GetCurrentScene();
	scene->unk_0D6 = 0;

	if (currentDecoder) {
		if ((currentDecoder->IsPaused() || !currentDecoder->isPlaying) && EMPVAUtils::IsDecoderUsed())
			sceAppMgrAcquireBgmPortWithPriority(0x81);
		else if ((currentDecoder->IsPaused() || !currentDecoder->isPlaying) && !EMPVAUtils::IsDecoderUsed())
			sceAppMgrAcquireBgmPortWithPriority(0x80);
	}

	sceKernelChangeThreadPriority(thread::GetMainThreadUID(), 77);
}

SceVoid EMPVAUtils::Deactivate()
{
	audio::GenericDecoder *currentDecoder = SCE_NULL;
	paf::ui::Widget *scene;

	if (g_currentPlayerInstance != SCE_NULL) {
		if (g_currentPlayerInstance->GetCore())
			currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
	}

	sceKernelClearEventFlag(g_eventFlagUid, ~FLAG_ELEVENMPVA_IS_FG);

	paf::Misc::SuspendTouchInput(SCE_TOUCH_PORT_FRONT);

	scene = Framework::s_frameworkInstance->GetCurrentScene();
	scene->unk_0D6 = 1;

	if (currentDecoder) {
		if (currentDecoder->IsPaused() || !currentDecoder->isPlaying)
			sceAppMgrReleaseBgmPort();
	}

	sceKernelChangeThreadPriority(thread::GetMainThreadUID(), SCE_KERNEL_COMMON_QUEUE_LOWEST_PRIORITY);
}

SceVoid EMPVAUtils::SetMemStatus()
{
	SceInt32 ret = -1;

	SceAppMgrBudgetInfo budgetInfo;
	sce_paf_memset(&budgetInfo, 0, sizeof(SceAppMgrBudgetInfo));
	budgetInfo.size = sizeof(SceAppMgrBudgetInfo);

	ret = sceAppMgrGetBudgetInfo(&budgetInfo);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[EMPVA_MAIN]  sceAppMgrGetBudgetInfo failed with code 0x%X\n", ret);
	}

	if (budgetInfo.budgetMain > 17 * 1024 * 1024) {
		if (budgetInfo.budgetMain < 33 * 1024 * 1024)
			s_memGrown = EMPVAUtils::MemState_Mid;
		else
			s_memGrown = EMPVAUtils::MemState_Full;
	}

	SCE_DBG_LOG_DEBUG("[EMPVA_DEBUG] Memory grow state: %u\n", s_memGrown);
}

SceInt32 EMPVAUtils::GetPagemode()
{
	return s_pagemode;
}

SceVoid EMPVAUtils::SetPagemode(SceInt32 mode)
{
	s_pagemode = mode;
}

SceInt32 EMPVAUtils::GetMemStatus()
{
	return s_memGrown;
}

SceVoid EMPVAUtils::PleaseWaitThread::EntryFunction()
{
	SceMsgDialogParam				msgParam;
	SceMsgDialogSystemMessageParam	sysMsgParam;
	SceMsgDialogUserMessageParam userMsgParam;
	SceCommonDialogStatus status;
	userCancel = SCE_FALSE;

	//sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	sceMsgDialogParamInit(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_SYSTEM_MSG;

	sce_paf_memset(&sysMsgParam, 0, sizeof(SceMsgDialogSystemMessageParam));
	msgParam.sysMsgParam = &sysMsgParam;
	if (withCancel)
		msgParam.sysMsgParam->sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL;
	else
		msgParam.sysMsgParam->sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL;

	sceMsgDialogInit(&msgParam);
	status = sceMsgDialogGetStatus();

	while (status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
		status = sceMsgDialogGetStatus();
		thread::Sleep(10);
	}

	while (status == SCE_COMMON_DIALOG_STATUS_RUNNING && !forceCancel) {
		status = sceMsgDialogGetStatus();
		thread::Sleep(100);
	}

	if (status != SCE_COMMON_DIALOG_STATUS_RUNNING)
		userCancel = SCE_TRUE;

	if (!userCancel) {
		sceMsgDialogClose();

		while (status != SCE_COMMON_DIALOG_STATUS_FINISHED) {
			status = sceMsgDialogGetStatus();
			thread::Sleep(10);
		}
	}

	sceMsgDialogTerm();

	//sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	sceKernelExitDeleteThread(0);
}

SceVoid EMPVAUtils::BeginPleaseWait(SceBool withCancel)
{
	if (s_pleaseWaitThread) {
		s_pleaseWaitThread->forceCancel = SCE_TRUE;
		s_pleaseWaitThread->Join();
		delete s_pleaseWaitThread;
	}
	s_pleaseWaitThread = new EMPVAUtils::PleaseWaitThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_4KiB, "EMPVA::PlsWaitDialog");
	s_pleaseWaitThread->withCancel = withCancel;
	s_pleaseWaitThread->forceCancel = SCE_FALSE;
	s_pleaseWaitThread->Start();
}

SceVoid EMPVAUtils::EndPleaseWait()
{
	if (s_pleaseWaitThread) {
		s_pleaseWaitThread->forceCancel = SCE_TRUE;
	}
}

SceVoid EMPVAUtils::RunCallbackAsJob(ui::Widget::EventCallback::EventHandler eventHandler, EMPVAUtils::AsyncEnqueue::FinishHandler finishHandler, SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	AsyncEnqueue *asJob = new AsyncEnqueue("EMPVA::AsyncCallback");
	asJob->eventHandler = eventHandler;
	asJob->finishHandler = finishHandler;
	asJob->eventId = eventId;
	asJob->self = self;
	asJob->a3 = a3;
	asJob->pUserData = pUserData;

	CleanupHandler *req = new CleanupHandler();
	req->refCount = 0;
	req->unk_08 = 1;
	req->cb = (CleanupHandler::CleanupCallback)AsyncEnqueue::JobKiller;

	ObjectWithCleanup itemParam;
	itemParam.object = asJob;
	itemParam.cleanup = req;

	s_cbJobQueue->Enqueue(&itemParam);
}

SceVoid EMPVAUtils::IPC::Enable()
{
	IpcDataRX packet;
	packet.cmd = EMPVA_IPC_ACTIVATE;

	if (s_ipcPipeRX > 0)
		sceKernelSendMsgPipe(s_ipcPipeRX, &packet, sizeof(IpcDataRX), SCE_KERNEL_MSG_PIPE_MODE_WAIT | SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL, SCE_NULL);
}

SceVoid EMPVAUtils::IPC::Disable()
{
	IpcDataRX packet;
	packet.cmd = EMPVA_IPC_DEACTIVATE;

	if (s_ipcPipeRX > 0)
		sceKernelSendMsgPipe(s_ipcPipeRX, &packet, sizeof(IpcDataRX), SCE_KERNEL_MSG_PIPE_MODE_WAIT | SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL, SCE_NULL);
}

SceVoid EMPVAUtils::IPC::SendInfo(WString *title, WString *artist, WString *album, SceInt32 playBtState)
{
	IpcDataRX packet;
	packet.cmd = EMPVA_IPC_INFO;
	packet.flags = 0;

	if (playBtState > -1) {
		packet.flags |= EMPVA_IPC_REFRESH_PBBT;
		packet.pbbtState = playBtState;
	}

	if (title != SCE_NULL) {
		packet.flags |= EMPVA_IPC_REFRESH_TEXT;
		sce_paf_memset(&packet.title, 0, sizeof(packet.title));
		sce_paf_wcsncpy((wchar_t *)&packet.title, title->data, 256);
	}

	if (artist != SCE_NULL) {
		packet.flags |= EMPVA_IPC_REFRESH_TEXT;
		sce_paf_memset(&packet.artist, 0, sizeof(packet.artist));
		sce_paf_wcsncpy((wchar_t *)&packet.artist, artist->data, 256);
	}

	if (artist != SCE_NULL) {
		packet.flags |= EMPVA_IPC_REFRESH_TEXT;
		sce_paf_memset(&packet.album, 0, sizeof(packet.album));
		sce_paf_wcsncpy((wchar_t *)&packet.album, album->data, 256);
	}

	if (s_ipcPipeRX > 0)
		sceKernelSendMsgPipe(s_ipcPipeRX, &packet, sizeof(IpcDataRX), SCE_KERNEL_MSG_PIPE_MODE_WAIT | SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL, SCE_NULL);
}

SceUInt32 EMPVAUtils::IPC::PeekTx()
{
	IpcDataTX packet;
	packet.cmd = 0;

	if (s_ipcPipeTX > 0)
		sceKernelTryReceiveMsgPipe(s_ipcPipeTX, &packet, sizeof(IpcDataTX), SCE_KERNEL_MSG_PIPE_MODE_FULL, SCE_NULL);

	return packet.cmd;
}