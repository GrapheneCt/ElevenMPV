#include <appmgr.h>
#include <kernel.h>
#include <shellsvc.h>
#include <libsysmodule.h>
#include <libdbg.h>
#include <shellaudio.h>
#include <net.h>
#include <libnetctl.h>
#include <libhttp.h>
#include <paf.h>
#include <stdlib.h>

#include "common.h"
#include "main.h"
#include "debug.h"
#include "utils.h"
#include "menu_displayfiles.h"
#include "menu_settings.h"
#include "menu_audioplayer.h"
#include "menu_youtube.h"

using namespace paf;

extern "C" {
	SCE_USER_MODULE_LIST("app0:module/libScePafPreload.suprx");
}

SceUID g_eventFlagUid;

SceBool g_isPlayerActive = SCE_FALSE;

Plugin *g_empvaPlugin;
ui::Widget *g_root;
ui::Widget *g_rootPage;
ui::Widget *g_player_page;
ui::Widget *g_topText;
graph::Surface *g_commonBgTex = SCE_NULL;
graph::Surface *g_coverBgTex = SCE_NULL;
graph::Surface *g_YtVitaIconTex = SCE_NULL;
graph::Surface *g_YtNetIconTex = SCE_NULL;
ui::BusyIndicator *g_commonBusyInidcator;

graph::Surface *g_texCheckMark;
graph::Surface *g_texTransparent;

job::JobQueue *g_coverJobQueue = SCE_NULL;

menu::audioplayer::Audioplayer *g_currentPlayerInstance = SCE_NULL;
menu::displayfiles::Page *g_currentDispFilePage;
menu::settings::SettingsButtonCB *g_settingsButtonCB;

static const EMPVAUtils::MemState k_ytModeLimit = EMPVAUtils::MemState_Mid;

SceVoid menu::main::PagemodeButtonCB::PagemodeButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Plugin::TemplateInitParam tmpParam;
	rco::Element searchParam;
	menu::settings::Settings *config = menu::settings::Settings::GetInstance();
	SceUInt32 currentPagemode = EMPVAUtils::GetPagemode();
	effect::EffectType animMode = effect::EffectType_SlideFromBottom1;

	if (eventId == 0) {
		currentPagemode = menu::settings::Settings::PageMode_Normal;
		animMode = effect::EffectType_Reset;
	}

	if (currentPagemode == menu::settings::Settings::PageMode_Normal) {

		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS)) {
			if (EMPVAUtils::GetMemStatus() > k_ytModeLimit) {
				menu::youtube::Base::FirstTimeInit();
			}
		}

		if (g_currentDispFilePage->coverState) {
			g_currentDispFilePage->coverLoader = new menu::displayfiles::CoverLoaderJob("EMPVA::CoverLoaderJob");
			g_currentDispFilePage->coverLoader->workPage = g_currentDispFilePage;
			g_currentDispFilePage->coverLoader->workFile = SCE_NULL;

			SharedPtr<job::JobItem> itemParam(g_currentDispFilePage->coverLoader);

			g_coverJobQueue->Enqueue(&itemParam);
		}

		menu::displayfiles::Page::ResetBgPlaneTex();

		if (g_currentDispFilePage != SCE_NULL) {
			if (g_currentDispFilePage->prev != SCE_NULL) {
				g_currentDispFilePage->prev->root->PlayEffectReverse(0.0f, animMode);
				if (g_currentDispFilePage->prev->root->animationStatus & 0x80)
					g_currentDispFilePage->prev->root->animationStatus &= ~0x80;
			}
			g_currentDispFilePage->root->PlayEffectReverse(0.0f, animMode);
			if (g_currentDispFilePage->root->animationStatus & 0x80)
				g_currentDispFilePage->root->animationStatus &= ~0x80;
		}

		menu::youtube::Base::InitCommon();

		menu::youtube::Base::InitSearch();

		self->SetSurfaceBase(&g_YtVitaIconTex);
		config->last_pagemode = menu::settings::Settings::PageMode_YouTube;
		config->GetAppSetInstance()->SetInt("last_pagemode", config->last_pagemode);
		EMPVAUtils::SetPagemode(config->last_pagemode);
	}
	else {

		if (g_currentDispFilePage->coverState) {

			g_currentDispFilePage->coverLoader = new menu::displayfiles::CoverLoaderJob("EMPVA::CoverLoaderJob");
			g_currentDispFilePage->coverLoader->workPage = g_currentDispFilePage;
			g_currentDispFilePage->coverLoader->workFile = g_currentDispFilePage->coverWork;

			SharedPtr<job::JobItem> itemParam(g_currentDispFilePage->coverLoader);

			g_coverJobQueue->Enqueue(&itemParam);
		}

		menu::youtube::Base::TermCurrentMode();

		menu::youtube::Base::TermCommon();

		if (g_currentDispFilePage != SCE_NULL) {
			if (g_currentDispFilePage->prev != SCE_NULL) {
				g_currentDispFilePage->prev->root->PlayEffect(0.0f, effect::EffectType_SlideFromBottom1);
				if (g_currentDispFilePage->prev->root->animationStatus & 0x80)
					g_currentDispFilePage->prev->root->animationStatus &= ~0x80;
				g_currentDispFilePage->prev->root->SetAlpha(0.3f);
			}
			g_currentDispFilePage->root->PlayEffect(0.0f, effect::EffectType_SlideFromBottom1);
			if (g_currentDispFilePage->root->animationStatus & 0x80)
				g_currentDispFilePage->root->animationStatus &= ~0x80;
		}

		self->SetSurfaceBase(&g_YtNetIconTex);
		config->last_pagemode = menu::settings::Settings::PageMode_Normal;
		config->GetAppSetInstance()->SetInt("last_pagemode", config->last_pagemode);
		EMPVAUtils::SetPagemode(config->last_pagemode);
	}
}

