#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>
#include <ini_file_processor.h>

#include "common.h"
#include "menu_youtube.h"
#include "menu_audioplayer.h"
#include "utils.h"
#include "yt_utils.h"
#include "youtube_parser.hpp"

using namespace paf;

static menu::youtube::FavPage *s_currentFavPage = SCE_NULL;

SceVoid menu::youtube::FavParserThread::CreateVideoButton(FavPage *page, const char *data, SceUInt32 index, const char *keyWord)
{
	SceInt32 res;
	WString text16;
	String text8;
	YouTubeVideoDetail *vidInfo;
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *box;
	ui::Widget *button;
	ui::Widget *subtext;
	VideoButtonCB *buttonCB;
	ObjectWithCleanup fres;
	graphics::Texture tmbTex;
	char url[256];
	char tmb[256];

	sce_paf_memset(url, 0, sizeof(url));
	sce_paf_memset(tmb, 0, sizeof(tmb));

	youtube_get_video_url_by_id(data, url, sizeof(url));
	vidInfo = youtube_parse_video_page(url);

	if (keyWord) {
		if (!sce_paf_strstr(vidInfo->title.c_str(), keyWord)) {
			youtube_destroy_struct(vidInfo);
			return;
		}
		else {
			EMPVAUtils::EndPleaseWait();
		}
	}

	searchParam.hash = EMPVAUtils::GetHash("youtube_scroll_box");
	box = page->thisPage->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube_result_button");
	g_empvaPlugin->TemplateOpen(box, &searchParam, &tmpParam);

	button = (ui::ImageButton *)box->GetChildByNum(box->childNum - 1);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_button_subtext");
	subtext = button->GetChildByHash(&searchParam, 0);

	text8 = vidInfo->title.c_str();
	text8.ToWString(&text16);
	button->SetLabel(&text16);

	menu::audioplayer::Audioplayer::ConvertSecondsToString(&text8, (SceUInt64)((SceFloat)vidInfo->duration_ms / 1000.0f), SCE_FALSE);
	text8.ToWString(&text16);
	subtext->SetLabel(&text16);

	buttonCB = new VideoButtonCB;
	buttonCB->pUserData = buttonCB;
	buttonCB->mode = menu::youtube::Base::Mode_Fav;
	buttonCB->url = url;
	button->RegisterEventCallback(ui::Widget::EventMain_Decide, buttonCB, 0);

	youtube_get_video_thumbnail_url_by_id(data, tmb, sizeof(tmb));

	youtube_destroy_struct(vidInfo);

	HttpFile::Open(&fres, tmb, &res, 0);
	if (res < 0) {
		return;
	}

	graphics::Texture::CreateFromFile(&tmbTex, g_empvaPlugin->memoryPool, &fres);

	fres.cleanup->cb(fres.object);
	delete fres.cleanup;

	if (tmbTex.texSurface == SCE_NULL) {
		return;
	}

	button->SetTextureBase(&tmbTex);
}

SceVoid menu::youtube::FavParserThread::EntryFunction()
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	SceUInt32 prevResNum = 0;
	SceInt32 parseRes = 0;
	char key[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];

	YTUtils::GetFavLog()->Reset();

	if (prevPage) {
		prevResNum = prevPage->resNum;
		for (SceInt32 i = 0; i < prevResNum; i++) {
			YTUtils::GetFavLog()->GetNext(key);
		}
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube");
	g_empvaPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_youtube_bg");
	workPage->thisPage = (ui::Plane *)g_root->GetChildByHash(&searchParam, 0);
	workPage->thisPage->hash = (SceUInt32)workPage->thisPage;

	if (workPage->prev != SCE_NULL) {
		if (workPage->prev->prev != SCE_NULL) {
			workPage->prev->prev->thisPage->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
			if (workPage->prev->prev->thisPage->animationStatus & 0x80)
				workPage->prev->prev->thisPage->animationStatus &= ~0x80;
		}
		workPage->prev->thisPage->PlayAnimation(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		if (workPage->prev->thisPage->animationStatus & 0x80)
			workPage->prev->thisPage->animationStatus &= ~0x80;
	}
	workPage->thisPage->PlayAnimation(-5000.0f, ui::Widget::Animation_3D_SlideFromFront);
	if (workPage->thisPage->animationStatus & 0x80)
		workPage->thisPage->animationStatus &= ~0x80;

	if (prevPage) {
		searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);
	}

	if (keyWord.length) {
		while (1) {
			YTUtils::GetMenuSema()->Wait();
			if (cancel) {
				YTUtils::GetMenuSema()->Signal();
				sceKernelExitDeleteThread(0);
			}
			parseRes = YTUtils::GetFavLog()->GetNext(key);
			if (parseRes) {
				YTUtils::GetMenuSema()->Signal();
				break;
			}
			CreateVideoButton(workPage, key, 0, keyWord.data);
			workPage->resNum++;
			YTUtils::GetMenuSema()->Signal();
		}
	}
	else {
		for (SceInt32 i = prevResNum; i < menu::youtube::FavPage::k_resPerPage + prevResNum; i++) {
			YTUtils::GetMenuSema()->Wait();
			if (cancel) {
				YTUtils::GetMenuSema()->Signal();
				sceKernelExitDeleteThread(0);
			}
			parseRes = YTUtils::GetFavLog()->GetNext(key);
			if (parseRes) {
				YTUtils::GetMenuSema()->Signal();
				break;
			}
			CreateVideoButton(workPage, key, i, SCE_NULL);
			workPage->resNum++;
			YTUtils::GetMenuSema()->Signal();
		}

		if (!parseRes) {
			searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
			commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
			commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);
		}
	}

	EMPVAUtils::EndPleaseWait();

	sceKernelExitDeleteThread(0);
}

