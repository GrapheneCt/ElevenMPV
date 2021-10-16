#include <apputil.h>
#include <kernel.h>
#include <shellsvc.h>
#include <power.h> 
#include <appmgr.h> 
#include <shellaudio.h> 
#include <taihen.h>
#include <paf.h>

#include "utils.h"
#include "config.h"
#include "touch.h"
#include "audio.h"
#include "common.h"
#include "ipc.h"
#include "vitaaudiolib.h"

static SceBool s_isDeactivated = SCE_FALSE;
static SceBool s_isDeactivatedByPowerCB = SCE_FALSE;

static char s_titleid[12];

static SceUID s_shellPluginUID = SCE_UID_INVALID_UID;
static SceUID s_shellPid = SCE_UID_INVALID_UID;

static SceUID s_ipcPipeRX = SCE_UID_INVALID_UID;
static SceUID s_ipcPipeTX = SCE_UID_INVALID_UID;

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

	searchRequest.id.Set(name);
	searchResult.hash = searchResult.GetHashById(&searchRequest);

	return searchResult.hash;
}

SceWChar16 *EMPVAUtils::GetStringWithNum(const char *name, SceUInt32 num)
{
	Resource::Element searchRequest;
	char fullName[128];

	sce_paf_snprintf(fullName, sizeof(fullName), "%s%u", name, num);

	searchRequest.hash = EMPVAUtils::GetHash(fullName);
	SceWChar16 *res = g_empvaPlugin->GetString(&searchRequest);

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
			if (g_currentPlayerInstance->GetCore()->GetDecoder() != SCE_NULL) {
				if (!g_currentPlayerInstance->GetCore()->GetDecoder()->IsPaused() && !EMPVAUtils::IsDecoderUsed())
					sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_PLAY, 0);
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
	audio::GenericDecoder *currentDecoder;
	paf::widget::Widget *scene;

	if (g_currentPlayerInstance != SCE_NULL) 
		currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
	else
		currentDecoder = SCE_NULL;

	if (sceKernelPollEventFlag(g_eventFlagUid, FLAG_ELEVENMPVA_IS_FG, SCE_KERNEL_EVF_WAITMODE_AND, SCE_NULL))
		thread::Thread::Sleep(100);

	sceAppMgrReceiveEventNum(&evNum);
	int ret = 0;
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

	sceShellUtilInitEvents(0);
	sceAppMgrAppParamGetString(SCE_KERNEL_PROCESS_ID_SELF, 12, s_titleid, 12);
	common::Utils::AddMainThreadTask(EMPVAUtils::AppWatchdogTask, SCE_NULL);

	SceInt32 ret = sceAppMgrGetIdByName(&s_shellPid, "NPXS19999");
	if (ret >= 0) {
		sce_paf_snprintf(pluginPath, 256, "%s%s%s", "ux0:app/", s_titleid, "/module/shell_plugin.suprx");
		s_shellPluginUID = taiLoadStartModuleForPid(s_shellPid, pluginPath, 0, SCE_NULL, 0);

		if (s_shellPluginUID > 0) {
			s_ipcPipeRX = sceKernelOpenMsgPipe("ElevenMPVA::ShellIPC_RX");
			s_ipcPipeTX = sceKernelOpenMsgPipe("ElevenMPVA::ShellIPC_TX");
		}
	}

	if (!Misc::IsDolce()) {
		SceUID powerCbid = sceKernelCreateCallback("EMPVA::PowerCb", 0, EMPVAUtils::PowerCallback, SCE_NULL);
		scePowerRegisterCallback(powerCbid);
	}
}

SceVoid EMPVAUtils::Exit()
{
	SceInt32 ret;

	if (s_shellPluginUID > 0)
		taiStopUnloadModuleForPid(s_shellPid, s_shellPluginUID, 0, SCE_NULL, 0, SCE_NULL, &ret);

	sceKernelExitProcess(0);
}

SceVoid EMPVAUtils::Activate()
{
	audio::GenericDecoder *currentDecoder;
	paf::widget::Widget *scene;

	if (g_currentPlayerInstance != SCE_NULL)
		currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
	else
		currentDecoder = SCE_NULL;

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

	sceKernelChangeThreadPriority(thread::Thread::GetMainThreadUID(), 77);
}

SceVoid EMPVAUtils::Deactivate()
{
	audio::GenericDecoder *currentDecoder;
	paf::widget::Widget *scene;

	if (g_currentPlayerInstance != SCE_NULL)
		currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
	else
		currentDecoder = SCE_NULL;

	sceKernelClearEventFlag(g_eventFlagUid, ~FLAG_ELEVENMPVA_IS_FG);

	paf::Misc::SuspendTouchInput(SCE_TOUCH_PORT_FRONT);

	scene = Framework::s_frameworkInstance->GetCurrentScene();
	scene->unk_0D6 = 1;

	if (currentDecoder) {
		if (currentDecoder->IsPaused() || !currentDecoder->isPlaying)
			sceAppMgrReleaseBgmPort();
	}

	sceKernelChangeThreadPriority(thread::Thread::GetMainThreadUID(), SCE_KERNEL_COMMON_QUEUE_LOWEST_PRIORITY);
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
		sce_paf_wcsncpy((wchar_t *)&packet.title, (wchar_t *)title->data, 256);
	}

	if (artist != SCE_NULL) {
		packet.flags |= EMPVA_IPC_REFRESH_TEXT;
		sce_paf_memset(&packet.artist, 0, sizeof(packet.artist));
		sce_paf_wcsncpy((wchar_t *)&packet.artist, (wchar_t *)artist->data, 256);
	}

	if (artist != SCE_NULL) {
		packet.flags |= EMPVA_IPC_REFRESH_TEXT;
		sce_paf_memset(&packet.album, 0, sizeof(packet.album));
		sce_paf_wcsncpy((wchar_t *)&packet.album, (wchar_t *)album->data, 256);
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