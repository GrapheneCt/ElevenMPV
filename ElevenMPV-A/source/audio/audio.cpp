#include <kernel.h>
#include <appmgr.h>
#include <audioout.h>
#include <shellaudio.h>

#include "common.h"
#include "audio.h"
#include "vitaaudiolib.h"
#include "utils.h"

enum Audio_BgmMode
{
	BgmMode_None,
	BgmMode_Normal,
	BgmMode_Shc
};

static SceUInt32 s_bgmMode = BgmMode_Normal;

SceVoid audio::PlayerCoverLoaderThread::EntryFunction()
{
	coverTex.texSurface = SCE_NULL;
	Resource::Element searchParam;
	widget::Widget *playerCover;
	widget::BusyIndicator *playerBusyInd;
	widget::Widget::Color col;
	SceFVector4 wsize;
	Misc::OpenResult fres;
	SceInt32 res;

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_player");
	playerBusyInd = (widget::BusyIndicator *)g_player_page->GetChildByHash(&searchParam, 0);
	playerBusyInd->Start();

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChildByHash(&searchParam, 0);

	if (g_currentDispFilePage->coverWork != SCE_NULL) {

		if (g_currentCoverLoader != SCE_NULL) {
			g_currentCoverLoader->Join();
		}
		else {
			menu::displayfiles::CoverLoaderThread *coverLoader = new menu::displayfiles::CoverLoaderThread(SCE_KERNEL_COMMON_QUEUE_LOWEST_PRIORITY - 20, 0x1000, "EMPVA::CoverLoader");
			coverLoader->workPage = g_currentDispFilePage;
			coverLoader->workFile = g_currentDispFilePage->coverWork;
			coverLoader->Start();
			coverLoader->Join();
			delete coverLoader;
		}

		if(g_currentCoverSurf != SCE_NULL) {
			coverTex.texSurface = g_currentCoverSurf;
			playerCover->SetTextureBase(&coverTex);

			col.r = 0.207;
			col.g = 0.247;
			col.b = 0.286;
			col.a = 1;
			g_root->SetFilterColor(&col);

			wsize.x = 960.0;
			wsize.y = 960.0;
			wsize.z = 0.0f;
			wsize.w = 0.0f;
			g_root->SetSize(&wsize);

			g_root->SetTextureBase(&coverTex);
		}

		playerBusyInd->Stop();
		sceKernelExitDeleteThread(0);
		return;
	}

	if ((g_currentDispFilePage->coverWork == SCE_NULL) && (workptr == SCE_NULL)) {
		playerBusyInd->Stop();
		sceKernelExitDeleteThread(0);
		return;
	}

	Misc::OpenMem(&fres, workptr, size, &res);

	if (res < 0) {
		if (isExtMem)
			sce_paf_free(workptr);
		playerBusyInd->Stop();
		sceKernelExitDeleteThread(0);
		return;
	}

	if (g_currentCoverSurf != SCE_NULL)
		menu::displayfiles::Page::ResetBgPlaneTex();

	graphics::Texture::CreateFromFile(&coverTex, g_empvaPlugin->memoryPool, &fres);

	if (coverTex.texSurface == SCE_NULL) {
		delete fres.localFile;
		sce_paf_free(fres.unk_04);
		if (isExtMem)
			sce_paf_free(workptr);
		playerBusyInd->Stop();
		sceKernelExitDeleteThread(0);
		return;
	}

	g_currentCoverSurf = coverTex.texSurface;

	if (isExtMem)
		sce_paf_free(workptr);

	delete fres.localFile;
	sce_paf_free(fres.unk_04);

	col.r = 0.207;
	col.g = 0.247;
	col.b = 0.286;
	col.a = 1;
	g_root->SetFilterColor(&col);

	wsize.x = 960.0;
	wsize.y = 960.0;
	wsize.z = 0.0f;
	wsize.w = 0.0f;
	g_root->SetSize(&wsize);

	g_root->SetTextureBase(&coverTex);

	playerCover->SetTextureBase(&coverTex);

	playerBusyInd->Stop();

	sceKernelExitDeleteThread(0);
}

audio::GenericDecoder::GenericDecoder(const char *path, SceBool isSwDecoderUsed)
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;
	coverLoader = SCE_NULL;

	metadata = new audio::GenericDecoder::Metadata();
	metadata->hasCover = SCE_FALSE;
	metadata->hasMeta = SCE_FALSE;

	if (isSwDecoderUsed)
		sceKernelSetEventFlag(g_eventFlagUid, FLAG_ELEVENMPVA_IS_DECODER_USED);
	else
		sceKernelClearEventFlag(g_eventFlagUid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);

	if (!EMPVAUtils::IsDecoderUsed() && s_bgmMode != BgmMode_Shc) {
		sceAppMgrReleaseBgmPort();
		sceAppMgrAcquireBgmPortWithPriority(0x80);
		s_bgmMode = BgmMode_Shc;
	}
	else if (EMPVAUtils::IsDecoderUsed() && s_bgmMode != BgmMode_Normal) {
		sceAppMgrReleaseBgmPort();
		sceAppMgrAcquireBgmPortWithPriority(0x81);
		s_bgmMode = BgmMode_Normal;
	}
}

audio::GenericDecoder::~GenericDecoder()
{
	Resource::Element searchParam;
	widget::Widget *playerCover;

	audio::DecoderCore::SetDecoder(SCE_NULL, SCE_NULL); // Clear channel callback

	sceKernelClearEventFlag(g_eventFlagUid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);

	metadata->title.Clear();
	metadata->album.Clear();
	metadata->artist.Clear();
	delete metadata;

	if (coverLoader != SCE_NULL) {
		coverLoader->Join();

		if (coverLoader->coverTex.texSurface != SCE_NULL) {
			searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
			playerCover = g_player_page->GetChildByHash(&searchParam, 0);
			playerCover->SetTextureBase(g_coverBgTex);
		}

		delete coverLoader;
	}
}

SceUInt64 audio::GenericDecoder::Seek(SceFloat32 percent)
{
	return 0;
}

SceVoid audio::GenericDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{

}

SceUInt32 audio::GenericDecoder::GetSampleRate()
{
	return 48000;
}

SceUInt8 audio::GenericDecoder::GetChannels()
{
	return 2;
}

SceUInt64 audio::GenericDecoder::GetPosition()
{
	return 0;
}

SceUInt64 audio::GenericDecoder::GetLength()
{
	return 0;
}

SceBool audio::GenericDecoder::IsPaused()
{
	return isPaused;
}

SceVoid audio::GenericDecoder::Pause()
{
	if (!EMPVAUtils::IsDecoderUsed() && isPaused)
		sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_PLAY, 0);
	else if (!EMPVAUtils::IsDecoderUsed()) {
		sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_STOP, 0);
	}
	isPaused = !isPaused;
}

SceVoid audio::GenericDecoder::Stop()
{
	if (!EMPVAUtils::IsDecoderUsed())
		sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_STOP, 0);
	isPlaying = !isPlaying;
}

audio::GenericDecoder::Metadata *audio::GenericDecoder::GetMetadataLocation()
{
	return metadata;
}

SceVoid audio::Utils::ResetBgmMode()
{
	s_bgmMode = BgmMode_None;
}