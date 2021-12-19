#include <kernel.h>
#include <audiocodec.h>
#include <stdlib.h>
#include <paf.h>

#include "common.h"
#include "audio.h"
#include "id3.h"
#include "utils.h"
#include "vitaaudiolib.h"
#include "menu_audioplayer.h"

#define EA3_HEADER_SIZE 96
#define GROUP_SIZE 64
#define AT3P_ES_GRAIN 2048
#define AT3_ES_GRAIN 1024

static SceOff s_currPos = 0;

SceInt32 audio::At3Decoder::sceAudiocodecGetAt3ConfigPSP2(SceUInt32 cmode, SceUInt32 nbytes)
{
	if ((cmode == 0) && (nbytes == 0xc0)) {
		return 4;
	}
	if ((cmode == 0) && (nbytes == 0x98)) {
		return 6;
	}
	if ((cmode == 1) && (nbytes == 0x60)) {
		return 0xb;
	}
	if ((cmode == 2) && (nbytes == 0xc0)) {
		return 0xe;
	}
	if ((cmode == 2) && (nbytes == 0x98)) {
		return 0xf;
	}
	return -1;
}

SceVoid audio::At3Decoder::InitCommon(const char *path, SceUInt8 type, SceUInt8 param1, SceUInt8 param2, SceSize dataOffset)
{
	SceInt32 ret;
	SceSize atDataSize;

	//Initialize SceAudiocodec
	sce_paf_memset(&codecCtrl, 0, sizeof(SceAudiocodecCtrl));
	switch (type) {
	case Type_AT3P:
		codecType = SCE_AUDIOCODEC_ATX;
		codecCtrl.atx.codecParam1 = param1;
		codecCtrl.atx.codecParam2 = param2;
		break;
	case Type_AT3:
		codecType = SCE_AUDIOCODEC_AT3;
		codecCtrl.at3.codecParam = sceAudiocodecGetAt3ConfigPSP2(param1, param2);
		break;
	}

	ret = sceAudiocodecQueryMemSize(&codecCtrl, codecType);
	if (-1 < ret) {

		SceSize memsize = 0x8000;
		if (codecCtrl.neededWorkMem > memsize)
			memsize = 0x10000;
		codecCtrl.pWorkMem = sce_paf_malloc(memsize);
		if (codecCtrl.pWorkMem == SCE_NULL)
			return;

		if (codecType == SCE_AUDIOCODEC_ATX)
			codecCtrl.unk_atx_0C = 1;

		ret = sceAudiocodecInit(&codecCtrl, codecType);
		if ((ret < 0) && (codecCtrl.pWorkMem != SCE_NULL)) {
			sce_paf_free(codecCtrl.pWorkMem);
			codecCtrl.pWorkMem = NULL;
			return;
		}
		else if (ret < 0) {
			return;
		}
	}
	else {
		return;
	}

	//Allocate stream buffer, read first es streams to buffer, set output grain

	switch (type) {
	case Type_AT3P:
		LOCALMediaInit(&nmHandle, &fio, SCE_NULL, codecCtrl.atx.inputEsSize * 128, 0, 0, SCE_NULL);
		fio.open(fio.objectPointer, path);
		esBuffer = sce_paf_malloc(codecCtrl.atx.inputEsSize);
		s_currPos = dataOffset;
		//Calculate total samples
		atDataSize = (SceSize)fio.size(fio.objectPointer) - dataOffset;
		totalEsSamples = atDataSize / codecCtrl.atx.inputEsSize;
		audio::DecoderCore::PreSetGrain(AT3P_ES_GRAIN);
		break;
	case Type_AT3:
		switch (codecCtrl.at3.codecParam) {
		case 4:
			codecCtrl.inputEsSize = 384;
			break;
		case 6:
			codecCtrl.inputEsSize = 304;
			break;
		case 0xB:
			codecCtrl.inputEsSize = 192;
			break;
		case 0xE:
			return; //unsupported
			break;
		case 0xF:
			return; //unsupported
			break;
		}
		LOCALMediaInit(&nmHandle, &fio, SCE_NULL, codecCtrl.inputEsSize * 128, 0, 0, SCE_NULL);
		fio.open(fio.objectPointer, path);
		esBuffer = sce_paf_malloc(codecCtrl.inputEsSize);
		s_currPos = dataOffset;
		//Calculate total samples
		atDataSize = (SceSize)fio.size(fio.objectPointer) - dataOffset;
		totalEsSamples = atDataSize / codecCtrl.inputEsSize;
		audio::DecoderCore::PreSetGrain(AT3_ES_GRAIN);
		break;
	}

	dataBeginOffset = dataOffset;
	isValid = SCE_TRUE;
}

