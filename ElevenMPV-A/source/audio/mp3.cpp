#include <kernel.h>
#include <appmgr.h>
#include <shellaudio.h>
#include <stdlib.h>

#include "audio.h"
#include "config.h"
#include "id3.h"
#include "utils.h"
#include "menu_audioplayer.h"

audio::Mp3Decoder::Mp3Decoder(const char *path, SceBool isSwDecoderUsed) : ShellCommonDecoder::ShellCommonDecoder(path, isSwDecoderUsed)
{
	String *text8 = new String();
	io::File file;
	SceInt32 ret = 0;

	ID3Tag *ID3 = (ID3Tag *)sce_paf_malloc(sizeof(ID3Tag));
	ParseID3(path, ID3);

	char* metaPtr = (char *)ID3;

	for (int i = 0; i < 792; i++) {
		if (metaPtr[i] != '\0') {
			metadata->hasMeta = SCE_TRUE;
			break;
		}
	}

	if (metadata->hasMeta) {
		text8->Set(ID3->ID3Title);
		text8->ToWString(&metadata->title);
		text8->Clear();

		text8->Set(ID3->ID3Artist);
		text8->ToWString(&metadata->artist);
		text8->Clear();

		text8->Set(ID3->ID3Album);
		text8->ToWString(&metadata->album);
		text8->Clear();
	}

	if ((ID3->ID3EncapsulatedPictureType == JPEG_IMAGE || ID3->ID3EncapsulatedPictureType == PNG_IMAGE) && !metadata->hasCover) {

		ret = file.Open(path, SCE_O_RDONLY, 0);
		if (ret >= 0) {
			coverLoader = new audio::PlayerCoverLoaderThread(SCE_KERNEL_COMMON_QUEUE_HIGHEST_PRIORITY, SCE_KERNEL_4KiB, "EMPVA::PlayerCoverLoader");
			coverLoader->workptr = sce_paf_malloc(ID3->ID3EncapsulatedPictureLength);
			if (coverLoader->workptr != SCE_NULL) {
				coverLoader->isExtMem = SCE_TRUE;
				file.Lseek(ID3->ID3EncapsulatedPictureOffset, SCE_SEEK_SET);
				file.Read(coverLoader->workptr, ID3->ID3EncapsulatedPictureLength);
				file.Close();
				coverLoader->size = ID3->ID3EncapsulatedPictureLength;
				coverLoader->Start();

				metadata->hasCover = SCE_TRUE;
			}
		}
	}

	sce_paf_free(ID3);

	delete text8;
}
