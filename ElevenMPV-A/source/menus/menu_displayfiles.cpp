#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>

#include "common.h"
#include "menu_displayfiles.h"
#include "menu_audioplayer.h"
#include "utils.h"

using namespace paf;

graphics::Surface *g_currentCoverSurf = SCE_NULL;

SceVoid menu::displayfiles::CoverLoaderJob::Run()
{
	Resource::Element searchParam;
	ObjectWithCleanup fres;
	graphics::Texture tex;
	ui::Widget::Color col;
	SceFVector4 wsize;
	SceInt32 res;
	String fullPath;

	if (!workFile) {
		menu::displayfiles::Page::ResetBgPlaneTex();
		return;
	}

	if (g_currentCoverSurf != SCE_NULL)
		menu::displayfiles::Page::ResetBgPlaneTex();

	if (g_currentDispFilePage != workPage) {
		return;
	}

	fullPath.Append(workPage->cwd->data, workPage->cwd->length);
	fullPath.Append(workFile->name->string.data, workFile->name->string.length);
	LocalFile::Open(&fres, fullPath.data, SCE_O_RDONLY, 0, &res);

	if (res < 0) {
		return;
	}

	if (g_currentDispFilePage != workPage) {
		fres.cleanup->cb(fres.object);
		delete fres.cleanup;
		return;
	}

	graphics::Texture::CreateFromFile(&tex, g_empvaPlugin->memoryPool, &fres);
	g_currentCoverSurf = tex.texSurface;

	if (g_currentDispFilePage != workPage) {
		fres.cleanup->cb(fres.object);
		delete fres.cleanup;
		return;
	}

	if (tex.texSurface == SCE_NULL) {
		fres.cleanup->cb(fres.object);
		delete fres.cleanup;
		return;
	}

	fres.cleanup->cb(fres.object);
	delete fres.cleanup;

	col.r = 0.207;
	col.g = 0.247;
	col.b = 0.286;
	col.a = 1;

	wsize.x = 960.0;
	wsize.y = 960.0;
	wsize.z = 0.0f;
	wsize.w = 0.0f;

	if (!g_isPlayerActive) {
		g_root->SetColor(&col);
		g_root->SetSize(&wsize);
		g_root->SetTextureBase(&tex);
	}
}

SceVoid menu::displayfiles::PlayerButtonCB::PlayerButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	menu::audioplayer::Audioplayer::Return();
}

SceVoid menu::displayfiles::CoverLoaderJob::Finish()
{

}

SceVoid menu::displayfiles::BackButtonCB::BackButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;
	Page *tmpCurr = g_currentDispFilePage;
	g_currentDispFilePage = g_currentDispFilePage->prev;
	delete tmpCurr;

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
	ui::Widget *backButton = g_rootPage->GetChildByHash(&searchParam, 0);
	if (!EMPVAUtils::IsRootDevice(g_currentDispFilePage->cwd->data))
		backButton->PlayAnimation(600.0f, ui::Widget::Animation_Reset);
	else
		backButton->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
}

SceVoid menu::displayfiles::ButtonCB::StartNewPlayer(menu::displayfiles::File *startFile)
{
	g_currentDispFilePage->coverLoader = new CoverLoaderJob("EMPVA::CoverLoaderJob");
	g_currentDispFilePage->coverLoader->workPage = g_currentDispFilePage;
	if (g_currentDispFilePage->coverState)
		g_currentDispFilePage->coverLoader->workFile = g_currentDispFilePage->coverWork;
	else
		g_currentDispFilePage->coverLoader->workFile = SCE_NULL;

	CleanupHandler *req = new CleanupHandler();
	req->userData = g_currentDispFilePage->coverLoader;
	req->refCount = 0;
	req->unk_08 = 1;
	req->cb = (CleanupHandler::CleanupCallback)menu::displayfiles::CoverLoaderJob::JobKiller;

	ObjectWithCleanup itemParam;
	itemParam.object = g_currentDispFilePage->coverLoader;
	itemParam.cleanup = req;

	g_coverJobQueue->Enqueue(&itemParam);

	new menu::audioplayer::Audioplayer::Audioplayer(g_currentDispFilePage->cwd->data, startFile, menu::audioplayer::Audioplayer::Mode_Normal);
}

