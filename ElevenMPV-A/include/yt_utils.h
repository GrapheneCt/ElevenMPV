#ifndef _ELEVENMPV_YTUTILS_H_
#define _ELEVENMPV_YTUTILS_H_

#include <kernel.h>
#include <paf.h>
#include <ini_file_processor.h>
#include <sceavplayer.h>

#include "netmedia.h"
#include "downloader.h"

using namespace paf;

class YTUtils
{
public:

	class Log
	{
	public:

		Log()
		{

		}

		virtual ~Log()
		{
			ini->flush();
			ini->close();
			delete ini;
		}

		virtual SceInt32 GetNext(char *data);

		virtual SceInt32 Get(const char *data);

		virtual SceVoid Reset();

		virtual SceVoid Add(const char *data);

		virtual SceVoid Remove(const char *data);

		virtual SceVoid Flush();

		virtual SceInt32 GetSize();

	protected:

		sce::Ini::IniFileProcessor *ini;
	};

	class HistLog : public Log
	{
	public:

		HistLog();

		static SceVoid Clean();

		static const SceUInt32 k_maxHistItems = 20;
	};

	class FavLog : public Log
	{
	public:

		FavLog();

		static SceVoid Clean();
	};

	static SceVoid Init();

	static SceVoid Term(SceBool isFullTerm = SCE_FALSE);

	static HistLog *GetHistLog();

	static FavLog *GetFavLog();

	static Downloader *GetDownloader();

	static SceVoid LockMenuParsers();

	static SceVoid UnlockMenuParsers();

	static SceVoid WaitMenuParsers();

	static SceVoid GetNETMedia(SceNmHandle *handle, SceAvPlayerFileReplacement *fio);

	static ScePVoid NMAllocate(ScePVoid argP, SceUInt32 argAlignment, SceUInt32 argSize);

	static SceVoid NMDeallocate(ScePVoid argP, ScePVoid argMemory);

	static SceBool DowbloadFile(char *url, ScePVoid *ppBuf, SceSize *pBufSize);
};

#endif
