#ifndef _ELEVENMPV_MENU_YOUTUBE_H_
#define _ELEVENMPV_MENU_YOUTUBE_H_

#include <paf.h>

#include "utils.h"
#include "youtube_parser.hpp"

using namespace paf;

namespace menu {
	namespace youtube {

		class SearchPage;
		class HistoryPage;
		class FavPage;

		class VideoButtonCB : public ui::Widget::EventCallback
		{
		public:

			VideoButtonCB()
			{
				eventHandler = VideoButtonCBFun;
			};

			virtual SceInt32 HandleEvent(SceInt32 eventId, ui::Widget *self, SceInt32 a3)
			{
				SceInt32 ret;

				if ((this->state & 1) == 0) {
					if (this->eventHandler != 0) {
						EMPVAUtils::RunCallbackAsJob(this->eventHandler, SCE_NULL, eventId, self, a3, this->pUserData);
					}
					ret = SCE_OK;
				}
				else {
					ret = SCE_PAF_ERROR_UI_EVENT_CALLBACK_UNHANDLED;
				}

				return ret;
			};

			virtual ~VideoButtonCB()
			{

			};

			static SceVoid VideoButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

			String url;
			SceUInt32 mode;
		};

		class SearchParserThread : public thread::Thread
		{
		public:

			using thread::Thread::Thread;

			SceVoid EntryFunction();

			static SceVoid CreateVideoButton(SearchPage *page, SceUInt32 index);

			String newRequset;
			SearchPage *prevPage;
			SearchPage *workPage;
			SceBool cancel;
		};

		class FavParserThread : public thread::Thread
		{
		public:

			using thread::Thread::Thread;

			SceVoid EntryFunction();

			static SceVoid CreateVideoButton(FavPage *page, const char *data, SceUInt32 index, const char *keyWord);

			String newRequset;
			FavPage *prevPage;
			FavPage *workPage;
			SceBool cancel;
			String keyWord;
		};

		class HistoryParserThread : public thread::Thread
		{
		public:

			using thread::Thread::Thread;

			SceVoid EntryFunction();

			static SceVoid CreateVideoButton(HistoryPage *page, const char *data, SceUInt32 index);

			String newRequset;
			HistoryPage *workPage;
			SceBool cancel;
		};

		class SearchActionButtonCB : public ui::Widget::EventCallback
		{
		public:

			SearchActionButtonCB()
			{
				eventHandler = SearchActionButtonCBFun;
			};

			virtual ~SearchActionButtonCB()
			{

			};

			static SceVoid SearchActionButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class SearchButtonCB : public ui::Widget::EventCallback
		{
		public:

			SearchButtonCB()
			{
				eventHandler = SearchButtonCBFun;
			};

			virtual ~SearchButtonCB()
			{

			};

			static SceVoid SearchButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class HistoryButtonCB : public ui::Widget::EventCallback
		{
		public:

			HistoryButtonCB()
			{
				eventHandler = HistoryButtonCBFun;
			};

			virtual ~HistoryButtonCB()
			{

			};

			static SceVoid HistoryButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class FavButtonCB : public ui::Widget::EventCallback
		{
		public:

			FavButtonCB()
			{
				eventHandler = FavButtonCBFun;
			};

			virtual ~FavButtonCB()
			{

			};

			static SceVoid FavButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class LeftButtonCB : public ui::Widget::EventCallback
		{
		public:

			LeftButtonCB()
			{
				eventHandler = LeftButtonCBFun;
			};

			virtual ~LeftButtonCB()
			{

			};

			static SceVoid LeftButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class RightButtonCB : public ui::Widget::EventCallback
		{
		public:

			RightButtonCB()
			{
				eventHandler = RightButtonCBFun;
			};

			virtual ~RightButtonCB()
			{

			};

			static SceVoid RightButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class Base
		{
		public:

			class NetCheckThread : public thread::Thread
			{
			public:

				using thread::Thread::Thread;

				SceVoid EntryFunction();
			};

			enum Mode
			{
				Mode_Search,
				Mode_History,
				Mode_Fav
			};

			static SceVoid InitCommon();

			static SceVoid InitSearch();

			static SceVoid InitHistory();

			static SceVoid InitFav();

			static SceVoid TermCommon();

			static SceVoid TermCurrentMode();

			static SceUInt32 GetCurrentMode();

			static SceVoid FirstTimeInit();

		private:

			static const SceUInt32 k_netMemSize = 16 * 1024;

			static SceInt32 InitYtStuff();

			static SceVoid netCtlStateCB(SceInt32 event_type, ScePVoid arg);
		};

		class SearchPage
		{
		public:

			SearchPage(SearchPage *prevPage);

			SearchPage(const char *searchWord);

			~SearchPage();

			static SceVoid LeftButtonOp();

			static SceVoid RightButtonOp();

			static SceVoid TermOp();

			static SceVoid SearchActionButtonOp();

			SearchPage *prev;
			SearchPage *next;
			YouTubeSearchResult *parseResult;
			SearchParserThread *parserThread;
			ui::Widget *thisPage;
			SceUInt32 resultCount;

		};

		class HistoryPage
		{
		public:

			HistoryPage();

			~HistoryPage();

			static SceVoid LeftButtonOp();

			static SceVoid RightButtonOp();

			static SceVoid TermOp();

			HistoryParserThread *parserThread;
			ui::Widget *thisPage;
			SceUInt32 resultCount;
		};

		class FavPage
		{
		public:

			FavPage(FavPage *prevPage);

			FavPage();

			FavPage(const char *keyWord);

			~FavPage();

			static SceVoid LeftButtonOp();

			static SceVoid RightButtonOp();

			static SceVoid TermOp();

			static SceVoid SearchActionButtonOp();

			FavPage *prev;
			FavPage *next;
			FavParserThread *parserThread;
			ui::Widget *thisPage;
			SceUInt32 resNum;
			static const SceInt32 k_resPerPage = 10;
		};
	}
}

#endif
