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
#include "invidious.h"

using namespace paf;

static SceBool s_isBtCbRegistered = SCE_FALSE;
static SceBool s_isYtBtCbRegistered = SCE_FALSE;

static ui::Widget *s_playerPlane = SCE_NULL;

static graph::Surface *s_pauseButtonTex;
static graph::Surface *s_playButtonTex;
static graph::Surface *s_shuffleOnButtonTex;
static graph::Surface *s_shuffleOffButtonTex;
static graph::Surface *s_repeatOnButtonTex;
static graph::Surface *s_repeatOnOneButtonTex;
static graph::Surface *s_repeatOffButtonTex;
static graph::Surface *s_favOffButtonTex;
static graph::Surface *s_favOnButtonTex;

static menu::audioplayer::PlayerButtonCB *s_playerButtonCb;

static InvItemVideo *s_ytVidInfo = SCE_NULL;
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
static string *s_totalLength;

SceVoid menu::audioplayer::Audioplayer::ReloadCoverForNext()
{
	// Handle cover
	if (!g_currentPlayerInstance->GetCore()->GetDecoder()->GetMetadataLocation()->hasCover) {

		auto coverLoader = new audio::PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
		coverLoader->workptr = SCE_NULL;

		SharedPtr<job::JobItem> itemParam(coverLoader);

		g_coverJobQueue->Enqueue(&itemParam);
	}
}

SceVoid menu::audioplayer::Audioplayer::_HandleNext(SceBool fromHandlePrev, SceBool fromFfButton)
{
	rco::Element searchParam;
	SceUInt32 i = 0;
	SceInt32 originalIdx = 0;
	string text8;
	wstring text16;

	if (g_currentPlayerInstance->GetCore()->GetDecoder()->IsPaused()) {
		searchParam.hash = EMPVAUtils::GetHash("player_play_button");
		ui::Widget *playButton = s_playerPlane->GetChild(&searchParam, 0);
		playButton->SetSurfaceBase(&s_pauseButtonTex);
		EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 0);
	}

	delete g_currentPlayerInstance->core;
	g_currentPlayerInstance->core = SCE_NULL;

	if (s_repeatState == REPEAT_STATE_ONE && !fromHandlePrev && !fromFfButton) {
		g_currentPlayerInstance->core = new AudioplayerCore(g_currentPlayerInstance->playlist.path[g_currentPlayerInstance->playlistIdx]->c_str());
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

	g_currentPlayerInstance->core = new AudioplayerCore(g_currentPlayerInstance->playlist.path[g_currentPlayerInstance->playlistIdx]->c_str());

	if (!g_currentPlayerInstance->GetCore()->IsValid()) {
		if (fromHandlePrev)
			_HandlePrev();
		else
			_HandleNext(fromHandlePrev, fromFfButton);
		return;
	}

	s_totalLength->clear();
	ConvertSecondsToString(s_totalLength, g_currentPlayerInstance->GetCore()->GetDecoder()->GetLength() / g_currentPlayerInstance->core->GetDecoder()->GetSampleRate(), SCE_FALSE);

	searchParam.hash = EMPVAUtils::GetHash("text_player_number");
	ui::Widget *numText = g_player_page->GetChild(&searchParam, 0);
	text8 = ccc::Sprintf("%u / %u", g_currentPlayerInstance->playlistIdx + 1, g_currentPlayerInstance->totalIdx);
	ccc::UTF8toUTF16(&text8, &text16);
	numText->SetLabel(&text16);
	text8.clear();
	text16.clear();
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
		task::Register(RegularTask, SCE_NULL);
}

SceVoid menu::audioplayer::Audioplayer::HandleNext(SceBool fromHandlePrev, SceBool fromFfButton)
{
	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {
		// Dirty hack, but works well enough
		task::Unregister(RegularTask, SCE_NULL);
		EMPVAUtils::RunCallbackAsJob((ui::EventCallback::EventHandler)_HandleNext, YtJobFinishHandler, fromHandlePrev, (ui::Widget *)fromFfButton, 0, SCE_NULL);
	}
	else
		_HandleNext(fromHandlePrev, fromFfButton);
}

SceVoid menu::audioplayer::Audioplayer::HandlePrev()
{
	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {
		// Dirty hack, but works well enough
		task::Unregister(RegularTask, SCE_NULL);
		EMPVAUtils::RunCallbackAsJob((ui::EventCallback::EventHandler)_HandlePrev, YtJobFinishHandler, 0, SCE_NULL, 0, SCE_NULL);
	}
	else
		_HandlePrev();
}

