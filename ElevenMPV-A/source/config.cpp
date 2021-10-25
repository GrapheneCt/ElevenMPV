#include <apputil.h>
#include <kernel.h>
#include <paf.h>

#include "config.h"

using namespace paf;

config::Config::Config()
{
	configReset = SCE_FALSE;
	SceUInt32 savedConfVer = 0;

	localConfig = (EMPVAConfig *)sce_paf_malloc(sizeof(EMPVAConfig));

	// set these to the following by default:
	localConfig->version = k_configVersion;
	localConfig->sort = k_defSort;
	localConfig->alc_mode = k_defAlcMode;
	localConfig->eq_mode = k_defEqMode;
	localConfig->eq_volume = k_defEqVolume;
	localConfig->motion_mode = k_defMotionMode;
	localConfig->motion_timer = k_defMotionTimer;
	localConfig->motion_degree = k_defMotionDeg;
	localConfig->stick_skip = k_defStickSkip;
	localConfig->power_saving = k_defPowerSaving;
	localConfig->power_timer = k_defPowerTimer;
	localConfig->device = k_defDevice;
	localConfig->last_pagemode = k_defLastPagemode;

	sceAppUtilLoadSafeMemory(&savedConfVer, sizeof(SceUInt32), 0);

	// Reset config if updated version. This will rarely happen.
	if (savedConfVer != k_configVersion) {
		sceAppUtilSaveSafeMemory(localConfig, sizeof(EMPVAConfig), 0);
		configReset = SCE_TRUE;
	}
	else {
		sceAppUtilLoadSafeMemory(localConfig, sizeof(EMPVAConfig), 0);
	}
}

config::Config::~Config()
{
	sce_paf_free(localConfig);
	localConfig = SCE_NULL;
}

SceVoid config::Config::Save()
{
	sceAppUtilSaveSafeMemory(localConfig, sizeof(EMPVAConfig), 0);
}

config::Config::EMPVAConfig *config::Config::GetConfigLocation()
{
	return localConfig;
}

SceVoid config::Config::SetLastDirectory(const char *cwd)
{
	SceInt32 len = sce_paf_strlen(cwd);
	sceAppUtilSaveSafeMemory((ScePVoid)&len, sizeof(SceInt32), sizeof(EMPVAConfig));
	sceAppUtilSaveSafeMemory((ScePVoid)cwd, len, sizeof(EMPVAConfig) + sizeof(SceInt32));
}

SceVoid config::Config::GetLastDirectory(String *cwd)
{
	SceInt32 ret = 0;
	const char *root_paths[] = {
		"ux0:/",
		"ur0:/",
		"uma0:/",
		"xmc0:/",
		"imc0:/",
		"grw0:/"
	};

	SceInt32 len;
	sceAppUtilLoadSafeMemory((ScePVoid)&len, sizeof(SceInt32), sizeof(EMPVAConfig));

	if (!len || configReset) {

		ret = sce_paf_snprintf(rootPath, 8, "ux0:/");

		sceAppUtilSaveSafeMemory((ScePVoid)&ret, sizeof(SceInt32), sizeof(EMPVAConfig));
		sceAppUtilSaveSafeMemory((ScePVoid)cwd, ret, sizeof(EMPVAConfig) + sizeof(SceInt32));

		cwd->Set(rootPath, ret);

		configReset = SCE_FALSE;
	}
	else {

		sce_paf_strncpy(rootPath, root_paths[localConfig->device], 8);

		char *buf = (char *)sce_paf_malloc(len + 1);

		sceAppUtilLoadSafeMemory((ScePVoid)buf, len, sizeof(EMPVAConfig) + sizeof(SceInt32));

		buf[len] = '\0';

		if (io::Misc::Exists(buf))
			cwd->Set(buf);
		else
			cwd->Set(rootPath);

		sce_paf_free(buf);
	}
}

