#include <audioout.h>
#include <appmgr.h>
#include <kernel.h>
#include <kernel/rng.h>
#include <power.h>
#include <shellsvc.h>
#include <ctrl.h>
#include <shellaudio.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>
#include <motion.h>

#include "audio.h"
#include "common.h"
#include "menu_displayfiles.h"
#include "menu_audioplayer.h"
#include "motion_e.h"
#include "utils.h"
#include "yt_utils.h"
#include "ipc.h"
#include "vitaaudiolib.h"
#include "youtube_parser.hpp"

using namespace paf;

static SceBool s_isBtCbRegistered = SCE_FALSE;
static SceBool s_isYtBtCbRegistered = SCE_FALSE;

static ui::Widget *s_playerPlane = SCE_NULL;
static ui::Widget *s_ytExt = SCE_NULL;

static graphics::Texture *s_pauseButtonTex;
static graphics::Texture *s_playButtonTex;
static graphics::Texture *s_shuffleOnButtonTex;
static graphics::Texture *s_shuffleOffButtonTex;
static graphics::Texture *s_repeatOnButtonTex;
static graphics::Texture *s_repeatOnOneButtonTex;
static graphics::Texture *s_repeatOffButtonTex;
static graphics::Texture *s_favOffButtonTex;
static graphics::Texture *s_favOnButtonTex;

static menu::audioplayer::PlayerButtonCB *s_playerButtonCb;

static YouTubeVideoDetail *s_ytVidInfo = SCE_NULL;
static SceBool s_ytFirstIdx = SCE_FALSE;

typedef enum RepeatState{
	REPEAT_STATE_NONE,
	REPEAT_STATE_ONE,
	REPEAT_STATE_ALL
} RepeatState;

typedef enum ShuffleState {
	SHUFFLE_STATE_OFF,
	SHUFFLE_STATE_ON
} ShuffleState;

static SceUInt32 s_repeatState = REPEAT_STATE_NONE;
static SceUInt32 s_shuffleState = SHUFFLE_STATE_OFF;

static SceUInt32 s_timerPof = 0;
static SceUInt64 s_oldCurrentPosSec = 0;
static String *s_totalLength;

SceVoid menu::audioplayer::Audioplayer::ReloadCoverForNext()
{
	// Handle cover
	if (!g_currentPlayerInstance->GetCore()->GetDecoder()->GetMetadataLocation()->hasCover) {

		auto coverLoader = new audio::PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
		coverLoader->workptr = SCE_NULL;

		CleanupHandler *req = new CleanupHandler();
		req->userData = coverLoader;
		req->refCount = 0;
		req->unk_08 = 1;
		req->cb = (CleanupHandler::CleanupCallback)audio::PlayerCoverLoaderJob::JobKiller;

		ObjectWithCleanup itemParam;
		itemParam.object = coverLoader;
		itemParam.cleanup = req;

		g_coverJobQueue->Enqueue(&itemParam);
	}
}