SceVoid menu::audioplayer::Audioplayer::ConvertSecondsToString(string *string, SceUInt64 seconds, SceBool needSeparator)
{
	SceInt32 h = 0, m = 0, s = 0;
	h = (seconds / 3600);
	m = (seconds - (3600 * h)) / 60;
	s = (seconds - (3600 * h) - (m * 60));

	if (needSeparator) {
		if (h > 0) {
			string->clear();
			*string = ccc::Sprintf("%02d:%02d:%02d / ", h, m, s);
		}
		else {
			string->clear();
			*string = ccc::Sprintf("%02d:%02d / ", m, s);
		}
	}
	else {
		if (h > 0) {
			string->clear();
			*string = ccc::Sprintf("%02d:%02d:%02d", h, m, s);
		}
		else {
			string->clear();
			*string = ccc::Sprintf("%02d:%02d", m, s);
		}
	}
}

SceVoid menu::audioplayer::Audioplayer::RegularTask(ScePVoid pUserData)
{
	string text8;
	wstring text16;
	wstring text16fake;
	SceUInt64 currentPos = 0;
	SceUInt64 currentPosSec = 0;
	SceUInt64 length = 0;
	SceInt32 motionCom = 0;
	SceUInt32 ipcCom = 0;
	SceCtrlData ctrlData;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	rco::Element searchParam;

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
		ui::ProgressBarTouch *playerProgBar = (ui::ProgressBarTouch *)g_player_page->GetChild(&searchParam, 0);

		length = currentDecoder->GetLength();
		currentPosSec = (SceUInt64)((SceFloat32)currentPos / (SceFloat32)currentDecoder->GetSampleRate());

		SceFloat32 progress = (SceFloat32)currentPos * 100.0f / (SceFloat32)length;

		playerProgBar->SetProgress(progress, 0, 0);

		if (s_oldCurrentPosSec != currentPosSec || currentPosSec == 0) {

			searchParam.hash = EMPVAUtils::GetHash("text_player_counter");
			commonWidget = g_player_page->GetChild(&searchParam, 0);

			ConvertSecondsToString(&text8, currentPosSec, SCE_TRUE);

			text8.append(s_totalLength->c_str(), s_totalLength->length());

			ccc::UTF8toUTF16(&text8, &text16);

			commonWidget->SetLabel(&text16);
		}

		s_oldCurrentPosSec = currentPosSec;
	}

	if (currentDecoder) {
		// Delay main PAF thread here to reduce CPU load (more battery life)
		if (menu::settings::Settings::GetInstance()->fps_limit && !SCE_PAF_IS_DOLCE) {
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
				commonWidget = g_player_page->GetChild(&searchParam, 0);

				currentDecoder->Pause();

				if (currentDecoder->IsPaused()) {
					commonWidget->SetSurfaceBase(&s_playButtonTex);
					EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 1);
				}
				else {
					commonWidget->SetSurfaceBase(&s_pauseButtonTex);
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
		if (EMPVAUtils::IsSleep() && !SCE_PAF_IS_DOLCE && config->stick_skip) {

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
		if (!SCE_PAF_IS_DOLCE && config->motion_mode) {
			motionCom = motion::Motion::GetCommand();
			switch (motionCom) {
			case motion::Motion::MOTION_STOP:

				searchParam.hash = EMPVAUtils::GetHash("player_play_button");
				commonWidget = g_player_page->GetChild(&searchParam, 0);

				currentDecoder->Pause();

				if (currentDecoder->IsPaused()) {
					commonWidget->SetSurfaceBase(&s_playButtonTex);
					EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 1);
				}
				else {
					commonWidget->SetSurfaceBase(&s_pauseButtonTex);
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
	rco::Element searchParam;
	Plugin::PageInitParam rwiParam;

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Player;

	// Hide (disable) displayfiles page
	g_rootPage->PlayEffectReverse(100.0f, effect::EffectType_Fadein1, SCE_NULL);

	// Get player widgets
	searchParam.hash = EMPVAUtils::GetHash("plane_player_bg");
	s_playerPlane = g_player_page->GetChild(&searchParam, 0);
	s_playerPlane->PlayEffect(0.0f, effect::EffectType_Fadein1, SCE_NULL);

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube)
		YTUtils::LockMenuParsers();
}

SceVoid menu::audioplayer::Audioplayer::Close()
{
	rco::Element searchParam;

	if (*(SceUInt32 *)g_settingsButtonCB->pUserData != menu::settings::SettingsButtonCB::Parent_Displayfiles)
		menu::audioplayer::BackButtonCB::BackButtonCBFun(0, SCE_NULL, 0, SCE_NULL);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
	ui::Widget *buttonPagemode = g_rootPage->GetChild(&searchParam, 0);
	if (buttonPagemode)
		buttonPagemode->PlayEffect(600.0f, effect::EffectType_Reset);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_player_button");
	ui::Widget *buttonPlayer = g_rootPage->GetChild(&searchParam, 0);
	buttonPlayer->PlayEffectReverse(0.0f, effect::EffectType_Reset);

	delete g_currentPlayerInstance;
}

SceVoid menu::audioplayer::BackButtonCB::BackButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	rco::Element searchParam;

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;

	// Show (enable) displayfiles page
	g_rootPage->PlayEffect(-1000.0f, effect::EffectType_Fadein1, SCE_NULL);

	// Get hashes for animations and play them in reverse
	searchParam.hash = EMPVAUtils::GetHash("plane_player_bg");
	ui::Widget *playerPlane = g_player_page->GetChild(&searchParam, 0);
	playerPlane->PlayEffectReverse(0.0f, effect::EffectType_Fadein1, SCE_NULL);

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube)
		YTUtils::UnlockMenuParsers();
}

SceVoid menu::audioplayer::PlayerButtonCB::PlayerButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	rco::Element searchParam;
	ui::ProgressBarTouch *bar = SCE_NULL;
	SceBool isPreSeekPaused = SCE_FALSE;

	audio::GenericDecoder *currentDecoder = g_currentPlayerInstance->GetCore()->GetDecoder();

	switch (self->elem.hash) {
	case ButtonHash_Play:

		currentDecoder->Pause();

		if (currentDecoder->IsPaused()) {
			self->SetSurfaceBase(&s_playButtonTex);
			EMPVAUtils::IPC::SendInfo(SCE_NULL, SCE_NULL, SCE_NULL, 1);
		}
		else {
			self->SetSurfaceBase(&s_pauseButtonTex);
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
			self->SetSurfaceBase(&s_repeatOffButtonTex);
		}
		else if (s_repeatState == REPEAT_STATE_ALL) {
			s_repeatState = REPEAT_STATE_ONE;
			self->SetSurfaceBase(&s_repeatOnOneButtonTex);
		}
		else {
			s_repeatState = REPEAT_STATE_ALL;
			self->SetSurfaceBase(&s_repeatOnButtonTex);
		}

		break;
	case ButtonHash_Shuffle:

		if (s_shuffleState == SHUFFLE_STATE_ON) {
			s_shuffleState = SHUFFLE_STATE_OFF;
			self->SetSurfaceBase(&s_shuffleOffButtonTex);
		}
		else {
			s_shuffleState = SHUFFLE_STATE_ON;
			self->SetSurfaceBase(&s_shuffleOnButtonTex);
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

		const char *idptr = s_ytVidInfo->id;

		if (!YTUtils::GetFavLog()->Get(idptr)) {
			YTUtils::GetFavLog()->Remove(idptr);
			self->SetSurfaceBase(&s_favOffButtonTex);
		}
		else {
			YTUtils::GetFavLog()->Add(idptr);
			self->SetSurfaceBase(&s_favOnButtonTex);
		}

		break;
	}
}

menu::audioplayer::Audioplayer::Audioplayer(const char *cwd, menu::displayfiles::File *startFile, Mode mode)
{
	string text8;
	wstring text16;
	string fullPath;
	rco::Element searchParam;
	Plugin::PageInitParam rwiParam;
	Plugin::TemplateInitParam tmpParam;
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
	g_rootPage->PlayEffectReverse(100.0f, effect::EffectType_Fadein1, SCE_NULL);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_pagemode_button");
	ui::Widget *buttonPagemode = g_rootPage->GetChild(&searchParam, 0);
	if (buttonPagemode)
		buttonPagemode->PlayEffectReverse(0.0f, effect::EffectType_Reset);

	searchParam.hash = EMPVAUtils::GetHash("displayfiles_player_button");
	ui::Widget *buttonPlayer = g_rootPage->GetChild(&searchParam, 0);
	buttonPlayer->PlayEffect(600.0f, effect::EffectType_Reset);

	// Create player scene
	searchParam.hash = EMPVAUtils::GetHash("page_player");
	g_player_page = g_empvaPlugin->PageOpen(&searchParam, &rwiParam);

	searchParam.hash = EMPVAUtils::GetHash("plane_player_cover");
	playerCover = g_player_page->GetChild(&searchParam, 0);

	g_isPlayerActive = SCE_TRUE;

	// Get player widgets
	searchParam.hash = EMPVAUtils::GetHash("plane_player_bg");
	s_playerPlane = g_player_page->GetChild(&searchParam, 0);
	s_playerPlane->PlayEffect(0.0f, effect::EffectType_Fadein1, SCE_NULL);

	if (!s_isBtCbRegistered) { // Register player plane button callbacks (one time per app lifetime)

		searchParam.hash = EMPVAUtils::GetHash("player_settings_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, g_settingsButtonCB, 0);

		auto backButtonCb = new menu::audioplayer::BackButtonCB();
		searchParam.hash = EMPVAUtils::GetHash("player_back_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, backButtonCb, 0);

		s_playerButtonCb = new menu::audioplayer::PlayerButtonCB();
		searchParam.hash = EMPVAUtils::GetHash("player_play_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("player_rew_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);
		commonWidget->SetDirectKey(SCE_CTRL_L1);

		searchParam.hash = EMPVAUtils::GetHash("player_ff_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);
		commonWidget->SetDirectKey(SCE_CTRL_R1);

		searchParam.hash = EMPVAUtils::GetHash("player_shuffle_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("player_repeat_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("player_close_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);
		commonWidget->SetDirectKey(0);

		searchParam.hash = EMPVAUtils::GetHash("progressbar_player");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x10000003, s_playerButtonCb, 0);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_pause");
		Plugin::GetTexture(&s_pauseButtonTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_play");
		Plugin::GetTexture(&s_playButtonTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_shuffle");
		Plugin::GetTexture(&s_shuffleOffButtonTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_shuffle_glow");
		Plugin::GetTexture(&s_shuffleOnButtonTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_repeat");
		Plugin::GetTexture(&s_repeatOffButtonTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_repeat_glow");
		Plugin::GetTexture(&s_repeatOnButtonTex, g_empvaPlugin, &searchParam);

		searchParam.hash = EMPVAUtils::GetHash("tex_button_repeat_glow_one");
		Plugin::GetTexture(&s_repeatOnOneButtonTex, g_empvaPlugin, &searchParam);

		s_isBtCbRegistered = SCE_TRUE;
	}

	// Player init
	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Player;

	sceAppMgrAcquireBgmPortWithPriority(0x81);

	EMPVAUtils::SetPowerTickTask(SCE_TRUE);

	if (mode == Mode_Youtube) {
		YTUtils::LockMenuParsers();
		invParseVideo(cwd, &s_ytVidInfo);

		//TODO: playlist
		/*
		if (s_ytVidInfo->playlist.videos.size() && s_ytVidInfo->playlist.selected_index > 0) {
			YouTubeVideoDetail *ytVidInfoOld = s_ytVidInfo;
			s_ytVidInfo = youtube_parse_video_page((char *)ytVidInfoOld->playlist.videos[0].url.c_str());
			youtube_destroy_struct(ytVidInfoOld);
		}
		*/

		s_ytFirstIdx = SCE_TRUE;
	}

	GetMusicList(startFile);

	if (totalIdx <= 1) {
		searchParam.hash = EMPVAUtils::GetHash("player_ff_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1, SCE_NULL);

		searchParam.hash = EMPVAUtils::GetHash("player_rew_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1, SCE_NULL);
	}
	else {
		searchParam.hash = EMPVAUtils::GetHash("player_ff_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->PlayEffect(0.0f, effect::EffectType_Fadein1, SCE_NULL);

		searchParam.hash = EMPVAUtils::GetHash("player_rew_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->PlayEffect(0.0f, effect::EffectType_Fadein1, SCE_NULL);
	}

	switch (mode) {
	case Mode_Normal:

		fullPath = cwd;
		fullPath.append(startFile->name->string.c_str(), startFile->name->string.length());

		core = new AudioplayerCore(fullPath.c_str());

		if (!core->IsValid()) {
			s_totalLength = new string();
			g_currentPlayerInstance = this;
			return;
		};

		// Handle cover
		meta = core->GetDecoder()->GetMetadataLocation();
		if (!meta->hasCover) {

			auto coverLoader = new audio::PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
			coverLoader->workptr = SCE_NULL;

			SharedPtr<job::JobItem> itemParam(coverLoader);

			g_coverJobQueue->Enqueue(&itemParam);
		}

		break;
	case Mode_Youtube:
		if (!s_isYtBtCbRegistered) {

			searchParam.hash = EMPVAUtils::GetHash("player_template_youtube");
			g_empvaPlugin->TemplateOpen(s_playerPlane, &searchParam, &tmpParam);

			searchParam.hash = EMPVAUtils::GetHash("tex_yt_icon_favourite_for_player_glow");
			Plugin::GetTexture(&s_favOnButtonTex, g_empvaPlugin, &searchParam);

			searchParam.hash = EMPVAUtils::GetHash("tex_yt_icon_favourite_for_player");
			Plugin::GetTexture(&s_favOffButtonTex, g_empvaPlugin, &searchParam);

			searchParam.hash = EMPVAUtils::GetHash("player_fav_button");
			commonWidget = s_playerPlane->GetChild(&searchParam, 0);
			commonWidget->RegisterEventCallback(0x10000008, s_playerButtonCb, 0);

			s_isYtBtCbRegistered = SCE_TRUE;
		}
		else {
			searchParam.hash = EMPVAUtils::GetHash("player_fav_button");
			commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		}

		commonWidget->PlayEffect(0.0f, effect::EffectType_Fadein1, SCE_NULL);

		YTUtils::GetFavLog()->Reset();

		if (!YTUtils::GetFavLog()->Get(playlist.path[0]->c_str())) {
			commonWidget->SetSurfaceBase(&s_favOnButtonTex);
		}

		core = new AudioplayerCore("https://");

		if (!core->IsValid()) {
			s_totalLength = new string();
			g_currentPlayerInstance = this;
			return;
		};

		break;
	}

	searchParam.hash = EMPVAUtils::GetHash("text_player_number");
	numText = g_player_page->GetChild(&searchParam, 0);
	text8 = ccc::Sprintf("%u / %u", playlistIdx + 1, totalIdx);
	ccc::UTF8toUTF16(&text8, &text16);
	numText->SetLabel(&text16);

	s_totalLength = new string();

	ConvertSecondsToString(s_totalLength, core->GetDecoder()->GetLength() / core->GetDecoder()->GetSampleRate(), SCE_FALSE);

	searchParam.hash = EMPVAUtils::GetHash("player_play_button");
	playButton = s_playerPlane->GetChild(&searchParam, 0);
	playButton->SetSurfaceBase(&s_pauseButtonTex);

	if (!SCE_PAF_IS_DOLCE && config->motion_mode) {
		motion::Motion::SetState(SCE_TRUE);
		motion::Motion::SetReleaseTimer(config->motion_timer);
		motion::Motion::SetAngleThreshold(config->motion_degree);
	}

	g_currentPlayerInstance = this;

	task::Register(RegularTask, SCE_NULL);
}

menu::audioplayer::Audioplayer::~Audioplayer()
{
	rco::Element searchParam;
	ui::Widget *commonWidget;
	SceInt32 i = 0;

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {
		searchParam.hash = EMPVAUtils::GetHash("player_fav_button");
		commonWidget = s_playerPlane->GetChild(&searchParam, 0);
		commonWidget->PlayEffectReverse(0.0f, effect::EffectType_Fadein1, SCE_NULL);
	}

	*(SceUInt32 *)g_settingsButtonCB->pUserData = menu::settings::SettingsButtonCB::Parent_Displayfiles;
	task::Unregister(RegularTask, SCE_NULL);
	if (core != SCE_NULL)
		delete core;

	g_currentPlayerInstance = SCE_NULL;

	EMPVAUtils::SetPowerTickTask(SCE_FALSE);
	sceAppMgrReleaseBgmPort();
	audio::Utils::ResetBgmMode();

	while (playlist.path[i] != SCE_NULL) {
		playlist.path[i]->clear();
		delete playlist.path[i];
		i++;
	}

	if (!SCE_PAF_IS_DOLCE)
		motion::Motion::SetState(SCE_FALSE);

	s_totalLength->clear();
	delete s_totalLength;

	g_isPlayerActive = SCE_FALSE;
}

SceVoid menu::audioplayer::Audioplayer::GetMusicList(menu::displayfiles::File *startFile)
{
	SceInt32 i = 0;

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {

		//TODO: playlist
		/*
		if (s_ytVidInfo->playlist.videos.size()) {
			for (SceInt32 j = 0; j < s_ytVidInfo->playlist.videos.size(); j++) {
				playlist.path[i] = new string(s_ytVidInfo->playlist.videos[j].url.c_str());
				playlist.isConsumed[i] = 0;
				i++;
			}
		}
		else
		*/

		{
			playlist.path[i] = new string(s_ytVidInfo->id);
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
				playlist.path[i] = new string(g_currentDispFilePage->cwd->c_str(), g_currentDispFilePage->cwd->length());
				playlist.path[i]->append(file->name->string.c_str(), file->name->string.length());
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

		//TODO: playlist
		/*
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
		*/

		{
			s_ytFirstIdx = SCE_FALSE;
		}

		if (!s_ytVidInfo->audioHqUrl && !s_ytVidInfo->audioMqUrl && !s_ytVidInfo->audioLqUrl) {
			decoder = new audio::YoutubeDecoder(SCE_NULL, SCE_TRUE);
		}
		else {

			const char *urlWithQuality = SCE_NULL;

			switch (menu::settings::Settings::GetInstance()->yt_quality) {
			case menu::settings::Settings::YtQuality_High:
				if (s_ytVidInfo->audioHqUrl)
					urlWithQuality = s_ytVidInfo->audioHqUrl;
				else if (s_ytVidInfo->audioMqUrl)
					urlWithQuality = s_ytVidInfo->audioMqUrl;
				else if (s_ytVidInfo->audioLqUrl)
					urlWithQuality = s_ytVidInfo->audioLqUrl;
				break;
			case menu::settings::Settings::YtQuality_Medium:
				if (s_ytVidInfo->audioMqUrl)
					urlWithQuality = s_ytVidInfo->audioMqUrl;
				else if (s_ytVidInfo->audioLqUrl)
					urlWithQuality = s_ytVidInfo->audioLqUrl;
				else if (s_ytVidInfo->audioHqUrl)
					urlWithQuality = s_ytVidInfo->audioHqUrl;
				break;
			case menu::settings::Settings::YtQuality_Low:
				if (s_ytVidInfo->audioLqUrl)
					urlWithQuality = s_ytVidInfo->audioLqUrl;
				else if (s_ytVidInfo->audioMqUrl)
					urlWithQuality = s_ytVidInfo->audioMqUrl;
				else if (s_ytVidInfo->audioHqUrl)
					urlWithQuality = s_ytVidInfo->audioHqUrl;
				break;
			}

			decoder = new audio::YoutubeDecoder(urlWithQuality, SCE_TRUE);
		}

		// Cover handler for YouTube
		auto coverLoader = new audio::YoutubePlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");

		coverLoader->url = s_ytVidInfo->thmbUrlHq;

		SharedPtr<job::JobItem> itemParam(coverLoader);

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
		invCleanupVideo(s_ytVidInfo);
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
	string text8;
	wstring text16;
	rco::Element searchParam;
	ui::Widget *textTitle;
	ui::Widget *textAlbum;
	ui::Widget *textArtist;

	searchParam.hash = EMPVAUtils::GetHash("text_player_title");
	textTitle = g_player_page->GetChild(&searchParam, 0);
	searchParam.hash = EMPVAUtils::GetHash("text_player_album");
	textAlbum = g_player_page->GetChild(&searchParam, 0);
	searchParam.hash = EMPVAUtils::GetHash("text_player_artist");
	textArtist = g_player_page->GetChild(&searchParam, 0);

	if (EMPVAUtils::GetPagemode() == menu::settings::Settings::PageMode_YouTube) {

		wstring title;
		wstring album;

		//TODO: playlist
		/*
		if (s_ytVidInfo->playlist.videos.size()) {
			text8 = s_ytVidInfo->playlist.title.c_str();
			ccc::UTF8toUTF16(&text8, &album);
			textAlbum->SetLabel(&album);
		}
		else
		*/

		{
			textAlbum->SetLabel(&album);
		}

		text8 = s_ytVidInfo->author;
		ccc::UTF8toUTF16(&text8, &text16);
		textArtist->SetLabel(&text16);

		text8 = s_ytVidInfo->title;
		ccc::UTF8toUTF16(&text8, &title);
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

			if (metadata->title.length() == 0) {
				text8 = name;
				ccc::UTF8toUTF16(&text8, &metadata->title);
			}
		}

		if (!metadata->hasMeta) {
			text8 = name;
			ccc::UTF8toUTF16(&text8, &metadata->title);
		}

		textTitle->SetLabel(&metadata->title);

		EMPVAUtils::IPC::SendInfo(&metadata->title, &metadata->artist, &metadata->album, -1);
	}
}