SceVoid menu::youtube::FavPage::SearchActionButtonOp()
{
	Resource::Element searchParam;
	ui::TextBox *searchBox;
	WString text16;
	String text8;

	EMPVAUtils::BeginPleaseWait(SCE_TRUE);

	// In destructor s_current(...)Page->prev is assigned to s_current(...)Page
	while (s_currentFavPage) {
		delete s_currentFavPage;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_text_box_top_search");
	searchBox = (ui::TextBox *)g_rootPage->GetChildByHash(&searchParam, 0);

	searchBox->GetLabel(&text16);

	if (text16.length != 0) {

		text16.ToString(&text8);

		new FavPage(text8.data);
	}
}

// Constructor for first page
menu::youtube::FavPage::FavPage()
{
	prev = SCE_NULL;
	next = SCE_NULL;
	parserThread = SCE_NULL;
	resNum = 0;

	thread::Sleep(100);
	parserThread = new FavParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::YtFavParser");
	parserThread->prevPage = SCE_NULL;
	parserThread->workPage = this;
	parserThread->cancel = SCE_FALSE;
	parserThread->Start();

	s_currentFavPage = this;
}

// Constructor for next page
menu::youtube::FavPage::FavPage(FavPage *prevPage)
{
	prev = prevPage;
	next = SCE_NULL;
	parserThread = SCE_NULL;
	resNum = 0;

	thread::Sleep(100);
	if (prevPage)
		prev->parserThread->Join();

	parserThread = new FavParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::YtFavParser");
	parserThread->prevPage = prevPage;
	parserThread->workPage = this;
	parserThread->cancel = SCE_FALSE;
	parserThread->Start();

	s_currentFavPage = this;
}

// Constructor for search page
menu::youtube::FavPage::FavPage(const char *keyWord)
{
	prev = SCE_NULL;
	next = SCE_NULL;
	parserThread = SCE_NULL;
	resNum = 0;

	parserThread = new FavParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::YtFavParser");
	parserThread->prevPage = SCE_NULL;
	parserThread->workPage = this;
	parserThread->cancel = SCE_FALSE;
	parserThread->keyWord = keyWord;
	parserThread->Start();

	s_currentFavPage = this;
}

menu::youtube::FavPage::~FavPage()
{
	Resource::Element searchParam;
	ui::Widget *box;
	ui::Widget *commonWidget;
	ui::ImageButton *button;
	graphics::Surface *tmpSurf;

	parserThread->cancel = SCE_TRUE;
	parserThread->Join();
	delete parserThread;

	searchParam.hash = EMPVAUtils::GetHash("youtube_scroll_box");
	box = thisPage->GetChildByHash(&searchParam, 0);
	
	for (SceInt32 i = 0; i < box->childNum; i++) {
		button = (ui::ImageButton *)box->GetChildByNum(i);
		tmpSurf = button->imageSurf;
		button->SetTextureBase(g_texTransparent);
		delete tmpSurf;
	}

	common::Utils::WidgetStateTransition(-100.0f, thisPage, ui::Widget::Animation_3D_SlideFromFront, SCE_TRUE, SCE_FALSE);
	if (prev != SCE_NULL) {
		prev->thisPage->PlayAnimationReverse(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		prev->thisPage->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
		if (prev->thisPage->animationStatus & 0x80)
			prev->thisPage->animationStatus &= ~0x80;
		if (prev->prev != SCE_NULL) {
			prev->prev->thisPage->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
			if (prev->prev->thisPage->animationStatus & 0x80)
				prev->prev->thisPage->animationStatus &= ~0x80;
		}
		else {
			searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
			commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
			commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);
		}
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);

	s_currentFavPage = prev;
}

SceVoid menu::youtube::FavPage::LeftButtonOp()
{
	delete s_currentFavPage;
}

SceVoid menu::youtube::FavPage::RightButtonOp()
{
	new FavPage(s_currentFavPage);
}

SceVoid menu::youtube::FavPage::TermOp()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	// In destructor s_current(...)Page->prev is assigned to s_current(...)Page
	while (s_currentFavPage) {
		delete s_currentFavPage;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);
}