SceVoid menu::displayfiles::ButtonCB::ButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	File *file = (File *)pUserData;

	String *tempCwd = new String();

	tempCwd->Setf("%s%s%s", g_currentDispFilePage->cwd->data, file->name->string.data, "/");
	if (!io::Misc::Exists(tempCwd->data)) {
		tempCwd->Clear();
		return;
	}
	else {
		if (file->type == File::Type_Dir) {
			Page *newPage = new menu::displayfiles::Page(tempCwd->data);
		}
		else if (file->type == File::Type_Music) {
			if (g_isPlayerActive) {
				if (!sce_paf_strncasecmp(g_currentPlayerInstance->playlist.path[g_currentPlayerInstance->playlistIdx]->data, tempCwd->data, g_currentPlayerInstance->playlist.path[g_currentPlayerInstance->playlistIdx]->length)) {
					menu::audioplayer::Audioplayer::Return();
				}
				else {
					StartNewPlayer(file);
				}
			}
			else {
				StartNewPlayer(file);
			}
		}
		tempCwd->Clear();
	}

	delete tempCwd;
}


SceVoid menu::displayfiles::BusyCB::BusyCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	g_commonBusyInidcator->Stop();
	self->UnregisterEventCallback(0x100004, 0, 0);
}

SceVoid menu::displayfiles::Page::Init()
{
	g_currentDispFilePage = SCE_NULL;
}

