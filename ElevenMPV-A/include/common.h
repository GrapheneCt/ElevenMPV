#ifndef _ELEVENMPV_COMMON_H_
#define _ELEVENMPV_COMMON_H_

#include <ctrl.h>
#include <appmgr.h>
#include <paf.h>

#include "menu_displayfiles.h"
#include "menu_settings.h"
#include "menu_audioplayer.h"

using namespace paf;

#define MAX_FILES 1024

extern SceUID g_eventFlagUid;

extern SceBool g_isPlayerActive;

extern Plugin *g_empvaPlugin;
extern ui::Widget *g_root;
extern ui::Widget *g_rootPage;
extern ui::Widget *g_player_page;
extern ui::Widget *g_topText;
extern graphics::Texture *g_commonBgTex;
extern graphics::Texture *g_coverBgTex;
extern ui::BusyIndicator *g_commonBusyInidcator;

extern thread::JobQueue *g_coverJobQueue;

extern graphics::Texture *g_texCheckMark;
extern graphics::Texture *g_texTransparent;
extern graphics::Surface *g_currentCoverSurf;

extern menu::audioplayer::Audioplayer *g_currentPlayerInstance;
extern menu::displayfiles::Page *g_currentDispFilePage;
extern menu::settings::SettingsButtonCB *g_settingsButtonCB;

#endif
