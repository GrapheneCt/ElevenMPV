#include <appmgr.h>
#include <kernel.h>
#include <shellsvc.h>
#include <libsysmodule.h>
#include <libdbg.h>
#include <shellaudio.h>
#include <paf.h>

#include "common.h"
#include "config.h"
#include "utils.h"
#include "menu_displayfiles.h"
#include "menu_settings.h"
#include "menu_audioplayer.h"

using namespace paf;

extern "C" {
	SCE_USER_MODULE_LIST("app0:module/libScePafPreload.suprx");

	unsigned int sceLibcHeapSize = 6 * 1024 * 1024;
}

SceUID g_eventFlagUid;

SceBool g_isPlayerActive = SCE_FALSE;

Plugin *g_empvaPlugin;
widget::Widget *g_root;
widget::Widget *g_root_page;
widget::Widget *g_settings_page;
widget::Widget *g_player_page;
widget::Widget *g_settings_option;
widget::Widget *g_top_text;
graphics::Texture *g_commonBgTex = SCE_NULL;
graphics::Texture *g_coverBgTex = SCE_NULL;
widget::BusyIndicator *g_commonBusyInidcator;
widget::Widget *g_commonOptionDialog;

graphics::Texture *g_texCheckMark;
graphics::Texture *g_texTransparent;

menu::audioplayer::Audioplayer *g_currentPlayerInstance = SCE_NULL;
menu::displayfiles::Page *g_currentDispFilePage;
menu::settings::SettingsButtonCB *g_settingsButtonCB;
config::Config *g_config;

static SceInt32 s_memGrown = 0;

SceVoid getMemStatus()
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
			s_memGrown = 1;
		else
			s_memGrown = 2;
	}

	SCE_DBG_LOG_DEBUG("[EMPVA_DEBUG] Memory grow state: %u\n", s_memGrown);
}

SceVoid pluginLoadCB(Plugin *plugin)
{
	if (plugin == SCE_NULL) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] Plugin load FAIL!\n");
		return;
	}

	g_empvaPlugin = plugin;

	Resource::Element searchParam;
	Plugin::SceneInitParam rwiParam;
	String initCwd;

	g_config = new config::Config();
	g_config->GetLastDirectory(&initCwd);

	g_commonBgTex = new graphics::Texture();

	if (s_memGrown == 2) {
		g_coverBgTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_common_bg_full");
		Plugin::LoadTexture(g_commonBgTex, plugin, &searchParam);
		searchParam.hash = EMPVAUtils::GetHash("tex_common_bg");
		Plugin::LoadTexture(g_coverBgTex, g_empvaPlugin, &searchParam);
	}
	else {
		searchParam.hash = EMPVAUtils::GetHash("tex_common_bg");
		Plugin::LoadTexture(g_commonBgTex, plugin, &searchParam);
		g_coverBgTex = g_commonBgTex;
	}

	searchParam.hash = EMPVAUtils::GetHash("page_common");
	g_root_page = g_empvaPlugin->CreateScene(&searchParam, &rwiParam);

	searchParam.hash = EMPVAUtils::GetHash("page_settings_option");
	g_settings_option = g_empvaPlugin->CreateScene(&searchParam, &rwiParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_settings_dialog_bg");
	g_commonOptionDialog = g_settings_option->GetChildByHash(&searchParam, 0);
	g_commonOptionDialog->PlayAnimationReverse(0.0f, widget::Widget::Animation_Reset, SCE_NULL);

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_common");
	g_commonBusyInidcator = (widget::BusyIndicator *)g_root_page->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("plane_common_bg");
	g_root = g_root_page->GetChildByHash(&searchParam, 0);
	g_root->SetTextureBase(g_commonBgTex);

	searchParam.hash = EMPVAUtils::GetHash("text_top_title");
	g_top_text = g_root_page->GetChildByHash(&searchParam, 0);

	g_texCheckMark = new graphics::Texture();
	searchParam.hash = EMPVAUtils::GetHash("_common_texture_check_mark");
	Plugin::LoadTexture(g_texCheckMark, Plugin::Find("__system__common_resource"), &searchParam);

	g_texTransparent = new graphics::Texture();
	searchParam.hash = EMPVAUtils::GetHash("_common_texture_transparent");
	Plugin::LoadTexture(g_texTransparent, Plugin::Find("__system__common_resource"), &searchParam);

	menu::displayfiles::Page::Init();
	new menu::displayfiles::Page(initCwd.data);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
	widget::Widget *backButton = g_root_page->GetChildByHash(&searchParam, 0);
	auto backButtonCB = new menu::displayfiles::BackButtonCB();
	backButton->RegisterEventCallback(0x10000008, backButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_settings_button");
	widget::Widget *settingsButton = g_root_page->GetChildByHash(&searchParam, 0);
	g_settingsButtonCB = new menu::settings::SettingsButtonCB();
	settingsButton->RegisterEventCallback(0x10000008, g_settingsButtonCB, 0);
	g_settingsButtonCB->pUserData = sce_paf_malloc(sizeof(SceUInt32));
	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;
}

#ifdef _DEBUG

static SceInt32 s_oldMemSize = 0;

SceVoid leakTestTask(ScePVoid pUserData)
{
	SceInt32 memsize = 0;
	Allocator *glAlloc = Allocator::GetGlobalAllocator();
	SceInt32 sz = glAlloc->GetFreeSize();
	String *str = new String();
	str->MemsizeFormat(sz);
	sceClibPrintf("[EMPVA_DEBUG] Free heap memory: %s\n", str->data);
	memsize = sz;
	SceInt32 delta = s_oldMemSize - memsize;
	delta = -delta;
	if (delta) {
		sceClibPrintf("[EMPVA_DEBUG] Memory delta: %d bytes\n", delta);
	}
	s_oldMemSize = sz;
	delete str;
}
#endif

int main() {

	SceInt32 ret = -1;

#ifdef _DEBUG
	sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_TRACE);
#else
	sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_ERROR);
