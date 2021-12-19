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

SceVoid audio::PlayerCoverLoaderJob::Run()
{
	coverTex.texSurface = SCE_NULL;
	Resource::Element searchParam;
	ui::Widget *playerCover;
	ui::BusyIndicator *playerBusyInd;
	ui::Widget::Color col;
	SceFVector4 wsize;
	ObjectWithCleanup fres;
	SceInt32 res;

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChildByHash(&searchParam, 0);

	if (g_currentDispFilePage->coverState) {
		if (g_currentCoverSurf != SCE_NULL) {
			coverTex.texSurface = g_currentCoverSurf;
			playerCover->SetTextureBase(&coverTex);

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

			g_root->SetTextureBase(&coverTex);
		}

		return;
	}

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_player");
	playerBusyInd = (ui::BusyIndicator *)g_player_page->GetChildByHash(&searchParam, 0);
	playerBusyInd->Start();

	if (g_currentCoverSurf != SCE_NULL)
		menu::displayfiles::Page::ResetBgPlaneTex();

	if (!g_currentDispFilePage->coverState && (workptr == SCE_NULL)) {
		playerBusyInd->Stop();
		return;
	}

	MemFile::Open(&fres, workptr, size, &res);

	if (res < 0) {
		if (isExtMem)
			sce_paf_free(workptr);
		playerBusyInd->Stop();
		return;
	}

	graphics::Texture::CreateFromFile(&coverTex, g_empvaPlugin->memoryPool, &fres);

	if (coverTex.texSurface == SCE_NULL) {
		fres.cleanup->cb(fres.object);
		delete fres.cleanup;
		if (isExtMem)
			sce_paf_free(workptr);
		playerBusyInd->Stop();
		return;
	}

	g_currentCoverSurf = coverTex.texSurface;

	if (isExtMem)
		sce_paf_free(workptr);

	fres.cleanup->cb(fres.object);
	delete fres.cleanup;

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

	g_root->SetTextureBase(&coverTex);

	playerCover->SetTextureBase(&coverTex);

	playerBusyInd->Stop();
}

SceVoid audio::YoutubePlayerCoverLoaderJob::Run()
{
	coverTex.texSurface = SCE_NULL;
	Resource::Element searchParam;
	ui::Widget *playerCover;
	ui::BusyIndicator *playerBusyInd;
	ui::Widget::Color col;
	SceFVector4 wsize;
	ObjectWithCleanup fres;
	SceInt32 res;
	SceUInt32 retryCount = 0;

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChildByHash(&searchParam, 0);

	searchParam.hash = EMPVAUtils::GetHash("busyindicator_player");
	playerBusyInd = (ui::BusyIndicator *)g_player_page->GetChildByHash(&searchParam, 0);
	playerBusyInd->Start();

	if (g_currentCoverSurf != SCE_NULL)
		menu::displayfiles::Page::ResetBgPlaneTex();

	HttpFile::Open(&fres, url.data, &res, 0);

	while (res < 0 && retryCount != 3) {
		HttpFile::Open(&fres, url.data, &res, 0);
		retryCount++;
	}

	if (res < 0) {
		playerBusyInd->Stop();
		return;
	}

	graphics::Texture::CreateFromFile(&coverTex, g_empvaPlugin->memoryPool, &fres);

	fres.cleanup->cb(fres.object);
	delete fres.cleanup;

	if (coverTex.texSurface == SCE_NULL) {
		playerBusyInd->Stop();
		return;
	}

	g_currentCoverSurf = coverTex.texSurface;

	playerCover->SetTextureBase(&coverTex);

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
	Resource::Element searchParam;
	ui::Widget *playerCover;

	audio::DecoderCore::SetDecoder(SCE_NULL, SCE_NULL); // Clear channel callback

	sceKernelClearEventFlag(g_eventFlagUid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);

	metadata->title.Clear();
	metadata->album.Clear();
	metadata->artist.Clear();
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