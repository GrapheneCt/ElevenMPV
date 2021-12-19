#ifndef _ELEVENMPV_MENU_DISPLAYFILES_H_
#define _ELEVENMPV_MENU_DISPLAYFILES_H_

#include <paf.h>

using namespace paf;

namespace menu {
	namespace displayfiles {

		class Page;
		class File;

		class CoverLoaderJob : public paf::thread::JobQueue::Item
		{
		public:

			using thread::JobQueue::Item::Item;

			~CoverLoaderJob()
			{

			}

			SceVoid Run();

			SceVoid Finish();

			static SceVoid JobKiller(thread::JobQueue::Item *job)
			{
				if (job)
					delete job;
			}

			Page *workPage;
			File *workFile;
		};

		class BackButtonCB : public ui::Widget::EventCallback
		{
		public:

			BackButtonCB()
			{
				eventHandler = BackButtonCBFun;
			};

			virtual ~BackButtonCB()
			{

			};

			static SceVoid BackButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class PlayerButtonCB : public ui::Widget::EventCallback
		{
		public:

			PlayerButtonCB()
			{
				eventHandler = PlayerButtonCBFun;
			};

			virtual ~PlayerButtonCB()
			{

			};

			static SceVoid PlayerButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class ButtonCB : public ui::Widget::EventCallback
		{
		public:

			ButtonCB()
			{
				eventHandler = ButtonCBFun;
			};

			virtual ~ButtonCB()
			{

			};

			static SceVoid ButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

			static SceVoid StartNewPlayer(menu::displayfiles::File *startFile);
		};

		class BusyCB : public ui::Widget::EventCallback
		{
		public:

			BusyCB()
			{
				eventHandler = BusyCBFun;
			};

			virtual ~BusyCB()
			{

			};

			static SceVoid BusyCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class File
		{
		public:

			enum Type 
			{
				Type_Unsupported,
				Type_Dir,
				Type_Music
			};

			File() :
				next(SCE_NULL),
				name(SCE_NULL),
				type(Type_Unsupported),
				button(SCE_NULL)
			{

			}

			~File()
			{
				delete name;
			}

			File *next;
			SWString *name;
			char ext[5];
			Type type;
			ui::ImageButton *button;
			ButtonCB *buttonCB;
		};

		class Page
		{
		public:

			Page(const char *path);

			~Page();

			static SceVoid ResetBgPlaneTex();

			static SceVoid Init();

			ui::Plane *root;
			ui::Box *box;
			String *cwd;
			Page *prev;
			File *files;
			BusyCB *busyCB;
			CoverLoaderJob *coverLoader;
			SceUInt32 fileNum;
			SceBool coverState;
			File *coverWork;

		private:

			static SceInt32 Cmpstringp(const ScePVoid p1, const ScePVoid p2);

			SceInt32 PopulateFiles(const char *rootPath);
			SceVoid ClearFiles(File *file);

			const SceUInt32 k_busyIndicatorFileLimit = 50;
		};
	}
}

#endif