SceVoid menu::audioplayer::Audioplayer::_HandleNext(SceBool fromHandlePrev, SceBool fromFfButton)
{
	Resource::Element searchParam;
	SceUInt32 i = 0;
	SceInt32 originalIdx = 0;
	String text8;
	WString text16;

	if (g_currentPlayerInstance->GetCore()->GetDecoder()->IsPaused()) {
		searchParam.hash = EMPVAUtils::GetHash("player_play_button");
		ui::Widget *playButton = s_playerPlane->GetChildByHash(&searchParam, 0);
		playButton->SetTextureBase(s_pauseButtonTex);
		EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 0);
	}

	delete g_currentPlayerInstance->core;
	g_currentPlayerInstance->core = SCE_NULL;

	if (s_repeatState == REPEAT_STATE_ONE && !fromHandlePrev && !fromFfButton) {
		g_currentPlayerInstance->core = new AudioplayerCore(g_currentPlayerInstance->playlist.path[g_currentPlayerInstance->playlistIdx]->data);
		ReloadCoverForNext();
		return;
	}

	if (s_shuffleState == SHUFFLE_STATE_ON) {

		if ((g_currentPlayerInstance->totalConsumedIdx == g_currentPlayerInstance->totalIdx) && (s_repeatState != REPEAT_STATE_ALL)) {
			menu::audioplayer::Audioplayer::Close();
			return;
		}
		else if (g_currentPlayerInstance->totalConsumedIdx == g_currentPlayerInstance->totalIdx) {
			g_currentPlayerInstance->totalConsumedIdx = 0;

			while (g_currentPlayerInstance->playlist.path[i] != SCE_NULL) {
				g_currentPlayerInstance->playlist.isConsumed[i] = 0;
				i++;
			}
		}

		if (!fromHandlePrev && !fromFfButton) {
			while (g_currentPlayerInstance->playlist.isConsumed[g_currentPlayerInstance->playlistIdx]) {
				sceKernelGetRandomNumber(&g_currentPlayerInstance->playlistIdx, sizeof(SceUInt32));
				g_currentPlayerInstance->playlistIdx = g_currentPlayerInstance->playlistIdx % (g_currentPlayerInstance->totalIdx);
			}
		}
		else {
			originalIdx = g_currentPlayerInstance->playlistIdx;
			while (g_currentPlayerInstance->playlistIdx == originalIdx) {
				sceKernelGetRandomNumber(&g_currentPlayerInstance->playlistIdx, sizeof(SceUInt32));
				g_currentPlayerInstance->playlistIdx = g_currentPlayerInstance->playlistIdx % (g_currentPlayerInstance->totalIdx);
			}
		}
	}
	else {
		g_currentPlayerInstance->playlistIdx++;
		if (g_currentPlayerInstance->playlistIdx >= g_currentPlayerInstance->totalIdx)
			g_currentPlayerInstance->playlistIdx = 0;

		if ((g_currentPlayerInstance->playlistIdx == g_currentPlayerInstance->startIdx) && (s_repeatState != REPEAT_STATE_ALL) && !fromHandlePrev && !fromFfButton) {
			menu::audioplayer::Audioplayer::Close();
			return;
		}
	}

	g_currentPlayerInstance->core = new AudioplayerCore(g_currentPlayerInstance->playlist.path[g_currentPlayerInstance->playlistIdx]->data);

	if (!g_currentPlayerInstance->GetCore()->IsValid()) {
		if (fromHandlePrev)
			_HandlePrev();
		else
			_HandleNext(fromHandlePrev, fromFfButton);
		return;
	}

	s_totalLength->Clear();
	ConvertSecondsToString(s_totalLength, g_currentPlayerInstance->GetCore()->GetDecoder()->GetLength() / g_currentPlayerInstance->core->GetDecoder()->GetSampleRate(), SCE_FALSE);

	searchParam.hash = EMPVAUtils::GetHash("text_player_number");
	ui::Widget *numText = g_player_page->GetChildByHash(&searchParam, 0);
	text8.Setf("%u / %u", g_currentPlayerInstance->playlistIdx + 1, g_currentPlayerInstance->totalIdx);
	text8.ToWString(&text16);
	numText->SetLabel(&text16);
	text8.Clear();
	text16.Clear();
	ReloadCoverForNext();
}

SceVoid menu::audioplayer::Audioplayer::_HandlePrev()
{
	if (s_shuffleState == SHUFFLE_STATE_ON) {
		_HandleNext(SCE_TRUE, SCE_FALSE);
		return;
	}

	g_currentPlayerInstance->playlistIdx--;
	if (g_currentPlayerInstance->playlistIdx < 0)
		g_currentPlayerInstance->playlistIdx = g_currentPlayerInstance->totalIdx - 1;

	g_currentPlayerInstance->playlist.isConsumed[g_currentPlayerInstance->playlistIdx] = 0;

	g_currentPlayerInstance->playlistIdx--;
	if (g_currentPlayerInstance->playlistIdx < 0)
		g_currentPlayerInstance->playlistIdx = g_currentPlayerInstance->totalIdx - 1;

	g_currentPlayerInstance->totalConsumedIdx--;

	_HandleNext(SCE_TRUE, SCE_FALSE);
}

SceVoid menu::audioplayer::Audioplayer::YtJobFinishHandler()
{
	if (g_isPlayerActive)
		common::Utils::AddMainThreadTask(RegularTask, SCE_NULL);
}

SceVoid menu::audioplayer::Audioplayer::HandleNext(SceBool fromHandlePrev, SceBool fromFfButton)
{
	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {
		// Dirty hack, but works well enough
		common::Utils::RemoveMainThreadTask(RegularTask, SCE_NULL);
		EMPVAUtils::RunCallbackAsJob((ui::Widget::EventCallback::EventHandler)_HandleNext, YtJobFinishHandler, fromHandlePrev, (ui::Widget *)fromFfButton, 0, SCE_NULL);
	}
	else
		_HandleNext(fromHandlePrev, fromFfButton);
}

SceVoid menu::audioplayer::Audioplayer::HandlePrev()
{
	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {
		// Dirty hack, but works well enough
		common::Utils::RemoveMainThreadTask(RegularTask, SCE_NULL);
		EMPVAUtils::RunCallbackAsJob((ui::Widget::EventCallback::EventHandler)_HandlePrev, YtJobFinishHandler, 0, SCE_NULL, 0, SCE_NULL);
	}
	else
		_HandlePrev();
}

