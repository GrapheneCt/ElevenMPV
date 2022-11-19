#include <kernel.h>
#include <paf.h>
#include <ini_file_processor.h>
#include <sceavplayer.h>
#include <psp2_compat/curl/curl.h>

#include "yt_utils.h"
#include "netmedia.h"
#include "downloader.h"

using namespace paf;
using namespace sce;

static YTUtils::HistLog *s_histLog = SCE_NULL;
static YTUtils::FavLog *s_favLog = SCE_NULL;
static SceUID s_menuLock = SCE_UID_INVALID_UID;
static SceAvPlayerFileReplacement s_fio;
static SceNmHandle s_nmHandle = SCE_NULL;
static SceAvPlayerMemAllocator s_nmAllocator;
static Downloader *s_downloader = SCE_NULL;
static CURL *s_curl = SCE_NULL;

ScePVoid YTUtils::NMAllocate(ScePVoid argP, SceUInt32 argAlignment, SceUInt32 argSize)
{
	return sce_paf_memalign(argAlignment, argSize);
}

SceVoid YTUtils::NMDeallocate(ScePVoid argP, ScePVoid argMemory)
{
	sce_paf_free(argMemory);
}

SceInt32 YTUtils::Log::GetNext(char *data)
{
	SceInt32 ret;
	char *sptr;
	char val[2];

	ret = ini->parse(data, val, sizeof(val));

	if (ret == SCE_OK) {

		// Restore '='
		sptr = sce_paf_strchr(data, '}');
		while (sptr) {
			*sptr = '=';
			sptr = sce_paf_strchr(sptr, '}');
		}
	}

	return ret;
}

SceInt32 YTUtils::Log::Get(const char *data)
{
	char *sptr;
	char dataCopy[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];
	char val[2];

	sce_paf_strncpy(dataCopy, data, SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE);

	// Restore '}'
	sptr = sce_paf_strchr(dataCopy, '=');
	while (sptr) {
		*sptr = '}';
		sptr = sce_paf_strchr(sptr, '=');
	}

	return  ini->get(dataCopy, val, sizeof(val), 0);
}

SceVoid YTUtils::Log::Flush()
{
	ini->flush();
}

SceInt32 YTUtils::Log::GetSize()
{
	return ini->size();
}

SceVoid YTUtils::Log::Reset()
{
	ini->reset();
}

SceVoid YTUtils::Log::Add(const char *data)
{
	char *sptr;
	char dataCopy[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];
	sce_paf_strncpy(dataCopy, data, SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE);

	// Replace '=' in playlists with '}' to not confuse INI processor
	sptr = sce_paf_strchr(dataCopy, '=');

	while (sptr) {
		*sptr = '}';
		sptr = sce_paf_strchr(sptr, '=');
	}

	ini->add(dataCopy, "");
}

SceVoid YTUtils::Log::Remove(const char *data)
{
	char *sptr;
	char dataCopy[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];
	sce_paf_strncpy(dataCopy, data, SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE);

	// Replace '=' in playlists with '}' to not confuse INI processor
	sptr = sce_paf_strchr(dataCopy, '=');

	while (sptr) {
		*sptr = '}';
		sptr = sce_paf_strchr(sptr, '=');
	}

	ini->del(dataCopy);
}

YTUtils::HistLog::HistLog()
{
	SceUInt32 i = 0;
	char val[2];
	char data[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];
	Ini::InitParameter param;
	Ini::MemAllocator alloc;
	alloc.allocate = sce_paf_malloc;
	alloc.deallocate = sce_paf_free;

	param.workmemSize = SCE_KERNEL_4KiB;
	param.unk_0x4 = SCE_KERNEL_4KiB;
	param.allocator = &alloc;

	ini = new Ini::IniFileProcessor();
	ini->initialize(&param);
	ini->open("savedata0:yt_hist_log.ini", "rw", 0);

	i = ini->size();
	if (i <= k_maxHistItems)
		return;

	i = i - k_maxHistItems;

	while (i != 0) {
		ini->parse(data, val, sizeof(val));
		ini->del(data);
		i--;
	}

	ini->reset();
}

YTUtils::FavLog::FavLog()
{
	Ini::InitParameter param;
	Ini::MemAllocator alloc;
	alloc.allocate = sce_paf_malloc;
	alloc.deallocate = sce_paf_free;

	param.workmemSize = SCE_KERNEL_4KiB;
	param.unk_0x4 = SCE_KERNEL_4KiB;
	param.allocator = &alloc;

	ini = new Ini::IniFileProcessor();
	ini->initialize(&param);
	ini->open("savedata0:yt_fav_log.ini", "rw", 0);
}

SceVoid YTUtils::FavLog::Clean()
{
	if (s_favLog) {
		delete s_favLog;
		sceIoRemove("savedata0:yt_fav_log.ini");
		s_favLog = new YTUtils::FavLog();
	}
}

SceVoid YTUtils::HistLog::Clean()
{
	if (s_histLog) {
		delete s_histLog;
		sceIoRemove("savedata0:yt_hist_log.ini");
		s_histLog = new YTUtils::HistLog();
	}
}

