#include <kernel.h>

#include "audio.h"
#include "config.h"
#include "menu_audioplayer.h"
#include "vitaaudiolib.h"
#include "opus/opusfile.h"

audio::OpusDecoder::OpusDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	SceInt32 error = 0;
	String *text8 = new String();
	OggOpusFile *opusFile = SCE_NULL;

	samplesRead = 0;
	maxSamples = 0;

	if ((opus = (ScePVoid)op_open_file(path, &error)) == SCE_NULL)
		return;

	opusFile = (OggOpusFile *)opus;

	if ((error = op_current_link(opusFile)) < 0)
		return;

	maxSamples = op_pcm_total(opusFile, -1);

	const OpusTags *tags = op_tags(opusFile, 0);

	if (opus_tags_query_count(tags, "title") > 0) {
		metadata->hasMeta = SCE_TRUE;

		text8->Set(opus_tags_query(tags, "title", 0));
		text8->ToWString(&metadata->title);
		text8->Clear();
	}

	if (opus_tags_query_count(tags, "album") > 0) {
		metadata->hasMeta = SCE_TRUE;

		text8->Set(opus_tags_query(tags, "album", 0));
		text8->ToWString(&metadata->album);
		text8->Clear();
	}

	if (opus_tags_query_count(tags, "artist") > 0) {
		metadata->hasMeta = SCE_TRUE;

		text8->Set(opus_tags_query(tags, "artist", 0));
		text8->ToWString(&metadata->artist);
		text8->Clear();
	}

	if ((opus_tags_query_count(tags, "METADATA_BLOCK_PICTURE") > 0)) {

		OpusPictureTag picture_tag;
		sce_paf_memset(&picture_tag, 0, sizeof(OpusPictureTag));
		opus_picture_tag_init(&picture_tag);
		const char* metadata_block = opus_tags_query(tags, "METADATA_BLOCK_PICTURE", 0);

		error = opus_picture_tag_parse(&picture_tag, metadata_block);
		if (error == 0) {
			if (picture_tag.type == 3 && !metadata->hasCover) {
				if (picture_tag.format == OP_PIC_FORMAT_JPEG || picture_tag.format == OP_PIC_FORMAT_PNG) {

					coverLoader = new audio::PlayerCoverLoaderThread(SCE_KERNEL_COMMON_QUEUE_HIGHEST_PRIORITY, SCE_KERNEL_4KiB, "EMPVA::PlayerCoverLoader");
					coverLoader->workptr = sce_paf_malloc(picture_tag.data_length);
					if (coverLoader->workptr != SCE_NULL) {
						coverLoader->isExtMem = SCE_TRUE;
						sce_paf_memcpy(coverLoader->workptr, picture_tag.data, picture_tag.data_length);
						coverLoader->size = picture_tag.data_length;
						coverLoader->Start();

						metadata->hasMeta = SCE_TRUE;
						metadata->hasCover = SCE_TRUE;
					}
				}
			}
		}

		opus_picture_tag_clear(&picture_tag);
	}

	delete text8;

	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::OpusDecoder::GetSampleRate()
{
	return 48000;
}

SceUInt8 audio::OpusDecoder::GetChannels()
{
	return 2;
}

SceVoid audio::OpusDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{
	OggOpusFile *opusFile = (OggOpusFile *)opus;

	if (isPlaying == SCE_FALSE || isPaused == SCE_TRUE) {
		short *bufShort = (short *)stream;
		SceUInt32 count;
		for (count = 0; count < length * 2; count++)
			*(bufShort + count) = 0;
	}
	else {
		SceInt32 read = op_read_stereo(opusFile, (opus_int16 *)stream, (SceInt32)length * (sizeof(SceInt16) * 2));
		if (read)
			samplesRead = op_pcm_tell(opusFile);

		if (samplesRead >= maxSamples)
			isPlaying = SCE_FALSE;
	}
}

SceUInt64 audio::OpusDecoder::GetPosition()
{
	return samplesRead;
}

SceUInt64 audio::OpusDecoder::GetLength()
{
	return maxSamples;
}

SceUInt64 audio::OpusDecoder::Seek(SceFloat32 percent)
{
	OggOpusFile *opusFile = (OggOpusFile *)opus;

	if (op_seekable(opusFile) >= 0) {

		ogg_int64_t seekSample = (ogg_int64_t)((SceFloat32)maxSamples * percent / 100.0f);
	
		if (op_pcm_seek(opusFile, seekSample) >= 0) {
			samplesRead = seekSample;
			return samplesRead;
		}
	}

	return -1;
}

audio::OpusDecoder::~OpusDecoder()
{
	OggOpusFile *opusFile = (OggOpusFile *)opus;

	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Thread::Sleep(100);
	audio::DecoderCore::End();

	op_free(opusFile);
}