menu::displayfiles::Page::Page(const char* path)
{
	// Using widget pointer value as hash is unreliable, but good enough for now

	coverWork = SCE_NULL;
	coverState = SCE_FALSE;

	char tmpPath[0x256];
	SceInt32 slashPos = 0;

	if (!EMPVAUtils::IsRootDevice(path)) {
		if (g_currentDispFilePage == SCE_NULL) {

			// Find last '/' in working directory
			SceInt32 i = sce_paf_strlen(path) - 2;
			for (; i >= 0; i--) {
				// Slash discovered
				if (path[i] == '/') {
					slashPos = i + 1; // Save pointer
					break; // Stop search
				}
			}

			sce_paf_memcpy(tmpPath, path, slashPos);
			tmpPath[slashPos] = 0;

			new Page(tmpPath);
		}
	}

	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;

	cwd = new String(path);

	WString topText;
	String topText8;
	topText8 = cwd->data;
	topText8.ToWString(&topText);
	g_topText->SetLabel(&topText);
	coverLoader = SCE_NULL;

	if (PopulateFiles(cwd->data) < 0)
		fileNum = 0;

	if (fileNum > k_busyIndicatorFileLimit)
		g_commonBusyInidcator->Start();

	if (g_currentDispFilePage == SCE_NULL)
		prev = SCE_NULL;
	else
		prev = g_currentDispFilePage;

	searchParam.hash = EMPVAUtils::GetHash("menu_template_displayfiles");
	g_empvaPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_displayfiles_bg");
	root = (ui::Plane *)g_root->GetChildByHash(&searchParam, 0);
	root->hash = (SceUInt32)root;

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_back_button");
	ui::Widget *backButton = g_rootPage->GetChildByHash(&searchParam, 0);
	if (!EMPVAUtils::IsRootDevice(cwd->data))
		backButton->PlayAnimation(300.0f, ui::Widget::Animation_Reset);
	else
		backButton->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_scroll_box");
	box = (ui::Box *)root->GetChildByHash(&searchParam, 0);
	box->hash = (SceUInt32)box;

	Resource::Element searchParamMusic;
	Resource::Element searchParamDir;
	searchParamMusic.hash = EMPVAUtils::GetHash("menu_template_displayfiles_button_mus");
	searchParamDir.hash = EMPVAUtils::GetHash("menu_template_displayfiles_button_dir");
	searchParam.hash = EMPVAUtils::GetHash("menu_template_displayfiles_button_unk");

	File *file = files;
	File *lastFile = SCE_NULL;

	for (int i = 0; i < fileNum; i++) {

		//Skip unsupported files for now (only covers and thumbnails get to here)
		if (file->type == File::Type_Unsupported) {
			file = file->next;
			continue;
		}

		switch (file->type) {
		case File::Type_Unsupported:
			g_empvaPlugin->TemplateOpen(box, &searchParam, &tmpParam);
			break;
		case File::Type_Dir:
			g_empvaPlugin->TemplateOpen(box, &searchParamDir, &tmpParam);
			break;
		case File::Type_Music:
			g_empvaPlugin->TemplateOpen(box, &searchParamMusic, &tmpParam);
			break;
		}

		file->button = (ui::ImageButton *)box->GetChildByNum(box->childNum - 1);
		file->button->hash = (SceUInt32)file->button;
		file->button->SetLabel(&file->name->wstring);
		if (file->type == File::Type_Unsupported)
			file->button->Disable(0);
		file->buttonCB = new ButtonCB;
		file->buttonCB->pUserData = file;
		file->button->RegisterEventCallback(ui::Widget::EventMain_Decide, file->buttonCB, 0);
		if (i == fileNum - 1)
			lastFile = file;
		file = file->next;
	}

	if (fileNum > k_busyIndicatorFileLimit) {
		if (lastFile) {
			busyCB = new BusyCB();
			lastFile->button->RegisterEventCallback(0x100004, busyCB, 0);
		}
	}

	if (fileNum == 0) {
		searchParam.hash = EMPVAUtils::GetHash("menu_template_displayfiles_text_empty_dir");
		g_empvaPlugin->TemplateOpen(root, &searchParam, &tmpParam);
	}

	if (g_currentDispFilePage != SCE_NULL) {
		if (g_currentDispFilePage->prev != SCE_NULL) {
			g_currentDispFilePage->prev->root->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
			if (g_currentDispFilePage->prev->root->animationStatus & 0x80)
				g_currentDispFilePage->prev->root->animationStatus &= ~0x80;
		}
		g_currentDispFilePage->root->PlayAnimation(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		if (g_currentDispFilePage->root->animationStatus & 0x80)
			g_currentDispFilePage->root->animationStatus &= ~0x80;
	}
	root->PlayAnimation(-5000.0f, ui::Widget::Animation_3D_SlideFromFront);
	if (root->animationStatus & 0x80)
		root->animationStatus &= ~0x80;

	menu::settings::Settings::GetInstance()->SetLastDirectory(path);

	g_currentDispFilePage = this;
}

menu::displayfiles::Page::~Page()
{
	String topText8;
	WString topText;
	char tmpPath[SCE_IO_MAX_PATH_LENGTH];

	sce_paf_memset(tmpPath, 0, SCE_IO_MAX_PATH_LENGTH);
	sce_paf_strncpy(tmpPath, prev->cwd->data, sce_paf_strlen(prev->cwd->data));
	topText8 = tmpPath;
	topText8.ToWString(&topText);
	g_topText->SetLabel(&topText);

	if (prev != SCE_NULL && !g_isPlayerActive) {

		coverLoader = new CoverLoaderJob("EMPVA::CoverLoaderJob");
		coverLoader->workPage = prev;
		if (prev->coverState)
			coverLoader->workFile = prev->coverWork;
		else
			coverLoader->workFile = SCE_NULL;

		CleanupHandler *req = new CleanupHandler();
		req->userData = coverLoader;
		req->refCount = 0;
		req->unk_08 = 1;
		req->cb = (CleanupHandler::CleanupCallback)menu::displayfiles::CoverLoaderJob::JobKiller;

		ObjectWithCleanup itemParam;
		itemParam.object = coverLoader;
		itemParam.cleanup = req;

		g_coverJobQueue->Enqueue(&itemParam);

	}

	common::Utils::WidgetStateTransition(-100.0f, root, ui::Widget::Animation_3D_SlideFromFront, SCE_TRUE, SCE_FALSE);
	if (prev != SCE_NULL) {
		prev->root->PlayAnimationReverse(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		prev->root->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
		if (prev->root->animationStatus & 0x80)
			prev->root->animationStatus &= ~0x80;
		if (prev->prev != SCE_NULL) {
			prev->prev->root->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
			if (prev->prev->root->animationStatus & 0x80)
				prev->prev->root->animationStatus &= ~0x80;
		}
	}

	if (files != SCE_NULL)
		ClearFiles(files);

	menu::settings::Settings::GetInstance()->SetLastDirectory(prev->cwd->data);

	delete cwd;
}

SceVoid menu::displayfiles::Page::ResetBgPlaneTex()
{
	ui::Widget::Color col;
	SceFVector4 wsize;
	Resource::Element searchParam;

	col.r = 1;
	col.g = 1;
	col.b = 1;
	col.a = 1;
	g_root->SetColor(&col);

	wsize.x = 960.0;
	wsize.y = 544.0;
	wsize.z = 0.0f;
	wsize.w = 0.0f;
	g_root->SetSize(&wsize);

	g_root->SetTextureBase(g_commonBgTex);

	if (g_player_page) {
		searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
		ui::Widget *playerCover = g_player_page->GetChildByHash(&searchParam, 0);
		if (playerCover)
			playerCover->SetTextureBase(g_coverBgTex);
	}

	if (g_currentCoverSurf)
		delete g_currentCoverSurf;
	g_currentCoverSurf = SCE_NULL;
}

SceVoid menu::displayfiles::Page::ClearFiles(File *file)
{
	if (file->next != SCE_NULL)
		ClearFiles(file->next);

	delete file;
}

SceInt32 menu::displayfiles::Page::Cmpstringp(const ScePVoid p1, const ScePVoid p2)
{
	io::Dir::Dirent *entryA = (io::Dir::Dirent *)p1;
	io::Dir::Dirent *entryB = (io::Dir::Dirent *)p2;

	if ((entryA->type == io::Type_Dir) && (entryB->type != io::Type_Dir))
		return -1;
	else if ((entryA->type != io::Type_Dir) && (entryB->type == io::Type_Dir))
		return 1;
	else {
		switch (menu::settings::Settings::GetInstance()->sort) {
		case 0: // Sort alphabetically (ascending - A to Z)
			return sce_paf_strcasecmp(entryA->name.data, entryB->name.data);
			break;
		case 1: // Sort alphabetically (descending - Z to A)
			return sce_paf_strcasecmp(entryB->name.data, entryA->name.data);
			break;
		case 2: // Sort by file size (largest first)
			return entryA->size > entryB->size ? -1 : entryA->size < entryB->size ? 1 : 0;
			break;
		case 3: // Sort by file size (smallest first)
			return entryB->size > entryA->size ? -1 : entryB->size < entryA->size ? 1 : 0;
			break;
		}
	}

	return 0;
}

SceInt32 menu::displayfiles::Page::PopulateFiles(const char *rootPath) 
{
	io::Dir dir;
	File *coverWorkItem = SCE_NULL;
	files = SCE_NULL;
	fileNum = 0;

	if (dir.Open(rootPath) >= 0) {

		int entryCount = 0;
		io::Dir::Dirent *entries = new io::Dir::Dirent[MAX_FILES];

		while (dir.Read(&entries[entryCount]) >= 0)
			entryCount++;

		dir.Close();
		sce_paf_qsort(entries, entryCount, sizeof(io::Dir::Dirent), (int(*)(const void *, const void *))Cmpstringp);

		for (int i = 0; i < entryCount; i++) {
			// Allocate Memory
			File *item = new File();

			// Set type and check if supported
			if (entries[i].type != io::Type_Dir) {
				sce_paf_strncpy(item->ext, EMPVAUtils::GetFileExt(entries[i].name.data), 5);

				if (EMPVAUtils::IsSupportedExtension(item->ext))
					item->type = File::Type_Music;
				else if (EMPVAUtils::IsSupportedCoverExtension(item->ext)) {
					if (!sce_paf_strncasecmp(entries[i].name.data, "cover", 5) || !sce_paf_strncasecmp(entries[i].name.data, "folder", 6)) {
						coverState = SCE_TRUE;
						coverWorkItem = item;
					}
					if (!coverState) {
						delete item;
						continue;
					}
				}
				else {
					delete item;
					continue;
				}
			}
			else if (entries[i].type == io::Type_Dir)
				item->type = File::Type_Dir;
			else {
				delete item;
				continue;
			}

			item->name = new SWString(entries[i].name.data);

			item->name->string.ToWString(&item->name->wstring);

			fileNum++;

			// New file
			if (files == SCE_NULL)
				files = item;

			// Existing file
			else {
				File *list = files;

				while (list->next != SCE_NULL)
					list = list->next;

				list->next = item;
			}
		}

		coverWork = coverWorkItem;

		if (!g_isPlayerActive) {
			coverLoader = new CoverLoaderJob("EMPVA::CoverLoaderJob");
			coverLoader->workPage = this;
			if (coverState)
				coverLoader->workFile = coverWorkItem;
			else
				coverLoader->workFile = SCE_NULL;

			CleanupHandler *req = new CleanupHandler();
			req->userData = coverLoader;
			req->refCount = 0;
			req->unk_08 = 1;
			req->cb = (CleanupHandler::CleanupCallback)menu::displayfiles::CoverLoaderJob::JobKiller;

			ObjectWithCleanup itemParam;
			itemParam.object = coverLoader;
			itemParam.cleanup = req;

			g_coverJobQueue->Enqueue(&itemParam);
		}

		delete[] entries;
	}
	else
		return SCE_ERROR_ERRNO_ENOENT;

	return 0;
}
