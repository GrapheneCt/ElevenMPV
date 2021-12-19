#include <kernel.h>

#include "common.h"
#include "utils.h"
#include "audio.h"
#include "menu_audioplayer.h"
#include "vitaaudiolib.h"
#include "opus/opusfile.h"

static SceOff s_currPos = 0;

SceInt32 audio::OpDecoder::OpRead(ScePVoid _stream, SceUChar8 *_ptr, SceInt32 _nbytes)
{
	DualIo *io = (DualIo *)_stream;
	SceInt32 read = -2;

	if (io->mio != SCE_NULL) {
		read = io->mio->Read(_ptr, _nbytes);
	}
	else {
		read = io->fio.readOffset(io->fio.objectPointer, (uint8_t *)_ptr, s_currPos, _nbytes);

		while (read == -2) {
			read = io->fio.readOffset(io->fio.objectPointer, (uint8_t *)_ptr, s_currPos, _nbytes);
			thread::Sleep(10);
		}

		if (read < 0)
			return 0;
	}

	s_currPos += read;

	return read;
}

SceInt32 audio::OpDecoder::OpSeek(ScePVoid _stream, SceInt64 _offset, SceInt32 _whence)
{
	DualIo *io = (DualIo *)_stream;

	if (io->mio != SCE_NULL) {
		s_currPos = io->mio->Lseek(_offset, _whence);
		return 0;
	}
	else {
		switch (_whence) {
		case SCE_SEEK_SET:
			s_currPos = _offset;
			break;
		case SCE_SEEK_CUR:
			s_currPos += _offset;
			break;
		case SCE_SEEK_END:
			s_currPos = io->fio.size(io->fio.objectPointer);
			s_currPos += _offset;
			break;
		}

		if (s_currPos > io->fio.size(io->fio.objectPointer))
			return -1;
		else
			return 0;
	}
}

SceInt64 audio::OpDecoder::OpTell(ScePVoid _stream)
{
	return s_currPos;
}

SceInt32 audio::OpDecoder::OpClose(ScePVoid _stream)
{
	DualIo *io = (DualIo *)_stream;

	io->fio.close(io->fio.objectPointer);

	return 0;
}

audio::OpDecoder::OpDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	SceInt32 error = 0;
	SceUInt32 bufMemSize = 64 * 1024;
	String text8;
	OggOpusFile *opusFile = SCE_NULL;
	OpusFileCallbacks opCb;

	samplesRead = 0;
	maxSamples = 0;
	s_currPos = 0;

	opCb.read = OpRead;
	opCb.seek = OpSeek;
	opCb.tell = OpTell;
	opCb.close = OpClose;

	if (EMPVAUtils::GetMemStatus() > 0)
		bufMemSize = 128 * 1024;

	LOCALMediaInit(&nmHandle, &io.fio, SCE_NULL, bufMemSize, 0, 0, SCE_NULL);

	io.fio.open(io.fio.objectPointer, path);
	io.mio = new io::File();
	io.mio->Open(path, SCE_O_RDONLY, 0);

	if ((opus = (ScePVoid)op_open_callbacks(&io, &opCb, SCE_NULL, 0, &error)) == SCE_NULL) {
		io.fio.close(io.fio.objectPointer);
		io.mio->Close();
		delete io.mio;
		io.mio = SCE_NULL;
		return;
	}

	opusFile = (OggOpusFile *)opus;

	if ((error = op_current_link(opusFile)) < 0) {
		io.fio.close(io.fio.objectPointer);
		io.mio->Close();
		delete io.mio;
		io.mio = SCE_NULL;
		return;
	}

	maxSamples = op_pcm_total(opusFile, -1);

	const OpusTags *tags = op_tags(opusFile, 0);

	if (opus_tags_query_count(tags, "title") > 0) {
		metadata->hasMeta = SCE_TRUE;

		text8 = opus_tags_query(tags, "title", 0);
		text8.ToWString(&metadata->title);
	}

	if (opus_tags_query_count(tags, "album") > 0) {
		metadata->hasMeta = SCE_TRUE;

		text8 = opus_tags_query(tags, "album", 0);
		text8.ToWString(&metadata->album);
	}

	if (opus_tags_query_count(tags, "artist") > 0) {
		metadata->hasMeta = SCE_TRUE;

		text8 = opus_tags_query(tags, "artist", 0);
		text8.ToWString(&metadata->artist);
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

					auto coverLoader = new PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
					coverLoader->workptr = sce_paf_malloc(picture_tag.data_length);

					if (coverLoader->workptr != SCE_NULL) {

						CleanupHandler *req = new CleanupHandler();
						req->userData = coverLoader;
						req->refCount = 0;
						req->unk_08 = 1;
						req->cb = (CleanupHandler::CleanupCallback)audio::PlayerCoverLoaderJob::JobKiller;

						ObjectWithCleanup itemParam;
						itemParam.object = coverLoader;
						itemParam.cleanup = req;

						coverLoader->isExtMem = SCE_TRUE;
						sce_paf_memcpy(coverLoader->workptr, picture_tag.data, picture_tag.data_length);
						coverLoader->size = picture_tag.data_length;

						g_coverJobQueue->Enqueue(&itemParam);

						metadata->hasMeta = SCE_TRUE;
						metadata->hasCover = SCE_TRUE;
					}
					else
						delete coverLoader;
				}
			}
		}

		opus_picture_tag_clear(&picture_tag);
	}

	isValid = SCE_TRUE;

	io.mio->Close();
	delete io.mio;
	io.mio = SCE_NULL;

	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::OpDecoder::GetSampleRate()
{
	return 48000;
}

SceUInt8 audio::OpDecoder::GetChannels()
{
	return 2;
}

SceVoid audio::OpDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
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

SceUInt64 audio::OpDecoder::GetPosition()
{
	return samplesRead;
}

SceUInt64 audio::OpDecoder::GetLength()
{
	return maxSamples;
}

SceUInt64 audio::OpDecoder::Seek(SceFloat32 percent)
{
	OggOpusFile *opusFile = (OggOpusFile *)opus;

	LOCALMediaInvalidateAllBuffers(nmHandle);

	if (op_seekable(opusFile) >= 0) {

		ogg_int64_t seekSample = (ogg_int64_t)((SceFloat32)maxSamples * percent / 100.0f);
	
		if (op_pcm_seek(opusFile, seekSample) >= 0) {
			samplesRead = seekSample;
			return samplesRead;
		}
	}

	return 0;
}

audio::OpDecoder::~OpDecoder()
{
	OggOpusFile *opusFile = (OggOpusFile *)opus;

	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Sleep(100);
	audio::DecoderCore::End();

	if (opusFile)
		op_free(opusFile);
	LOCALMediaDeInit(nmHandle);
}
