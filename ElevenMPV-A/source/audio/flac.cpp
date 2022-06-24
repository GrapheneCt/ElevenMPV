#include <kernel.h>
#include <sceavplayer.h>

#include "common.h"
#include "utils.h"
#include "vitaaudiolib.h"
#include "audio.h"
#include "dr_flac.h"
#include "menu_audioplayer.h"
#include "localmedia.h"

static SceOff s_currPos = 0;

static audio::FlacDecoder *s_currentDecoderInstance = SCE_NULL;

SceVoid audio::FlacDecoder::MetadataCbEntry(ScePVoid pUserData, ScePVoid pMeta)
{
	string *text8 = new string();
	drflac_metadata *pMetadata = (drflac_metadata *)pMeta;
	s_currentDecoderInstance->metadata->hasMeta = SCE_TRUE;
	if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_PICTURE && !s_currentDecoderInstance->metadata->hasCover) {
		if (pMetadata->data.picture.type == DRFLAC_PICTURE_TYPE_COVER_FRONT) {
			if (!sce_paf_strncasecmp("image/jpg", pMetadata->data.picture.mime, 9) ||
				!sce_paf_strncasecmp("image/jpeg", pMetadata->data.picture.mime, 10) ||
				!sce_paf_strncasecmp("image/png", pMetadata->data.picture.mime, 9)) {

				auto coverLoader = new PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
				coverLoader->workptr = sce_paf_malloc(pMetadata->data.picture.pictureDataSize);

				if (coverLoader->workptr != SCE_NULL) {

					SharedPtr<job::JobItem> itemParam(coverLoader);

					coverLoader->isExtMem = SCE_TRUE;
					sce_paf_memcpy(coverLoader->workptr, pMetadata->data.picture.pPictureData, pMetadata->data.picture.pictureDataSize);
					coverLoader->size = pMetadata->data.picture.pictureDataSize;

					g_coverJobQueue->Enqueue(&itemParam);

					s_currentDecoderInstance->metadata->hasMeta = SCE_TRUE;
					s_currentDecoderInstance->metadata->hasCover = SCE_TRUE;
				}
				else
					delete coverLoader;
			}
		}
	}
	else if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
		unsigned int len;
		drflac_vorbis_comment_iterator iter;
		drflac_init_vorbis_comment_iterator(&iter, pMetadata->data.vorbis_comment.commentCount, pMetadata->data.vorbis_comment.pComments);
		for (int i = 0; i < pMetadata->data.vorbis_comment.commentCount; i++) {
			const char* tag = drflac_next_vorbis_comment(&iter, &len);

			if (!sce_paf_strncasecmp("TITLE=", tag, 6)) {
				s_currentDecoderInstance->metadata->hasMeta = SCE_TRUE;
				text8->append(tag + 6, len - 6);
				ccc::UTF8toUTF16(text8, &s_currentDecoderInstance->metadata->title);
				text8->clear();
			}

			if (!sce_paf_strncasecmp("ALBUM=", tag, 6)) {
				s_currentDecoderInstance->metadata->hasMeta = SCE_TRUE;
				text8->append(tag + 6, len - 6);
				ccc::UTF8toUTF16(text8, &s_currentDecoderInstance->metadata->album);
				text8->clear();
			}

			if (!sce_paf_strncasecmp("ARTIST=", tag, 7)) {
				s_currentDecoderInstance->metadata->hasMeta = SCE_TRUE;
				text8->append(tag + 7, len - 7);
				ccc::UTF8toUTF16(text8, &s_currentDecoderInstance->metadata->artist);
				text8->clear();
			}
		}
	}

	delete text8;
}

SceSize audio::FlacDecoder::drflacReadCB(ScePVoid pUserData, ScePVoid pBufferOut, SceSize bytesToRead)
{
	DualIo *io = (DualIo *)pUserData;
	SceInt32 read = -2;

	if (io->mio != SCE_NULL) {
		read = io->mio->Read(pBufferOut, bytesToRead);
	}
	else {
		read = io->fio.readOffset(io->fio.objectPointer, (uint8_t *)pBufferOut, s_currPos, bytesToRead);

		while (read == -2) {
			read = io->fio.readOffset(io->fio.objectPointer, (uint8_t *)pBufferOut, s_currPos, bytesToRead);
			thread::Sleep(10);
		}

		if (read < 0)
			return 0;
	}

	s_currPos += read;

	return read;
}

