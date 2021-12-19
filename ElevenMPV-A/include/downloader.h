#ifndef _ELEVENMPV_DOWNLOADER_H_
#define _ELEVENMPV_DOWNLOADER_H_

#include <kernel.h>
#include <paf.h>
#include <ipmi.h>
#include <download_service.h>

using namespace paf;

class Downloader
{
public:

	Downloader();

	~Downloader();

	SceInt32 Enqueue(const char *url, const char *name);

	SceInt32 EnqueueAsync(const char *url, const char *name);

private:

	class AsyncEnqueue : public paf::thread::JobQueue::Item
	{
	public:

		using thread::JobQueue::Item::Item;

		~AsyncEnqueue() {}

		SceVoid Run()
		{
			Downloader *pdownloader = (Downloader *)downloader;
			pdownloader->Enqueue(url8.data, name8.data);
		}

		SceVoid Finish() {}

		static SceVoid JobKiller(thread::JobQueue::Item *job)
		{
			if (job)
				delete job;
		}

		String url8;
		String name8;
		ScePVoid downloader;
	};

	sce::Download dw;
};

#endif
