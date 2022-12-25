#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <message_dialog.h>
#include <libsysmodule.h>
#include <libdbg.h>
#include <net.h>
#include <libnetctl.h>
#include <libhttp.h>
#include <shellsvc.h>
#include <paf.h>
#include <curl_file.h>

#include "common.h"
#include "main.h"
#include "menu_youtube.h"
#include "menu_audioplayer.h"
#include "dialog.h"
#include "utils.h"
#include "yt_utils.h"

using namespace paf;

static SceUInt32 s_currentYtMode = menu::youtube::Base::Mode_Search;
static menu::youtube::Base::NetCheckThread *s_netCheckThread;
static SceInt32 s_netCtlStateCbId = SCE_UID_INVALID_UID;

SceVoid menu::youtube::Base::netCtlStateCB(SceInt32 event_type, ScePVoid arg)
{
	s_netCheckThread = new NetCheckThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_16KiB, "EMPVA::NetCheckThread");
	s_netCheckThread->Start();
}

SceVoid menu::youtube::Base::NetCheckThread::EntryFunction()
{
	rco::Element searchParam;
	ui::Widget *button;
	CurlFile::OpenArg testHttp;
	CurlFile testFile;
	SceInt32 ret = SCE_NET_CTL_ERROR_NOT_AVAIL;

	//sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	
	Dialog::OpenPleaseWait(g_empvaPlugin, SCE_NULL, EMPVAUtils::GetString("msg_wait"));

	testHttp.SetUrl("https://www.youtube.com/s/desktop/b75d77f8/img/favicon_32x32.png");
	testHttp.SetOpt(10000000, CurlFile::OpenArg::Opt_ResolveTimeOut);
	testHttp.SetOpt(10000000, CurlFile::OpenArg::Opt_ConnectTimeOut);
	testHttp.SetShare(YTUtils::GetCurlFileShare());

	ret = testFile.Open(&testHttp);
	if (ret == SCE_OK)
		testFile.Close();

	Dialog::Close();

	if (ret != SCE_OK) {

		Dialog::OpenError(g_empvaPlugin, ret, EMPVAUtils::GetString("msg_error_server_peer_connect_timeout"));
		Dialog::WaitEnd();

		searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
		button = g_rootPage->GetChild(&searchParam, 0);

		menu::main::PagemodeButtonCB::PagemodeButtonCBFun(1, button, 0, SCE_NULL);
	}
	else {
		if (s_netCtlStateCbId == SCE_UID_INVALID_UID)
			sceNetCtlInetRegisterCallback(netCtlStateCB, SCE_NULL, &s_netCtlStateCbId);
	}

	//sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	Cancel();
}

SceVoid menu::youtube::VideoButtonCB::VideoButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	rco::Element searchParam;
	wstring text16;
	string text8;
	const char *idptr;

	VideoButtonCB *thisCb = (VideoButtonCB *)pUserData;

	idptr = thisCb->url.c_str();

	if (thisCb->mode != menu::youtube::Base::Mode_History)
		YTUtils::GetHistLog()->Add(idptr);

	new menu::audioplayer::Audioplayer::Audioplayer(idptr, SCE_NULL, menu::audioplayer::Audioplayer::Mode_Youtube);
}

SceVoid menu::youtube::SearchActionButtonCB::SearchActionButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	if (eventId == 0x1000000B)
	{
		ui::TextBox *tb = (ui::TextBox *)self;
		tb->Hide();
	}

	switch (s_currentYtMode) {
	case menu::youtube::Base::Mode_Search:
		menu::youtube::SearchPage::SearchActionButtonOp();
		break;
	case menu::youtube::Base::Mode_Fav:
		menu::youtube::FavPage::SearchActionButtonOp();
		break;
	}
}

SceVoid menu::youtube::Base::InitCommon()
{
	rco::Element searchParam;
	ui::Widget *btMenu;
	ui::Widget *commonWidget;

	s_netCheckThread = new NetCheckThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::NetCheckJob");
	s_netCheckThread->Start();

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_bottommenu");
	btMenu = g_rootPage->GetChild(&searchParam, 0);
	btMenu->PlayEffect(0.0f, effect::EffectType_SlideFromBottom1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Reset);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Reset);

	if (g_currentDispFilePage->prev != SCE_NULL) {
		searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
		commonWidget = g_rootPage->GetChild(&searchParam, 0);

		commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Reset);
	}

	g_topText->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);

	YTUtils::Init();
}

