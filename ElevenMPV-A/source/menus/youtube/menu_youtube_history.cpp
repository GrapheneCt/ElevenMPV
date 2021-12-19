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

static menu::youtube::HistoryPage *s_currentHistoryPage = SCE_NULL;

SceVoid menu::youtube::HistoryParserThread::CreateVideoButton(HistoryPage *page, const char *data, SceUInt32 index)
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
	char vidCount[30];

	sce_paf_memset(url, 0, sizeof(url));
	sce_paf_memset(tmb, 0, sizeof(tmb));

	searchParam.hash = EMPVAUtils::GetHash("youtube_scroll_box");
	box = page->thisPage->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube_result_button");
	g_empvaPlugin->TemplateOpen(box, &searchParam, &tmpParam);

	button = (ui::ImageButton *)box->GetChildByNum(box->childNum - 1);

	searchParam.hash = EMPVAUtils::GetHash("yt_text_button_subtext");
	subtext = button->GetChildByHash(&searchParam, 0);

	youtube_get_video_url_by_id(data, url, sizeof(url));
	vidInfo = youtube_parse_video_page(url);

	if (vidInfo->playlist.videos.size()) {

		sce_paf_memset(vidCount, 0, sizeof(vidCount));

		text8 = vidInfo->playlist.title.c_str();
		text8.ToWString(&text16);
		button->SetLabel(&text16);

		sce_paf_snprintf(vidCount, sizeof(vidCount), "Playlist, %d videos", vidInfo->playlist.videos.size());
		text8 = vidCount;
		text8.ToWString(&text16);
		subtext->SetLabel(&text16);

		sce_paf_strncpy(tmb, vidInfo->playlist.videos[0].thumbnail_url.c_str(), sizeof(tmb));
	}
	else {

		text8 = vidInfo->title.c_str();
		text8.ToWString(&text16);
		button->SetLabel(&text16);

		menu::audioplayer::Audioplayer::ConvertSecondsToString(&text8, (SceUInt64)((SceFloat)vidInfo->duration_ms / 1000.0f), SCE_FALSE);
		text8.ToWString(&text16);
		subtext->SetLabel(&text16);

		youtube_get_video_thumbnail_url_by_id(data, tmb, sizeof(tmb));
	}

	youtube_destroy_struct(vidInfo);

	buttonCB = new VideoButtonCB;
	buttonCB->pUserData = buttonCB;
	buttonCB->mode = menu::youtube::Base::Mode_History;
	buttonCB->url = url;
	button->RegisterEventCallback(ui::Widget::EventMain_Pressed, buttonCB, 0);

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

SceVoid menu::youtube::HistoryParserThread::EntryFunction()
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	char *entryData;

	YTUtils::GetHistLog()->Reset();
	SceInt32 totalNum = YTUtils::GetHistLog()->GetSize();

	entryData = (char *)sce_paf_calloc(totalNum, SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE);

	for (SceInt32 i = 0; i < totalNum; i++) {
		YTUtils::GetHistLog()->GetNext(entryData + (i * SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE));
	}

	searchParam.hash = EMPVAUtils::GetHash("menu_template_youtube");
	g_empvaPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_youtube_bg");
	workPage->thisPage = (ui::Plane *)g_root->GetChildByHash(&searchParam, 0);
	workPage->thisPage->hash = (SceUInt32)workPage->thisPage;

	workPage->thisPage->PlayAnimation(-5000.0f, ui::Widget::Animation_3D_SlideFromFront);
	if (workPage->thisPage->animationStatus & 0x80)
		workPage->thisPage->animationStatus &= ~0x80;

	for (SceInt32 i = totalNum - 1; i != -1; i--) {
		YTUtils::GetMenuSema()->Wait();
		if (cancel) {
			sceClibPrintf("pre signal 0\n");
			YTUtils::GetMenuSema()->Signal();
			break;
		}
		CreateVideoButton(workPage, entryData + (i * SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE), i);
		sceClibPrintf("pre signal 1\n");
		YTUtils::GetMenuSema()->Signal();
	}

	sce_paf_free(entryData);

	sceKernelExitDeleteThread(0);
}

menu::youtube::HistoryPage::HistoryPage()
{
	parserThread = SCE_NULL;

	parserThread = new HistoryParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "EMPVA::YtHistoryParser");
	parserThread->workPage = this;
	parserThread->cancel = SCE_FALSE;
	parserThread->Start();

	s_currentHistoryPage = this;
}

menu::youtube::HistoryPage::~HistoryPage()
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

	s_currentHistoryPage = SCE_NULL;
}

SceVoid menu::youtube::HistoryPage::TermOp()
{
	delete s_currentHistoryPage;
}