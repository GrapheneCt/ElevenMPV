/*
  VitaTweaks: Download Enabler
  Copyright (C) 2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <kernel.h>
#include <paf.h>
#include <taihen.h>

using namespace paf;

static tai_hook_ref_t ExportFileRef;
static tai_hook_ref_t GetFileTypeRef;

static SceUID hooks[3];

static SceInt32 ExportFilePatched(uint32_t *data)
{
	int res = TAI_NEXT(ExportFilePatched, ExportFileRef, data);

	if (res == 0x80101A09) {
		char download_path[1024];
		char bgdl_path[1024];
		char file_name[256];
		char short_name[256];
		uint16_t url_length = 0;
		uint32_t count = 0;

		uint32_t num = *(uint32_t *)data[0];

		sce_paf_snprintf(bgdl_path, sizeof(bgdl_path), "ux0:bgdl/t/%08x/d0.pdb", num);

		SceUID fd = sceIoOpen(bgdl_path, SCE_O_RDONLY, 0);
		if (fd < 0)
			return fd;

		sceIoPread(fd, &url_length, sizeof(uint16_t), 0xD6);
		sceIoPread(fd, file_name, sizeof(file_name), 0xF7 + url_length);
		sceIoClose(fd);

		sce_paf_snprintf(bgdl_path, sizeof(bgdl_path), "ux0:bgdl/t/%08x/%s", num, file_name);

		char *ext = sce_paf_strrchr(file_name, '.');

		if (!ext || sce_paf_strcmp(ext, ".webmyt")) {
			return 0x80101A09;
		}

		ext[5] = '\0';
		int len = ext - file_name;
		if (len > sizeof(short_name) - 1)
			len = sizeof(short_name) - 1;
		sce_paf_strncpy(short_name, file_name, len);
		short_name[len] = '\0';

		while (1) {
			if (count == 0)
				sce_paf_snprintf(download_path, sizeof(download_path), "ux0:empva_download/%s", file_name);
			else
				sce_paf_snprintf(download_path, sizeof(download_path), "ux0:empva_download/%s (%d)%s", short_name, count, ext);

			if (!LocalFile::Exists(download_path))
				break;

			count++;
		}

		res = Dir::Create("ux0:empva_download");
		if (res < 0 && res != 0x80010011)
			return res;

		return LocalFile::RenameFile(bgdl_path, download_path);
	}

	return res;
}

static SceInt32 GetFileTypePatched(int unk, int *type, char **filename, char **mime_type)
{
	int res = TAI_NEXT(GetFileTypePatched, GetFileTypeRef, unk, type, filename, mime_type);

	if (res == 0x80103A21) {
		*type = 1; // Type photo
		return 0;
	}

	return res;
}

extern "C" {

	#include <moduleinfo.h>

	SCE_MODULE_INFO(ElevenMPV_A_DE, 2, 1, 1)

	int module_start(SceSize args, const void * argp)
	{
		tai_module_info_t info;
		info.size = sizeof(info);
		if (taiGetModuleInfo("SceShell", &info) >= 0) {
			switch (info.module_nid) {
			case 0x0552F692: // 3.60 retail
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x50A4A8, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x1163F6, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x11B5E4, 1, GetFileTypePatched);
				break;
			}

			case 0x6CB01295: // 3.60 PDEL
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x4F9A18, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x111D5A, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x116F48, 1, GetFileTypePatched);
				break;
			}

			case 0xEAB89D5C: // 3.60 PTEL
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x4FEBF8, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x112756, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x117944, 1, GetFileTypePatched);
				break;
			}

			case 0x5549BF1F: // 3.65 retail
			case 0x34B4D82E: // 3.67 retail
			case 0x12DAC0F3: // 3.68 retail
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x50A9E8, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x11644E, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x11B63C, 1, GetFileTypePatched);
				break;
			}

			case 0x0703C828: // 3.69 retail
			case 0x2053B5A5: // 3.70 retail
			case 0xF476E785: // 3.71 retail
			case 0x939FFBE9: // 3.72 retail
			case 0x734D476A: // 3.73 retail
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x50AA28, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x11644E, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x11B63C, 1, GetFileTypePatched);
				break;
			}

			case 0xE6A02F2B: // 3.65 PDEL
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x4F9F58, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x111DB2, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x116FA0, 1, GetFileTypePatched);
				break;
			}

			case 0x587F9CED: // 3.65 PTEL
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x4FF0F8, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&ExportFileRef, info.modid, 0, 0x1127AE, 1, ExportFilePatched);
				hooks[2] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x11799C, 1, GetFileTypePatched);
				break;
			}
			}
		}

		return SCE_KERNEL_START_SUCCESS;
	}

	int module_stop(SceSize args, const void * argp)
	{
		if (hooks[2] >= 0)
			taiHookRelease(hooks[2], GetFileTypeRef);
		if (hooks[1] >= 0)
			taiHookRelease(hooks[1], ExportFileRef);
		if (hooks[0] >= 0)
			taiInjectRelease(hooks[0]);
		return SCE_KERNEL_STOP_SUCCESS;
	}

}