SceUInt32 audio::FlacDecoder::drflacSeekCB(ScePVoid pUserData, SceInt32 offset, SceInt32 origin)
{
	DualIo *io = (DualIo *)pUserData;

	if (io->mio != SCE_NULL) {
		s_currPos = io->mio->Seek(offset, origin);
		return DRFLAC_TRUE;
	}
	else {
		switch (origin) {
		case SCE_SEEK_SET:
			s_currPos = offset;
			break;
		case SCE_SEEK_CUR:
			s_currPos += offset;
			break;
		case SCE_SEEK_END:
			s_currPos = io->fio.size(io->fio.objectPointer);
			s_currPos += offset;
			break;
		}

		if (s_currPos > io->fio.size(io->fio.objectPointer))
			return DRFLAC_FALSE;
		else
			return DRFLAC_TRUE;
	}
}

audio::FlacDecoder::FlacDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	LocalFile::OpenArg oarg;
	SceUInt32 bufMemSize = 256 * 1024;

	framesRead = 0;
	s_currPos = 0;

	flac = SCE_NULL;

	s_currentDecoderInstance = this;

	if (EMPVAUtils::GetMemStatus() > 0)
		bufMemSize = 512 * 1024;

	LOCALMediaInit(&nmHandle, &io.fio, SCE_NULL, bufMemSize, 0, 0, SCE_NULL);

	io.fio.open(io.fio.objectPointer, path);
	io.mio = new LocalFile();
	oarg.filename = path;
	io.mio->Open(&oarg);

	flac = (ScePVoid)drflac_open_with_metadata(drflacReadCB, (drflac_seek_proc)drflacSeekCB, (drflac_meta_proc)MetadataCbEntry, &io, SCE_NULL);

	if (flac)
		isValid = SCE_TRUE;

	io.mio->Close();
	delete io.mio;
	io.mio = SCE_NULL;

	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::FlacDecoder::GetSampleRate()
{
	drflac *flacHandle = (drflac *)flac;
	return flacHandle->sampleRate;
}

SceUInt8 audio::FlacDecoder::GetChannels()
{
	drflac *flacHandle = (drflac *)flac;
	return flacHandle->channels;
}

SceVoid audio::FlacDecoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{
	drflac *flacHandle = (drflac *)flac;

	if (isPlaying == SCE_FALSE || isPaused == SCE_TRUE) {
		short *bufShort = (short *)stream;
		SceUInt32 count;
		for (count = 0; count < length * 2; count++)
			*(bufShort + count) = 0;
	}
	else {
		framesRead += drflac_read_pcm_frames_s16(flacHandle, (drflac_uint64)length, (drflac_int16 *)stream);

		if (framesRead >= flacHandle->totalPCMFrameCount)
			isPlaying = SCE_FALSE;
	}
}

SceUInt64 audio::FlacDecoder::GetPosition()
{
	return framesRead;
}

SceUInt64 audio::FlacDecoder::GetLength()
{
	drflac *flacHandle = (drflac *)flac;

	return flacHandle->totalPCMFrameCount;
}

SceUInt64 audio::FlacDecoder::Seek(SceFloat32 percent)
{
	drflac *flacHandle = (drflac *)flac;

	drflac_uint64 seekFrame = (drflac_uint64)((SceFloat32)flacHandle->totalPCMFrameCount * percent / 100.0f);

	LOCALMediaInvalidateAllBuffers(nmHandle);

	if (drflac_seek_to_pcm_frame(flacHandle, seekFrame) == DRFLAC_TRUE) {
		framesRead = seekFrame;
		if (framesRead)
			return framesRead;
	}

	return 0;
}

audio::FlacDecoder::~FlacDecoder()
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Sleep(100);
	audio::DecoderCore::End();

	if (flac)
		drflac_close((drflac *)flac);
	io.fio.close(io.fio.objectPointer);
	LOCALMediaDeInit(nmHandle);

	s_currentDecoderInstance = SCE_NULL;
}