SceVoid YTUtils::Init()
{
	if (!s_histLog)
		s_histLog = new YTUtils::HistLog();
	if (!s_favLog)
		s_favLog = new YTUtils::FavLog();
	if (!s_downloader)
		s_downloader = new Downloader();
	if (!s_curl) {
		s_curl = curl_easy_init();
		curl_easy_setopt(s_curl, CURLOPT_USERAGENT, "Mozilla/5.0 (PlayStation Vita 3.74) AppleWebKit/537.73 (KHTML, like Gecko) Silk/3.2");
		curl_easy_setopt(s_curl, CURLOPT_TIMEOUT, 5L);
		curl_easy_setopt(s_curl, CURLOPT_ACCEPT_ENCODING, "");
		curl_easy_setopt(s_curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(s_curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(s_curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(s_curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(s_curl, CURLOPT_TCP_KEEPALIVE, 1L);
		curl_easy_setopt(s_curl, CURLOPT_NOPROGRESS, 1L);
	}

	if (!s_nmHandle) {

		s_nmAllocator.allocate = NMAllocate;
		s_nmAllocator.deallocate = NMDeallocate;

		NETMediaInit(&s_nmHandle, &s_fio, SCE_NULL, 256 * 1024, 0, 0, &s_nmAllocator);
	}

	if (s_menuLock == SCE_UID_INVALID_UID)
		s_menuLock = sceKernelCreateEventFlag("EMPVA::YtMenuLock", SCE_KERNEL_ATTR_MULTI, 1, SCE_NULL);
}

SceVoid YTUtils::Term(SceBool isFullTerm)
{
	if (!isFullTerm) {
		if (s_histLog) {
			s_histLog->Flush();
		}
		if (s_favLog) {
			s_favLog->Flush();
		}
	}
	else {
		if (s_histLog) {
			delete s_histLog;
			s_histLog = SCE_NULL;
		}
		if (s_favLog) {
			delete s_favLog;
			s_favLog = SCE_NULL;
		}

		if (s_curl) {
			curl_easy_cleanup(s_curl);
			s_curl = SCE_NULL;
		}

		/*
		if (s_nmHandle) {
			NETMediaDeInit(s_nmHandle, &s_nmAllocator);
			s_nmHandle = SCE_NULL;
		}
		*/

		if (s_menuLock != SCE_UID_INVALID_UID) {
			sceKernelDeleteEventFlag(s_menuLock);
			s_menuLock = SCE_UID_INVALID_UID;
		}
	}
}

SceVoid YTUtils::GetNETMedia(SceNmHandle *handle, SceAvPlayerFileReplacement *fio)
{
	if (s_nmHandle) {
		*handle = s_nmHandle;

		sce_paf_memcpy(fio, &s_fio, sizeof(SceAvPlayerFileReplacement));
	}
}

YTUtils::HistLog *YTUtils::GetHistLog()
{
	return s_histLog;
}

YTUtils::FavLog *YTUtils::GetFavLog()
{
	return s_favLog;
}

Downloader *YTUtils::GetDownloader()
{
	return s_downloader;
}

SceVoid YTUtils::LockMenuParsers()
{
	sceKernelClearEventFlag(s_menuLock, ~1);
}

SceVoid YTUtils::UnlockMenuParsers()
{
	sceKernelSetEventFlag(s_menuLock, 1);
}

SceVoid YTUtils::WaitMenuParsers()
{
	sceKernelWaitEventFlag(s_menuLock, 1, SCE_KERNEL_EVF_WAITMODE_OR, SCE_NULL, SCE_NULL);
}

SceSize YTUtils::InvDownloadCore(char *buffer, SceSize size, SceSize nitems, ScePVoid userdata)
{
	InvDownloadData *dw = (InvDownloadData *)userdata;
	SceSize toCopy = size * nitems;

	if (dw->pos > SCE_KERNEL_256KiB)
		return 0;

	if (toCopy != 0) {
		sce_paf_memcpy(dw->buf + dw->pos, buffer, toCopy);
		dw->pos += toCopy;
		return toCopy;
	}

	return 0;
}

SceBool YTUtils::InvDownload(char *url, ScePVoid *ppBuf, SceSize *pBufSize)
{
	if (!s_curl)
		return SCE_FALSE;

	InvDownloadData dw;
	dw.buf = sce_paf_malloc(SCE_KERNEL_256KiB);
	dw.pos = 0;

	if (!dw.buf) {
		return SCE_FALSE;
	}

	curl_easy_setopt(s_curl, CURLOPT_URL, url);
	curl_easy_setopt(s_curl, CURLOPT_WRITEFUNCTION, InvDownloadCore);
	curl_easy_setopt(s_curl, CURLOPT_WRITEDATA, &dw);

	if (curl_easy_perform(s_curl) != CURLE_OK) {
		sce_paf_free(dw.buf);
		return SCE_FALSE;
	}

	*ppBuf = dw.buf;
	*pBufSize = dw.pos;

	return SCE_TRUE;
}