SceVoid audio::At3Decoder::InitOMA(const char *path)
{
	io::File file;
	SceInt32 ret;
	SceSize offset;
	SceUInt8 codecType;
	SceUInt8 param1;
	SceUInt8 param2;
	String text8;
	ID3Tag *ID3 = (ID3Tag *)sce_paf_malloc(sizeof(ID3Tag));

	ParseID3(path, ID3);

	char* metaPtr = (char *)ID3;

	for (SceInt32 i = 0; i < 792; i++) {
		if (metaPtr[i] != '\0') {
			metadata->hasMeta = SCE_TRUE;
			break;
		}
	}

	if (metadata->hasMeta) {
		text8 = ID3->ID3Title;
		text8.ToWString(&metadata->title);

		text8 = ID3->ID3Artist;
		text8.ToWString(&metadata->artist);

		text8 = ID3->ID3Album;
		text8.ToWString(&metadata->album);
	}

	if ((ID3->ID3EncapsulatedPictureType == JPEG_IMAGE || ID3->ID3EncapsulatedPictureType == PNG_IMAGE) && !metadata->hasCover) {

		ret = file.Open(path, SCE_O_RDONLY, 0);
		if (ret >= 0) {

			auto coverLoader = new PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
			coverLoader->workptr = sce_paf_malloc(ID3->ID3EncapsulatedPictureLength);

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
				file.Lseek(ID3->ID3EncapsulatedPictureOffset, SCE_SEEK_SET);
				file.Read(coverLoader->workptr, ID3->ID3EncapsulatedPictureLength);
				file.Close();
				coverLoader->size = ID3->ID3EncapsulatedPictureLength;

				g_coverJobQueue->Enqueue(&itemParam);

				metadata->hasCover = SCE_TRUE;
			}
			else
				delete coverLoader;
		}
	}

	offset = ID3v2TagSize(path);

	sce_paf_free(ID3);

	//Locate EA3 header
	SceInt32 index = 0;
	char* ea3Header_init = (char *)sce_paf_malloc(1024);
	char* ea3Header = ea3Header_init;
	sce_paf_memset(ea3Header, 0, 1024);

	file.Open(path, SCE_O_RDONLY, 0);
	file.Lseek((SceOff)offset, SCE_SEEK_SET);
	file.Read(ea3Header, 1024);
	file.Close();

	while (*ea3Header == '\0' && index < 1024) {
		index++;
		ea3Header++;
	}

	if (sce_paf_memcmp(ea3Header, "EA3", 3)) {
		sce_paf_free(ea3Header_init);
		return;
	}

	codecType = ea3Header[32];

	if (codecType == Type_AT3P) {
		param1 = ea3Header[34];
		param2 = ea3Header[35];
	}
	else if (codecType == Type_AT3) {
		//Everything here is hardcoded: no ATRAC3 header documentation anywhere
		param1 = ea3Header[33];
		if (param1 == 2)
			param1 = 1;

		param2 = ea3Header[35];
		switch (param2) {
		case 0x18:
			param2 = 0x60;
			break;
		case 0x26:
			param2 = 0x98;
			break;
		case 0x30:
			param2 = 0xC0;
			break;
		default:
			sce_paf_free(ea3Header_init);
			return;
			break;
		}		
	}
	else {
		sce_paf_free(ea3Header_init);
		return;
	}

	sce_paf_free(ea3Header_init);

	return InitCommon(path, codecType, param1, param2, offset + index + EA3_HEADER_SIZE);
}

SceVoid audio::At3Decoder::InitRIFF(const char *path)
{
	io::File headerFile;
	SceUInt8 codecType = 0;
	SceUInt8 param1;
	SceUInt8 param2;

	//Locate AT3 info header
	SceSize index = 0;
	char* ea3Header = (char *)sce_paf_malloc(1024);
	char* dataStart = ea3Header;
	sce_paf_memset(ea3Header, 0, 1024);

	headerFile.Open(path, SCE_O_RDONLY, 0);
	headerFile.Read(ea3Header, 1024);
	headerFile.Close();

	//"data"
	while (*(SceInt32 *)dataStart != 0x61746164 && index < 1024) {
		index++;
		dataStart++;
	}
	index += 8;

	switch (* (SceUInt16 *)&ea3Header[20]) {
	case 0xFFFE:
		codecType = Type_AT3P;
		break;
	case 0x0270:
		codecType = Type_AT3;
		break;
	default:
		sce_paf_free(ea3Header);
		return;
		break;
	}

	if (codecType == Type_AT3P) {
		param1 = ea3Header[62];
		param2 = ea3Header[63];
	}
	else if (codecType == Type_AT3) {
		//Everything here is hardcoded: no ATRAC3 header documentation anywhere

		SceUInt16 codecInfo;
		codecInfo = *(SceUInt16 *)&ea3Header[28];
		switch (codecInfo) {
		case 0x204C:
			param2 = 0x60;
			param1 = 1;
			break;
		case 0x3324:
			param2 = 0x98;
			param1 = 0;
			break;
		case 0x4099:
			param2 = 0xC0;
			param1 = 0;
			break;
		default:
			sce_paf_free(ea3Header);
			return;
			break;
		}
	}
	else {
		sce_paf_free(ea3Header);
		return;
	}

	sce_paf_free(ea3Header);

	return InitCommon(path, codecType, param1, param2, index);
}

