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

#include "common.h"
#include "main.h"
#include "menu_youtube.h"
#include "menu_audioplayer.h"
#include "utils.h"
#include "yt_utils.h"
#include "youtube_parser.hpp"

using namespace paf;

static SceUInt32 s_currentYtMode = menu::youtube::Base::Mode_Search;
static menu::youtube::Base::NetCheckThread *s_netCheckThread;
static SceInt32 s_netCtlStateCbId = SCE_UID_INVALID_UID;

SceVoid menu::youtube::Base::netCtlStateCB(SceInt32 event_type, ScePVoid arg)
{
	s_netCheckThread = new NetCheckThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_4KiB, "EMPVA::NetCheckThread");
	s_netCheckThread->Start();
}

SceVoid menu::youtube::Base::NetCheckThread::EntryFunction()
{
	Resource::Element searchParam;
	ui::Widget *button;
	HttpFile::Param testHttp;
	HttpFile testFile;
	String text8;
	WString text16;
	SceMsgDialogParam				msgParam;
	SceMsgDialogSystemMessageParam	sysMsgParam;
	SceMsgDialogUserMessageParam userMsgParam;
	SceCommonDialogStatus status;
	SceInt32 ret = -1;

	sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	sceMsgDialogParamInit(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_SYSTEM_MSG;

	sce_paf_memset(&sysMsgParam, 0, sizeof(SceMsgDialogSystemMessageParam));
	msgParam.sysMsgParam = &sysMsgParam;
	msgParam.sysMsgParam->sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL;

	sceMsgDialogInit(&msgParam);
	testHttp.SetUrl("https://www.youtube.com/s/desktop/b75d77f8/img/favicon_32x32.png");
	testHttp.SetOpt(10000000, HttpFile::Param::SCE_PAF_HTTP_FILE_PARAM_RESOLVE_TIME_OUT);
	testHttp.SetOpt(10000000, HttpFile::Param::SCE_PAF_HTTP_FILE_PARAM_CONNECT_TIME_OUT);

	ret = testFile.Open(&testHttp);

	if (ret == SCE_OK)
		testFile.Close();

	status = sceMsgDialogGetStatus();

	while (status != SCE_COMMON_DIALOG_STATUS_RUNNING) {
		status = sceMsgDialogGetStatus();
		thread::Sleep(10);
	}

	sceMsgDialogClose();

	while (status != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		status = sceMsgDialogGetStatus();
		thread::Sleep(10);
	}

	sceMsgDialogTerm();

	if (ret != SCE_OK) {

		if (ret == SCE_HTTP_ERROR_SSL) {

			msgParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
			msgParam.sysMsgParam = SCE_NULL;

			sce_paf_memset(&userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
			msgParam.userMsgParam = &userMsgParam;
			msgParam.userMsgParam->buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
			searchParam.hash = EMPVAUtils::GetHash("msg_netcheck_fail");
			text16 = (wchar_t *)g_empvaPlugin->GetString(&searchParam);
			text16.ToString(&text8);
			msgParam.userMsgParam->msg = (const SceChar8 *)text8.data;
		}
		else {
			msgParam.sysMsgParam->sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION;
		}

		sceMsgDialogInit(&msgParam);

		status = sceMsgDialogGetStatus();

		while (status != SCE_COMMON_DIALOG_STATUS_FINISHED) {
			status = sceMsgDialogGetStatus();
			thread::Sleep(100);
		}

		sceMsgDialogTerm();

		searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
		button = g_rootPage->GetChildByHash(&searchParam, 0);

		menu::main::PagemodeButtonCB::PagemodeButtonCBFun(1, button, 0, SCE_NULL);
	}
	else {
		if (s_netCtlStateCbId == SCE_UID_INVALID_UID)
			sceNetCtlInetRegisterCallback(netCtlStateCB, SCE_NULL, &s_netCtlStateCbId);
	}

	sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
	sceKernelExitDeleteThread(0);
}

SceVoid menu::youtube::VideoButtonCB::VideoButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	YouTubeVideoDetail *vidInfo;
	Resource::Element searchParam;
	ui::Widget *videolink;
	WString text16;
	String text8;
	char *idptr;
	char *listptr;

	VideoButtonCB *thisCb = (VideoButtonCB *)pUserData;

	idptr = sce_paf_strchr(thisCb->url.data, '=');
	idptr += 1;

	if (thisCb->mode != menu::youtube::Base::Mode_History)
		YTUtils::GetHistLog()->Add(idptr);

	new menu::audioplayer::Audioplayer::Audioplayer(idptr, SCE_NULL, menu::audioplayer::Audioplayer::Mode_Youtube);
}

SceVoid menu::youtube::SearchActionButtonCB::SearchActionButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
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
	Resource::Element searchParam;
	ui::Widget *btMenu;
	ui::Widget *commonWidget;

	s_netCheckThread = new NetCheckThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_4KiB, "EMPVA::NetCheckJob");
	s_netCheckThread->Start();

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_bottommenu");
	btMenu = g_rootPage->GetChildByHash(&searchParam, 0);
	btMenu->PlayAnimation(0.0f, ui::Widget::Animation_SlideFromBottom1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	if (g_currentDispFilePage->prev != SCE_NULL) {
		searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);

		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
	}

	g_topText->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

	YTUtils::Init();
}

