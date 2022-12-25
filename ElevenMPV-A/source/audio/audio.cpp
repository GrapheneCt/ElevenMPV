#include <kernel.h>
#include <appmgr.h>
#include <audioout.h>
#include <shellaudio.h>
#include <curl_file.h>

#include "common.h"
#include "audio.h"
#include "vitaaudiolib.h"
#include "utils.h"
#include "yt_utils.h"

enum Audio_BgmMode
{
	BgmMode_None,
	BgmMode_Normal,
	BgmMode_Shc
};

static SceUInt32 s_bgmMode = BgmMode_Normal;

SceVoid audio::PlayerCoverLoaderJob::Run()
{
	coverTex = SCE_NULL;
	rco::Element searchParam;
	ui::Widget *playerCover;
	ui::BusyIndicator *playerBusyInd;
	Rgba col;
	Vector4 wsize;
	SceInt32 res;
	SharedPtr<MemFile> fres;

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChild(&searchParam, 0);

	if (g_currentDispFilePage->coverState) {
		if (g_currentCoverSurf != SCE_NULL) {
			coverTex = g_currentCoverSurf;
			playerCover->SetSurfaceBase(&coverTex);

			col.r = 0.207;
			col.g = 0.247;
			col.b = 0.286;
			col.a = 1;
			g_root->SetColor(&col);

			wsize.x = 960.0;
			wsize.y = 960.0;
			wsize.z = 0.0f;
			wsize.w = 0.0f;
			g_root->SetSize(&wsize);

			g_root->SetSurfaceBase(&coverTex);
		}

		return;
	}

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_player");
	playerBusyInd = (ui::BusyIndicator *)g_player_page->GetChild(&searchParam, 0);
	playerBusyInd->Start();

	if (g_currentCoverSurf != SCE_NULL)
		menu::displayfiles::Page::ResetBgPlaneTex();

	if (!g_currentDispFilePage->coverState && (workptr == SCE_NULL)) {
		playerBusyInd->Stop();
		return;
	}

	fres = MemFile::Open(workptr, size, &res);

	if (res < 0) {
		if (isExtMem)
			sce_paf_free(workptr);
		playerBusyInd->Stop();
		return;
	}

	graph::Surface::Create(&coverTex, g_empvaPlugin->memoryPool, (SharedPtr<File>*)&fres);

	if (coverTex == SCE_NULL) {
		fres.reset();
		if (isExtMem)
			sce_paf_free(workptr);
		playerBusyInd->Stop();
		return;
	}

	g_currentCoverSurf = coverTex;

	if (isExtMem)
		sce_paf_free(workptr);

	fres.reset();

	col.r = 0.207;
	col.g = 0.247;
	col.b = 0.286;
	col.a = 1;
	g_root->SetColor(&col);

	wsize.x = 960.0;
	wsize.y = 960.0;
	wsize.z = 0.0f;
	wsize.w = 0.0f;
	g_root->SetSize(&wsize);

	g_root->SetSurfaceBase(&coverTex);

	playerCover->SetSurfaceBase(&coverTex);

	playerBusyInd->Stop();
}

SceVoid audio::YoutubePlayerCoverLoaderJob::Run()
{
	coverTex = SCE_NULL;
	rco::Element searchParam;
	ui::Widget *playerCover;
	ui::BusyIndicator *playerBusyInd;
	Rgba col;
	SceInt32 res;
	SharedPtr<CurlFile> fres;

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChild(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_player");
	playerBusyInd = (ui::BusyIndicator *)g_player_page->GetChild(&searchParam, 0);
	playerBusyInd->Start();

	if (g_currentCoverSurf != SCE_NULL)
		menu::displayfiles::Page::ResetBgPlaneTex();

	fres = CurlFile::Open(url.c_str(), &res, 0, YTUtils::GetCurlFileShare());
	if (res < 0) {
		playerBusyInd->Stop();
		return;
	}

	graph::Surface::Create(&coverTex, g_empvaPlugin->memoryPool, (SharedPtr<File>*)&fres);

	fres.reset();

	if (coverTex == SCE_NULL) {
		playerBusyInd->Stop();
		return;
	}

	g_currentCoverSurf = coverTex;

	playerCover->SetSurfaceBase(&coverTex);

	playerBusyInd->Stop();
}

SceVoid audio::YoutubePlayerCoverLoaderJob::Finish()
{

}

SceVoid audio::PlayerCoverLoaderJob::Finish()
{

}

audio::GenericDecoder::GenericDecoder(const char *path, SceBool isSwDecoderUsed)
{
	isValid = SCE_FALSE;
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	metadata = new audio::GenericDecoder::Metadata();
	metadata->hasCover = SCE_FALSE;
	metadata->hasMeta = SCE_FALSE;

	if (path) {
		dataPath = path;
	}

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
	rco::Element searchParam;

	audio::DecoderCore::SetDecoder(SCE_NULL, SCE_NULL); // Clear channel callback

	sceKernelClearEventFlag(g_eventFlagUid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);

	delete metadata;
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

SceBool audio::GenericDecoder::IsValid()
{
	return isValid;
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