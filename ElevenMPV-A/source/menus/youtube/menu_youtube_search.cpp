#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>

#include "common.h"
#include "menu_youtube.h"
#include "menu_audioplayer.h"
#include "utils.h"
#include "yt_utils.h"
#include "youtube_parser.hpp"

using namespace paf;

static menu::youtube::SearchPage *s_currentSearchPage = SCE_NULL;

SceVoid menu::youtube::SearchParserThread::CreateVideoButton(SearchPage *page, SceUInt32 index)
{
	SceInt32 res;
	WString text16;
	String text8;
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *box;
	ui::Widget *button;
	ui::Widget *subtext;
	VideoButtonCB *buttonCB;
	ObjectWithCleanup fres;
	graphics::Texture tmbTex;

	if (page->parseResult->results[index].type == YouTubeSuccinctItem::CHANNEL)
		return;

	searchParam.hash = EMPVAUtils::GetHash("youtube_scroll_box");
	box = page->thisPage->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube_result_button");
	g_empvaPlugin->TemplateOpen(box, &searchParam, &tmpParam);

	button = (ui::ImageButton *)box->GetChildByNum(box->childNum - 1);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_button_subtext");
	subtext = button->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_button_subtext");
	subtext = button->GetChildByHash(&searchParam, 0);

	if (page->parseResult->results[index].type == YouTubeSuccinctItem::VIDEO) {

		text8 = page->parseResult->results[index].video.title.c_str();
		text8.ToWString(&text16);
		button->SetLabel(&text16);

		text8 = page->parseResult->results[index].video.duration_text.c_str();
		text8.ToWString(&text16);
		subtext->SetLabel(&text16);

		HttpFile::Open(&fres, page->parseResult->results[index].video.thumbnail_url.c_str(), &res, 0);

		buttonCB = new VideoButtonCB;
		buttonCB->pUserData = buttonCB;
		buttonCB->mode = menu::youtube::Base::Mode_Search;
		buttonCB->url = page->parseResult->results[index].video.url.c_str();
		button->RegisterEventCallback(ui::Widget::EventMain_Pressed, buttonCB, 0);
	}
	else if (page->parseResult->results[index].type == YouTubeSuccinctItem::PLAYLIST) {

		text8 = page->parseResult->results[index].playlist.title.c_str();
		text8.ToWString(&text16);
		button->SetLabel(&text16);

		text8 = "Playlist, ";
		text8 += page->parseResult->results[index].playlist.video_count_str.c_str();
		text8.ToWString(&text16);
		subtext->SetLabel(&text16);

		HttpFile::Open(&fres, page->parseResult->results[index].playlist.thumbnail_url.c_str(), &res, 0);

		buttonCB = new VideoButtonCB;
		buttonCB->pUserData = buttonCB;
		buttonCB->mode = menu::youtube::Base::Mode_Search;
		buttonCB->url = page->parseResult->results[index].playlist.url.c_str();
		button->RegisterEventCallback(ui::Widget::EventMain_Pressed, buttonCB, 0);
	}

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

SceVoid menu::youtube::SearchParserThread::EntryFunction()
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	SceUInt32 prevResNum = 0;
	SceUInt32 curResNum = 0;

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);

	if (prevPage) {

		workPage->parseResult = youtube_continue_search(*prevPage->parseResult);

		prevResNum = prevPage->resultCount;
		SearchPage *tres;

		tres = prevPage->prev;
		while (tres) {
			prevResNum += tres->resultCount;
			tres = tres->prev;
		}

		workPage->resultCount = workPage->parseResult->results.size() - prevResNum;
	}
	else {
		//YTUtils::GetParseMutex()->Lock();
		workPage->parseResult = youtube_parse_search_word(newRequset.data);
		//YTUtils::GetParseMutex()->Unlock();

		workPage->resultCount = workPage->parseResult->results.size();
	}

	curResNum = prevResNum + workPage->resultCount;

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

	for (SceInt32 i = prevResNum; i < workPage->resultCount + prevResNum; i++) {
		YTUtils::GetMenuSema()->Wait();
		if (cancel) {
			YTUtils::GetMenuSema()->Signal();
			sceKernelExitDeleteThread(0);
		}
		CreateVideoButton(workPage, i);
		YTUtils::GetMenuSema()->Signal();
	}

	if (workPage->parseResult->estimated_result_num > curResNum) {
		searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);
	}

	sceKernelExitDeleteThread(0);
}

SceVoid menu::youtube::SearchPage::SearchActionButtonOp()
{
	Resource::Element searchParam;
	ui::TextBox *searchBox;
	WString text16;
	String text8;

	// In destructor s_current(...)Page->prev is assigned to s_current(...)Page
	while (s_currentSearchPage) {
		delete s_currentSearchPage;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_text_box_top_search");
	searchBox = (ui::TextBox *)g_rootPage->GetChildByHash(&searchParam, 0);

	searchBox->GetLabel(&text16);

	if (text16.length != 0) {

		text16.ToString(&text8);

		if (!sce_paf_strncmp("playlist:", text8.data, 9) || !sce_paf_strncmp("video:", text8.data, 6)) {

			char *idptr = text8.data + 6;

			YTUtils::GetHistLog()->Add(idptr);

			new menu::audioplayer::Audioplayer::Audioplayer(idptr, SCE_NULL, menu::audioplayer::Audioplayer::Mode_Youtube);
		}
		else {

			new SearchPage(text8.data);
		}
	}
}

// Constructor for next page
menu::youtube::SearchPage::SearchPage(SearchPage *prevPage)
{
	prev = prevPage;
	next = SCE_NULL;
	parseResult = SCE_NULL;
	parserThread = SCE_NULL;

	prev->parserThread->Join();

	parserThread = new SearchParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::YtSearchParser");
	parserThread->prevPage = prevPage;
	parserThread->workPage = this;
	parserThread->cancel = SCE_FALSE;
	parserThread->Start();

	s_currentSearchPage = this;
}

// Constructor for initial page
menu::youtube::SearchPage::SearchPage(const char *searchWord)
{
	prev = SCE_NULL;
	next = SCE_NULL;
	parseResult = SCE_NULL;
	parserThread = SCE_NULL;

	parserThread = new SearchParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::YtSearchParser");
	parserThread->prevPage = SCE_NULL;
	parserThread->workPage = this;
	parserThread->newRequset = searchWord;
	parserThread->cancel = SCE_FALSE;
	parserThread->Start();

	s_currentSearchPage = this;
}

menu::youtube::SearchPage::~SearchPage()
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

	youtube_destroy_struct(this->parseResult);

	s_currentSearchPage = prev;
}

SceVoid menu::youtube::SearchPage::LeftButtonOp()
{
	delete s_currentSearchPage;
}

SceVoid menu::youtube::SearchPage::RightButtonOp()
{
	new SearchPage(s_currentSearchPage);
}

SceVoid menu::youtube::SearchPage::TermOp()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	// In destructor s_current(...)Page->prev is assigned to s_current(...)Page
	while (s_currentSearchPage) {
		delete s_currentSearchPage;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);
}