SceVoid pluginLoadCB(Plugin *plugin)
{
	if (plugin == SCE_NULL) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] Plugin load FAIL!\n");
		return;
	}

	g_empvaPlugin = plugin;

	ui::Widget *buttonPagemode = SCE_NULL;
	rco::Element searchParam;
	Plugin::PageInitParam rwiParam;
	Plugin::TemplateInitParam tmpParam;
	string initCwd;
	SceUInt32 pagemode = menu::settings::Settings::PageMode_Normal;

	new menu::settings::Settings();
	menu::settings::Settings *config = menu::settings::Settings::GetInstance();
	config->GetLastDirectory(&initCwd);
	pagemode = config->last_pagemode;

	SCE_DBG_LOG_DEBUG("[EMPVA_DEBUG] pagemode set to %u\n", pagemode);

	if (pagemode == menu::settings::Settings::PageMode_YouTube) {
		if (EMPVAUtils::GetMemStatus() <= k_ytModeLimit) {
			SCE_DBG_LOG_DEBUG("[EMPVA_DEBUG] Failed to grow memory: resetting pagemode to NORMAL\n");
			pagemode = menu::settings::Settings::PageMode_Normal;
		}
	}

	EMPVAUtils::SetPagemode(pagemode);

	if (EMPVAUtils::GetMemStatus() > k_ytModeLimit) {
		searchParam.hash = EMPVAUtils::GetHash("tex_yt_icon_vita");
		Plugin::GetTexture(&g_YtVitaIconTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_yt_icon_net");
		Plugin::GetTexture(&g_YtNetIconTex, g_empvaPlugin, &searchParam);
	}

	job::JobQueue::Option queueOpt;
	queueOpt.workerNum = 1;
	queueOpt.workerOpt = SCE_NULL;
	queueOpt.workerPriority = SCE_KERNEL_HIGHEST_PRIORITY_USER + 20;
	queueOpt.workerStackSize = SCE_KERNEL_16KiB;

	g_coverJobQueue = new job::JobQueue("EMPVA::CoverLoaderJobQueue", &queueOpt);

	if (EMPVAUtils::GetMemStatus() == EMPVAUtils::MemState_Full) {
		searchParam.hash = EMPVAUtils::GetHash("tex_common_bg_full");
		Plugin::GetTexture(&g_commonBgTex, g_empvaPlugin, &searchParam);
		searchParam.hash = EMPVAUtils::GetHash("tex_common_bg");
		Plugin::GetTexture(&g_coverBgTex, g_empvaPlugin, &searchParam);
	}
	else {
		searchParam.hash = EMPVAUtils::GetHash("tex_common_bg");
		Plugin::GetTexture(&g_commonBgTex, g_empvaPlugin, &searchParam);
		g_coverBgTex = g_commonBgTex;
	}

	searchParam.hash = EMPVAUtils::GetHash("page_common");
	g_rootPage = g_empvaPlugin->PageOpen(&searchParam, &rwiParam);

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_common");
	g_commonBusyInidcator = (ui::BusyIndicator *)g_rootPage->GetChild(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("plane_common_bg");
	g_root = g_rootPage->GetChild(&searchParam, 0);
	g_root->SetSurfaceBase(&g_commonBgTex);

	searchParam.hash = EMPVAUtils::GetHash("text_top_title");
	g_topText = g_rootPage->GetChild(&searchParam, 0);

	Plugin *CRplugin = Plugin::Find("__system__common_resource");

	searchParam.hash = EMPVAUtils::GetHash("_common_texture_check_mark");
	Plugin::GetTexture(&g_texCheckMark, CRplugin, &searchParam);

	searchParam.hash = EMPVAUtils::GetHash("_common_texture_transparent");
	Plugin::GetTexture(&g_texTransparent, CRplugin, &searchParam);

	if (EMPVAUtils::GetMemStatus() > k_ytModeLimit) {

		searchParam.hash = EMPVAUtils::GetHash("yt_menu_template_corner_switch");
		g_empvaPlugin->TemplateOpen(g_rootPage, &searchParam, &tmpParam);

		searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
		buttonPagemode = g_rootPage->GetChild(&searchParam, 0);

		buttonPagemode->RegisterEventCallback(0x10000008, new menu::main::PagemodeButtonCB(), 0);
	}
	else {

		Vector4 topTextSize;
		topTextSize.x = 920.0f;
		topTextSize.y = 0.0f;
		topTextSize.z = 0.0f;
		topTextSize.w = 0.0f;

		g_topText->SetSize(&topTextSize);
	}

	menu::displayfiles::Page::Init();

	new menu::displayfiles::Page(initCwd.c_str());

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_settings_button");
	ui::Widget *settingsButton = g_rootPage->GetChild(&searchParam, 0);
	g_settingsButtonCB = new menu::settings::SettingsButtonCB();
	settingsButton->RegisterEventCallback(0x10000008, g_settingsButtonCB, 0);
	g_settingsButtonCB->pUserData = sce_paf_malloc(sizeof(SceUInt32));
	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;

	if (pagemode == menu::settings::Settings::PageMode_YouTube) {
		menu::main::PagemodeButtonCB::PagemodeButtonCBFun(0, buttonPagemode, 0, SCE_NULL);
	}

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
	ui::Widget *backButton = g_rootPage->GetChild(&searchParam, 0);
	auto backButtonCB = new menu::displayfiles::BackButtonCB();
	backButton->RegisterEventCallback(0x10000008, backButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_player_button");
	ui::Widget *playerButton = g_rootPage->GetChild(&searchParam, 0);
	auto playerButtonCB = new menu::displayfiles::PlayerButtonCB();
	playerButton->RegisterEventCallback(0x10000008, playerButtonCB, 0);
	playerButton->PlayEffectReverse(0.0f, effect::EffectType_Reset);
	playerButton->SetDirectKey(0x80); //square

	sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
}

int main()
{

	sceShellUtilInitEvents(0);
	sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

	EMPVAUtils::SetMemStatus();

	Framework::InitParam fwParam;
	fwParam.LoadDefaultParams();
	fwParam.applicationMode = Framework::Mode_Application;
	//fwParam.optionalFeatureFlags = Framework::InitParam::FeatureFlag_DisableInternalCallbackChecks;

	if (EMPVAUtils::GetMemStatus() == EMPVAUtils::MemState_Full) {
		fwParam.defaultSurfacePoolSize = 18 * 1024 * 1024;
		fwParam.textSurfaceCacheSize = 2 * 1024 * 1024;
	}
	else if (EMPVAUtils::GetMemStatus() == EMPVAUtils::MemState_Mid) {
		fwParam.defaultSurfacePoolSize = 11 * 1024 * 1024;
		fwParam.textSurfaceCacheSize = 2 * 1024 * 1024;
	}
	else {
		fwParam.defaultSurfacePoolSize = 4 * 1024 * 1024;
		fwParam.textSurfaceCacheSize = 1 * 1024 * 1024;
	}

	fwParam.graphMemSystemHeapSize = 512 * 1024;
	//fwParam.graphicsFlags = 7;

	Framework *fw = new Framework(fwParam);

	fw->LoadCommonResourceAsync();

#ifdef _DEBUG
	sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_TRACE);
	InitDebug();
#endif

	SceAppUtilInitParam init;
	SceAppUtilBootParam boot;
	sce_paf_memset(&init, 0, sizeof(SceAppUtilInitParam));
	sce_paf_memset(&boot, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init, &boot);

	EMPVAUtils::Init();

	//Reset repeat state
	sceMusicPlayerServiceInitialize(0);
	sceMusicPlayerServiceSetRepeatMode(SCE_MUSIC_REPEAT_DISABLE);
	sceMusicPlayerServiceTerminate();

	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOCODEC);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BXCE);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG);
	new Module("app0:module/libLocalMedia.suprx", 0, 0, 0);

	g_eventFlagUid = sceKernelCreateEventFlag("EMPVA::GlobalEvf", SCE_KERNEL_ATTR_MULTI, FLAG_ELEVENMPVA_IS_FG | FLAG_ELEVENMPVA_IS_DECODER_USED, SCE_NULL);

	Plugin::InitParam pluginParam;

	pluginParam.pluginName = "empva_plugin";
	pluginParam.resourcePath = "app0:empva_plugin.rco";
	pluginParam.scopeName = "__main__";

	pluginParam.pluginStartCB = pluginLoadCB;

	fw->LoadPluginAsync(&pluginParam);

	fw->EnterRenderingLoop();

	sceKernelExitProcess(0);

	return 0;
}
