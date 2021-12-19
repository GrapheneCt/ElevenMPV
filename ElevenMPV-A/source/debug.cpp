#ifdef _DEBUG

#include <kernel.h>
#include <shellsvc.h>
#include <paf.h>
#include <libdbg.h>
#include <kernel/threadmgr_internal.h>
#include <kernel/threadmgr_mono.h>

#include "common.h"

//#define DEBUG_RESUME_FAULTY_THREAD

//extern unsigned int sceUserMainThreadAttribute = SCE_KERNEL_THREAD_ATTR_NOTIFY_EXCEPTION;
//extern const SceKernelThreadOptParamForMono sceUserMainThreadOptParam = { sizeof(SceKernelThreadOptParamForMono), SCE_KERNEL_THREAD_OPT_ATTR_NOTIFY_EXCP_MASK, 0, 0, 0, 0, SCE_KERNEL_EXCEPTION_TYPE_ANY };

static SceInt32 s_oldMemSize = 0;
static SceInt32 s_oldGraphMemSize = 0;
static SceUInt32 s_iter = 0;

int ExceptionHandlerThread(SceSize argSize, void *pArgBlock)
{
	SceInt32 ret;
	SceKernelExceptionInfoForMono excpInfo;
	SceKernelThreadContextCpuRegisterInfo cpureg;
	SceKernelThreadContextVfpRegisterInfo vfpreg;

	while (1) {
		excpInfo.size = sizeof(SceKernelExceptionInfoForMono);

		sceClibPrintf("[EMPVA_DEBUG] entering exception wait...\n");

		ret = sceKernelWaitExceptionForMono(SCE_KERNEL_EXCEPTION_TYPE_ANY, &excpInfo, NULL);

		if (ret == SCE_KERNEL_ERROR_EXCEPTION_MULTI)
			break;

		if (ret < 0) {
			sceClibPrintf("sceKernelWaitExceptionForMono returned 0x%x\n", ret);
		}

		sceClibMemset(&cpureg, 0, sizeof(SceKernelThreadContextCpuRegisterInfo));
		sceClibMemset(&vfpreg, 0, sizeof(SceKernelThreadContextVfpRegisterInfo));
		cpureg.size = sizeof(SceKernelThreadContextCpuRegisterInfo);
		vfpreg.size = sizeof(SceKernelThreadContextVfpRegisterInfo);

		ret = sceKernelGetThreadContextForMono(excpInfo.threadId, &cpureg, &vfpreg);
		if (ret < 0) {
			sceClibPrintf("sceKernelGetThreadContextForMono fail:0x%x\n", ret);
		}

		sceClibPrintf("[EMPVA_DEBUG] EXCEPTION\n");
		sceClibPrintf("exception from thread 0x%x\n", excpInfo.threadId);
		sceClibPrintf("cause 0x%x\n", excpInfo.cause);
		sceClibPrintf("r15 0x%x\n", cpureg.reg[15]);

#ifdef DEBUG_RESUME_FAULTY_THREAD
		cpureg.reg[15] += 2;

		sceClibPrintf("resuming...\n");

		sceKernelSetThreadContextForMono(excpInfo.threadId, &cpureg, &vfpreg);
		sceKernelResumeThreadForMono(excpInfo.threadId);
		continue;
#else
		sceShellUtilInitEvents(0);
		sceShellUtilRequestColdReset(0);
		sceKernelExitProcess(0);
#endif
	}

	sceClibPrintf("[EMPVA_DEBUG] unable to wait for exception due to another thread already waiting...debugger?\n");

	return 0;
}

SceVoid leakTestTask(ScePVoid pUserData)
{
	Allocator *glAlloc = Allocator::GetGlobalAllocator();
	SceInt32 sz = glAlloc->GetFreeSize();
	String *str = new String();

	str->MemsizeFormat(sz);
	sceClibPrintf("[EMPVA_DEBUG] Free heap memory: %s\n", str->data);
	SceInt32 delta = s_oldMemSize - sz;
	delta = -delta;
	if (delta) {
		sceClibPrintf("[EMPVA_DEBUG] Memory delta: %d bytes\n", delta);
	}
	s_oldMemSize = sz;
	delete str;

	str = new String();

	sz = Framework::s_frameworkInstance->GetDefaultGraphicsMemoryPool()->GetFreeSize();
	str->MemsizeFormat(sz);

	sceClibPrintf("[EMPVA_DEBUG] Free graphics pool: %s\n", str->data);

	delta = s_oldGraphMemSize - sz;
	delta = -delta;
	if (delta) {
		sceClibPrintf("[EMPVA_DEBUG] Graphics pool delta: %d bytes\n", delta);
	}
	s_oldGraphMemSize = sz;
	delete str;
}

SceVoid JobQueueTestTask(ScePVoid pUserData)
{
	if (g_coverJobQueue) {

		if (s_iter == 100) {
			sceClibPrintf("[EMPVA_DEBUG] EMPVA::CoverLoaderJobQueue, num of jobs: %u\n", g_coverJobQueue->GetSize());
			s_iter = 0;
		}
		else
			s_iter++;
	}
}

void InitDebug()
{
	/*SceUID exh = sceKernelCreateThread("EMPVA::ExceptionHandlerThread", ExceptionHandlerThread, SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_4KiB, 0, 0, 0);
	sceKernelStartThread(exh, 0, NULL);*/

	sceAppMgrSetInfobarState(SCE_TRUE, 0, 0); // In .sfo for release
	common::Utils::AddMainThreadTask(leakTestTask, SCE_NULL);
	//common::Utils::AddMainThreadTask(JobQueueTestTask, SCE_NULL);
}


#endif