#endif

	getMemStatus();

	Framework::InitParam fwParam;
	fwParam.LoadDefaultParams();
	fwParam.applicationMode = Framework::Mode_ApplicationA;
	//fwParam.optionalFeatureFlags = Framework::InitParam::FeatureFlag_DisableInternalCallbackChecks;

	if (s_memGrown == 2) {
		fwParam.defaultSurfacePoolSize = 17 * 1024 * 1024 + 512 * 1024;
		fwParam.textSurfaceCacheSize = 2 * 1024 * 1024;
	}
	else if (s_memGrown == 1) {
		fwParam.defaultSurfacePoolSize = 11 * 1024 * 1024 + 512 * 1024;
		fwParam.textSurfaceCacheSize = 2 * 1024 * 1024;
	}
	else {
		fwParam.defaultSurfacePoolSize = 4 * 1024 * 1024;
		fwParam.textSurfaceCacheSize = 1 * 1024 * 1024;
	}

	fwParam.graphMemSystemHeapSize = 512 * 1024;
	//fwParam.graphicsFlags = 7;

	Framework *fw = new Framework(&fwParam);

	fw->LoadCommonResourceAsync();

	SceAppUtilInitParam init;
	SceAppUtilBootParam boot;
	sce_paf_memset(&init, 0, sizeof(SceAppUtilInitParam));
	sce_paf_memset(&boot, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init, &boot);

	EMPVAUtils::Init();

#ifdef _DEBUG
	sceAppMgrSetInfobarState(SCE_TRUE, 0, 0); // In .sfo for release
	//common::Utils::AddMainThreadTask(leakTestTask, SCE_NULL);
#endif

	//Reset repeat state
	sceMusicPlayerServiceInitialize(0);
	sceMusicPlayerServiceSetRepeatMode(SCE_MUSIC_REPEAT_DISABLE);
	sceMusicPlayerServiceTerminate();

	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOCODEC);

	g_eventFlagUid = sceKernelCreateEventFlag("EMPVA::GlobalEvf", SCE_KERNEL_ATTR_MULTI, FLAG_ELEVENMPVA_IS_FG | FLAG_ELEVENMPVA_IS_DECODER_USED, SCE_NULL);

	Framework::PluginInitParam pluginParam;

	pluginParam.pluginName.Set("empva_plugin");
	pluginParam.resourcePath.Set("app0:empva_plugin.rco");
	pluginParam.scopeName.Set("__main__");

	pluginParam.pluginStartCB = pluginLoadCB;

	fw->LoadPluginAsync(&pluginParam);

	fw->EnterRenderingLoop();

	return 0;
}
