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
	wstring title16;
	wstring subtext16;
	string text8;
	rco::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *box;
	ui::Widget *button;
	ui::Widget *subtext;
	VideoButtonCB *buttonCB;
	SharedPtr<HttpFile> fres;
	graph::Surface *tmbTex;

	if (page->parseResult->results[index].type == YouTubeSuccinctItem::CHANNEL)
		return;

	searchParam.hash = EMPVAUtils::GetHash("youtube_scroll_box");
	box = page->thisPage->GetChild(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube_result_button");
	g_empvaPlugin->TemplateOpen(box, &searchParam, &tmpParam);

	button = (ui::ImageButton *)box->GetChild(box->childNum - 1);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_button_subtext");
	subtext = button->GetChild(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_button_subtext");
	subtext = button->GetChild(&searchParam, 0);

	if (page->parseResult->results[index].type == YouTubeSuccinctItem::VIDEO) {

		text8 = page->parseResult->results[index].video.title.c_str();
		ccc::UTF8toUTF16(&text8, &title16);

		text8 = page->parseResult->results[index].video.duration_text.c_str();
		text8 += "  ";
		text8 += page->parseResult->results[index].video.author.c_str();
		ccc::UTF8toUTF16(&text8, &subtext16);

		fres = HttpFile::Open(page->parseResult->results[index].video.thumbnail_url.c_str(), &res, 0);

		buttonCB = new VideoButtonCB;
		buttonCB->pUserData = buttonCB;
		buttonCB->mode = menu::youtube::Base::Mode_Search;
		buttonCB->url = page->parseResult->results[index].video.url.c_str();

		thread::s_mainThreadMutex.Lock();
		button->SetLabel(&title16);
		subtext->SetLabel(&subtext16);
		button->RegisterEventCallback(ui::EventMain_Decide, buttonCB, 0);
		thread::s_mainThreadMutex.Unlock();
	}
	else if (page->parseResult->results[index].type == YouTubeSuccinctItem::PLAYLIST) {

		text8 = page->parseResult->results[index].playlist.title.c_str();
		ccc::UTF8toUTF16(&text8, &title16);

		text8 = "Playlist, ";
		text8 += page->parseResult->results[index].playlist.video_count_str.c_str();
		ccc::UTF8toUTF16(&text8, &subtext16);

		fres = HttpFile::Open(page->parseResult->results[index].playlist.thumbnail_url.c_str(), &res, 0);

		buttonCB = new VideoButtonCB;
		buttonCB->pUserData = buttonCB;
		buttonCB->mode = menu::youtube::Base::Mode_Search;
		buttonCB->url = page->parseResult->results[index].playlist.url.c_str();

		thread::s_mainThreadMutex.Lock();
		button->SetLabel(&title16);
		subtext->SetLabel(&subtext16);
		button->RegisterEventCallback(ui::EventMain_Decide, buttonCB, 0);
		thread::s_mainThreadMutex.Unlock();
	}

	if (res < 0) {
		return;
	}

	graph::Surface::Create(&tmbTex, g_empvaPlugin->memoryPool, (SharedPtr<File>*)&fres);

	fres.reset();

	if (tmbTex == SCE_NULL) {
		return;
	}

	thread::s_mainThreadMutex.Lock();
	tmbTex->UnsafeRelease();
	button->SetSurfaceBase(&tmbTex);
	thread::s_mainThreadMutex.Unlock();
}

SceVoid menu::youtube::SearchParserThread::EntryFunction()
{
	rco::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	SceUInt32 prevResNum = 0;
	SceUInt32 curResNum = 0;

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChild(&searchParam, 0);
	commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
	commonWidget = g_rootPage->GetChild(&searchParam, 0);
	commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);

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
		workPage->parseResult = youtube_parse_search_word((char *)newRequset.c_str());
		workPage->resultCount = workPage->parseResult->results.size();
	}

	curResNum = prevResNum + workPage->resultCount;

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube");
	g_empvaPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_youtube_bg");
	workPage->thisPage = (ui::Plane *)g_root->GetChild(&searchParam, 0);
	workPage->thisPage->hash = (SceUInt32)workPage->thisPage;

	if (workPage->prev != SCE_NULL) {
		if (workPage->prev->prev != SCE_NULL) {
			workPage->prev->prev->thisPage->PlayEffectReverse(0.0f, effect::EffectType_Reset);
			if (workPage->prev->prev->thisPage->animationStatus & 0x80)
				workPage->prev->prev->thisPage->animationStatus &= ~0x80;
		}
		workPage->prev->thisPage->PlayEffect(0.0f, effect::EffectType_3D_SlideToBack1);
		if (workPage->prev->thisPage->animationStatus & 0x80)
			workPage->prev->thisPage->animationStatus &= ~0x80;
	}
	workPage->thisPage->PlayEffect(-5000.0f, effect::EffectType_3D_SlideFromFront);
	if (workPage->thisPage->animationStatus & 0x80)
		workPage->thisPage->animationStatus &= ~0x80;

	if (prevPage) {
		searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
		commonWidget = g_rootPage->GetChild(&searchParam, 0);
		commonWidget->PlayEffect(0.0f, effect::EffectType_Fadein1);
	}

	for (SceInt32 i = prevResNum; i < workPage->resultCount + prevResNum; i++) {
		YTUtils::WaitMenuParsers();
		if (IsCanceled()) {
			Cancel();
			return;
		}
		CreateVideoButton(workPage, i);
	}

	if (workPage->parseResult->estimated_result_num > curResNum) {
		searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
		commonWidget = g_rootPage->GetChild(&searchParam, 0);
		commonWidget->PlayEffect(0.0f, effect::EffectType_Fadein1);
	}

	Cancel();
}

