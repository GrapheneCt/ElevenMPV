#ifndef _ELEVENMPV_AUDIO_LIB_H_
#define _ELEVENMPV_AUDIO_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <paf.h>
#include <audioout.h>

#include "audio.h"

using namespace paf;

namespace audio {

	class DecoderCore
	{
	public:

		class DecoderCoreThread : public thread::Thread
		{
		public:

			using thread::Thread::Thread;

			SceVoid EntryFunction();
		};

		typedef struct VITA_audio_channelinfo
		{
			DecoderCoreThread *threadhandle;
			SceInt32 handle;
			audio::GenericDecoder *decoder;
			ScePVoid userdata;
		} VITA_audio_channelinfo;

		static SceInt32 Init(SceUInt32 frequency, SceUInt32 mode);

		static SceVoid EndPre();

		static SceVoid PreSetGrain(SceUInt32 ngrain);

		static SceUInt32 GetGrain();

		static SceUInt32 GetDefaultGrain();

		static SceVoid End();

		static SceVoid SetDecoder(audio::GenericDecoder *decoder, ScePVoid userdata);
	};
}

#ifdef __cplusplus
}
#endif

#endif
