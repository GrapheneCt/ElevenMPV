#include <kernel.h>
#include <appmgr.h>
#include <shellaudio.h>
#include <stdlib.h>

#include "common.h"
#include "audio.h"
#include "id3.h"
#include "utils.h"
#include "menu_audioplayer.h"

audio::Mp3Decoder::Mp3Decoder(const char *path, SceBool isSwDecoderUsed) : ShellCommonDecoder::ShellCommonDecoder(path, isSwDecoderUsed)
{
	string text8;
	LocalFile file;
	LocalFile::OpenArg oarg;
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
		text8 = ID3->ID3Title;
		ccc::UTF8toUTF16(&text8, &metadata->title);

		text8 = ID3->ID3Artist;
		ccc::UTF8toUTF16(&text8, &metadata->artist);

		text8 = ID3->ID3Album;
		ccc::UTF8toUTF16(&text8, &metadata->album);
	}

	oarg.filename = path;

	if ((ID3->ID3EncapsulatedPictureType == JPEG_IMAGE || ID3->ID3EncapsulatedPictureType == PNG_IMAGE) && !metadata->hasCover) {

		ret = file.Open(&oarg);
		if (ret >= 0) {

			auto coverLoader = new PlayerCoverLoaderJob("EMPVA::PlayerCoverLoaderJob");
			coverLoader->workptr = sce_paf_malloc(ID3->ID3EncapsulatedPictureLength);

			if (coverLoader->workptr != SCE_NULL) {

				SharedPtr<job::JobItem> itemParam(coverLoader);

				coverLoader->isExtMem = SCE_TRUE;
				file.Seek(ID3->ID3EncapsulatedPictureOffset, SCE_SEEK_SET);
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

	sce_paf_free(ID3);
}