SceVoid menu::youtube::SearchPage::SearchActionButtonOp()
{
	rco::Element searchParam;
	ui::TextBox *searchBox;
	wstring text16;
	string text8;

	// In destructor s_current(...)Page->prev is assigned to s_current(...)Page
	while (s_currentSearchPage) {
		delete s_currentSearchPage;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_text_box_top_search");
	searchBox = (ui::TextBox *)g_rootPage->GetChild(&searchParam, 0);

	searchBox->GetLabel(&text16);

	if (text16.length() != 0) {

		ccc::UTF16toUTF8(&text16, &text8);

		if (!sce_paf_strncmp("playlist:", text8.c_str(), 9) || !sce_paf_strncmp("video:", text8.c_str(), 6)) {

			char *idptr = (char *)text8.c_str() + 6;

			YTUtils::GetHistLog()->Add(idptr);

			new menu::audioplayer::Audioplayer::Audioplayer(idptr, SCE_NULL, menu::audioplayer::Audioplayer::Mode_Youtube);
		}
		else {

			new SearchPage(text8.c_str());
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
	parserThread->Start();

	s_currentSearchPage = this;
}

menu::youtube::SearchPage::~SearchPage()
{
	rco::Element searchParam;
	ui::Widget *commonWidget;
	ui::ImageButton *button;
	graph::Surface *tmpSurf;

	parserThread->Cancel();
	thread::s_mainThreadMutex.Unlock();
	parserThread->Join();
	thread::s_mainThreadMutex.Lock();
	delete parserThread;

	effect::Play(-100.0f, thisPage, effect::EffectType_3D_SlideFromFront, SCE_TRUE, SCE_FALSE);
	if (prev != SCE_NULL) {
		prev->thisPage->PlayEffectReverse(0.0f, effect::EffectType_3D_SlideToBack1);
		prev->thisPage->PlayEffect(0.0f, effect::EffectType_Reset);
		if (prev->thisPage->animationStatus & 0x80)
			prev->thisPage->animationStatus &= ~0x80;
		if (prev->prev != SCE_NULL) {
			prev->prev->thisPage->PlayEffect(0.0f, effect::EffectType_Reset);
			if (prev->prev->thisPage->animationStatus & 0x80)
				prev->prev->thisPage->animationStatus &= ~0x80;
		}
		else {
			searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_left");
			commonWidget = g_rootPage->GetChild(&searchParam, 0);
			commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);
		}
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChild(&searchParam, 0);
	commonWidget->PlayEffect(0.0f, effect::EffectType_Fadein1);

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
	rco::Element searchParam;
	ui::Widget *commonWidget;

	// In destructor s_current(...)Page->prev is assigned to s_current(...)Page
	while (s_currentSearchPage) {
		delete s_currentSearchPage;
	}

	searchParam.hash = EMPVAUtils::GetHash("yt_button_btmenu_right");
	commonWidget = g_rootPage->GetChild(&searchParam, 0);
	commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);
}