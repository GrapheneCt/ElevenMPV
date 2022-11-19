#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>
#include <audioout.h>
#include <shellaudio.h>
#include <libsysmodule.h>
#include <libdbg.h>
#include <bxce.h>
#include <app_settings.h>
#include <ini_file_processor.h>

#include "motion_e.h"
#include "common.h"
#include "menu_settings.h"
#include "utils.h"
#include "yt_utils.h"
#include "downloader.h"

using namespace paf;
using namespace sce;

static SceUInt32 s_callerMode = 0;

static menu::settings::Settings *s_settingsInstance = SCE_NULL;

static SceBool s_needPageReload = SCE_FALSE;
static SceBool s_needCwdReload = SCE_FALSE;
static SceInt32 s_lastError = SCE_OK;

menu::settings::Settings::Settings()
{
	SceInt32 ret;
	SceSize fsize = 0;
	const char *fmime = SCE_NULL;
	Plugin::InitParam pluginParam;
	AppSettings::InitParam sparam;

	settingsReset = SCE_FALSE;

	pluginParam.pluginName = "app_settings_plugin";
	pluginParam.resourcePath = "vs0:vsh/common/app_settings_plugin.rco";
	pluginParam.scopeName = "__main__";

	pluginParam.pluginSetParamCB = AppSettings::PluginCreateCB;
	pluginParam.pluginInitCB = AppSettings::PluginInitCB;
	pluginParam.pluginStartCB = AppSettings::PluginStartCB;
	pluginParam.pluginStopCB = AppSettings::PluginStopCB;
	pluginParam.pluginExitCB = AppSettings::PluginExitCB;
	pluginParam.pluginPath = "vs0:vsh/common/app_settings.suprx";
	pluginParam.unk_58 = 0x96;

	s_frameworkInstance->LoadPlugin(&pluginParam);

	sparam.xmlFile = g_empvaPlugin->resource->GetFile(EMPVAUtils::GetHash("file_empva_settings"), &fsize, &fmime);

	sparam.allocCB = sce_paf_malloc;
	sparam.freeCB = sce_paf_free;
	sparam.reallocCB = sce_paf_realloc;
	sparam.safeMemoryOffset = 0;
	sparam.safeMemorySize = k_safeMemIniLimit;

	ret = sce::AppSettings::GetInstance(&sparam, &appSet);

	ret = -1;
	appSet->GetInt("settings_version", &ret, 0);
	if (ret != k_settingsVersion) {
		ret = appSet->Initialize();

		appSet->SetInt("device", k_defDevice);
		appSet->SetInt("sort", k_defSort);
		appSet->SetInt("eq_mode", k_defEqMode);
		appSet->SetInt("alc_mode", k_defAlcMode);
		appSet->SetInt("eq_volume", k_defEqVolume);
		appSet->SetInt("power_saving", k_defPowerSaving);
		appSet->SetInt("power_timer", k_defPowerTimer);
		appSet->SetInt("stick_skip", k_defStickSkip);
		appSet->SetInt("motion_mode", k_defMotionMode);
		appSet->SetInt("motion_timer", k_defMotionTimer);
		appSet->SetInt("motion_degree", k_defMotionDeg);
		appSet->SetInt("last_pagemode", k_defLastPagemode);
		appSet->SetInt("fps_limit", k_defFpsLimit);
		appSet->SetInt("yt_quality", k_defYtQuality);

		ret = appSet->SetInt("settings_version", k_settingsVersion);

		settingsReset = SCE_TRUE;
	}

	appSet->GetInt("eq_mode", &eq_mode, k_defEqMode);
	appSet->GetInt("eq_volume", &eq_volume, k_defEqVolume);
	appSet->GetInt("power_saving", &power_saving, k_defPowerSaving);
	appSet->GetInt("power_timer", &power_timer, k_defPowerTimer);
	appSet->GetInt("stick_skip", &stick_skip, k_defStickSkip);
	appSet->GetInt("motion_mode", &motion_mode, k_defMotionMode);
	appSet->GetInt("motion_timer", &motion_timer, k_defMotionTimer);
	appSet->GetInt("motion_degree", &motion_degree, k_defMotionDeg);
	appSet->GetInt("alc_mode", &alc_mode, k_defAlcMode);
	appSet->GetInt("sort", &sort, k_defSort);
	appSet->GetInt("device", &device, k_defDevice);
	appSet->GetInt("last_pagemode", &last_pagemode, k_defLastPagemode);
	appSet->GetInt("fps_limit", &fps_limit, k_defFpsLimit);
	appSet->GetInt("yt_quality", &yt_quality, k_defYtQuality);

	s_settingsInstance = this;
}

