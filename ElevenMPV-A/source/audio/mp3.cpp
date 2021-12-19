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
	String text8;
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

	sce_paf_free(ID3);
}