SceVoid menu::audioplayer::Audioplayer::ConvertSecondsToString(String *string, SceUInt64 seconds, SceBool needSeparator)
{
	SceInt32 h = 0, m = 0, s = 0;
	h = (seconds / 3600);
	m = (seconds - (3600 * h)) / 60;
	s = (seconds - (3600 * h) - (m * 60));

	if (needSeparator) {
		if (h > 0)
			string->Setf("%02d:%02d:%02d / ", h, m, s);
		else
			string->Setf("%02d:%02d / ", m, s);
	}
	else {
		if (h > 0)
			string->Setf("%02d:%02d:%02d", h, m, s);
		else
			string->Setf("%02d:%02d", m, s);
	}
}

SceVoid menu::audioplayer::Audioplayer::RegularTask(ScePVoid pUserData)
{
	String text8;
	WString text16;
	WString text16fake;
	SceUInt64 currentPos = 0;
	SceUInt64 currentPosSec = 0;
	SceUInt64 length = 0;
	SceInt32 motionCom = 0;
	SceUInt32 ipcCom = 0;
	SceCtrlData ctrlData;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	Resource::Element searchParam;

	audio::GenericDecoder *currentDecoder = SCE_NULL;

	if (g_currentPlayerInstance) {
		if (g_currentPlayerInstance->GetCore()) {
			currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();
			currentPos = currentDecoder->GetPosition();
		}
	}
	
	menu::settings::Settings *config = menu::settings::Settings::GetInstance();

	// Set progressbar value
	if (!sceKernelPollEventFlag(g_eventFlagUid, FLAG_ELEVENMPVA_IS_FG, SCE_KERNEL_EVF_WAITMODE_AND, SCE_NULL)) {
		searchParam.hash = EMPVAUtils::GetHash("progressbar_player");
		ui::ProgressBarTouch *playerProgBar = (ui::ProgressBarTouch *)g_player_page->GetChildByHash(&searchParam, 0);

		length = currentDecoder->GetLength();
		currentPosSec = (SceUInt64)((SceFloat32)currentPos / (SceFloat32)currentDecoder->GetSampleRate());

		SceFloat32 progress = (SceFloat32)currentPos * 100.0f / (SceFloat32)length;

		playerProgBar->SetProgress(progress, 0, 0);

		if (s_oldCurrentPosSec != currentPosSec || currentPosSec == 0) {

			searchParam.hash = EMPVAUtils::GetHash("text_player_counter");
			commonWidget = g_player_page->GetChildByHash(&searchParam, 0);

			ConvertSecondsToString(&text8, currentPosSec, SCE_TRUE);

			text8.Append(s_totalLength->data, s_totalLength->length);
			text8.ToWString(&text16);

			commonWidget->SetLabel(&text16);
		}

		s_oldCurrentPosSec = currentPosSec;
	}

	if (currentDecoder) {
		// Delay main PAF thread here to reduce CPU load (more battery life)
		if (menu::settings::Settings::GetInstance()->fps_limit && !Misc::IsDolce()) {
			if (!currentDecoder->IsPaused()) {
				sceDisplayWaitVblankStart();
			}
		}

		// Check auto suspend feature
		if (g_currentPlayerInstance != SCE_NULL) {
			if (currentDecoder->IsPaused() && config->power_saving) {
				if ((sceKernelGetProcessTimeLow() - s_timerPof) > 60000000 * config->power_timer)
					menu::audioplayer::Audioplayer::Close();
			}
		}

		// Handle next file
		if (!currentDecoder->isPlaying) {
			if (s_repeatState != REPEAT_STATE_ONE) {
				g_currentPlayerInstance->playlist.isConsumed[g_currentPlayerInstance->playlistIdx] = 1;
				g_currentPlayerInstance->totalConsumedIdx++;
			}
			HandleNext(SCE_FALSE, SCE_FALSE);
			return;
		}

		// Check Shell IPC
		if (!EMPVAUtils::IsSleep()) {
			ipcCom = EMPVAUtils::IPC::PeekTx();
			switch (ipcCom) {
			case EMPVA_IPC_PLAY:
				searchParam.hash = EMPVAUtils::GetHash("player_play_button");
				commonWidget = g_player_page->GetChildByHash(&searchParam, 0);

				currentDecoder->Pause();

				if (currentDecoder->IsPaused()) {
					commonWidget->SetTextureBase(s_playButtonTex);
					EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 1);
				}
				else {
					commonWidget->SetTextureBase(s_pauseButtonTex);
					EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 0);
				}

				s_timerPof = sceKernelGetProcessTimeLow();

				return;
				break;
			case EMPVA_IPC_FF:
				menu::audioplayer::Audioplayer::HandleNext(SCE_FALSE, SCE_TRUE);
				return;
				break;
			case EMPVA_IPC_REW:
				menu::audioplayer::Audioplayer::HandlePrev();
				return;
				break;
			}
		}

		// Check analog stick
		if (EMPVAUtils::IsSleep() && !Misc::IsDolce() && config->stick_skip) {

			sce_paf_memset(&ctrlData, 0, sizeof(SceCtrlData));
			sceCtrlPeekBufferPositive(0, &ctrlData, 1);

			if (ctrlData.rx < 0x10) {
				menu::audioplayer::Audioplayer::HandlePrev();
				return;
			}
			else if (ctrlData.rx > 0xEF) {
				menu::audioplayer::Audioplayer::HandleNext(SCE_FALSE, SCE_TRUE);
				return;
			}
		}

		// Check motion feature
		if (!Misc::IsDolce() && config->motion_mode) {
			motionCom = motion::Motion::GetCommand();
			switch (motionCom) {
			case motion::Motion::MOTION_STOP:

				searchParam.hash = EMPVAUtils::GetHash("player_play_button");
				commonWidget = g_player_page->GetChildByHash(&searchParam, 0);

				currentDecoder->Pause();

				if (currentDecoder->IsPaused()) {
					commonWidget->SetTextureBase(s_playButtonTex);
					EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 1);
				}
				else {
					commonWidget->SetTextureBase(s_pauseButtonTex);
					EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 0);
				}

				s_timerPof = sceKernelGetProcessTimeLow();

				break;
			case motion::Motion::MOTION_NEXT:

				menu::audioplayer::Audioplayer::HandleNext(SCE_FALSE, SCE_TRUE);

				break;
			case motion::Motion::MOTION_PREVIOUS:

				menu::audioplayer::Audioplayer::HandlePrev();

				break;
			}
		}
	}
}