menu::settings::Settings::~Settings()
{
	SCE_DBG_LOG_ERROR("[EMPVA_SETTINGS] invalid function call\n");
	sceKernelExitProcess(0);
}

SceVoid menu::settings::Settings::Open(SceUInt32 mode)
{
	s_callerMode = mode;

	AppSettings::InterfaceCallbacks ifCb;

	ifCb.listChangeCb = CBListChange;
	ifCb.listForwardChangeCb = CBListForwardChange;
	ifCb.listBackChangeCb = CBListBackChange;
	ifCb.isVisibleCb = CBIsVisible;
	ifCb.elemInitCb = CBElemInit;
	ifCb.elemAddCb = CBElemAdd;
	ifCb.valueChangeCb = CBValueChange;
	ifCb.valueChangeCb2 = CBValueChange2;
	ifCb.termCb = CBTerm;
	ifCb.getStringCb = CBGetString;
	ifCb.getTexCb = CBGetTex;

	Plugin *appSetPlug = paf::Plugin::Find("app_settings_plugin");
	AppSettings::Interface *appSetIf = (sce::AppSettings::Interface *)appSetPlug->GetInterface(1);
	appSetIf->Show(&ifCb);
}

SceVoid menu::settings::Settings::CBListChange(const char *elementId)
{

}

SceVoid menu::settings::Settings::CBListForwardChange(const char *elementId)
{

}

SceVoid menu::settings::Settings::CBListBackChange(const char *elementId)
{

}

SceInt32 menu::settings::Settings::CBIsVisible(const char *elementId, SceBool *pIsVisible)
{
	*pIsVisible = SCE_TRUE;

	switch (s_callerMode) {
	case menu::settings::SettingsButtonCB::Parent_Player:
		if (!sce_paf_strcmp(elementId, "list_device") || !sce_paf_strcmp(elementId, "list_sort")) {
			*pIsVisible = SCE_FALSE;
		}
		break;
	default:
		break;
	}

	if (EMPVAUtils::GetPagemode() != menu::settings::Settings::PageMode_YouTube) {
		if (!sce_paf_strcmp(elementId, "setting_list_youtube")) {
			*pIsVisible = SCE_FALSE;
		}
	}

	if (s_callerMode != menu::settings::SettingsButtonCB::Parent_Player || EMPVAUtils::GetPagemode() != menu::settings::Settings::PageMode_YouTube) {
		if (!sce_paf_strcmp(elementId, "button_youtube_download")) {
			*pIsVisible = SCE_FALSE;
		}
	}

	// Check if PSTV to disable unsupported settings lists
#ifdef NDEBUG
	if (SCE_PAF_IS_DOLCE) {
		if (!sce_paf_strcmp(elementId, "setting_list_power") || !sce_paf_strcmp(elementId, "setting_list_controls")) {
			*pIsVisible = SCE_FALSE;
		}
	}
#endif

	return SCE_OK;
}

SceInt32 menu::settings::Settings::CBElemInit(const char *elementId)
{
	return SCE_OK;
}

SceInt32 menu::settings::Settings::CBElemAdd(const char *elementId, paf::ui::Widget *widget)
{
	return SCE_OK;
}

