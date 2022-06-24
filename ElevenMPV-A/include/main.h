#ifndef _ELEVENMPV_MAIN_H_
#define _ELEVENMPV_MAIN_H_

#include <libime.h>
#include <notification_util.h>
#include <kernel.h>
#include <paf.h>

using namespace paf;

namespace menu {
	namespace main {

		class PagemodeButtonCB : public ui::EventCallback
		{
		public:

			PagemodeButtonCB()
			{
				eventHandler = PagemodeButtonCBFun;
			};

			virtual ~PagemodeButtonCB()
			{

			};

			static SceInt32 LoadYoutubeStuff();

			static SceVoid PagemodeButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};
	}
}


#endif