SceVoid menu::audioplayer::Audioplayer::Return()
{
	Resource::Element searchParam;
	Plugin::SceneInitParam rwiParam;

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Player;

	// Hide (disable) displayfiles page
	g_rootPage->PlayAnimationReverse(100.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

	// Get player widgets
	searchParam.hash = EMPVAUtils::GetHash("plane_player_bg");
	s_playerPlane = g_player_page->GetChildByHash(&searchParam, 0);
	s_playerPlane->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube)
		YTUtils::GetMenuSema()->Wait();
}

SceVoid menu::audioplayer::Audioplayer::Close()
{
	Resource::Element searchParam;

	if (*(SceUInt32 *)g_settingsButtonCB->pUserData != menu::settings::SettingsButtonCB::Parent_Displayfiles)
		menu::audioplayer::BackButtonCB::BackButtonCBFun(0, SCE_NULL, 0, SCE_NULL);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
	ui::Widget *buttonPagemode = g_rootPage->GetChildByHash(&searchParam, 0);
	if (buttonPagemode)
		buttonPagemode->PlayAnimation(600.0f, ui::Widget::Animation_Reset);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_player_button");
	ui::Widget *buttonPlayer = g_rootPage->GetChildByHash(&searchParam, 0);
	buttonPlayer->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	delete g_currentPlayerInstance;
}

SceVoid menu::audioplayer::BackButtonCB::BackButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;

	// Show (enable) displayfiles page
	g_rootPage->PlayAnimation(-1000.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

	// Get hashes for animations and play them in reverse
	searchParam.hash = EMPVAUtils::GetHash("plane_player_bg");
	ui::Widget *playerPlane = g_player_page->GetChildByHash(&searchParam, 0);
	playerPlane->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube)
		YTUtils::GetMenuSema()->Signal();
}

SceVoid menu::audioplayer::PlayerButtonCB::PlayerButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;
	ui::ProgressBarTouch *bar = SCE_NULL;
	SceBool isPreSeekPaused = SCE_FALSE;

	audio::GenericDecoder *currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();

	switch (self->hash) {
	case ButtonHash_Play:

		currentDecoder->Pause();

		if (currentDecoder->IsPaused()) {
			self->SetTextureBase(s_playButtonTex);
			EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 1);
		}
		else {
			self->SetTextureBase(s_pauseButtonTex);
			EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 0);
		}

		s_timerPof = sceKernelGetProcessTimeLow();

		break;
	case ButtonHash_Rew:

		menu::audioplayer::Audioplayer::HandlePrev();

		break;
	case ButtonHash_Ff:

		menu::audioplayer::Audioplayer::HandleNext(SCE_FALSE, SCE_TRUE);

		break;
	case ButtonHash_Repeat:

		if (s_repeatState == REPEAT_STATE_ONE) {
			s_repeatState = REPEAT_STATE_NONE;
			self->SetTextureBase(s_repeatOffButtonTex);
		}
		else if (s_repeatState == REPEAT_STATE_ALL) {
			s_repeatState = REPEAT_STATE_ONE;
			self->SetTextureBase(s_repeatOnOneButtonTex);
		}
		else {
			s_repeatState = REPEAT_STATE_ALL;
			self->SetTextureBase(s_repeatOnButtonTex);
		}

		break;
	case ButtonHash_Shuffle:

		if (s_shuffleState == SHUFFLE_STATE_ON) {
			s_shuffleState = SHUFFLE_STATE_OFF;
			self->SetTextureBase(s_shuffleOffButtonTex);
		}
		else {
			s_shuffleState = SHUFFLE_STATE_ON;
			self->SetTextureBase(s_shuffleOnButtonTex);
		}

		break;
	case ButtonHash_Progressbar:

		bar = (ui::ProgressBarTouch *)self;

		isPreSeekPaused = currentDecoder->IsPaused();

		if (!isPreSeekPaused && EMPVAUtils::IsDecoderUsed())
			currentDecoder->Pause();

		currentDecoder->Seek(bar->currentValue);

		if (currentDecoder->IsPaused() && EMPVAUtils::IsDecoderUsed() && !isPreSeekPaused)
			currentDecoder->Pause();

		break;
	case ButtonHash_Close:

		menu::audioplayer::Audioplayer::Close();

		break;
	case ButtonHash_Favourite:

		char *idptr = sce_paf_strchr(g_currentPlayerInstance->playlist.path[0]->data, '=');
		idptr += 1;

		if (!YTUtils::GetFavLog()->Get(idptr)) {
			YTUtils::GetFavLog()->Remove(idptr);
			self->SetTextureBase(s_favOffButtonTex);
		}
		else {
			YTUtils::GetFavLog()->Add(idptr);
			self->SetTextureBase(s_favOnButtonTex);
		}

		break;
	}
}