SceInt32 menu::settings::Settings::CBValueChange(const char *elementId, const char *newValue)
{
	char *end;
	SceInt32 ret = SCE_OK;
	SceUInt32 elemHash = EMPVAUtils::GetHash(elementId);
	SceInt32 value = sce_paf_strtol(newValue, &end, 10);
	string *text8 = SCE_NULL;
	wstring label16;
	string label8;
	rco::Element searchParam;

	switch (elemHash) {
	case Hash_Device:
		text8 = ccc::UTF16toUTF8WithAlloc(EMPVAUtils::GetStringWithNum("msg_option_device_", value));
		if (LocalFile::Exists(text8->c_str())) {
			s_needPageReload = SCE_TRUE;
			s_needCwdReload = SCE_TRUE;
			GetInstance()->device = value;
		}
		else
			ret = SCE_ERROR_ERRNO_ENOENT;
		break;
	case Hash_Sort:
		s_needPageReload = SCE_TRUE;
		GetInstance()->sort = value;
		break;
	case Hash_AudioEq:
		GetInstance()->eq_mode = value;
		if (g_isPlayerActive) {
			if (EMPVAUtils::IsDecoderUsed())
				sceAudioOutSetEffectType(value);
			else
				sceMusicPlayerServiceSetEQ(value);
		}
		break;
	case Hash_AudioLimitVolume:
		GetInstance()->eq_volume = value;
		break;
	case Hash_AudioAlc:
		if (g_isPlayerActive) {
			if (EMPVAUtils::IsDecoderUsed())
				sceAudioOutSetAlcMode(value);
			else
				sceMusicPlayerServiceSetALC(value);
		}
		GetInstance()->alc_mode = value;
		break;
	case Hash_PowerFps:
		GetInstance()->fps_limit = value;
		break;
	case Hash_PowerSuspend:
		GetInstance()->power_saving = value;
		break;
	case Hash_PowerTimer:
		GetInstance()->power_timer = value;
		break;
	case Hash_ControlsSkip:
		GetInstance()->stick_skip = value;
		break;
	case Hash_ControlsMotion:
		if (g_isPlayerActive)
			motion::Motion::SetState(SCE_TRUE);
		GetInstance()->motion_mode = value;
		break;
	case Hash_ControlsTimeout:
		if (g_isPlayerActive)
			motion::Motion::SetReleaseTimer(value);
		GetInstance()->motion_timer = value;
		break;
	case Hash_ControlsAngle:
		if (g_isPlayerActive)
			motion::Motion::SetAngleThreshold(value);
		GetInstance()->motion_degree = value;
		break;
	case Hash_YoutubeCleanHistory:
		YTUtils::HistLog::Clean();
		break;
	case Hash_YoutubeCleanFav:
		YTUtils::FavLog::Clean();
		break;
	case Hash_YoutubeDownload:
		if (g_currentPlayerInstance->GetCore()) {
			if (g_currentPlayerInstance->GetCore()->GetDecoder()) {
				if (g_currentPlayerInstance->GetCore()->GetDecoder()->IsValid()) {
					searchParam.hash = EMPVAUtils::GetHash("text_player_title");
					ui::Widget *textTitle = g_player_page->GetChild(&searchParam, 0);
					textTitle->GetLabel(&label16);
					ccc::UTF16toUTF8(&label16, &label8);
					label8.append(".webmyt", 8);

					ret = YTUtils::GetDownloader()->Enqueue(g_currentPlayerInstance->GetCore()->GetDecoder()->dataPath.c_str(), label8.c_str());

					break;
				}
			}
		}
		ret = SCE_ERROR_ERRNO_ENXIO;
		break;
	case Hash_YoutubeQuality:
		GetInstance()->yt_quality = value;
		break;
	default:
		break;
	}


	if (text8) {
		delete text8;
	}

	s_lastError = ret;
	return ret;
}

SceInt32 menu::settings::Settings::CBValueChange2(const char *elementId, const char *newValue)
{
	return SCE_OK;
}

