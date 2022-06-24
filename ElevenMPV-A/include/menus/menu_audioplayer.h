#ifndef _ELEVENMPV_MENU_PLAYAUDIO_H_
#define _ELEVENMPV_MENU_PLAYAUDIO_H_

#include <paf.h>

#include "audio.h"
#include "menu_displayfiles.h"

using namespace paf;

namespace menu {
	namespace audioplayer {

		class PlayerButtonCB : public ui::EventCallback
		{
		public:

			enum ButtonHash
			{
				ButtonHash_Play = 0x22535800,
				ButtonHash_Rew = 0x8ff51fc8,
				ButtonHash_Ff = 0x373fc526,
				ButtonHash_Repeat = 0x28bfa2c9,
				ButtonHash_Shuffle = 0x46be756f,
				ButtonHash_Progressbar = 0x354adaae,
				ButtonHash_Close = 0xf6ac8379,
				ButtonHash_Favourite = 0xb59196e3
			};

			PlayerButtonCB()
			{
				eventHandler = PlayerButtonCBFun;
			};

			virtual ~PlayerButtonCB()
			{

			};

			static SceVoid PlayerButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class BackButtonCB : public ui::EventCallback
		{
		public:

			BackButtonCB()
			{
				eventHandler = BackButtonCBFun;
			};

			virtual ~BackButtonCB()
			{

			};

			static SceVoid BackButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class AudioplayerCore
		{
		public:

			AudioplayerCore(const char *file);

			~AudioplayerCore();

			audio::GenericDecoder *GetDecoder();

			SceVoid SetInitialParams();

			SceVoid SetMetadata(const char *file);

			SceBool IsValid();

		private:

			static const SceUInt8 k_ytRetryAttemptNum = 3;

			audio::GenericDecoder *decoder;
			SceBool isValid;
		};

		class Audioplayer 
		{
		public:

			enum Mode
			{
				Mode_Normal,
				Mode_Youtube
			};

			class Playlist
			{
			public:

				string *path[1024];
				SceUInt8 isConsumed[1024];
			};

			Audioplayer(const char *cwd, menu::displayfiles::File *startFile, Mode mode);

			~Audioplayer();

			static SceVoid RegularTask(ScePVoid pUserData);

			static SceVoid HandleNext(SceBool fromHandlePrev, SceBool fromFfButton);

			static SceVoid ReloadCoverForNext();

			static SceVoid HandlePrev();

			static SceVoid _HandleNext(SceBool fromHandlePrev, SceBool fromFfButton);

			static SceVoid _HandlePrev();

			static SceVoid YtJobFinishHandler();

			static SceVoid ConvertSecondsToString(string *string, SceUInt64 seconds, SceBool needSeparator);

			static SceVoid Return();

			static SceVoid Close();
			
			SceVoid GetMusicList(menu::displayfiles::File *startFile);

			AudioplayerCore *GetCore();

			Playlist playlist;
			SceInt32 playlistIdx;

		private:

			AudioplayerCore *core;
			SceUInt32 startIdx;
			SceUInt32 totalIdx;
			SceUInt32 totalConsumedIdx;
		};

	}
}

#endif
