#include <kernel.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include "utils.h"
#include "audio.h"
#include "common.h"
#include "vitaaudiolib.h"

static SceOff s_currPos = 0;

SceUInt32 audio::OggDecoder::OggRead(ScePVoid ptr, SceUInt32 size, SceUInt32 nmemb, ScePVoid datasource)
{
	SceAvPlayerFileReplacement *fio = (SceAvPlayerFileReplacement *)datasource;
	SceInt32 read = -2;

	read = fio->readOffset(fio->objectPointer, (uint8_t *)ptr, s_currPos, size * nmemb);

	while (read == -2) {
		read = fio->readOffset(fio->objectPointer, (uint8_t *)ptr, s_currPos, size * nmemb);
		thread::Sleep(10);
	}

	if (read < 0)
		return 0;

	s_currPos += read;

	return read;
}

SceInt32 audio::OggDecoder::OggSeek(ScePVoid datasource, ogg_int64_t offset, SceInt32 whence)
{
	SceAvPlayerFileReplacement *fio = (SceAvPlayerFileReplacement *)datasource;

	switch (whence) {
	case SCE_SEEK_SET:
		s_currPos = offset;
		break;
	case SCE_SEEK_CUR:
		s_currPos += offset;
		break;
	case SCE_SEEK_END:
		s_currPos = fio->size(fio->objectPointer);
		s_currPos += offset;
		break;
	}

	if (s_currPos > fio->size(fio->objectPointer))
		return -1;
	else
		return 0;
}

long audio::OggDecoder::OggTell(ScePVoid datasource)
{
	return s_currPos;
}

SceInt32 audio::OggDecoder::OggClose(ScePVoid datasource)
{
	SceAvPlayerFileReplacement *fio = (SceAvPlayerFileReplacement *)datasource;

	fio->close(fio->objectPointer);

	return 0;
}

audio::OggDecoder::OggDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	String text8;
	ov_callbacks ioCb;
	SceUInt32 bufMemSize = 64 * 1024;

	s_currPos = 0;

	ioCb.read_func = OggRead;
	ioCb.seek_func = OggSeek;
	ioCb.tell_func = OggTell;
	ioCb.close_func = OggClose;

	if (EMPVAUtils::GetMemStatus() > 0)
		bufMemSize = 128 * 1024;

	LOCALMediaInit(&nmHandle, &fio, SCE_NULL, bufMemSize, 0, 0, SCE_NULL);
	fio.open(fio.objectPointer, path);

	if (ov_open_callbacks(&fio, &ogg, SCE_NULL, 0, ioCb) < 0) {
		return;
	}

	if ((oggInfo = ov_info(&ogg, -1)) == SCE_NULL)
		return;

	maxLength = ov_pcm_total(&ogg, -1);

	vorbis_comment *comment = ov_comment(&ogg, -1);
	if (comment != SCE_NULL) {

		char *value = SCE_NULL;

		if ((value = vorbis_comment_query(comment, "title", 0)) != SCE_NULL) {
			text8 = value;
			text8.ToWString(&metadata->title);
			metadata->hasMeta = SCE_TRUE;
		}

		if ((value = vorbis_comment_query(comment, "album", 0)) != SCE_NULL) {
			text8 = value;
			text8.ToWString(&metadata->album);
			metadata->hasMeta = SCE_TRUE;
		}

		if ((value = vorbis_comment_query(comment, "artist", 0)) != SCE_NULL) {
			text8 = value;
			text8.ToWString(&metadata->artist);
			metadata->hasMeta = SCE_TRUE;
		}
	}

	isValid = SCE_TRUE;

	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::OggDecoder::GetSampleRate()
{
	return oggInfo->rate;
}

SceUInt8 audio::OggDecoder::GetChannels()
{
	return oggInfo->channels;
}

SceUInt64 audio::OggDecoder::FillBuffer(char *out)
{
	SceUInt32 grain = audio::DecoderCore::GetGrain();
	SceInt32 samplesToRead = (sizeof(SceInt16) * oggInfo->channels) * grain;

	SceInt32 currentSection, samplesJustRead;
	samplesJustRead = 0;

	while(samplesToRead > 0) {
		samplesJustRead = ov_read(&ogg, out, samplesToRead > grain ? grain : samplesToRead, 0, 2, 1, &currentSection);

		if (samplesJustRead < 0)
			return samplesJustRead;
		else if (samplesJustRead == 0)
			break;

		samplesToRead -= samplesJustRead;
		out += samplesJustRead;
	}

	return samplesJustRead;
}

SceVoid audio::OggDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{
	if (isPlaying == SCE_FALSE || isPaused == SCE_TRUE) {
		short *bufShort = (short *)stream;
		SceUInt32 count;
		for (count = 0; count < length * 2; count++)
			*(bufShort + count) = 0;
	}
	else {
		FillBuffer((char *)stream);

		samplesRead = ov_pcm_tell(&ogg);

		if (samplesRead >= maxLength)
			isPlaying = SCE_FALSE;
	}
}

SceUInt64 audio::OggDecoder::GetPosition()
{
	return samplesRead;
}

SceUInt64 audio::OggDecoder::GetLength()
{
	return maxLength;
}

SceUInt64 audio::OggDecoder::Seek(SceFloat32 percent)
{
	ogg_int64_t seekSample = (ogg_int64_t)((SceFloat32)maxLength * percent / 100.0f);
	
	LOCALMediaInvalidateAllBuffers(nmHandle);

	if (ov_pcm_seek(&ogg, seekSample) >= 0) {
		samplesRead = seekSample;
		return samplesRead;
	}

	return 0;
}

audio::OggDecoder::~OggDecoder()
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Sleep(100);
	audio::DecoderCore::End();

	ov_clear(&ogg);
	LOCALMediaDeInit(nmHandle);
}
