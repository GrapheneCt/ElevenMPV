#ifndef _ELEVENMPV_COMMON_H_
#define _ELEVENMPV_COMMON_H_

#include <ctrl.h>
#include <appmgr.h>
#include <paf.h>
#include <vita2d_sys.h>
#include <scejpegenc.h>

#include "menu_displayfiles.h"
#include "menu_settings.h"
#include "menu_audioplayer.h"
#include "config.h"

using namespace paf;

#define MAX_FILES 1024

extern SceUID g_mainThreadUid;
extern SceUID g_eventFlagUid;

extern SceBool g_isPlayerActive;

extern Plugin *g_empvaPlugin;
extern widget::Widget *g_root;
extern widget::Widget *g_root_page;
extern widget::Widget *g_settings_page;
extern widget::Widget *g_player_page;
extern widget::Widget *g_settings_option;
extern widget::Widget *g_top_text;
extern graphics::Texture *g_commonBgTex;
extern graphics::Texture *g_coverBgTex;
extern widget::BusyIndicator *g_commonBusyInidcator;
extern widget::Widget *g_commonOptionDialog;

extern graphics::Texture *g_texCheckMark;
extern graphics::Texture *g_texTransparent;
extern graphics::Surface *g_currentCoverSurf;

extern menu::audioplayer::Audioplayer *g_currentPlayerInstance;
extern menu::displayfiles::Page *g_currentDispFilePage;
extern menu::settings::SettingsButtonCB *g_settingsButtonCB;
extern config::Config *g_config;

extern menu::displayfiles::CoverLoaderThread *g_currentCoverLoader;

#endif
