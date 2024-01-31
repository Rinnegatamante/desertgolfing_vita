/*
 * main.c
 *
 * ARMv7 Shared Libraries loader. Soulcalibur Edition
 *
 * Copyright (C) 2021 Andy Nguyen
 * Copyright (C) 2021-2023 Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/settings.h"
#include "utils/logger.h"

#include <psp2/kernel/threadmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <AFakeNative/AFakeNative.h>
#include <vitasdk.h>
#include <stdio.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 24 * 1024 * 1024;
#endif

so_module so_mod;

#define SCE_POWER_CB_APP_SUSPEND 0x00400000
#define SCE_POWER_CB_BUTTON_PS_PRESS 0x20000000
#define SCE_POWER_CB_SYSTEM_SUSPEND 0x00010000

int power_cb(int notifyId, int notifyCount, int powerInfo, void *common) {
	if ((powerInfo & SCE_POWER_CB_BUTTON_PS_PRESS) ||
		(powerInfo & SCE_POWER_CB_APP_SUSPEND) ||
		(powerInfo & SCE_POWER_CB_SYSTEM_SUSPEND)) {
		printf("CrustySaveState\n");
		void (*CrustySaveState)() = (void *) so_symbol(&so_mod, "_Z15CrustySaveStatev");
		CrustySaveState();
	}
	return 0;
}

int callbacks_thread(unsigned int args, void* arg) {
	int cbid = sceKernelCreateCallback("Power Callback", 0, power_cb, NULL);
	scePowerRegisterCallback(cbid);
	for (;;) {
		sceKernelDelayThreadCB(10000000);
	}
	
	return 0;
}

int main() {
	sceSysmoduleLoadModule(SCE_SYSMODULE_RAZOR_CAPTURE);
	SceAppUtilInitParam appUtilParam;
	SceAppUtilBootParam appUtilBootParam;
	memset(&appUtilParam, 0, sizeof(SceAppUtilInitParam));
	memset(&appUtilBootParam, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&appUtilParam, &appUtilBootParam);
	
	printf("Loading FMOD Studio...\n");
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	int ret = sceNetShowNetstat();
	SceNetInitParam initparam;
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		initparam.memory = malloc(141 * 1024);
		initparam.size = 141 * 1024;
		initparam.flags = 0;
		sceNetInit(&initparam);
	}
	printf("sceKernelLoadStartModule %x\n", sceKernelLoadStartModule("vs0:sys/external/libfios2.suprx", 0, NULL, 0, NULL, NULL));
	printf("sceKernelLoadStartModule %x\n", sceKernelLoadStartModule("vs0:sys/external/libc.suprx", 0, NULL, 0, NULL, NULL));
	printf("sceKernelLoadStartModule %x\n", sceKernelLoadStartModule("ur0:data/libfmodstudio.suprx", 0, NULL, 0, NULL, NULL));
	
	soloader_init_all();

	int (*ANativeActivity_onCreate)(ANativeActivity *activity, void *savedState,
									size_t savedStateSize) = (void *) so_symbol(&so_mod, "ANativeActivity_onCreate");

	ANativeActivity *activity = ANativeActivity_create();
	log_info("Created NativeActivity object");

	ANativeActivity_onCreate(activity, NULL, 0);
	log_info("ANativeActivity_onCreate() passed");

	activity->callbacks->onStart(activity);
	log_info("onStart() passed");

	AInputQueue *aInputQueue = AInputQueue_create();
	activity->callbacks->onInputQueueCreated(activity, aInputQueue);
	log_info("onInputQueueCreated() passed");

	ANativeWindow *aNativeWindow = ANativeWindow_create();
	activity->callbacks->onNativeWindowCreated(activity, aNativeWindow);
	log_info("onNativeWindowCreated() passed");

	activity->callbacks->onWindowFocusChanged(activity, 1);
	log_info("onWindowFocusChanged() passed");
	
	// Starting power callbacks handler
	SceUID thid = sceKernelCreateThread("callbackThread", callbacks_thread, 0x10000100, 0x10000, 0, 0, NULL);
	if (thid >= 0)
		sceKernelStartThread(thid, 0, 0);

	log_info("Main thread shutting down");

	sceKernelExitDeleteThread(0);
}