SceVoid menu::settings::Settings::CBTerm()
{
	rco::Element searchParam;
	string *text8 = SCE_NULL;
	SceInt32 value = 0;

	switch (s_callerMode) {
	case menu::settings::SettingsButtonCB::Parent_Player:
		// Show (enable) player page
		ui::Widget::SetControlFlags(g_player_page, 1);
		break;
	case menu::settings::SettingsButtonCB::Parent_Displayfiles:
		// Show (enable) displayfiles page
		ui::Widget::SetControlFlags(g_rootPage, 1);
		break;
	}

	// Reset file browser pages if needed
	if (s_needPageReload) {

		menu::displayfiles::Page *tmpCurr;

		if (s_needCwdReload) {

			while (g_currentDispFilePage->prev != SCE_NULL) {
				tmpCurr = g_currentDispFilePage;
				g_currentDispFilePage = g_currentDispFilePage->prev;
				delete tmpCurr;
			}

			GetAppSetInstance()->GetInt("device", &value, 0);
			text8 = ccc::UTF16toUTF8WithAlloc(EMPVAUtils::GetStringWithNum("msg_option_device_", value));

			menu::displayfiles::Page *newPage = new menu::displayfiles::Page(text8->c_str());

			delete text8;

			s_needCwdReload = SCE_FALSE;
		}
		else {
			text8 = new string(g_currentDispFilePage->cwd->c_str());

			if (g_currentDispFilePage->prev != SCE_NULL) {
				tmpCurr = g_currentDispFilePage;
				g_currentDispFilePage = g_currentDispFilePage->prev;
				delete tmpCurr;
			}

			menu::displayfiles::Page *newPage = new menu::displayfiles::Page(text8->c_str());

			delete text8;
		}

		s_needPageReload = SCE_FALSE;
	}
}

wchar_t *menu::settings::Settings::CBGetString(const char *elementId)
{
	rco::Element searchParam;
	searchParam.hash = EMPVAUtils::GetHash(elementId);

	return g_empvaPlugin->GetWString(&searchParam);
}

SceInt32 menu::settings::Settings::CBGetTex(graph::Surface **tex, const char *elementId)
{
	return SCE_OK;
}

menu::settings::Settings *menu::settings::Settings::GetInstance()
{
	return s_settingsInstance;
}

AppSettings *menu::settings::Settings::GetAppSetInstance()
{
	return s_settingsInstance->appSet;
}

SceVoid menu::settings::Settings::SetLastDirectory(const char *cwd)
{
	SceInt32 len = sce_paf_strlen(cwd);
	sceAppUtilSaveSafeMemory((ScePVoid)&len, sizeof(SceInt32), k_safeMemIniLimit);
	sceAppUtilSaveSafeMemory((ScePVoid)cwd, len, k_safeMemIniLimit + sizeof(SceInt32));
}

SceVoid menu::settings::Settings::GetLastDirectory(string *cwd)
{
	SceInt32 ret = 0;
	const char *root_paths[] = {
		"ux0:/",
		"ur0:/",
		"uma0:/",
		"xmc0:/",
		"imc0:/",
		"grw0:/"
	};

	SceInt32 len;
	sceAppUtilLoadSafeMemory((ScePVoid)&len, sizeof(SceInt32), k_safeMemIniLimit);

	if (!len || settingsReset) {

		ret = sce_paf_snprintf(rootPath, 8, "ux0:/");

		sceAppUtilSaveSafeMemory((ScePVoid)&ret, sizeof(SceInt32), k_safeMemIniLimit);
		sceAppUtilSaveSafeMemory((ScePVoid)cwd, ret, k_safeMemIniLimit + sizeof(SceInt32));

		cwd->clear();
		cwd->append(rootPath, ret);

		settingsReset = SCE_FALSE;
	}
	else {

		sce_paf_strncpy(rootPath, root_paths[device], 8);

		char *buf = (char *)sce_paf_malloc(len + 1);

		sceAppUtilLoadSafeMemory((ScePVoid)buf, len, k_safeMemIniLimit + sizeof(SceInt32));

		buf[len] = '\0';

		if (LocalFile::Exists(buf))
			*cwd = buf;
		else
			*cwd = rootPath;

		sce_paf_free(buf);
	}
}

SceVoid menu::settings::SettingsButtonCB::SettingsButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	rco::Element searchParam;
	SceUInt32 callerMode = *(SceUInt32 *)pUserData;

	switch (callerMode) {
	case Parent_Player:
		// Hide (disable) player page
		ui::Widget::SetControlFlags(g_player_page, 0);
		break;
	case Parent_Displayfiles:
		// Hide (disable) displayfiles page
		ui::Widget::SetControlFlags(g_rootPage, 0);
		break;
	}

	menu::settings::Settings::GetInstance()->Open(callerMode);
}