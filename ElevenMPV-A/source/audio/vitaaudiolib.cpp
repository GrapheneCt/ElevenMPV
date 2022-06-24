#include <kernel.h>
#include <apputil.h>
#include <libdbg.h>

#include "common.h"
#include "vitaaudiolib.h"
#include "utils.h"
#include "audio.h"

static const SceUInt32 k_defaultGrain = 960;

static SceInt16 *s_vitaAudioSoundBuffer;
static audio::DecoderCore::VITA_audio_channelinfo s_vitaAudioStatus;
static volatile SceBool s_audioTerminate = SCE_FALSE;
static SceBool s_audioReady = SCE_FALSE;
static SceUInt32 s_grain = k_defaultGrain;

SceVoid audio::DecoderCore::SetDecoder(audio::GenericDecoder *decoder, ScePVoid userdata)
{
	volatile audio::DecoderCore::VITA_audio_channelinfo *pci = &s_vitaAudioStatus;

	pci->decoder = decoder;
	pci->userdata = userdata;
}

SceVoid audio::DecoderCore::DecoderCoreThread::EntryFunction()
{
	SceInt32 vol[2];

	while (!s_audioTerminate) {

		if (s_vitaAudioStatus.decoder) {
			while (s_vitaAudioStatus.decoder->isPaused) {
				thread::Sleep(100);
			}
		}

		if (s_vitaAudioStatus.decoder) {
			s_vitaAudioStatus.decoder->Decode(s_vitaAudioSoundBuffer, s_grain, s_vitaAudioStatus.userdata);
		}
		else {
			SceUInt32 *ptr = (SceUInt32 *)s_vitaAudioSoundBuffer;
			SceInt32 i;
			for (i = 0; i < s_grain; ++i)
				*(ptr++) = 0;
		}

		if (s_audioReady) {
			sceAppUtilSystemParamGetInt(9, &vol[0]);

			if (menu::settings::Settings::GetInstance()->eq_volume && menu::settings::Settings::GetInstance()->eq_mode != 0) {
				vol[0] = vol[0] / 2;
				vol[1] = vol[0];
				sceAudioOutSetVolume(s_vitaAudioStatus.handle, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
			}
			else {
				vol[1] = vol[0];
				sceAudioOutSetVolume(s_vitaAudioStatus.handle, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
			}

			sceAudioOutOutput(s_vitaAudioStatus.handle, s_vitaAudioSoundBuffer);
		}
	}

	Cancel();
}

SceInt32 audio::DecoderCore::Init(SceUInt32 frequency, SceUInt32 mode)
{
	SceInt32 ret = 0;

	s_audioTerminate = SCE_FALSE;
	s_audioReady = SCE_FALSE;

	s_vitaAudioStatus.handle = -1;
	s_vitaAudioStatus.threadhandle = SCE_NULL;

	s_vitaAudioSoundBuffer = (SceInt16 *)sce_paf_calloc(1, sizeof(SceInt16) * s_grain * (mode + 1));

	if ((s_vitaAudioStatus.handle = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, s_grain, frequency, mode)) < 0) {
		ret = -1;
		goto error;
	}

	s_audioReady = SCE_TRUE;

	s_vitaAudioStatus.threadhandle = new audio::DecoderCore::DecoderCoreThread(0x100000E0, SCE_KERNEL_64KiB, "EMPVA::Audioout", SCE_NULL);

	if (s_vitaAudioStatus.threadhandle == SCE_NULL) {
		SCE_DBG_LOG_ERROR("[EMPVA_DECODER_CORE] Failed to create decoder core thread\n");
		ret = -1;
		goto error;
	}

	ret = s_vitaAudioStatus.threadhandle->Start();

error:

	if (ret < 0) {
		s_audioTerminate = SCE_TRUE;
		if (s_vitaAudioStatus.threadhandle != SCE_NULL) {
			s_vitaAudioStatus.threadhandle->Join();
			delete s_vitaAudioStatus.threadhandle;
		}

		s_vitaAudioStatus.threadhandle = SCE_NULL;

		s_audioReady = SCE_FALSE;
		return 0;
	}

	return 1;
}

SceVoid audio::DecoderCore::EndPre()
{
	s_audioReady = SCE_FALSE;
	s_audioTerminate = SCE_TRUE;
}

SceVoid audio::DecoderCore::PreSetGrain(SceUInt32 ngrain)
{
	s_grain = ngrain;
}

SceUInt32 audio::DecoderCore::GetGrain()
{
	return s_grain;
}

SceUInt32 audio::DecoderCore::GetDefaultGrain()
{
	return k_defaultGrain;
}

SceVoid audio::DecoderCore::End()
{
	s_audioReady = SCE_FALSE;
	s_audioTerminate = SCE_TRUE;

	if (s_vitaAudioStatus.threadhandle != SCE_NULL) {
		s_vitaAudioStatus.threadhandle->Join();
		delete s_vitaAudioStatus.threadhandle;
	}

	s_vitaAudioStatus.threadhandle = SCE_NULL;

	if (s_vitaAudioStatus.handle != -1) {
		sceAudioOutReleasePort(s_vitaAudioStatus.handle);
		s_vitaAudioStatus.handle = -1;
	}

	if (s_vitaAudioSoundBuffer != SCE_NULL)
		sce_paf_free(s_vitaAudioSoundBuffer);

	s_vitaAudioSoundBuffer = SCE_NULL;

	s_vitaAudioStatus.decoder = SCE_NULL;
	s_vitaAudioStatus.userdata = SCE_NULL;
}
