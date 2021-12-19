#ifndef _ELEVENMPV_MENU_SETTINGS_H_
#define _ELEVENMPV_MENU_SETTINGS_H_

#include <paf.h>
#include <app_settings.h>

using namespace paf;

namespace menu {
	namespace settings {

		class Settings
		{
		public:

			enum PageMode
			{
				PageMode_Normal,
				PageMode_YouTube
			};

			enum Hash
			{
				Hash_Device = 0xd3d05f84,
				Hash_Sort = 0xbc47f234,
				Hash_AudioEq = 0x7c296cc5,
				Hash_AudioAlc = 0xa1f24c03,
				Hash_AudioLimitVolume = 0xf18da3ee,
				Hash_PowerFps = 0xb86f7b54,
				Hash_PowerSuspend = 0xa9538556,
				Hash_PowerTimer = 0x1f178760,
				Hash_ControlsSkip = 0x9af3a9e9,
				Hash_ControlsMotion = 0xe165ed76,
				Hash_ControlsTimeout = 0x4e96f7bc,
				Hash_ControlsAngle = 0x21112208,
				Hash_YoutubeCleanHistory = 0x26dd8c17,
				Hash_YoutubeCleanFav = 0xe79d8bdc,
				Hash_YoutubeDownload = 0x15fb99aa
			};

			Settings();

			~Settings();

			static Settings *GetInstance();

			static sce::AppSettings *GetAppSetInstance();

			SceVoid Open(SceUInt32 mode);

			SceVoid SetLastDirectory(const char *cwd);

			SceVoid GetLastDirectory(String *cwd);

			SceInt32 device;
			SceInt32 sort;
			SceInt32 eq_mode;
			SceInt32 eq_volume;
			SceInt32 alc_mode;
			SceInt32 power_saving;
			SceInt32 power_timer;
			SceInt32 stick_skip;
			SceInt32 motion_mode;
			SceInt32 motion_timer;
			SceInt32 motion_degree;
			SceInt32 last_pagemode;
			SceInt32 fps_limit;

			const SceUInt32 k_safeMemIniLimit = 0x400;
			const SceInt32 k_settingsVersion = 4;

		private:

			static SceVoid CBListChange(const char *elementId);

			static SceVoid CBListForwardChange(const char *elementId);

			static SceVoid CBListBackChange(const char *elementId);

			static SceInt32 CBIsVisible(const char *elementId, SceBool *pIsVisible);

			static SceInt32 CBElemInit(const char *elementId);

			static SceInt32 CBElemAdd(const char *elementId, paf::ui::Widget *widget);

			static SceInt32 CBValueChange(const char *elementId, const char *newValue);

			static SceInt32 CBValueChange2(const char *elementId, const char *newValue);

			static SceVoid CBTerm();

			static SceWChar16 *CBGetString(const char *elementId);

			static SceInt32 CBGetTex(graphics::Texture *tex, const char *elementId);

			sce::AppSettings *appSet;
			SceBool settingsReset;
			char rootPath[8];

			const SceInt32 k_defSort = 0;
			const SceInt32 k_defAlcMode = 0;
			const SceInt32 k_defEqMode = 0;
			const SceInt32 k_defEqVolume = 0;
			const SceInt32 k_defMotionMode = 0;
			const SceInt32 k_defMotionTimer = 3;
			const SceInt32 k_defMotionDeg = 45;
			const SceInt32 k_defStickSkip = 0;
			const SceInt32 k_defPowerSaving = 1;
			const SceInt32 k_defPowerTimer = 5;
			const SceInt32 k_defDevice = 0;
			const SceInt32 k_defLastPagemode = 0;
			const SceInt32 k_defFpsLimit = 1;
		};

		class SettingsButtonCB : public ui::Widget::EventCallback
		{
		public:

			enum Parent
			{
				Parent_Displayfiles,
				Parent_Player
			};

			SettingsButtonCB()
			{
				eventHandler = SettingsButtonCBFun;
			};

			virtual ~SettingsButtonCB()
			{

			};

			static SceVoid SettingsButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);
		};

		/*class SettingsBackButtonCB : public widget::Widget::EventCallback
		{
		public:

			SettingsBackButtonCB()
			{
				eventHandler = SettingsBackButtonCBFun;
			};

			virtual ~SettingsBackButtonCB()
			{

			};

			static SceVoid SettingsBackButtonCBFun(SceInt32 eventId, widget::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class SettingsOptionSelectionButtonCB : public widget::Widget::EventCallback
		{
		public:

			SettingsOptionSelectionButtonCB()
			{
				eventHandler = SettingsOptionSelectionButtonCBFun;
			};

			virtual ~SettingsOptionSelectionButtonCB()
			{

			};

			static SceVoid SettingsOptionSelectionButtonCBFun(SceInt32 eventId, widget::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class SettingsOptionButtonCB : public widget::Widget::EventCallback
		{
		public:

			enum ButtonHash 
			{
				ButtonHash_Device = 0x24b457f2,
				ButtonHash_Sort = 0x2557bb59,
				ButtonHash_Audio = 0x71d038c4,
				ButtonHash_Audio_Eq = 0xa9e40b09,
				ButtonHash_Audio_Alc = 0x38b41306,
				ButtonHash_Audio_Limit = 0xed27a668,
				ButtonHash_Power = 0xb43153d8,
				ButtonHash_Power_Save = 0x96e4b893,
				ButtonHash_Power_Time = 0x45a66e04,
				ButtonHash_Control = 0x8fbd8abf,
				ButtonHash_Control_Stick = 0x4cfd1a03,
				ButtonHash_Control_Motion = 0x4263f274,
				ButtonHash_Control_Time = 0xd0c5b691,
				ButtonHash_Control_Angle = 0xe255baed
			};

			SettingsOptionButtonCB()
			{
				eventHandler = SettingsOptionButtonCBFun;
			};

			virtual ~SettingsOptionButtonCB()
			{

			};

			static SceVoid SettingsOptionButtonCBFun(SceInt32 eventId, widget::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class SettingsButtonCB : public widget::Widget::EventCallback
		{
		public:

			enum Parent
			{
				Parent_Displayfiles,
				Parent_Player
			};

			SettingsButtonCB()
			{
				eventHandler = SettingsButtonCBFun;
			};

			virtual ~SettingsButtonCB()
			{

			};

			static SceVoid RefreshButtonText();

			static SceVoid SettingsButtonCBFun(SceInt32 eventId, widget::Widget *self, SceInt32 a3, ScePVoid pUserData);
		};

		class CloseButtonCB : public widget::Widget::EventCallback
		{
		public:

			CloseButtonCB()
			{
				eventHandler = CloseButtonCBFun;
			};

			virtual ~CloseButtonCB()
			{

			};

			static SceVoid CloseButtonCBFun(SceInt32 eventId, widget::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};*/
	}
}

#endif
