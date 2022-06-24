#include <kernel.h>
#include <audioout.h>
#include <string.h>

#include "audio.h"
#include "xmp.h"
#include "vitaaudiolib.h"

audio::XmDecoder::XmDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	samplesRead = 0;

	string text8;
	xmp = xmp_create_context();

	if (xmp_load_module(xmp, path) < 0)
		return;

	xmp_start_player(xmp, 44100, 0);
	xmp_get_frame_info(xmp, &frame_info);
	totalSamples = (SceUInt64)(frame_info.total_time * 44.1);

	xmp_get_module_info(xmp, &module_info);
	if (module_info.mod->name[0] != '\0') {
		metadata->hasMeta = SCE_TRUE;
		text8 = module_info.mod->name;
		ccc::UTF8toUTF16(&text8, &metadata->title);
	}

	isValid = SCE_TRUE;

	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::XmDecoder::GetSampleRate()
{
	return 44100;
}

SceUInt8 audio::XmDecoder::GetChannels()
{
	return 2;
}

SceVoid audio::XmDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{
	if (isPlaying == SCE_FALSE || isPaused == SCE_TRUE) {
		short *bufShort = (short *)stream;
		SceUInt32 count;
		for (count = 0; count < length * 2; count++)
			*(bufShort + count) = 0;
	}
	else {
		xmp_play_buffer(xmp, stream, (SceInt32)length * (sizeof(SceInt16) * 2), 0);
		samplesRead += (SceUInt64)length;

		if (samplesRead >= totalSamples)
			isPlaying = SCE_FALSE;
	}
}

SceUInt64 audio::XmDecoder::GetPosition()
{
	return samplesRead;
}

SceUInt64 audio::XmDecoder::GetLength()
{
	return totalSamples;
}

SceUInt64 audio::XmDecoder::Seek(SceFloat32 percent)
{
	SceInt32 seekSample = (SceInt32)((SceFloat32)totalSamples * percent / 100.0f);
	
	if (xmp_seek_time(xmp, (int)(seekSample / 44.1)) >= 0) {
		samplesRead = seekSample;
		return samplesRead;
	}

	return 0;
}

audio::XmDecoder::~XmDecoder()
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Sleep(100);
	audio::DecoderCore::End();

	xmp_end_player(xmp);
	xmp_release_module(xmp);
	xmp_free_context(xmp);
}