SceVoid menu::youtube::Base::TermCommon()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	if (s_netCtlStateCbId != SCE_UID_INVALID_UID) {
		sceNetCtlInetUnregisterCallback(s_netCtlStateCbId);
		s_netCtlStateCbId = SCE_UID_INVALID_UID;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_bottommenu");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_SlideFromBottom1);

	if (g_currentDispFilePage->prev != SCE_NULL) {
		searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);

		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
	}

	g_topText->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);

	YTUtils::Term();

	s_netCheckThread->Join();
	delete s_netCheckThread;

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;
}

SceVoid menu::youtube::Base::InitSearch()
{
	Resource::Element searchParam;
	ui::Widget *topTitleBar;

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
	topTitleBar = g_rootPage->GetChildByHash(&searchParam, 0);
	topTitleBar->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);

	s_currentYtMode = menu::youtube::Base::Mode_Search;
}

SceVoid menu::youtube::Base::InitHistory()
{
	s_currentYtMode = menu::youtube::Base::Mode_History;
}

SceVoid menu::youtube::Base::InitFav()
{
	Resource::Element searchParam;
	ui::Widget *topTitleBar;

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
	topTitleBar = g_rootPage->GetChildByHash(&searchParam, 0);
	topTitleBar->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);

	s_currentYtMode = menu::youtube::Base::Mode_Fav;
}

SceVoid menu::youtube::Base::TermCurrentMode()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	switch (s_currentYtMode) {
	case menu::youtube::Base::Mode_Search:

		menu::youtube::SearchPage::TermOp();

		searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

		break;
	case menu::youtube::Base::Mode_History:
		menu::youtube::HistoryPage::TermOp();
		break;
	case menu::youtube::Base::Mode_Fav:

		menu::youtube::FavPage::TermOp();

		searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

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
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	ui::Widget *btMenu;

	InitYtStuff();

	searchParam.hash = EMPVAUtils::GetHash("yt_menu_template_base");
	g_empvaPlugin->TemplateOpen(g_rootPage, &searchParam, &tmpParam);

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_top_search");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	searchParam.hash = EMPVAUtils::GetHash("yt_image_button_top_search");
	commonWidget = commonWidget->GetChildByHash(&searchParam, 0);
	auto searchActionButtonCB = new menu::youtube::SearchActionButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, searchActionButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_plane_bottommenu");
	btMenu = g_rootPage->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	auto leftButtonCB = new menu::youtube::LeftButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, leftButtonCB, 0);
	commonWidget->AssignButton(SCE_CTRL_L1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_search");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	auto searchButtonCB = new menu::youtube::SearchButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, searchButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_history");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	auto historyButtonCB = new menu::youtube::HistoryButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, historyButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_favourite");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	auto favButtonCB = new menu::youtube::FavButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, favButtonCB, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = btMenu->GetChildByHash(&searchParam, 0);
	auto rightButtonCB = new menu::youtube::RightButtonCB();
	commonWidget->RegisterEventCallback(0x10000008, rightButtonCB, 0);
	commonWidget->AssignButton(SCE_CTRL_R1);
}

SceInt32 menu::youtube::Base::InitYtStuff()
{
	SceNetInitParam param;
	SceInt32 ret = 0;

	sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);

	new Module("vs0:sys/external/libfios2.suprx", 0, 0, 0);
	new Module("vs0:sys/external/libc.suprx", 0, 0, 0);
	new Module("app0:module/libThirdTube.suprx", 0, 0, 0);
	new Module("app0:module/libNetMedia.suprx", 0, 0, 0);

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

	ret = sceSslInit(300 * 1024);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] sceSslInit() error: 0x%08X\n", ret);
	}

	ret = sceHttpInit(40 * 1024);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[EMPVA_PLUGIN_BASE] sceHttpInit() error: 0x%08X\n", ret);
	}

	return SCE_OK;
}