menu::audioplayer::Audioplayer::Audioplayer(const char *cwd, menu::displayfiles::File *startFile, Mode mode)
{
	String text8;
	WString text16;
	String fullPath;
	Resource::Element searchParam;
	Plugin::SceneInitParam rwiParam;
	Plugin::TemplateInitParam tmpParam;
	graphics::Texture coverTex;
	ui::Widget *playerCover = SCE_NULL;
	ui::Widget *commonWidget;
	ui::Widget *numText;
	ui::Widget *playButton;
	audio::GenericDecoder::Metadata *meta;
	menu::settings::Settings *config = menu::settings::Settings::GetInstance();

	if (g_currentPlayerInstance)
		delete g_currentPlayerInstance;

	playlistIdx = 0;
	s_timerPof = 0;

	// Hide (disable) root page
	g_rootPage->PlayAnimationReverse(100.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
	ui::Widget *buttonPagemode = g_rootPage->GetChildByHash(&searchParam, 0);
	if (buttonPagemode)
		buttonPagemode->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_player_button");
	ui::Widget *buttonPlayer = g_rootPage->GetChildByHash(&searchParam, 0);
	buttonPlayer->PlayAnimation(600.0f, ui::Widget::Animation_Reset);

	// Create player scene
	searchParam.hash = EMPVAUtils::GetHash("page_player");
	g_player_page = g_empvaPlugin->CreateScene(&searchParam, &rwiParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChildByHash(&searchParam, 0);

	g_isPlayerActive = SCE_TRUE;

	// Get player widgets
	searchParam.hash = EMPVAUtils::GetHash("plane_player_bg");
	s_playerPlane = g_player_page->GetChildByHash(&searchParam, 0);
	s_playerPlane->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

	if (!s_isBtCbRegistered) { // Register player plane button callbacks (one time per app lifetime)

		searchParam.hash = EMPVAUtils::GetHash("player_settings_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, g_settingsButtonCB, 0);

		auto backButtonCb = new menu::audioplayer::BackButtonCB();
		searchParam.hash = EMPVAUtils::GetHash("player_back_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, backButtonCb, 0);

		s_playerButtonCb = new menu::audioplayer::PlayerButtonCB();
		searchParam.hash = EMPVAUtils::GetHash("player_play_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("player_rew_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);
		commonWidget->AssignButton(SCE_CTRL_L1);

		searchParam.hash = EMPVAUtils::GetHash("player_ff_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);
		commonWidget->AssignButton(SCE_CTRL_R1);

		searchParam.hash = EMPVAUtils::GetHash("player_shuffle_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("player_repeat_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("player_close_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);
		commonWidget->AssignButton(0);

		searchParam.hash = EMPVAUtils::GetHash("progressbar_player");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000003, s_playerButtonCb, 0);

		s_pauseButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_pause");
		Plugin::LoadTexture(s_pauseButtonTex, g_empvaPlugin, &searchParam);

		s_playButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_play");
		Plugin::LoadTexture(s_playButtonTex, g_empvaPlugin, &searchParam);

		s_shuffleOffButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_shuffle");
		Plugin::LoadTexture(s_shuffleOffButtonTex, g_empvaPlugin, &searchParam);

		s_shuffleOnButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_shuffle_glow");
		Plugin::LoadTexture(s_shuffleOnButtonTex, g_empvaPlugin, &searchParam);

		s_repeatOffButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_repeat");
		Plugin::LoadTexture(s_repeatOffButtonTex, g_empvaPlugin, &searchParam);

		s_repeatOnButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_repeat_glow");
		Plugin::LoadTexture(s_repeatOnButtonTex, g_empvaPlugin, &searchParam);

		s_repeatOnOneButtonTex = new graphics::Texture();
		searchParam.hash = EMPVAUtils::GetHash("tex_button_repeat_glow_one");
		Plugin::LoadTexture(s_repeatOnOneButtonTex, g_empvaPlugin, &searchParam);

		s_isBtCbRegistered = SCE_TRUE;
	}

	// Player init
	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Player;

	sceAppMgrAcquireBgmPortWithPriority(0x81);

	EMPVAUtils::SetPowerTickTask(SCE_TRUE);

	if (mode == Mode_Youtube) {

		char url[256];

		YTUtils::GetMenuSema()->Wait();
		youtube_get_video_url_by_id(cwd, url, sizeof(url));
		s_ytVidInfo = youtube_parse_video_page(url);
		if (s_ytVidInfo->playlist.videos.size() && s_ytVidInfo->playlist.selected_index > 0) {
			YouTubeVideoDetail *ytVidInfoOld = s_ytVidInfo;
			s_ytVidInfo = youtube_parse_video_page((char *)ytVidInfoOld->playlist.videos[0].url.c_str());
			youtube_destroy_struct(ytVidInfoOld);
		}

		s_ytFirstIdx = SCE_TRUE;
	}

	GetMusicList(startFile);

	if (totalIdx <= 1) {
		searchParam.hash = EMPVAUtils::GetHash("player_ff_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

		searchParam.hash = EMPVAUtils::GetHash("player_rew_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);
	}
	else {
		searchParam.hash = EMPVAUtils::GetHash("player_ff_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

		searchParam.hash = EMPVAUtils::GetHash("player_rew_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);
	}

	switch (mode) {
	case Mode_Normal:

		fullPath = cwd;
		fullPath.Append(startFile->name->string.data, startFile->name->string.length);

		core = new AudioplayerCore(fullPath.data);

		if (!core->IsValid()) {
			s_totalLength = new String();
			g_currentPlayerInstance = this;
			return;
		};

		// Handle cover
		meta = core->GetDecoder()->GetMetadataLocation();
		if (!meta->hasCover) {

			auto coverLoader = new audio::PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
			coverLoader->workptr = SCE_NULL;

			CleanupHandler *req = new CleanupHandler();
			req->userData = coverLoader;
			req->refCount = 0;
			req->unk_08 = 1;
			req->cb = (CleanupHandler::CleanupCallback)audio::PlayerCoverLoaderJob::JobKiller;

			ObjectWithCleanup itemParam;
			itemParam.object = coverLoader;
			itemParam.cleanup = req;

			g_coverJobQueue->Enqueue(&itemParam);
		}

		break;
	case Mode_Youtube:

		if (!s_isYtBtCbRegistered) {

			searchParam.hash = EMPVAUtils::GetHash("player_template_youtube");
			g_empvaPlugin->TemplateOpen(s_playerPlane, &searchParam, &tmpParam);

			s_favOnButtonTex = new graphics::Texture();
			searchParam.hash = EMPVAUtils::GetHash("tex_yt_icon_favourite_for_player_glow");
			Plugin::LoadTexture(s_favOnButtonTex, g_empvaPlugin, &searchParam);

			s_favOffButtonTex = new graphics::Texture();
			searchParam.hash = EMPVAUtils::GetHash("tex_yt_icon_favourite_for_player");
			Plugin::LoadTexture(s_favOffButtonTex, g_empvaPlugin, &searchParam);

			searchParam.hash = EMPVAUtils::GetHash("player_fav_button");
			commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
			commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

			s_isYtBtCbRegistered = SCE_TRUE;
		}
		else {
			searchParam.hash = EMPVAUtils::GetHash("player_fav_button");
			commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		}

		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);

		YTUtils::GetFavLog()->Reset();

		if (!YTUtils::GetFavLog()->Get(playlist.path[0]->data)) {
			commonWidget->SetTextureBase(s_favOnButtonTex);
		}

		core = new AudioplayerCore(playlist.path[0]->data);

		if (!core->IsValid()) {
			s_totalLength = new String();
			g_currentPlayerInstance = this;
			return;
		};

		break;
	}

	searchParam.hash = EMPVAUtils::GetHash("text_player_number");
	numText = g_player_page->GetChildByHash(&searchParam, 0);
	text8.Setf("%u / %u", playlistIdx + 1, totalIdx);
	text8.ToWString(&text16);
	numText->SetLabel(&text16);

	s_totalLength = new String();

	ConvertSecondsToString(s_totalLength, core->GetDecoder()->GetLength() / core->GetDecoder()->GetSampleRate(), SCE_FALSE);

	searchParam.hash = EMPVAUtils::GetHash("player_play_button");
	playButton = s_playerPlane->GetChildByHash(&searchParam, 0);
	playButton->SetTextureBase(s_pauseButtonTex);

	if (!Misc::IsDolce() && config->motion_mode) {
		motion::Motion::SetState(SCE_TRUE);
		motion::Motion::SetReleaseTimer(config->motion_timer);
		motion::Motion::SetAngleThreshold(config->motion_degree);
	}

	g_currentPlayerInstance = this;

	common::Utils::AddMainThreadTask(RegularTask, SCE_NULL);
}

menu::audioplayer::Audioplayer::~Audioplayer()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;
	SceInt32 i = 0;

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {
		searchParam.hash = EMPVAUtils::GetHash("player_fav_button");
		commonWidget = s_playerPlane->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1, SCE_NULL);
	}

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;
	common::Utils::RemoveMainThreadTask(RegularTask, SCE_NULL);
	if (core != SCE_NULL)
		delete core;

	g_currentPlayerInstance = SCE_NULL;

	EMPVAUtils::SetPowerTickTask(SCE_FALSE);
	sceAppMgrReleaseBgmPort();
	audio::Utils::ResetBgmMode();

	while (playlist.path[i] != SCE_NULL) {
		playlist.path[i]->Clear();
		delete playlist.path[i];
		i++;
	}

	if (!Misc::IsDolce())
		motion::Motion::SetState(SCE_FALSE);

	s_totalLength->Clear();
	delete s_totalLength;

	g_isPlayerActive = SCE_FALSE;
}

SceVoid menu::audioplayer::Audioplayer::GetMusicList(menu::displayfiles::File *startFile)
{
	SceInt32 i = 0;

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {

		if (s_ytVidInfo->playlist.videos.size()) {
			for (SceInt32 j = 0; j < s_ytVidInfo->playlist.videos.size(); j++) {
				playlist.path[i] = new String(s_ytVidInfo->playlist.videos[j].url.c_str());
				playlist.isConsumed[i] = 0;
				i++;
			}
		}
		else {
			playlist.path[i] = new String(s_ytVidInfo->url.c_str());
			playlist.isConsumed[i] = 0;
			i = 1;
		}

		playlistIdx = 0;
		startIdx = 0;

	}
	else {
		menu::displayfiles::File *file = g_currentDispFilePage->files;

		while (file != SCE_NULL) {

			if (file->type == menu::displayfiles::File::Type_Music) {
				playlist.path[i] = new String(g_currentDispFilePage->cwd->data, g_currentDispFilePage->cwd->length);
				playlist.path[i]->Append(file->name->string.data, file->name->string.length);
				playlist.isConsumed[i] = 0;

				if (startFile == file) {
					playlistIdx = i;
					startIdx = i;
				}

				i++;
			}

			file = file->next;
		}
	}

	totalIdx = i;
	playlist.path[i] = SCE_NULL;
	totalConsumedIdx = 0;
}

menu::audioplayer::AudioplayerCore *menu::audioplayer::Audioplayer::GetCore()
{
	return core;
}

menu::audioplayer::AudioplayerCore::AudioplayerCore(const char *file)
{
	SceUInt8 retryAttempt = 0;
	SceUInt32 decoderType = EMPVAUtils::GetDecoderType(file);

	switch (decoderType) {
	case 0:
	case 1:
	case 2:
	case 3:
		decoder = new audio::XmDecoder(file, SCE_TRUE);
		break;
	case 4:
	case 5:
	case 6:
		decoder = new audio::At3Decoder(file, SCE_TRUE);
		break;
	case 7:
		decoder = new audio::OggDecoder(file, SCE_TRUE);
		break;
	case 8:
		decoder = new audio::Mp3Decoder(file, SCE_FALSE);
		break;
	case 9:
		decoder = new audio::OpDecoder(file, SCE_TRUE);
		break;
	case 10:
		decoder = new audio::FlacDecoder(file, SCE_TRUE);
		break;
	case 11:
	case 12:
	case 13:
	case 14:
		decoder = new audio::ShellCommonDecoder(file, SCE_FALSE);
		break;
	case 15:
		decoder = new audio::WebmOpusDecoder(file, SCE_TRUE);
		break;
	case 1000:

		if (!s_ytFirstIdx) {
			s_ytVidInfo = youtube_parse_video_page((char *)file);

			if (!s_ytVidInfo->audio_stream_url.length()) {
				while (!s_ytVidInfo->audio_stream_url.length() && retryAttempt != k_ytRetryAttemptNum) {
					thread::Sleep(100);
					s_ytVidInfo = youtube_parse_video_page((char *)file);
					retryAttempt++;
				}
			}
		}
		else
			s_ytFirstIdx = SCE_FALSE;

		if (!s_ytVidInfo->audio_stream_url.length()) {
			decoder = new audio::YoutubeDecoder(SCE_NULL, SCE_TRUE);
		}
		else {
			decoder = new audio::YoutubeDecoder(s_ytVidInfo->audio_stream_url.c_str(), SCE_TRUE);
		}

		// Cover handler for YouTube
		auto coverLoader = new audio::YoutubePlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
		char url[256];
		char id[32];
		char *idptr = sce_paf_strchr(s_ytVidInfo->url.c_str(), '=');
		char *listptr = sce_paf_strchr(idptr, '&');
		idptr += 1;

		sce_paf_memset(id, 0, sizeof(id));
		if (listptr)
			sce_paf_strncpy(id, idptr, listptr - idptr);
		else
			sce_paf_strncpy(id, idptr, sizeof(id));

		youtube_get_video_thumbnail_hq_url_by_id(id, url, sizeof(url));
		coverLoader->url = url;

		CleanupHandler *req = new CleanupHandler();
		req->userData = coverLoader;
		req->refCount = 0;
		req->unk_08 = 1;
		req->cb = (CleanupHandler::CleanupCallback)audio::YoutubePlayerCoverLoaderJob::JobKiller;

		ObjectWithCleanup itemParam;
		itemParam.object = coverLoader;
		itemParam.cleanup = req;

		g_coverJobQueue->Enqueue(&itemParam);

		break;
	}

	isValid = decoder->IsValid();

	if (isValid) {

		SetInitialParams();

		SetMetadata(file);

		EMPVAUtils::IPC::Enable();
	}
}

menu::audioplayer::AudioplayerCore::~AudioplayerCore()
{
	delete decoder;
	decoder = SCE_NULL;

	if (s_ytVidInfo) {
		youtube_destroy_struct(s_ytVidInfo);
		s_ytVidInfo = SCE_NULL;
	}

	EMPVAUtils::IPC::Disable();
}

SceBool menu::audioplayer::AudioplayerCore::IsValid()
{
	return isValid;
}

audio::GenericDecoder *menu::audioplayer::AudioplayerCore::GetDecoder()
{
	return decoder;
}

SceVoid menu::audioplayer::AudioplayerCore::SetInitialParams()
{
	SceBool isDecoderUsed = EMPVAUtils::IsDecoderUsed();
	menu::settings::Settings *config = menu::settings::Settings::GetInstance();

	if (isDecoderUsed) {
		sceAudioOutSetEffectType(config->eq_mode);
		sceAudioOutSetAlcMode(config->alc_mode);
	}
	else {
		sceMusicPlayerServiceSetEQ(config->eq_mode);
		sceMusicPlayerServiceSetALC(config->alc_mode);
	}
}

SceVoid menu::audioplayer::AudioplayerCore::SetMetadata(const char *file)
{
	String text8;
	WString text16;
	Resource::Element searchParam;
	ui::Widget *textTitle;
	ui::Widget *textAlbum;
	ui::Widget *textArtist;

	searchParam.hash = EMPVAUtils::GetHash("text_player_title");
	textTitle = g_player_page->GetChildByHash(&searchParam, 0);
	searchParam.hash = EMPVAUtils::GetHash("text_player_album");
	textAlbum = g_player_page->GetChildByHash(&searchParam, 0);
	searchParam.hash = EMPVAUtils::GetHash("text_player_artist");
	textArtist = g_player_page->GetChildByHash(&searchParam, 0);

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {

		WString title;
		WString album;

		if (s_ytVidInfo->playlist.videos.size()) {
			text8 = s_ytVidInfo->playlist.title.c_str();
			text8.ToWString(&album);
			textAlbum->SetLabel(&album);
		}
		else {
			textAlbum->SetLabel(&album);
		}

		text8 = s_ytVidInfo->author.name.c_str();
		text8.ToWString(&text16);
		textArtist->SetLabel(&text16);

		text8 = s_ytVidInfo->title.c_str();
		text8.ToWString(&title);
		textTitle->SetLabel(&title);

		EMPVAUtils::IPC::SendInfo(&title, &text16, &album, -1);
	}
	else {
		char *name = sce_paf_strrchr(file, '/');
		name = name + 1;
		audio::GenericDecoder::Metadata *metadata = decoder->GetMetadataLocation();

		textAlbum->SetLabel(&text16);
		textArtist->SetLabel(&text16);
		textTitle->SetLabel(&text16);

		if (metadata->hasMeta) {
			textAlbum->SetLabel(&metadata->album);
			textArtist->SetLabel(&metadata->artist);

			if (metadata->title.length == 0) {
				text8 = name;
				text8.ToWString(&metadata->title);
			}
		}

		if (!metadata->hasMeta) {
			text8 = name;
			text8.ToWString(&metadata->title);
		}

		textTitle->SetLabel(&metadata->title);

		EMPVAUtils::IPC::SendInfo(&metadata->title, &metadata->artist, &metadata->album, -1);
	}
}