#include <kernel.h>
#include <sceavplayer.h>

#include "utils.h"
#include "yt_utils.h"
#include "audio.h"
#include "menu_audioplayer.h"
#include "vitaaudiolib.h"

#include "opus/opus.h"
#include "nestegg.h"
#include "netmedia.h"

static SceOff s_currPos = 0;

static SceAvPlayerFileReplacement s_fio;
static SceNmHandle s_nmHandle = SCE_NULL;


SceInt32 audio::YoutubeDecoder::NeRead(ScePVoid buffer, SceSize length, ScePVoid userdata)
{
	// success = 1
	// eof = 0
	// error = -1

	SceAvPlayerFileReplacement *fio = (SceAvPlayerFileReplacement *)userdata;

	SceInt32 read = -2;

	read = fio->readOffset(fio->objectPointer, (uint8_t *)buffer, s_currPos, length);

	while (read == -2) {
		read = fio->readOffset(fio->objectPointer, (uint8_t *)buffer, s_currPos, length);
		thread::Sleep(10);
	}
	
	s_currPos += read;

	if (read < 0)
		return -1;

	if (read < length)
		return 0;

	return 1;
}

SceInt32 audio::YoutubeDecoder::NeSeek(SceInt64 offset, SceInt32 whence, ScePVoid userdata)
{
	SceAvPlayerFileReplacement *fio = (SceAvPlayerFileReplacement *)userdata;

	switch (whence) {
	case NESTEGG_SEEK_SET:
		s_currPos = offset;
		break;
	case NESTEGG_SEEK_CUR:
		s_currPos += offset;
		break;
	case NESTEGG_SEEK_END:
		s_currPos = fio->size(fio->objectPointer);
		break;
	}

	return 0;
}

SceInt64 audio::YoutubeDecoder::NeTell(ScePVoid userdata)
{
	return s_currPos;
}

audio::YoutubeDecoder::YoutubeDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	SceInt32 ret = 0;
	SceUInt32 lures = 0;
	SceUInt64 llures = 0;
	nestegg_audio_params aparams;

	samplesRead = 0;
	seekTargetSamples = 0;
	totalTime = 0;
	sampleRate = 0;
	s_currPos = 0;
	isSeeking = SCE_FALSE;
	opusDec = SCE_NULL;
	ne = SCE_NULL;
	metadata->hasCover = SCE_TRUE;

	YTUtils::GetNETMedia(&nmHandle, &fio);

	if (!path)
		return;

	fio.open(fio.objectPointer, path);

	nestegg_io ne_io;
	ne_io.read = NeRead;
	ne_io.seek = NeSeek;
	ne_io.tell = NeTell;
	ne_io.userdata = &fio;

	ret = nestegg_init(&ne, ne_io, SCE_NULL, -1);
	if (ret != 0)
		return;

	ret = nestegg_track_count(ne, &lures);
	if (lures > 1 || ret != 0)
		return;

	ret = nestegg_track_type(ne, 0);
	if (ret != NESTEGG_TRACK_AUDIO)
		return;

	ret = nestegg_track_codec_id(ne, 0);
	if (ret != NESTEGG_CODEC_OPUS)
		return;

	ret = nestegg_track_audio_params(ne, 0, &aparams);
	if (ret != 0)
		return;

	channelNum = aparams.channels;
	sampleRate = (SceUInt32)aparams.rate;

	ret = nestegg_duration(ne, &llures);
	if (ret != 0)
		return;

	totalTime = (SceUInt64)((SceDouble)llures * aparams.rate / 1000000000.0f); // This imitates total length in samples

	maxSamples = (SceInt32)(aparams.rate * 0.06 + 0.5); //Maximum frame size (for 60 ms frame)

	opusDec = opus_decoder_create(sampleRate, aparams.channels, &ret);
	if (ret != OPUS_OK)
		return;

	isValid = SCE_TRUE;
	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::YoutubeDecoder::GetSampleRate()
{
	return sampleRate;
}

SceUInt8 audio::YoutubeDecoder::GetChannels()
{
	return channelNum;
}

SceVoid audio::YoutubeDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{
	SceInt32 ret = 0;
	SceUInt32 uret = 0;
	nestegg_packet *packet = SCE_NULL;

	if (isPlaying == SCE_FALSE || isPaused == SCE_TRUE) {
		short *bufShort = (short *)stream;
		SceUInt32 count;
		for (count = 0; count < length * 2; count++)
			*(bufShort + count) = 0;
	}
	else {

		ret = nestegg_read_packet(ne, &packet);
		if (packet != SCE_NULL && ret == 1) {

			if (isSeeking) {
				SceUInt64 tstamp = 0;
				nestegg_packet_tstamp(packet, &tstamp);
				samplesRead = (SceUInt64)((SceDouble)tstamp * (SceDouble)sampleRate / 1000000000.0f); // This imitates length in samples
				isSeeking = SCE_FALSE;
			}

			nestegg_packet_count(packet, &uret);
			if (uret == 1) {
				unsigned char* data;
				ret = nestegg_packet_data(packet, 0, &data, &uret);
				if (ret == 0) {
					const int samples = opus_decode(opusDec, data, uret, (opus_int16 *)stream, maxSamples, 0);
					if (samples >= 0)
					{
						samplesRead += samples;
					}
				}
			}

			nestegg_free_packet(packet);
		}
		else {
			isPlaying = SCE_FALSE;
		}
	}
}

SceUInt64 audio::YoutubeDecoder::GetPosition()
{
	if (isSeeking)
		return seekTargetSamples;
	else
		return samplesRead;
}

SceUInt64 audio::YoutubeDecoder::GetLength()
{
	return totalTime;
}

SceUInt64 audio::YoutubeDecoder::Seek(SceFloat32 percent)
{
	SceFloat32 seekTime = (SceFloat32)(((SceFloat32)totalTime * (percent / 100.0f)) / (SceDouble)sampleRate);
	seekTime = seekTime / 10.0f;
	SceFloat32 seekIterNum = sce_paf_ceilf(seekTime);
	SceUInt64 llSeekTime = (SceInt64)(seekIterNum * 10001000000.0f);
	seekTargetSamples = (SceUInt64)((SceDouble)llSeekTime * (SceDouble)sampleRate / 1000000000.0f); // This imitates length in samples

	NETMediaInvalidateAllBuffers(nmHandle);

	nestegg_track_seek(ne, 0, llSeekTime);

	isSeeking = SCE_TRUE;

	return 0;
}

audio::YoutubeDecoder::~YoutubeDecoder()
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Sleep(100);
	audio::DecoderCore::End();

	if (opusDec)
		opus_decoder_destroy(opusDec);
	if (ne)
		nestegg_destroy(ne);
	fio.close(fio.objectPointer);
	//NETMediaDeInit(nmHandle);
}