SceVoid menu::youtube::Base::TermCommon()
{
	rco::Element searchParam;
	ui::Widget *commonWidget;

	if (s_netCtlStateCbId != SCE_UID_INVALID_UID) {
		sceNetCtlInetUnregisterCallback(s_netCtlStateCbId);
		s_netCtlStateCbId = SCE_UID_INVALID_UID;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_bottommenu");
	commonWidget = g_rootPage->GetChild(&searchParam, 0);
	commonWidget->PlayEffectReverse(0.0f, effect::EffectType_SlideFromBottom1);

	if (g_currentDispFilePage->prev != SCE_NULL) {
		searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
		commonWidget = g_rootPage->GetChild(&searchParam, 0);

		commonWidget->PlayEffect(0.0f, effect::EffectType_Reset);
	}

	g_topText->PlayEffect(0.0f, effect::EffectType_Fadein1);

	YTUtils::Term();

	s_netCheckThread->Join();
	delete s_netCheckThread;

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;
}

SceVoid menu::youtube::Base::InitSearch()
{
	rco::Element searchParam;
	ui::Widget *topTitleBar;

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
	topTitleBar = g_rootPage->GetChild(&searchParam, 0);
	topTitleBar->PlayEffect(0.0f, effect::EffectType_Fadein1);

	s_currentYtMode = menu::youtube::Base::Mode_Search;
}

SceVoid menu::youtube::Base::InitHistory()
{
	s_currentYtMode = menu::youtube::Base::Mode_History;
}

SceVoid menu::youtube::Base::InitFav()
{
	rco::Element searchParam;
	ui::Widget *topTitleBar;

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
	topTitleBar = g_rootPage->GetChild(&searchParam, 0);
	topTitleBar->PlayEffect(0.0f, effect::EffectType_Fadein1);

	s_currentYtMode = menu::youtube::Base::Mode_Fav;
}

SceVoid menu::youtube::Base::TermCurrentMode()
{
	rco::Element searchParam;
	ui::Widget *commonWidget;

	switch (s_currentYtMode) {
	case menu::youtube::Base::Mode_Search:

		menu::youtube::SearchPage::TermOp();

		searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
		commonWidget = g_rootPage->GetChild(&searchParam, 0);
		commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);

		break;
	case menu::youtube::Base::Mode_History:
		menu::youtube::HistoryPage::TermOp();
		break;
	case menu::youtube::Base::Mode_Fav:

		menu::youtube::FavPage::TermOp();

		searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
		commonWidget = g_rootPage->GetChild(&searchParam, 0);
		commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);

		break;
	}
}

SceVoid menu::youtube::SearchButtonCB::SearchButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	if (s_currentYtMode == menu::youtube::Base::Mode_Search)
		return;

	Base::TermCurrentMode();

	Base::InitSearch();
}

SceVoid menu::youtube::HistoryButtonCB::HistoryButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	if (s_currentYtMode == menu::youtube::Base::Mode_History)
		return;

	Base::TermCurrentMode();

	Base::InitHistory();

	new HistoryPage();
}

SceVoid menu::youtube::FavButtonCB::FavButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	if (s_currentYtMode == menu::youtube::Base::Mode_Fav)
		return;

	Base::TermCurrentMode();

	Base::InitFav();

	new FavPage();
}

SceVoid menu::youtube::LeftButtonCB::LeftButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	switch (s_currentYtMode) {
	case menu::youtube::Base::Mode_Search:
		menu::youtube::SearchPage::LeftButtonOp();
		break;
	}
}

SceVoid menu::youtube::RightButtonCB::RightButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	switch (s_currentYtMode) {
	case menu::youtube::Base::Mode_Search:
		menu::youtube::SearchPage::RightButtonOp();
		break;
	}
}

SceUInt32 menu::youtube::Base::GetCurrentMode()
{
	return s_currentYtMode;
}

SceVoid menu::youtube::Base::FirstTimeInit()
{
	rco::Element searchParam;
	Plugin::TemplateOpenParam tmpParam;
	ui::Widget *commonWidget;
	ui::Widget *ytTopPlane;
	ui::Widget *btMenu;

	InitYtStuff();

	searchParam.hash = EMPVAUtils::GetHash("yt_menu_template_base");
	g_empvaPlugin->TemplateOpen(g_rootPage, &searchParam, &tmpParam);

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
	ytTopPlane = g_rootPage->GetChild(&searchParam, 0);
	ytTopPlane->PlayEffectReverse(0.0f, effect::EffectType_Reset);

	searchParam.hash = EMPVAUtils::GetHash("yt_image_button_top_search");
	commonWidget = ytTopPlane->GetChild(&searchParam, 0);
	auto searchActionButtonCB = new menu::youtube::SearchActionButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, searchActionButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_box_top_search");
	commonWidget = ytTopPlane->GetChild(&searchParam, 0);
	searchActionButtonCB = new menu::youtube::SearchActionButtonCB();
	commonWidget->RegisterEventCallback(0x1000000B, searchActionButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_bottommenu");
	btMenu = g_rootPage->GetChild(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	auto leftButtonCB = new menu::youtube::LeftButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, leftButtonCB, 0);
	commonWidget->SetDirectKey(SCE_CTRL_L1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_search");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	auto searchButtonCB = new menu::youtube::SearchButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, searchButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_history");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	auto historyButtonCB = new menu::youtube::HistoryButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, historyButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_favourite");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	auto favButtonCB = new menu::youtube::FavButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, favButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = btMenu->GetChild(&searchParam, 0);
	auto rightButtonCB = new menu::youtube::RightButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, rightButtonCB, 0);
	commonWidget->SetDirectKey(SCE_CTRL_R1);
}

SceInt32 menu::youtube::Base::InitYtStuff()
{
	SceNetInitParam param;
	SceInt32 ret = 0;
	SceInt32 quality = 0;

	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceSysmoduleLoadModule(SCE_SYSMODULE_FIBER);

	new Module("app0:module/libcurl.suprx", 0, 0, 0);
	new Module("app0:module/libInvidious.suprx", 0, 0, 0);
	new Module("app0:module/libNetMedia.suprx", 0, 0, 0);

	/* curl */
	curl_global_memmanager_set_np(sce_paf_malloc, sce_paf_free, sce_paf_realloc);

	/* libnet */
	param.memory = sce_paf_malloc(k_netMemSize);
	param.size = k_netMemSize;
	param.flags = 0;
	ret = sceNetInit(&param);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] sceNetInit() error: 0x%08X\n", ret);
	}

	/* libnetctl */
	ret = sceNetCtlInit();
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] sceNetCtlInit() error: 0x%08X\n", ret);
	}

	ret = invInit(sce_paf_malloc, sce_paf_free, YTUtils::InvDownload);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] invInit() error: 0x%08X\n", ret);
	}

	return SCE_OK;
}