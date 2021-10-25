#ifndef _ELEVENMPV_CONFIG_H_
#define _ELEVENMPV_CONFIG_H_

#include <kernel.h>
#include <paf.h>

/* Config layout in safemem: 

0: EMPVAConfig
sizeof(EMPVAConfig): cwd_exist
sizeof(EMPVAConfig) + 4: cwd string

*/

namespace config {

	class Config 
	{
	public:

		class EMPVAConfig 
		{
		public:

			SceUInt32 version;
			SceBool meta_flac;
			SceBool meta_mp3;
			SceBool meta_opus;
			SceUInt32 sort;
			SceBool alc_mode;
			SceUInt32 eq_mode;
			SceBool eq_volume;
			SceBool motion_mode;
			SceUInt32 motion_timer;
			SceUInt32 motion_degree;
			SceBool stick_skip;
			SceBool power_saving;
			SceUInt32 power_timer;
			SceUInt32 notify_mode;
			SceUInt32 device;
			SceUInt32 last_pagemode;

		};

		Config();

		~Config();

		SceVoid Save();

		EMPVAConfig *GetConfigLocation();

		SceVoid GetLastDirectory(paf::String *cwd);

		SceVoid SetLastDirectory(const char *cwd);

	private:

		EMPVAConfig *localConfig;

		char rootPath[8];

		SceBool configReset;

		const SceUInt32 k_configVersion = 16;
		const SceUInt32 k_defSort = 0;
		const SceUInt32 k_defAlcMode = 0;
		const SceUInt32 k_defEqMode = 0;
		const SceUInt32 k_defEqVolume = 0;
		const SceUInt32 k_defMotionMode = 0;
		const SceUInt32 k_defMotionTimer = 3;
		const SceUInt32 k_defMotionDeg = 45;
		const SceUInt32 k_defStickSkip = 1;
		const SceUInt32 k_defPowerSaving = 1;
		const SceUInt32 k_defPowerTimer = 1;
		const SceUInt32 k_defDevice = 0;
		const SceUInt32 k_defLastPagemode = 0;

		SceUInt32 config_version_holder;
	};
}

#endif