audio::At3Decoder::At3Decoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	dataBeginOffset = 0;
	totalEsSamples = 0;
	totalEsPlayed = 0;
	codecType = 0;
	s_currPos = 0;
	esBuffer = SCE_NULL;

	//Check ATRAC3 file header type
	const char* ext = EMPVAUtils::GetFileExt(path);
	if (!sce_paf_strncasecmp(ext, "oma", 3) || !sce_paf_strncasecmp(ext, "aa3", 3)) {
		InitOMA(path);
	}
	else if (!sce_paf_strncasecmp(ext, "at3", 3))
		InitRIFF(path);

	audio::DecoderCore::SetDecoder(this, SCE_NULL);
	audio::DecoderCore::Init(GetSampleRate(), GetChannels() == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
}

SceUInt32 audio::At3Decoder::GetSampleRate()
{
	return 44100;
}

SceUInt8 audio::At3Decoder::GetChannels()
{
	//TODO: actually check from header
	return 2;
}

SceVoid audio::At3Decoder::Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata)
{
	SceInt32 read = -2;
	SceUInt32 readSize = 0;

	if (isPlaying == SCE_FALSE || isPaused == SCE_TRUE) {
		short *bufShort = (short *)stream;
		SceUInt32 count;
		for (count = 0; count < length * 2; count++)
			*(bufShort + count) = 0;
	}
	else {
		//Read data
		switch (codecType) {
		case SCE_AUDIOCODEC_ATX:
			read = fio.readOffset(fio.objectPointer, (uint8_t *)esBuffer, s_currPos, codecCtrl.atx.inputEsSize);
			readSize = codecCtrl.atx.inputEsSize;
			break;
		case SCE_AUDIOCODEC_AT3:
			read = fio.readOffset(fio.objectPointer, (uint8_t *)esBuffer, s_currPos, codecCtrl.inputEsSize);
			readSize = codecCtrl.inputEsSize;
			break;
		}

		while (read == -2) {
			read = fio.readOffset(fio.objectPointer, (uint8_t *)esBuffer, s_currPos, readSize);
			thread::Sleep(10);
		}

		//Decode current ES samples

		codecCtrl.pEs = esBuffer;
		codecCtrl.pPcm = stream;

		if (sceAudiocodecDecode(&codecCtrl, codecType) < 0)
			isPlaying = SCE_FALSE;

		s_currPos += codecCtrl.inputEsSize;

		totalEsPlayed++;

		if (totalEsPlayed >= totalEsSamples)
			isPlaying = SCE_FALSE;
	}
}

SceUInt64 audio::At3Decoder::GetPosition()
{
	switch (codecType) {
	case SCE_AUDIOCODEC_ATX:
		return totalEsPlayed * AT3P_ES_GRAIN;
		break;
	case SCE_AUDIOCODEC_AT3:
		return totalEsPlayed * AT3_ES_GRAIN;
		break;
	}

	return 0;
}

SceUInt64 audio::At3Decoder::GetLength()
{
	switch (codecType) {
	case SCE_AUDIOCODEC_ATX:
		return totalEsSamples * AT3P_ES_GRAIN;
		break;
	case SCE_AUDIOCODEC_AT3:
		return totalEsSamples * AT3_ES_GRAIN;
		break;
	}

	return 0;
}

SceUInt64 audio::At3Decoder::Seek(SceFloat32 percent)
{
	SceSize esGrain = AT3P_ES_GRAIN;
	if (codecType == SCE_AUDIOCODEC_AT3)
		esGrain = AT3_ES_GRAIN;

	SceUInt64 seekSamples = (ogg_int64_t)((SceFloat32)(totalEsSamples * esGrain) * percent / 100.0f);
	SceUInt32 seekEsNum = (SceUInt32)sce_paf_floorf((SceFloat32)seekSamples / (SceFloat32)esGrain);
	seekSamples = (SceUInt64)(seekEsNum * esGrain);

	totalEsPlayed = seekEsNum;

	s_currPos = dataBeginOffset + (seekEsNum * codecCtrl.inputEsSize);

	LOCALMediaInvalidateAllBuffers(nmHandle);

	return seekSamples;
}

audio::At3Decoder::~At3Decoder()
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	audio::DecoderCore::EndPre();
	thread::Sleep(100);
	audio::DecoderCore::End();

	sce_paf_free(codecCtrl.pWorkMem);
	sce_paf_free(esBuffer);

	audio::DecoderCore::PreSetGrain(audio::DecoderCore::GetDefaultGrain());
	fio.close(fio.objectPointer);
	LOCALMediaDeInit(nmHandle);
}
