/*
 * dynlib.c
 *
 * Resolving dynamic imports of the .so.
 *
 * Copyright (C) 2021 Andy Nguyen
 * Copyright (C) 2021 Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

// Disable IDE complaints about _identifiers and global interfaces
#pragma ide diagnostic ignored "bugprone-reserved-identifier"
#pragma ide diagnostic ignored "cppcoreguidelines-interfaces-global-init"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

// Suppress `mktemp` deprecation warning
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define _POSIX_TIMERS
#include "dynlib.h"

#include <psp2/kernel/clib.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <zlib.h>
#include <dirent.h>
#include <locale.h>
#include <poll.h>

#include <SLES/OpenSLES.h>

#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>

#include <so_util/so_util.h>

#include "utils/glutil.h"
#include "utils/utils.h"
#include "utils/logger.h"

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "reimpl/env.h"
#include "reimpl/errno.h"
#include "reimpl/io.h"
#include "reimpl/ioctl.h"
#include "reimpl/log.h"
#include "reimpl/mem.h"
#include "reimpl/pthr.h"
#include "reimpl/sys.h"

#include <AFakeNative/ALooper.h>
#include <AFakeNative/AAssetManager.h>
#include <AFakeNative/AInput.h>
#include <AFakeNative/AConfiguration.h>
#include <AFakeNative/ANativeWindow.h>
#include <AFakeNative/polling/pseudo_pipe.h>
#include <AFakeNative/PseudoEpoll.h>
#include <AFakeNative/AFakeNative.h>

#include <fmod/fmod.h>

extern void *_ZN4FMOD6System17getSoftwareFormatEPiP16FMOD_SPEAKERMODES1_;
extern void *_ZN4FMOD6Studio4Bank6unloadEv;
extern void *_ZN4FMOD5Sound11getUserDataEPPv;
extern void *_ZN4FMOD5Sound12getOpenStateEP14FMOD_OPENSTATEPjPbS4_;
extern void *_ZN4FMOD5Sound15getNumSubSoundsEPi;
extern void *_ZN4FMOD5Sound11getSubSoundEiPPS0_;
extern void *_ZN4FMOD12ChannelGroup12getNumGroupsEPi;
extern void *_ZN4FMOD12ChannelGroup8getGroupEiPPS0_;
extern void *_ZN4FMOD12ChannelGroup14getNumChannelsEPi;
extern void *_ZN4FMOD12ChannelGroup10getChannelEiPPNS_7ChannelE;
extern void *_ZN4FMOD7Channel15getCurrentSoundEPPNS_5SoundE;
extern void *_ZN4FMOD14ChannelControl4stopEv;
extern void *_ZN4FMOD5Sound11setUserDataEPv;
extern void *_ZN4FMOD5Sound7releaseEv;
extern void *_ZN4FMOD6System10getChannelEiPPNS_7ChannelE;
extern void *_ZN4FMOD6Studio6System6updateEv;
extern void *_ZN4FMOD6Studio4Bank7isValidEv;
extern void *_ZN4FMOD6Studio4Bank15getLoadingStateEP25FMOD_STUDIO_LOADING_STATE;
extern void *_ZN4FMOD6Studio6System12loadBankFileEPKcjPPNS0_4BankE;
extern void *_ZN4FMOD5Sound15getSystemObjectEPPNS_6SystemE;
extern void *_ZN4FMOD6System21getMasterChannelGroupEPPNS_12ChannelGroupE;
extern void *_ZN4FMOD6Studio16EventDescription16unloadSampleDataEv;
extern void *_ZN4FMOD6Studio16EventDescription21getSampleLoadingStateEP25FMOD_STUDIO_LOADING_STATE;
extern void *_ZN4FMOD6Studio16EventDescription14loadSampleDataEv;
extern void *_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE;
extern void *_ZN4FMOD5Sound9getLengthEPjj;
extern void *_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKcPjPPvS5_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESI_i;
extern void *_ZN4FMOD6Studio6System10lookupPathEPK9FMOD_GUIDPciPi;
extern void *_ZN4FMOD6Studio6System6createEPPS1_j;
extern void *_ZN4FMOD6Studio6System17getLowLevelSystemEPPNS_6SystemE;
extern void *_ZN4FMOD6System11setCallbackEPF11FMOD_RESULTP11FMOD_SYSTEMjPvS4_S4_Ej;
extern void *_ZN4FMOD6System10getVersionEPj;
extern void *_ZN4FMOD6System19setStreamBufferSizeEjj;
extern void *_ZN4FMOD6System16setDSPBufferSizeEji;
extern void *_ZN4FMOD6System17setSoftwareFormatEi16FMOD_SPEAKERMODEi;
extern void *_ZN4FMOD6Studio6System10initializeEijjPv;
extern void *_ZN4FMOD6Studio6System7releaseEv;
extern void *_ZN4FMOD6Studio6System13flushCommandsEv;
extern void *_ZN4FMOD6Studio6System12getBankCountEPi;
extern void *_ZN4FMOD6Studio4Bank15getLoadingStateEP25FMOD_STUDIO_LOADING_STATE;
extern void *_ZN4FMOD6Studio6System12loadBankFileEPKcjPPNS0_4BankE;
extern void *_ZN4FMOD5Sound15getSystemObjectEPPNS_6SystemE;
extern void *_ZN4FMOD6System21getMasterChannelGroupEPPNS_12ChannelGroupE;
extern void *_ZN4FMOD6Studio6System12getEventByIDEPK9FMOD_GUIDPPNS0_16EventDescriptionE;
extern void *_ZN4FMOD6Studio16EventDescription16unloadSampleDataEv;
extern void *_ZN4FMOD6Studio16EventDescription21getSampleLoadingStateEP25FMOD_STUDIO_LOADING_STATE;
extern void *_ZN4FMOD6Studio16EventDescription14loadSampleDataEv;
extern void *_ZN4FMOD5Sound9getLengthEPjj;
extern void *_ZN4FMOD6Studio6System11getBankListEPPNS0_4BankEiPi;
extern void *_ZN4FMOD6Studio4Bank13getEventCountEPi;
extern void *_ZN4FMOD6Studio4Bank12getEventListEPPNS0_16EventDescriptionEiPi;
extern void *_ZN4FMOD6Studio16EventDescription5getIDEP9FMOD_GUID;
extern void *_ZN4FMOD6Studio4Bank11getBusCountEPi;
extern void *_ZN4FMOD6Studio4Bank10getBusListEPPNS0_3BusEiPi;
extern void *_ZN4FMOD6Studio3Bus5getIDEP9FMOD_GUID;
//extern void *_ZN4FMOD6Studio3Bus13setFaderLevelEf;
extern void *_ZN4FMOD6Studio3Bus7setMuteEb;
extern void *_ZN4FMOD6Studio3Bus9setPausedEb;
extern void *_ZN4FMOD6Studio3Bus7isValidEv;
extern void *_ZN4FMOD6Studio3Bus15getChannelGroupEPPNS_12ChannelGroupE;
extern void *_ZN4FMOD6Studio6System10getBusByIDEPK9FMOD_GUIDPPNS0_3BusE;
extern void *_ZN4FMOD6Studio3Bus18unlockChannelGroupEv;
extern void *_ZN4FMOD6Studio3Bus16lockChannelGroupEv;
extern void *_ZN4FMOD6Studio16EventDescription17getParameterCountEPi;
extern void *_ZN4FMOD6Studio16EventDescription19getParameterByIndexEiP33FMOD_STUDIO_PARAMETER_DESCRIPTION;
extern void *_ZN4FMOD6Studio13EventInstance7isValidEv;
extern void *_ZN4FMOD6Studio13EventInstance9setPausedEb;
extern void *_ZN4FMOD6Studio13EventInstance19setTimelinePositionEi;
extern void *_ZN4FMOD6Studio16EventDescription4is3DEPb;
extern void *_ZN4FMOD6Studio13EventInstance11setUserDataEPv;
extern void *_ZN4FMOD6Studio13EventInstance11setCallbackEPF11FMOD_RESULTjP25FMOD_STUDIO_EVENTINSTANCEPvEj;
extern void *_ZN4FMOD6Studio13EventInstance5startEv;
extern void *_ZN4FMOD6Studio16EventDescription14createInstanceEPPNS0_13EventInstanceE;
extern void *_ZN4FMOD6Studio13EventInstance15set3DAttributesEPK18FMOD_3D_ATTRIBUTES;
extern void *_ZN4FMOD6Studio13EventInstance7releaseEv;
extern void *_ZN4FMOD6Studio13EventInstance9setVolumeEf;
extern void *_ZN4FMOD6Studio16EventDescription9getLengthEPi;
extern void *_ZN4FMOD6Studio13EventInstance16getPlaybackStateEP26FMOD_STUDIO_PLAYBACK_STATE;
extern void *_ZN4FMOD6Studio16EventDescription9isOneshotEPb;
extern void *_ZN4FMOD6Studio16EventDescription15getUserPropertyEPKcP25FMOD_STUDIO_USER_PROPERTY;
extern void *_ZN4FMOD7Channel11setPositionEjj;
extern void *_ZN4FMOD7Channel11getPositionEPjj;
extern void *_ZN4FMOD6Studio13EventInstance4stopE21FMOD_STUDIO_STOP_MODE;
//extern void *_ZN4FMOD6Studio13EventInstance13getCueByIndexEiPPNS0_11CueInstanceE;
extern void *_ZN4FMOD6Studio13EventInstance12getParameterEPKcPPNS0_17ParameterInstanceE;
extern void *_ZN4FMOD6Studio17ParameterInstance8setValueEf;
//extern void *_ZN4FMOD6Studio11CueInstance7triggerEv;
extern void *_ZN4FMOD6Studio13EventInstance19getTimelinePositionEPi;
extern void *_ZN4FMOD6Studio13EventInstance15getChannelGroupEPPNS_12ChannelGroupE;
extern void *_ZN4FMOD14ChannelControl13getAudibilityEPf;
extern void *_ZN4FMOD6Studio13EventInstance17getParameterCountEPi;
extern void *_ZN4FMOD6Studio13EventInstance19getParameterByIndexEiPPNS0_17ParameterInstanceE;
extern void *_ZN4FMOD6Studio17ParameterInstance14getDescriptionEP33FMOD_STUDIO_PARAMETER_DESCRIPTION;
extern void *_ZN4FMOD6Studio16EventDescription7isValidEv;
extern void *_ZN4FMOD6Studio17ParameterInstance7isValidEv;
extern void *_ZN4FMOD6Studio13EventInstance11getUserDataEPPv;
extern void *_ZN4FMOD6Studio6System12getSoundInfoEPKcP22FMOD_STUDIO_SOUND_INFO;
extern void *_ZN4FMOD6Studio6System21setListenerAttributesEiPK18FMOD_3D_ATTRIBUTES;
extern void *_ZN4FMOD6System11mixerResumeEv;
extern void *_ZN4FMOD6System12mixerSuspendEv;
extern void *_ZN4FMOD14ChannelControl9getVolumeEPf;
extern void *_ZN4FMOD14ChannelControl9setVolumeEf;
extern void *_ZN4FMOD14ChannelControl7setMuteEb;
extern void *_ZN4FMOD14ChannelControl9setPausedEb;
extern void *_ZN4FMOD3DSP7releaseEv;
extern void *_ZN4FMOD12ChannelGroup7releaseEv;
extern void *_ZN4FMOD3DSP17setParameterFloatEif;
extern void *_ZN4FMOD6System18createChannelGroupEPKcPPNS_12ChannelGroupE;
extern void *_ZN4FMOD14ChannelControl6getDSPEiPPNS_3DSPE;
extern void *_ZN4FMOD3DSP16setChannelFormatEji16FMOD_SPEAKERMODE;
extern void *_ZN4FMOD12ChannelGroup8addGroupEPS0_bPPNS_13DSPConnectionE;
extern void *_ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE;
extern void *_ZN4FMOD14ChannelControl6addDSPEiPNS_3DSPE;
extern void *_ZN4FMOD14ChannelControl7setModeEj;
extern void *_ZN4FMOD14ChannelControl11setUserDataEPv;
extern void *_ZN4FMOD14ChannelControl11setCallbackEPF11FMOD_RESULTP19FMOD_CHANNELCONTROL24FMOD_CHANNELCONTROL_TYPE33FMOD_CHANNELCONTROL_CALLBACK_TYPEPvS6_E;
extern void *_ZN4FMOD14ChannelControl9isPlayingEPb;
extern void *_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_S3_;
extern void *_ZN4FMOD14ChannelControl11getUserDataEPPv;
extern void *_ZN4FMOD14ChannelControl19setReverbPropertiesEif;
extern void *_ZN4FMOD14ChannelControl6setPanEf;
extern void *_ZN4FMOD14ChannelControl8setPitchEf;
extern void *_ZN4FMOD14ChannelControl19get3DMinMaxDistanceEPfS1_;
extern void *_ZN4FMOD14ChannelControl19set3DMinMaxDistanceEff;
extern void *_ZN4FMOD7Channel12setLoopCountEi;
extern void *_ZN4FMOD14ChannelControl15getSystemObjectEPPNS_6SystemE;
extern void *_ZN4FMOD7Channel15getChannelGroupEPPNS_12ChannelGroupE;
extern void *_ZN4FMOD7Channel15setChannelGroupEPNS_12ChannelGroupE;
extern void *_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE;
extern void *_ZN4FMOD5Sound13getLoopPointsEPjjS1_j;
extern void *_ZN4FMOD5Sound16getNumSyncPointsEPi;
extern void *_ZN4FMOD3DSP15setParameterIntEii;
extern void *_ZN4FMOD6System13getNumDriversEPi;
extern void *_ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE;
extern void *_ZN4FMOD6System4initEijPv;
extern void *_ZN4FMOD6System6updateEv;
extern void *_ZN4FMOD6System23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_;
extern void *_ZN4FMOD7Channel12setFrequencyEf;
extern void *_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_;
extern void *_ZN4FMOD14ChannelControl14set3DOcclusionEff;
extern void *_ZN4FMOD7Channel11setPriorityEi;
extern void *_ZN4FMOD6System7releaseEv;
extern void *_ZN4FMOD6System12createStreamEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE;
extern void *_ZN4FMOD7Channel8getIndexEPi;

extern void * _ZNSt9exceptionD2Ev;
extern void * _ZSt17__throw_bad_allocv;
extern void * _ZSt9terminatev;
extern void * _ZdaPv;
extern void * _ZdlPv;
extern void * _Znaj;
extern void * __cxa_allocate_exception;
extern void * __cxa_begin_catch;
extern void * __cxa_end_catch;
extern void * __cxa_free_exception;
extern void * __cxa_rethrow;
extern void * __cxa_throw;
extern void * __gxx_personality_v0;
extern void *_ZNSt8bad_castD1Ev;
extern void *_ZTISt8bad_cast;
extern void *_ZTISt9exception;
extern void *_ZTVN10__cxxabiv117__class_type_infoE;
extern void *_ZTVN10__cxxabiv120__si_class_type_infoE;
extern void *_ZTVN10__cxxabiv121__vmi_class_type_infoE;
extern void *_Znwj;
extern void *__aeabi_atexit;
extern void *__cxa_atexit;
extern void *__cxa_finalize;
extern void *__cxa_pure_virtual;
extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;
extern void *__gnu_unwind_frame;
extern void *__stack_chk_fail;
extern void *__stack_chk_guard;

extern void *__aeabi_d2lz;
extern void *__aeabi_dadd;
extern void *__aeabi_dcmpgt;
extern void *__aeabi_dcmplt;
extern void *__aeabi_ddiv;
extern void *__aeabi_dmul;
extern void *__aeabi_f2lz;
extern void *__aeabi_i2d;
extern void *__aeabi_idiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_l2d;
extern void *__aeabi_l2f;
extern void *__aeabi_ldivmod;
extern void *__aeabi_uidiv;
extern void *__aeabi_uidivmod;
extern void *__aeabi_ui2d;
extern void *__aeabi_ul2d;
extern void *__aeabi_ul2f;
extern void *__aeabi_uldivmod;
extern void *__aeabi_unwind_cpp_pr0;
extern void *__aeabi_unwind_cpp_pr1;
extern void *__gnu_ldivmod_helper;

extern void *__aeabi_memclr;
extern void *__aeabi_memcpy;
extern void *__aeabi_memmove;
extern void *__aeabi_memset;
extern void *__aeabi_memset4;
extern void *__aeabi_memset8;

extern void *__srget;
extern void *__swbuf;

extern const char *BIONIC_ctype_;
extern const short *BIONIC_tolower_tab_;
extern const short *BIONIC_toupper_tab_;

static FILE __sF_fake[3];

int __atomic_dec(volatile int *ptr) {
	return __sync_fetch_and_sub (ptr, 1);
}

int __atomic_inc(volatile int *ptr) {
	return __sync_fetch_and_add (ptr, 1);
}

int __system_property_get(const char* name, char* value) {
	logv_error("__system_property_get(name: \"%s\")", name);
	return 0;
}

int sigaction(int signal, const struct sigaction* bionic_new_action, struct sigaction* bionic_old_action) {
	logv_error("sigaction: %i", signal);
	return 0;
}

int AAsset_getBuffer() {
	log_error("unimpl: AAsset_getBuffer");
	return 0;
}
int AAsset_getLength() {
	log_error("unimpl: AAsset_getLength");
	return 0;
}
int AAsset_openFileDescriptor() {
	log_error("unimpl: AAsset_openFileDescriptor");
	return 0;
}
int AAssetDir_close() {
	log_error("unimpl: AAssetDir_close");
	return 0;
}
int AAssetDir_getNextFileName() {
	log_error("unimpl: AAssetDir_getNextFileName");
	return 0;
}

int AAssetManager_openDir() {
	log_error("unimpl: AAssetManager_openDir");
	return 0;
}
int AInputEvent_getDeviceId() {
	//log_error("unimpl: AInputEvent_getDeviceId");
	return 0;
}
int AKeyEvent_getFlags() {
	//log_error("unimpl: AKeyEvent_getFlags");
	return 0;
}
int AKeyEvent_getMetaState() {
	//log_error("unimpl: AKeyEvent_getMetaState");
	return 0;
}
int AMotionEvent_getFlags() {
	//log_error("unimpl: AMotionEvent_getFlags");
	return 0;
}
int AMotionEvent_getHistoricalX() {
	log_error("unimpl: AMotionEvent_getHistoricalX");
	return 0;
}
int AMotionEvent_getHistoricalY() {
	log_error("unimpl: AMotionEvent_getHistoricalY");
	return 0;
}
int AMotionEvent_getHistorySize() {
	//log_error("unimpl: AMotionEvent_getHistorySize");
	return 0;
}
int AMotionEvent_getMetaState() {
	//log_error("unimpl: AMotionEvent_getMetaState");
	return 0;
}
int __gnu_Unwind_Find_exidx() {
	log_error("ret0d function __gnu_Unwind_Find_exidx called");
	return 0;
}

int __exidx_end() {
	log_error("ret0d function __exidx_end called");
	return 0;
}

int __exidx_start() {
	log_error("ret0d function __exidx_start called");
	return 0;
}

void exit_soloader(int status) {
	logv_info("exit(%i) called from %p", status, __builtin_return_address(0));
	exit(status);
}

void *dlsym_fake(void *restrict handle, const char *restrict symbol) {
	if (strcmp("AMotionEvent_getAxisValue", symbol) == 0) {
		return &AMotionEvent_getAxisValue;
	} else if (strcmp("AMotionEvent_getHistoricalAxisValue", symbol) == 0) {
		return &AMotionEvent_getHistoricalAxisValue;
	}

	logv_error("symbol %s not found", symbol);
	return NULL;
}


int AAssetManager_open_hook(void *mgr, const char *fname, int mode) {
	char full_fname[256];
	sprintf(full_fname, "ux0:data/desertgolfing/assets/%s", fname);
	printf("%s\n", full_fname);
	int r = open(full_fname, 0);
	if (r == -1)
		return 0;
	return r;
}

int AAsset_close_hook(int f) {
	return close(f);
}

off_t AAsset_getLength_hook(int f) {
	off_t res = lseek(f, 0, SEEK_END);
	lseek(f, 0, SEEK_SET);
	return res;
}

int AAsset_read_hook(int f, void *buf, size_t count) {
	int r = read(f, buf, count);
	return r;
}

size_t AAsset_seek_hook(int f, size_t offs, int whence) {
	return lseek(f, offs, whence);
}

const void *AAsset_getBuffer_hook(int f) {
	off_t len = lseek(f, 0, SEEK_END);
	lseek(f, 0, SEEK_SET);
	void *res = malloc(len);
	read(f, res, len);
	return res;
}

so_default_dynlib default_dynlib[] = {
		// FMOD
		{ "_ZN4FMOD6System7releaseEv", (uintptr_t)&_ZN4FMOD6System7releaseEv },
		{ "_ZN4FMOD6System12createStreamEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE", (uintptr_t)&_ZN4FMOD6System12createStreamEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE },
		{ "_ZN4FMOD7Channel8getIndexEPi", (uintptr_t)&_ZN4FMOD7Channel8getIndexEPi },
		{ "_ZN4FMOD6System17getSoftwareFormatEPiP16FMOD_SPEAKERMODES1_", (uintptr_t)&_ZN4FMOD6System17getSoftwareFormatEPiP16FMOD_SPEAKERMODES1_ },
		{ "_ZN4FMOD6Studio4Bank6unloadEv", (uintptr_t)&_ZN4FMOD6Studio4Bank6unloadEv },
		{ "_ZN4FMOD5Sound11getUserDataEPPv", (uintptr_t)&_ZN4FMOD5Sound11getUserDataEPPv },
		{ "_ZN4FMOD5Sound12getOpenStateEP14FMOD_OPENSTATEPjPbS4_", (uintptr_t)&_ZN4FMOD5Sound12getOpenStateEP14FMOD_OPENSTATEPjPbS4_ },
		{ "_ZN4FMOD5Sound15getNumSubSoundsEPi", (uintptr_t)&_ZN4FMOD5Sound15getNumSubSoundsEPi },
		{ "_ZN4FMOD5Sound11getSubSoundEiPPS0_", (uintptr_t)&_ZN4FMOD5Sound11getSubSoundEiPPS0_ },
		{ "_ZN4FMOD12ChannelGroup12getNumGroupsEPi", (uintptr_t)&_ZN4FMOD12ChannelGroup12getNumGroupsEPi },
		{ "_ZN4FMOD12ChannelGroup8getGroupEiPPS0_", (uintptr_t)&_ZN4FMOD12ChannelGroup8getGroupEiPPS0_ },
		{ "_ZN4FMOD12ChannelGroup14getNumChannelsEPi", (uintptr_t)&_ZN4FMOD12ChannelGroup14getNumChannelsEPi },
		{ "_ZN4FMOD12ChannelGroup10getChannelEiPPNS_7ChannelE", (uintptr_t)&_ZN4FMOD12ChannelGroup10getChannelEiPPNS_7ChannelE },
		{ "_ZN4FMOD7Channel15getCurrentSoundEPPNS_5SoundE", (uintptr_t)&_ZN4FMOD7Channel15getCurrentSoundEPPNS_5SoundE },
		{ "_ZN4FMOD14ChannelControl4stopEv", (uintptr_t)&_ZN4FMOD14ChannelControl4stopEv },
		{ "_ZN4FMOD5Sound11setUserDataEPv", (uintptr_t)&_ZN4FMOD5Sound11setUserDataEPv },
		{ "_ZN4FMOD5Sound7releaseEv", (uintptr_t)&_ZN4FMOD5Sound7releaseEv },
		{ "_ZN4FMOD6System10getChannelEiPPNS_7ChannelE", (uintptr_t)&_ZN4FMOD6System10getChannelEiPPNS_7ChannelE },
		{ "_ZN4FMOD6Studio6System6updateEv", (uintptr_t)&_ZN4FMOD6Studio6System6updateEv },
		{ "_ZNK4FMOD6Studio4Bank7isValidEv", (uintptr_t)&_ZN4FMOD6Studio4Bank7isValidEv },
		{ "_ZNK4FMOD6Studio4Bank15getLoadingStateEP25FMOD_STUDIO_LOADING_STATE", (uintptr_t)&_ZN4FMOD6Studio4Bank15getLoadingStateEP25FMOD_STUDIO_LOADING_STATE },
		{ "_ZN4FMOD6Studio6System12loadBankFileEPKcjPPNS0_4BankE", (uintptr_t)&_ZN4FMOD6Studio6System12loadBankFileEPKcjPPNS0_4BankE },
		{ "_ZN4FMOD5Sound15getSystemObjectEPPNS_6SystemE", (uintptr_t)&_ZN4FMOD5Sound15getSystemObjectEPPNS_6SystemE },
		{ "_ZN4FMOD6System21getMasterChannelGroupEPPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD6System21getMasterChannelGroupEPPNS_12ChannelGroupE },
		{ "_ZN4FMOD6Studio16EventDescription16unloadSampleDataEv", (uintptr_t)&_ZN4FMOD6Studio16EventDescription16unloadSampleDataEv },
		{ "_ZNK4FMOD6Studio16EventDescription21getSampleLoadingStateEP25FMOD_STUDIO_LOADING_STATE", (uintptr_t)&_ZN4FMOD6Studio16EventDescription21getSampleLoadingStateEP25FMOD_STUDIO_LOADING_STATE },
		{ "_ZN4FMOD6Studio16EventDescription14loadSampleDataEv", (uintptr_t)&_ZN4FMOD6Studio16EventDescription14loadSampleDataEv },
		{ "FMOD_Memory_GetStats", (uintptr_t)&FMOD_Memory_GetStats },
		{ "_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE", (uintptr_t)&_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE },
		{ "_ZN4FMOD5Sound9getLengthEPjj", (uintptr_t)&_ZN4FMOD5Sound9getLengthEPjj },
		{ "_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKcPjPPvS5_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESI_i", (uintptr_t)&_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKcPjPPvS5_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESI_i },
		{ "FMOD_Memory_Initialize", (uintptr_t)&FMOD_Memory_Initialize },
		{ "_ZNK4FMOD6Studio6System10lookupPathEPK9FMOD_GUIDPciPi", (uintptr_t)&_ZN4FMOD6Studio6System10lookupPathEPK9FMOD_GUIDPciPi },
		{ "_ZN4FMOD6Studio6System6createEPPS1_j", (uintptr_t)&_ZN4FMOD6Studio6System6createEPPS1_j },
		{ "_ZNK4FMOD6Studio6System17getLowLevelSystemEPPNS_6SystemE", (uintptr_t)&_ZN4FMOD6Studio6System17getLowLevelSystemEPPNS_6SystemE },
		{ "_ZN4FMOD6System11setCallbackEPF11FMOD_RESULTP11FMOD_SYSTEMjPvS4_S4_Ej", (uintptr_t)&_ZN4FMOD6System11setCallbackEPF11FMOD_RESULTP11FMOD_SYSTEMjPvS4_S4_Ej },
		{ "_ZN4FMOD6System10getVersionEPj", (uintptr_t)&_ZN4FMOD6System10getVersionEPj },
		{ "_ZN4FMOD6System19setStreamBufferSizeEjj", (uintptr_t)&_ZN4FMOD6System19setStreamBufferSizeEjj },
		{ "_ZN4FMOD6System16setDSPBufferSizeEji", (uintptr_t)&_ZN4FMOD6System16setDSPBufferSizeEji },
		{ "_ZN4FMOD6System17setSoftwareFormatEi16FMOD_SPEAKERMODEi", (uintptr_t)&_ZN4FMOD6System17setSoftwareFormatEi16FMOD_SPEAKERMODEi },
		{ "_ZN4FMOD6Studio6System10initializeEijjPv", (uintptr_t)&_ZN4FMOD6Studio6System10initializeEijjPv },
		{ "_ZN4FMOD6Studio6System7releaseEv", (uintptr_t)&_ZN4FMOD6Studio6System7releaseEv },
		{ "_ZN4FMOD6Studio6System13flushCommandsEv", (uintptr_t)&_ZN4FMOD6Studio6System13flushCommandsEv },
		{ "_ZNK4FMOD6Studio6System12getBankCountEPi", (uintptr_t)&_ZN4FMOD6Studio6System12getBankCountEPi },
		{ "_ZN4FMOD5Sound9getLengthEPjj", (uintptr_t)&_ZN4FMOD5Sound9getLengthEPjj },
		{ "_ZNK4FMOD6Studio6System11getBankListEPPNS0_4BankEiPi", (uintptr_t)&_ZN4FMOD6Studio6System11getBankListEPPNS0_4BankEiPi },
		{ "_ZNK4FMOD6Studio4Bank13getEventCountEPi", (uintptr_t)&_ZN4FMOD6Studio4Bank13getEventCountEPi },
		{ "_ZNK4FMOD6Studio4Bank12getEventListEPPNS0_16EventDescriptionEiPi", (uintptr_t)&_ZN4FMOD6Studio4Bank12getEventListEPPNS0_16EventDescriptionEiPi },
		{ "_ZNK4FMOD6Studio16EventDescription5getIDEP9FMOD_GUID", (uintptr_t)&_ZN4FMOD6Studio16EventDescription5getIDEP9FMOD_GUID },
		{ "_ZNK4FMOD6Studio4Bank11getBusCountEPi", (uintptr_t)&_ZN4FMOD6Studio4Bank11getBusCountEPi },
		{ "_ZNK4FMOD6Studio4Bank10getBusListEPPNS0_3BusEiPi", (uintptr_t)&_ZN4FMOD6Studio4Bank10getBusListEPPNS0_3BusEiPi },
		{ "_ZNK4FMOD6Studio3Bus5getIDEP9FMOD_GUID", (uintptr_t)&_ZN4FMOD6Studio3Bus5getIDEP9FMOD_GUID },
		{ "_ZN4FMOD6Studio3Bus13setFaderLevelEf", (uintptr_t)&ret0}, //&_ZN4FMOD6Studio3Bus13setFaderLevelEf },
		{ "_ZN4FMOD6Studio3Bus7setMuteEb", (uintptr_t)&_ZN4FMOD6Studio3Bus7setMuteEb },
		{ "_ZN4FMOD6Studio3Bus9setPausedEb", (uintptr_t)&_ZN4FMOD6Studio3Bus9setPausedEb },
		{ "_ZNK4FMOD6Studio3Bus7isValidEv", (uintptr_t)&_ZN4FMOD6Studio3Bus7isValidEv },
		{ "_ZNK4FMOD6Studio3Bus15getChannelGroupEPPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD6Studio3Bus15getChannelGroupEPPNS_12ChannelGroupE },
		{ "_ZNK4FMOD6Studio6System10getBusByIDEPK9FMOD_GUIDPPNS0_3BusE", (uintptr_t)&_ZN4FMOD6Studio6System10getBusByIDEPK9FMOD_GUIDPPNS0_3BusE },
		{ "_ZN4FMOD6Studio3Bus18unlockChannelGroupEv", (uintptr_t)&_ZN4FMOD6Studio3Bus18unlockChannelGroupEv },
		{ "_ZN4FMOD6Studio3Bus16lockChannelGroupEv", (uintptr_t)&_ZN4FMOD6Studio3Bus16lockChannelGroupEv },
		{ "_ZNK4FMOD6Studio16EventDescription17getParameterCountEPi", (uintptr_t)&_ZN4FMOD6Studio16EventDescription17getParameterCountEPi },
		{ "_ZNK4FMOD6Studio16EventDescription19getParameterByIndexEiP33FMOD_STUDIO_PARAMETER_DESCRIPTION", (uintptr_t)&_ZN4FMOD6Studio16EventDescription19getParameterByIndexEiP33FMOD_STUDIO_PARAMETER_DESCRIPTION },
		{ "_ZNK4FMOD6Studio13EventInstance7isValidEv", (uintptr_t)&_ZN4FMOD6Studio13EventInstance7isValidEv },
		{ "_ZN4FMOD6Studio13EventInstance9setPausedEb", (uintptr_t)&_ZN4FMOD6Studio13EventInstance9setPausedEb },
		{ "_ZN4FMOD6Studio13EventInstance19setTimelinePositionEi", (uintptr_t)&_ZN4FMOD6Studio13EventInstance19setTimelinePositionEi },
		{ "_ZNK4FMOD6Studio16EventDescription4is3DEPb", (uintptr_t)&_ZN4FMOD6Studio16EventDescription4is3DEPb },
		{ "_ZN4FMOD6Studio13EventInstance11setUserDataEPv", (uintptr_t)&_ZN4FMOD6Studio13EventInstance11setUserDataEPv },
		{ "_ZN4FMOD6Studio13EventInstance11setCallbackEPF11FMOD_RESULTjP25FMOD_STUDIO_EVENTINSTANCEPvEj", (uintptr_t)&_ZN4FMOD6Studio13EventInstance11setCallbackEPF11FMOD_RESULTjP25FMOD_STUDIO_EVENTINSTANCEPvEj },
		{ "_ZN4FMOD6Studio13EventInstance5startEv", (uintptr_t)&_ZN4FMOD6Studio13EventInstance5startEv },
		{ "_ZNK4FMOD6Studio16EventDescription14createInstanceEPPNS0_13EventInstanceE", (uintptr_t)&_ZN4FMOD6Studio16EventDescription14createInstanceEPPNS0_13EventInstanceE },
		{ "_ZN4FMOD6Studio13EventInstance15set3DAttributesEPK18FMOD_3D_ATTRIBUTES", (uintptr_t)&_ZN4FMOD6Studio13EventInstance15set3DAttributesEPK18FMOD_3D_ATTRIBUTES },
		{ "_ZN4FMOD6Studio13EventInstance7releaseEv", (uintptr_t)&_ZN4FMOD6Studio13EventInstance7releaseEv },
		{ "_ZN4FMOD6Studio13EventInstance9setVolumeEf", (uintptr_t)&_ZN4FMOD6Studio13EventInstance9setVolumeEf },
		{ "_ZNK4FMOD6Studio16EventDescription9getLengthEPi", (uintptr_t)&_ZN4FMOD6Studio16EventDescription9getLengthEPi },
		{ "_ZNK4FMOD6Studio13EventInstance16getPlaybackStateEP26FMOD_STUDIO_PLAYBACK_STATE", (uintptr_t)&_ZN4FMOD6Studio13EventInstance16getPlaybackStateEP26FMOD_STUDIO_PLAYBACK_STATE },
		{ "_ZNK4FMOD6Studio16EventDescription9isOneshotEPb", (uintptr_t)&_ZN4FMOD6Studio16EventDescription9isOneshotEPb },
		{ "_ZNK4FMOD6Studio16EventDescription15getUserPropertyEPKcP25FMOD_STUDIO_USER_PROPERTY", (uintptr_t)&_ZN4FMOD6Studio16EventDescription15getUserPropertyEPKcP25FMOD_STUDIO_USER_PROPERTY },
		{ "_ZN4FMOD7Channel11setPositionEjj", (uintptr_t)&_ZN4FMOD7Channel11setPositionEjj },
		{ "_ZN4FMOD7Channel11getPositionEPjj", (uintptr_t)&_ZN4FMOD7Channel11getPositionEPjj },
		{ "_ZN4FMOD6Studio13EventInstance4stopE21FMOD_STUDIO_STOP_MODE", (uintptr_t)&_ZN4FMOD6Studio13EventInstance4stopE21FMOD_STUDIO_STOP_MODE },
		{ "_ZNK4FMOD6Studio13EventInstance13getCueByIndexEiPPNS0_11CueInstanceE", (uintptr_t)&ret0 }, //_ZN4FMOD6Studio13EventInstance13getCueByIndexEiPPNS0_11CueInstanceE },
		{ "_ZNK4FMOD6Studio13EventInstance12getParameterEPKcPPNS0_17ParameterInstanceE", (uintptr_t)&_ZN4FMOD6Studio13EventInstance12getParameterEPKcPPNS0_17ParameterInstanceE },
		{ "_ZN4FMOD6Studio17ParameterInstance8setValueEf", (uintptr_t)&_ZN4FMOD6Studio17ParameterInstance8setValueEf },
		{ "_ZN4FMOD6Studio11CueInstance7triggerEv", (uintptr_t)&ret0 }, //_ZN4FMOD6Studio11CueInstance7triggerEv },
		{ "_ZNK4FMOD6Studio13EventInstance19getTimelinePositionEPi", (uintptr_t)&_ZN4FMOD6Studio13EventInstance19getTimelinePositionEPi },
		{ "_ZNK4FMOD6Studio13EventInstance15getChannelGroupEPPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD6Studio13EventInstance15getChannelGroupEPPNS_12ChannelGroupE },
		{ "_ZN4FMOD14ChannelControl13getAudibilityEPf", (uintptr_t)&_ZN4FMOD14ChannelControl13getAudibilityEPf },
		{ "_ZNK4FMOD6Studio13EventInstance17getParameterCountEPi", (uintptr_t)&_ZN4FMOD6Studio13EventInstance17getParameterCountEPi },
		{ "_ZNK4FMOD6Studio13EventInstance19getParameterByIndexEiPPNS0_17ParameterInstanceE", (uintptr_t)&_ZN4FMOD6Studio13EventInstance19getParameterByIndexEiPPNS0_17ParameterInstanceE },
		{ "_ZNK4FMOD6Studio17ParameterInstance14getDescriptionEP33FMOD_STUDIO_PARAMETER_DESCRIPTION", (uintptr_t)&_ZN4FMOD6Studio17ParameterInstance14getDescriptionEP33FMOD_STUDIO_PARAMETER_DESCRIPTION },
		{ "_ZNK4FMOD6Studio16EventDescription7isValidEv", (uintptr_t)&_ZN4FMOD6Studio16EventDescription7isValidEv },
		{ "_ZNK4FMOD6Studio17ParameterInstance7isValidEv", (uintptr_t)&_ZN4FMOD6Studio17ParameterInstance7isValidEv },
		{ "_ZNK4FMOD6Studio13EventInstance11getUserDataEPPv", (uintptr_t)&_ZN4FMOD6Studio13EventInstance11getUserDataEPPv },
		{ "_ZNK4FMOD6Studio6System12getSoundInfoEPKcP22FMOD_STUDIO_SOUND_INFO", (uintptr_t)&_ZN4FMOD6Studio6System12getSoundInfoEPKcP22FMOD_STUDIO_SOUND_INFO },
		{ "_ZN4FMOD6Studio6System21setListenerAttributesEiPK18FMOD_3D_ATTRIBUTES", (uintptr_t)&_ZN4FMOD6Studio6System21setListenerAttributesEiPK18FMOD_3D_ATTRIBUTES },
		{ "_ZN4FMOD6System11mixerResumeEv", (uintptr_t)&_ZN4FMOD6System11mixerResumeEv },
		{ "_ZN4FMOD6System12mixerSuspendEv", (uintptr_t)&_ZN4FMOD6System12mixerSuspendEv },
		{ "_ZN4FMOD14ChannelControl9getVolumeEPf", (uintptr_t)&_ZN4FMOD14ChannelControl9getVolumeEPf },
		{ "_ZN4FMOD14ChannelControl9setVolumeEf", (uintptr_t)&_ZN4FMOD14ChannelControl9setVolumeEf },
		{ "_ZN4FMOD14ChannelControl7setMuteEb", (uintptr_t)&_ZN4FMOD14ChannelControl7setMuteEb },
		{ "_ZN4FMOD14ChannelControl9setPausedEb", (uintptr_t)&_ZN4FMOD14ChannelControl9setPausedEb },
		{ "_ZN4FMOD3DSP7releaseEv", (uintptr_t)&_ZN4FMOD3DSP7releaseEv },
		{ "_ZN4FMOD12ChannelGroup7releaseEv", (uintptr_t)&_ZN4FMOD12ChannelGroup7releaseEv },
		{ "_ZN4FMOD3DSP17setParameterFloatEif", (uintptr_t)&_ZN4FMOD3DSP17setParameterFloatEif },
		{ "_ZN4FMOD6System18createChannelGroupEPKcPPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD6System18createChannelGroupEPKcPPNS_12ChannelGroupE },
		{ "_ZN4FMOD14ChannelControl6getDSPEiPPNS_3DSPE", (uintptr_t)&_ZN4FMOD14ChannelControl6getDSPEiPPNS_3DSPE },
		{ "_ZN4FMOD3DSP16setChannelFormatEji16FMOD_SPEAKERMODE", (uintptr_t)&_ZN4FMOD3DSP16setChannelFormatEji16FMOD_SPEAKERMODE },
		{ "_ZN4FMOD12ChannelGroup8addGroupEPS0_bPPNS_13DSPConnectionE", (uintptr_t)&_ZN4FMOD12ChannelGroup8addGroupEPS0_bPPNS_13DSPConnectionE },
		{ "_ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE", (uintptr_t)&_ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE },
		{ "_ZN4FMOD14ChannelControl6addDSPEiPNS_3DSPE", (uintptr_t)&_ZN4FMOD14ChannelControl6addDSPEiPNS_3DSPE },
		{ "_ZN4FMOD14ChannelControl7setModeEj", (uintptr_t)&_ZN4FMOD14ChannelControl7setModeEj },
		{ "_ZN4FMOD14ChannelControl11setUserDataEPv", (uintptr_t)&_ZN4FMOD14ChannelControl11setUserDataEPv },
		{ "_ZN4FMOD14ChannelControl11setCallbackEPF11FMOD_RESULTP19FMOD_CHANNELCONTROL24FMOD_CHANNELCONTROL_TYPE33FMOD_CHANNELCONTROL_CALLBACK_TYPEPvS6_E", (uintptr_t)&_ZN4FMOD14ChannelControl11setCallbackEPF11FMOD_RESULTP19FMOD_CHANNELCONTROL24FMOD_CHANNELCONTROL_TYPE33FMOD_CHANNELCONTROL_CALLBACK_TYPEPvS6_E },
		{ "_ZN4FMOD14ChannelControl9isPlayingEPb", (uintptr_t)&_ZN4FMOD14ChannelControl9isPlayingEPb },
		{ "_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_S3_", (uintptr_t)&_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_S3_ },
		{ "_ZN4FMOD14ChannelControl11getUserDataEPPv", (uintptr_t)&_ZN4FMOD14ChannelControl11getUserDataEPPv },
		{ "_ZN4FMOD14ChannelControl19setReverbPropertiesEif", (uintptr_t)&_ZN4FMOD14ChannelControl19setReverbPropertiesEif },
		{ "_ZN4FMOD14ChannelControl6setPanEf", (uintptr_t)&_ZN4FMOD14ChannelControl6setPanEf },
		{ "_ZN4FMOD14ChannelControl8setPitchEf", (uintptr_t)&_ZN4FMOD14ChannelControl8setPitchEf },
		{ "_ZN4FMOD14ChannelControl19get3DMinMaxDistanceEPfS1_", (uintptr_t)&_ZN4FMOD14ChannelControl19get3DMinMaxDistanceEPfS1_ },
		{ "_ZN4FMOD14ChannelControl19set3DMinMaxDistanceEff", (uintptr_t)&_ZN4FMOD14ChannelControl19set3DMinMaxDistanceEff },
		{ "_ZN4FMOD7Channel12setLoopCountEi", (uintptr_t)&_ZN4FMOD7Channel12setLoopCountEi },
		{ "_ZN4FMOD14ChannelControl15getSystemObjectEPPNS_6SystemE", (uintptr_t)&_ZN4FMOD14ChannelControl15getSystemObjectEPPNS_6SystemE },
		{ "_ZN4FMOD7Channel15getChannelGroupEPPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD7Channel15getChannelGroupEPPNS_12ChannelGroupE },
		{ "_ZN4FMOD7Channel15setChannelGroupEPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD7Channel15setChannelGroupEPNS_12ChannelGroupE },
		{ "_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE", (uintptr_t)&_ZN4FMOD6System9playSoundEPNS_5SoundEPNS_12ChannelGroupEbPPNS_7ChannelE },
		{ "_ZN4FMOD5Sound13getLoopPointsEPjjS1_j", (uintptr_t)&_ZN4FMOD5Sound13getLoopPointsEPjjS1_j },
		{ "_ZN4FMOD5Sound16getNumSyncPointsEPi", (uintptr_t)&_ZN4FMOD5Sound16getNumSyncPointsEPi },
		{ "_ZN4FMOD3DSP15setParameterIntEii", (uintptr_t)&_ZN4FMOD3DSP15setParameterIntEii },
		{ "FMOD_System_Create", (uintptr_t)&FMOD_System_Create },
		{ "_ZN4FMOD6System13getNumDriversEPi", (uintptr_t)&_ZN4FMOD6System13getNumDriversEPi },
		{ "_ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE", (uintptr_t)&_ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE },
		{ "_ZN4FMOD6System4initEijPv", (uintptr_t)&_ZN4FMOD6System4initEijPv },
		{ "_ZN4FMOD6System6updateEv", (uintptr_t)&_ZN4FMOD6System6updateEv },
		{ "_ZN4FMOD6System23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_", (uintptr_t)&_ZN4FMOD6System23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_ },
		{ "_ZN4FMOD7Channel12setFrequencyEf", (uintptr_t)&_ZN4FMOD7Channel12setFrequencyEf },
		{ "_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_", (uintptr_t)&ret0 }, //_ZN4FMOD14ChannelControl15set3DAttributesEPK11FMOD_VECTORS3_ },
		{ "_ZN4FMOD14ChannelControl14set3DOcclusionEff", (uintptr_t)&_ZN4FMOD14ChannelControl14set3DOcclusionEff },
		{ "_ZN4FMOD7Channel11setPriorityEi", (uintptr_t)&_ZN4FMOD7Channel11setPriorityEi },
		
		// Common C/C++ internals
		{ "newlocale", (uintptr_t)&newlocale},
		{ "uselocale", (uintptr_t)&uselocale},
		{ "freelocale", (uintptr_t)&freelocale},
		{ "_ZNSt8bad_castD1Ev", (uintptr_t)&_ZNSt8bad_castD1Ev },
		{ "_ZNSt9exceptionD2Ev", (uintptr_t)&_ZNSt9exceptionD2Ev },
		{ "_ZSt17__throw_bad_allocv", (uintptr_t)&_ZSt17__throw_bad_allocv },
		{ "_ZSt9terminatev", (uintptr_t)&_ZSt9terminatev },
		{ "_ZTISt8bad_cast", (uintptr_t)&_ZTISt8bad_cast },
		{ "_ZTISt9exception", (uintptr_t)&_ZTISt9exception },
		{ "_ZTVN10__cxxabiv117__class_type_infoE", (uintptr_t)&_ZTVN10__cxxabiv117__class_type_infoE },
		{ "_ZTVN10__cxxabiv120__si_class_type_infoE", (uintptr_t)&_ZTVN10__cxxabiv120__si_class_type_infoE },
		{ "_ZTVN10__cxxabiv121__vmi_class_type_infoE", (uintptr_t)&_ZTVN10__cxxabiv121__vmi_class_type_infoE },
		{ "_ZdaPv", (uintptr_t)&_ZdaPv },
		{ "_ZdlPv", (uintptr_t)&_ZdlPv },
		{ "_Znaj", (uintptr_t)&_Znaj },
		{ "_Znwj", (uintptr_t)&_Znwj },
		{ "__aeabi_atexit", (uintptr_t)&__aeabi_atexit },
		{ "__aeabi_d2lz", (uintptr_t)&__aeabi_d2lz },
		{ "__aeabi_dadd", (uintptr_t)&__aeabi_dadd },
		{ "__aeabi_dcmpgt", (uintptr_t)&__aeabi_dcmpgt },
		{ "__aeabi_dcmplt", (uintptr_t)&__aeabi_dcmplt },
		{ "__aeabi_ddiv", (uintptr_t)&__aeabi_ddiv },
		{ "__aeabi_dmul", (uintptr_t)&__aeabi_dmul },
		{ "__aeabi_f2lz", (uintptr_t)&__aeabi_f2lz },
		{ "__aeabi_i2d", (uintptr_t)&__aeabi_i2d },
		{ "__aeabi_idiv", (uintptr_t)&__aeabi_idiv },
		{ "__aeabi_idivmod", (uintptr_t)&__aeabi_idivmod },
		{ "__aeabi_l2d", (uintptr_t)&__aeabi_l2d },
		{ "__aeabi_l2f", (uintptr_t)&__aeabi_l2f },
		{ "__aeabi_ldivmod", (uintptr_t)&__aeabi_ldivmod },
		{ "__aeabi_memclr", (uintptr_t)&__aeabi_memclr },
		{ "__aeabi_memclr4", (uintptr_t)&__aeabi_memclr },
		{ "__aeabi_memclr8", (uintptr_t)&__aeabi_memclr },
		{ "__aeabi_memcpy", (uintptr_t)&__aeabi_memcpy },
		{ "__aeabi_memcpy4", (uintptr_t)&__aeabi_memcpy },
		{ "__aeabi_memcpy8", (uintptr_t)&__aeabi_memcpy },
		{ "__aeabi_memmove", (uintptr_t)&__aeabi_memmove },
		{ "__aeabi_memmove4", (uintptr_t)&__aeabi_memmove },
		{ "__aeabi_memmove8", (uintptr_t)&__aeabi_memmove },
		{ "__aeabi_memset", (uintptr_t)&__aeabi_memset },
		{ "__aeabi_memset4",  (uintptr_t)&__aeabi_memset4 },
		{ "__aeabi_memset8", (uintptr_t)&__aeabi_memset8 },
		{ "__aeabi_ui2d", (uintptr_t)&__aeabi_ui2d },
		{ "__aeabi_uidiv", (uintptr_t)&__aeabi_uidiv },
		{ "__aeabi_uidivmod", (uintptr_t)&__aeabi_uidivmod },
		{ "__aeabi_ul2d", (uintptr_t)&__aeabi_ul2d },
		{ "__aeabi_ul2f", (uintptr_t)&__aeabi_ul2f },
		{ "__aeabi_uldivmod", (uintptr_t)&__aeabi_uldivmod },
		{ "__aeabi_unwind_cpp_pr0", (uintptr_t)&__aeabi_unwind_cpp_pr0 },
		{ "__aeabi_unwind_cpp_pr1", (uintptr_t)&__aeabi_unwind_cpp_pr1 },
		{ "__atomic_dec", (uintptr_t)&__atomic_dec },
		{ "__atomic_inc", (uintptr_t)&__atomic_inc },
		{ "__cxa_allocate_exception", (uintptr_t)&__cxa_allocate_exception },
		{ "__cxa_atexit", (uintptr_t)&__cxa_atexit },
		{ "__cxa_begin_catch", (uintptr_t)&__cxa_begin_catch },
		{ "__cxa_end_catch", (uintptr_t)&__cxa_end_catch },
		{ "__cxa_finalize", (uintptr_t)&__cxa_finalize },
		{ "__cxa_free_exception", (uintptr_t)&__cxa_free_exception },
		{ "__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire },
		{ "__cxa_guard_release", (uintptr_t)&__cxa_guard_release },
		{ "__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual },
		{ "__cxa_rethrow", (uintptr_t)&__cxa_rethrow },
		{ "__cxa_throw", (uintptr_t)&__cxa_throw },
		{ "__exidx_end", (uintptr_t)&__exidx_end },
		{ "__exidx_start", (uintptr_t)&__exidx_start },
		{ "__gnu_Unwind_Find_exidx", (uintptr_t)&__gnu_Unwind_Find_exidx },
		{ "__gnu_ldivmod_helper", (uintptr_t)&__gnu_ldivmod_helper },
		{ "__gnu_unwind_frame", (uintptr_t)&__gnu_unwind_frame },
		{ "__google_potentially_blocking_region_begin", (uintptr_t)&ret0 },
		{ "__google_potentially_blocking_region_end", (uintptr_t)&ret0 },
		{ "__gxx_personality_v0", (uintptr_t)&__gxx_personality_v0 },
		{ "__sF", (uintptr_t)&__sF_fake },
		{ "__srget", (uintptr_t)&__srget },
		{ "__stack_chk_fail", (uintptr_t)&__stack_chk_fail },
		{ "__stack_chk_guard", (uintptr_t)&__stack_chk_guard },
		{ "__swbuf", (uintptr_t)&__swbuf },
		{ "__system_property_get", (uintptr_t)&__system_property_get },


		// ANative
		{ "AAssetDir_close", (uintptr_t)&AAssetDir_close },
		{ "AAssetDir_getNextFileName", (uintptr_t)&AAssetDir_getNextFileName },
		{ "AAssetManager_open", (uintptr_t)&AAssetManager_open_hook },
		{ "AAssetManager_openDir", (uintptr_t)&AAssetManager_openDir },
		{ "AAsset_close", (uintptr_t)&AAsset_close_hook },
		{ "AAsset_getBuffer", (uintptr_t)&AAsset_getBuffer_hook },
		{ "AAsset_getLength", (uintptr_t)&AAsset_getLength_hook },
		{ "AAsset_openFileDescriptor", (uintptr_t)&AAsset_openFileDescriptor },
		{ "AAsset_read", (uintptr_t)&AAsset_read_hook },
		{ "AAsset_seek", (uintptr_t)&AAsset_seek_hook },
		{ "AConfiguration_delete", (uintptr_t)&AConfiguration_delete },
		{ "AConfiguration_fromAssetManager", (uintptr_t)&AConfiguration_fromAssetManager },
		{ "AConfiguration_getCountry", (uintptr_t)&AConfiguration_getCountry },
		{ "AConfiguration_getLanguage", (uintptr_t)&AConfiguration_getLanguage },
		{ "AConfiguration_new", (uintptr_t)&AConfiguration_new },
		{ "AInputEvent_getDeviceId", (uintptr_t)&AInputEvent_getDeviceId },
		{ "AInputEvent_getSource", (uintptr_t)&AInputEvent_getSource },
		{ "AInputEvent_getType", (uintptr_t)&AInputEvent_getType },
		{ "AInputQueue_attachLooper", (uintptr_t)&AInputQueue_attachLooper },
		{ "AInputQueue_detachLooper", (uintptr_t)&AInputQueue_detachLooper },
		{ "AInputQueue_finishEvent", (uintptr_t)&AInputQueue_finishEvent },
		{ "AInputQueue_getEvent", (uintptr_t)&AInputQueue_getEvent },
		{ "AInputQueue_preDispatchEvent", (uintptr_t)&AInputQueue_preDispatchEvent },
		{ "AKeyEvent_getAction", (uintptr_t)&AKeyEvent_getAction },
		{ "AKeyEvent_getFlags", (uintptr_t)&AKeyEvent_getFlags },
		{ "AKeyEvent_getKeyCode", (uintptr_t)&AKeyEvent_getKeyCode },
		{ "AKeyEvent_getMetaState", (uintptr_t)&AKeyEvent_getMetaState },
		{ "ALooper_addFd", (uintptr_t)&ALooper_addFd },
		{ "ALooper_pollAll", (uintptr_t)&ALooper_pollAll },
		{ "ALooper_prepare", (uintptr_t)&ALooper_prepare },
		{ "AMotionEvent_getAction", (uintptr_t)&AMotionEvent_getAction },
		{ "AMotionEvent_getAxisValue", (uintptr_t)&AMotionEvent_getAxisValue },
		{ "AMotionEvent_getFlags", (uintptr_t)&AMotionEvent_getFlags },
		{ "AMotionEvent_getHistoricalX", (uintptr_t)&AMotionEvent_getHistoricalX },
		{ "AMotionEvent_getHistoricalY", (uintptr_t)&AMotionEvent_getHistoricalY },
		{ "AMotionEvent_getHistorySize", (uintptr_t)&AMotionEvent_getHistorySize },
		{ "AMotionEvent_getMetaState", (uintptr_t)&AMotionEvent_getMetaState },
		{ "AMotionEvent_getPointerCount", (uintptr_t)&AMotionEvent_getPointerCount },
		{ "AMotionEvent_getPointerId", (uintptr_t)&AMotionEvent_getPointerId },
		{ "AMotionEvent_getX", (uintptr_t)&AMotionEvent_getX },
		{ "AMotionEvent_getY", (uintptr_t)&AMotionEvent_getY },
		{ "ANativeActivity_finish", (uintptr_t)&ANativeActivity_finish },
		{ "ANativeActivity_setWindowFlags", (uintptr_t)&ANativeActivity_setWindowFlags },
		{ "ANativeWindow_getHeight", (uintptr_t)&ANativeWindow_getHeight },
		{ "ANativeWindow_getWidth", (uintptr_t)&ANativeWindow_getWidth },
		{ "ANativeWindow_setBuffersGeometry", (uintptr_t)&ANativeWindow_setBuffersGeometry },
		{ "ASensorEventQueue_disableSensor", (uintptr_t)&ASensorEventQueue_disableSensor },
		{ "ASensorEventQueue_enableSensor", (uintptr_t)&ASensorEventQueue_enableSensor },
		{ "ASensorEventQueue_getEvents", (uintptr_t)&ASensorEventQueue_getEvents },
		{ "ASensorEventQueue_setEventRate", (uintptr_t)&ASensorEventQueue_setEventRate },
		{ "ASensorManager_createEventQueue", (uintptr_t)&ASensorManager_createEventQueue },
		{ "ASensorManager_getDefaultSensor", (uintptr_t)&ASensorManager_getDefaultSensor },
		{ "ASensorManager_getInstance", (uintptr_t)&ASensorManager_getInstance },


		// ctype
		{ "_ctype_", (uintptr_t)&BIONIC_ctype_ },
		{ "_tolower_tab_", (uintptr_t)&BIONIC_tolower_tab_ },
		{ "_toupper_tab_", (uintptr_t)&BIONIC_toupper_tab_ },
		{ "isalnum", (uintptr_t)&isalnum },
		{ "isalpha", (uintptr_t)&isalpha },
		{ "isblank", (uintptr_t)&isblank },
		{ "iscntrl", (uintptr_t)&iscntrl },
		{ "isgraph", (uintptr_t)&isgraph },
		{ "islower", (uintptr_t)&islower },
		{ "isprint", (uintptr_t)&isprint },
		{ "ispunct", (uintptr_t)&ispunct },
		{ "isspace", (uintptr_t)&isspace },
		{ "isupper", (uintptr_t)&isupper },
		{ "isxdigit", (uintptr_t)&isxdigit },
		{ "tolower", (uintptr_t)&tolower },
		{ "toupper", (uintptr_t)&toupper },


		// Android SDK standard logging
		{ "__android_log_print", (uintptr_t)&android_log_print },
		{ "__android_log_vprint", (uintptr_t)&android_log_vprint },
		{ "__android_log_write", (uintptr_t)&android_log_write },


		// Math
		{ "acos", (uintptr_t)&acos },
		{ "acosf", (uintptr_t)&acosf },
		{ "asin", (uintptr_t)&asin },
		{ "asinf", (uintptr_t)&asinf },
		{ "atan", (uintptr_t)&atan },
		{ "atan2", (uintptr_t)&atan2 },
		{ "atan2f", (uintptr_t)&atan2f },
		{ "atanf", (uintptr_t)&atanf },
		{ "ceil", (uintptr_t)&ceil },
		{ "ceilf", (uintptr_t)&ceilf },
		{ "cos", (uintptr_t)&cos },
		{ "cosf", (uintptr_t)&cosf },
		{ "exp", (uintptr_t)&exp },
		{ "exp2", (uintptr_t)&exp2 },
		{ "exp2f", (uintptr_t)&exp2f },
		{ "expf", (uintptr_t)&expf },
		{ "floor", (uintptr_t)&floor },
		{ "floorf", (uintptr_t)&floorf },
		{ "fmod", (uintptr_t)&fmod },
		{ "fmodf", (uintptr_t)&fmodf },
		{ "frexp", (uintptr_t)&frexp },
		{ "ldexp", (uintptr_t)&ldexp },
		{ "ldexpf", (uintptr_t)&ldexpf },
		{ "log", (uintptr_t)&log },
		{ "log10", (uintptr_t)&log10 },
		{ "log10f", (uintptr_t)&log10f },
		{ "logf", (uintptr_t)&logf },
		{ "lrint", (uintptr_t)&lrint },
		{ "lrintf", (uintptr_t)&lrintf },
		{ "lround", (uintptr_t)&lround },
		{ "lroundf", (uintptr_t)&lroundf },
		{ "modf", (uintptr_t)&modf },
		{ "modff", (uintptr_t)&modff },
		{ "pow", (uintptr_t)&pow },
		{ "powf", (uintptr_t)&powf },
		{ "rint", (uintptr_t)&rint },
		{ "rintf", (uintptr_t)&rintf },
		{ "round", (uintptr_t)&round },
		{ "roundf", (uintptr_t)&roundf },
		{ "scalbn", (uintptr_t)&scalbn },
		{ "scalbnf", (uintptr_t)&scalbnf },
		{ "sin", (uintptr_t)&sin },
		{ "sincos", (uintptr_t)&sincos },
		{ "sincosf", (uintptr_t)&sincosf },
		{ "sinf", (uintptr_t)&sinf },
		{ "sinh", (uintptr_t)&sinh },
		{ "sqrt", (uintptr_t)&sqrt },
		{ "sqrtf", (uintptr_t)&sqrtf },
		{ "tan", (uintptr_t)&tan },
		{ "tanf", (uintptr_t)&tanf },
		{ "tanh", (uintptr_t)&tanh },
		{ "trunc", (uintptr_t)&trunc },
		{ "truncf", (uintptr_t)&truncf },


		// Sockets
		{ "accept", (uintptr_t)&accept },
		{ "bind", (uintptr_t)&bind },
		{ "connect", (uintptr_t)&connect },
		{ "freeaddrinfo", (uintptr_t)&freeaddrinfo },
		{ "getaddrinfo", (uintptr_t)&getaddrinfo },
		{ "gethostbyaddr", (uintptr_t)&gethostbyaddr },
		{ "gethostbyname", (uintptr_t)&gethostbyname },
		//{ "gethostname", (uintptr_t)&gethostname },
		{ "getpeername", (uintptr_t)&getpeername },
		{ "getservbyname", (uintptr_t)&getservbyname },
		{ "getsockname", (uintptr_t)&getsockname },
		{ "getsockopt", (uintptr_t)&getsockopt },
		{ "inet_aton", (uintptr_t)&inet_aton },
		{ "inet_ntoa", (uintptr_t)&inet_ntoa },
		{ "inet_ntop", (uintptr_t)&inet_ntop },
		{ "listen", (uintptr_t)&listen },
		{ "poll", (uintptr_t)&poll },
		{ "recv", (uintptr_t)&recv },
		{ "recvfrom", (uintptr_t)&recvfrom },
		{ "recvmsg", (uintptr_t)&recvmsg },
		{ "select", (uintptr_t)&select },
		{ "send", (uintptr_t)&send },
		{ "sendmsg", (uintptr_t)&sendmsg },
		{ "sendto", (uintptr_t)&sendto },
		{ "setsockopt", (uintptr_t)&setsockopt },
		{ "shutdown", (uintptr_t)&shutdown },
		{ "socket", (uintptr_t)&socket },
		

		// Memory
		{ "calloc", (uintptr_t)&calloc },
		{ "free", (uintptr_t)&free },
		{ "malloc", (uintptr_t)&malloc },
		{ "memalign", (uintptr_t)&memalign },
		{ "memcmp", (uintptr_t)&memcmp },
		{ "memcpy", (uintptr_t)&memcpy },
		{ "memmem", (uintptr_t)&memmem },
		{ "memmove", (uintptr_t)&memmove },
		{ "memset", (uintptr_t)&memset },
		{ "mmap", (uintptr_t)&mmap },
		{ "munmap", (uintptr_t)&munmap },
		{ "realloc", (uintptr_t)&realloc },
		{ "valloc", (uintptr_t)&valloc },
		

		// IO
		{ "close", (uintptr_t)&close_soloader },
		{ "closedir", (uintptr_t)&closedir_soloader },
		{ "fclose", (uintptr_t)&fclose_soloader },
		{ "fcntl", (uintptr_t)&fcntl_soloader },
		{ "fopen", (uintptr_t)&fopen_soloader },
		{ "fstat", (uintptr_t)&fstat_soloader },
		{ "ioctl", (uintptr_t)&ioctl_soloader },
		{ "open", (uintptr_t)&open_soloader },
		{ "opendir", (uintptr_t)&opendir_soloader },
		{ "readdir", (uintptr_t)&readdir_soloader },
		{ "readdir_r", (uintptr_t)&readdir_r_soloader },
		{ "stat", (uintptr_t)&stat_soloader },

		#ifdef USE_SCELIBC_IO
			{ "fdopen", (uintptr_t)&sceLibcBridge_fdopen },
			{ "feof", (uintptr_t)&sceLibcBridge_feof },
			{ "ferror", (uintptr_t)&sceLibcBridge_ferror },
			{ "fflush", (uintptr_t)&sceLibcBridge_fflush },
			{ "fgetc", (uintptr_t)&sceLibcBridge_fgetc },
			{ "fgetpos", (uintptr_t)&sceLibcBridge_fgetpos },
			{ "fgets", (uintptr_t)&sceLibcBridge_fgets },
			{ "fputc", (uintptr_t)&sceLibcBridge_fputc },
			{ "fputs", (uintptr_t)&sceLibcBridge_fputs },
			{ "fread", (uintptr_t)&sceLibcBridge_fread },
			{ "freopen", (uintptr_t)&sceLibcBridge_freopen },
			{ "fseek", (uintptr_t)&sceLibcBridge_fseek },
			{ "fsetpos", (uintptr_t)&sceLibcBridge_fsetpos },
			{ "ftell", (uintptr_t)&sceLibcBridge_ftell },
			{ "fwrite", (uintptr_t)&sceLibcBridge_fwrite },
			{ "getc", (uintptr_t)&sceLibcBridge_getc },
			{ "getwc", (uintptr_t)&sceLibcBridge_getwc },
			{ "putc", (uintptr_t)&sceLibcBridge_putc },
			{ "putchar", (uintptr_t)&sceLibcBridge_putchar },
			{ "puts", (uintptr_t)&sceLibcBridge_puts },
			{ "putwc", (uintptr_t)&sceLibcBridge_putwc },
			{ "setvbuf", (uintptr_t)&sceLibcBridge_setvbuf },
			{ "ungetc", (uintptr_t)&sceLibcBridge_ungetc },
			{ "ungetwc", (uintptr_t)&sceLibcBridge_ungetwc },
		#else
			{ "fdopen", (uintptr_t)&fdopen },
			{ "feof", (uintptr_t)&feof },
			{ "ferror", (uintptr_t)&ferror },
			{ "fflush", (uintptr_t)&fflush },
			{ "fgetc", (uintptr_t)&fgetc },
			{ "fgetpos", (uintptr_t)&fgetpos },
			{ "fgets", (uintptr_t)&fgets },
			{ "fputc", (uintptr_t)&fputc },
			{ "fputs", (uintptr_t)&fputs },
			{ "fread", (uintptr_t)&fread },
			{ "freopen", (uintptr_t)&freopen },
			{ "fseek", (uintptr_t)&fseek },
			{ "fsetpos", (uintptr_t)&fsetpos },
			{ "ftell", (uintptr_t)&ftell },
			{ "fwrite", (uintptr_t)&fwrite },
			{ "getc", (uintptr_t)&getc },
			{ "getwc", (uintptr_t)&getwc },
			{ "putc", (uintptr_t)&putc },
			{ "putchar", (uintptr_t)&putchar },
			{ "puts", (uintptr_t)&puts },
			{ "putwc", (uintptr_t)&putwc },
			{ "setvbuf", (uintptr_t)&setvbuf },
			{ "ungetc", (uintptr_t)&ungetc },
			{ "ungetwc", (uintptr_t)&ungetwc },
		#endif

		{ "access", (uintptr_t)&access },
		{ "chdir", (uintptr_t)&chdir },
		{ "chmod", (uintptr_t)&chmod },
		{ "dup", (uintptr_t)&dup },
		{ "fileno", (uintptr_t)&fileno },
		{ "fseeko", (uintptr_t)&fseeko }, // TODO: wrap normal fseek for SceLibc version?
		{ "ftello", (uintptr_t)&ftello },
		{ "ftruncate", (uintptr_t)&ftruncate },
		{ "getcwd", (uintptr_t)&getcwd },
		{ "lseek", (uintptr_t)&lseek },
		//{ "lstat", (uintptr_t)&lstat },
		{ "mkdir", (uintptr_t)&mkdir },
		{ "pipe", (uintptr_t)&pseudo_pipe },
		{ "read", (uintptr_t)&pseudo_read },
		{ "realpath", (uintptr_t)&realpath },
		{ "remove", (uintptr_t)&remove },
		{ "rename", (uintptr_t)&rename },
		{ "rewind", (uintptr_t)&rewind },
		{ "rmdir", (uintptr_t)&rmdir },
		{ "truncate", (uintptr_t)&truncate },
		{ "unlink", (uintptr_t)&unlink },
		{ "write", (uintptr_t)&pseudo_write },


		// *printf, *scanf
		{ "snprintf", (uintptr_t)&snprintf },
		{ "sprintf", (uintptr_t)&sprintf },
		{ "vasprintf", (uintptr_t)&vasprintf },
		{ "vprintf", (uintptr_t)&vprintf },
		{ "vsnprintf", (uintptr_t)&vsnprintf },
		{ "vsprintf", (uintptr_t)&vsprintf },
		{ "vsscanf", (uintptr_t)&vsscanf },
		{ "vswprintf", (uintptr_t)&vswprintf },
		{ "printf", (uintptr_t)&sceClibPrintf },

		#ifdef USE_SCELIBC_IO
			{ "fprintf", (uintptr_t)&sceLibcBridge_fprintf },
			{ "fscanf", (uintptr_t)&sceLibcBridge_fscanf },
			{ "sscanf", (uintptr_t)&sceLibcBridge_sscanf },
			{ "vfprintf", (uintptr_t)&sceLibcBridge_vfprintf },
		#else
			{ "fprintf", (uintptr_t)&fprintf },
			{ "fscanf", (uintptr_t)&fscanf },
			{ "sscanf", (uintptr_t)&sscanf },
			{ "vfprintf", (uintptr_t)&vfprintf },
		#endif


		// OpenGL
		{ "glActiveTexture", (uintptr_t)&glActiveTexture },
		{ "glAlphaFuncx", (uintptr_t)&glAlphaFuncx },
		{ "glAttachShader", (uintptr_t)&glAttachShader },
		{ "glBindAttribLocation", (uintptr_t)&glBindAttribLocation },
		{ "glBindBuffer", (uintptr_t)&glBindBuffer },
		{ "glBindFramebufferOES", (uintptr_t)&glBindFramebuffer },
		{ "glBindRenderbufferOES", (uintptr_t)&glBindRenderbuffer },
		{ "glBindTexture", (uintptr_t)&glBindTexture },
		{ "glBlendEquation", (uintptr_t)&glBlendEquation },
		{ "glBlendEquationSeparate", (uintptr_t)&glBlendEquationSeparate },
		{ "glBlendFunc", (uintptr_t)&glBlendFunc },
		{ "glBlendFuncSeparate", (uintptr_t)&glBlendFuncSeparate },
		{ "glBufferData", (uintptr_t)&glBufferData },
		{ "glBufferSubData", (uintptr_t)&glBufferSubData },
		{ "glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus },
		{ "glClear", (uintptr_t)&glClear },
		{ "glClearColor", (uintptr_t)&glClearColor },
		{ "glClearColorx", (uintptr_t)&glClearColorx },
		{ "glClearDepthf", (uintptr_t)&glClearDepthf },
		{ "glClearDepthx", (uintptr_t)&glClearDepthx },
		{ "glClearStencil", (uintptr_t)&glClearStencil },
		{ "glColor4f", (uintptr_t)&glColor4f },
		{ "glColor4x", (uintptr_t)&glColor4x },
		{ "glColorMask", (uintptr_t)&glColorMask },
		{ "glColorPointer", (uintptr_t)&glColorPointer },
		{ "glCompileShader", (uintptr_t)&glCompileShader },
		{ "glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D },
		{ "glCompressedTexSubImage2D", (uintptr_t)&ret0 },
		{ "glCopyTexImage2D", (uintptr_t)&glCopyTexImage2D },
		{ "glCopyTexSubImage2D", (uintptr_t)&glCopyTexSubImage2D },
		{ "glCreateProgram", (uintptr_t)&glCreateProgram },
		{ "glCreateShader", (uintptr_t)&glCreateShader },
		{ "glCullFace", (uintptr_t)&glCullFace },
		{ "glDeleteBuffers", (uintptr_t)&glDeleteBuffers },
		{ "glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers },
		{ "glDeleteProgram", (uintptr_t)&glDeleteProgram },
		{ "glDeleteRenderbuffers", (uintptr_t)&glDeleteRenderbuffers },
		{ "glDeleteShader", (uintptr_t)&glDeleteShader },
		{ "glDeleteTextures", (uintptr_t)&glDeleteTextures },
		{ "glDepthFunc", (uintptr_t)&glDepthFunc },
		{ "glDepthMask", (uintptr_t)&glDepthMask },
		{ "glDepthRangef", (uintptr_t) &glDepthRangef },
		{ "glDetachShader", (uintptr_t)&ret0 },
		{ "glDisable", (uintptr_t)&glDisable },
		{ "glDisableClientState", (uintptr_t)&glDisableClientState },
		{ "glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray },
		{ "glDrawArrays", (uintptr_t)&glDrawArrays },
		{ "glDrawElements", (uintptr_t)&glDrawElements },
		{ "glEnable", (uintptr_t)&glEnable },
		{ "glEnableClientState", (uintptr_t)&glEnableClientState },
		{ "glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray },
		{ "glFlush", (uintptr_t)&glFlush },
		{ "glFramebufferRenderbuffer", (uintptr_t)&glFramebufferRenderbuffer },
		{ "glFramebufferTexture2DOES", (uintptr_t)&glFramebufferTexture2D },
		{ "glFrontFace", (uintptr_t)&glFrontFace },
		{ "glGenBuffers", (uintptr_t)&glGenBuffers },
		{ "glGenerateMipmap", (uintptr_t)&glGenerateMipmap },
		{ "glGenFramebuffersOES", (uintptr_t)&glGenFramebuffers },
		{ "glGenRenderbuffers", (uintptr_t)&glGenRenderbuffers },
		{ "glGenTextures", (uintptr_t)&glGenTextures },
		{ "glGetActiveAttrib", (uintptr_t)&glGetActiveAttrib },
		{ "glGetActiveUniform", (uintptr_t)&glGetActiveUniform },
		{ "glGetAttribLocation", (uintptr_t)&glGetAttribLocation },
		{ "glGetError", (uintptr_t)&glGetError },
		{ "glGetFloatv", (uintptr_t)&glGetFloatv },
		{ "glGetIntegerv", (uintptr_t)&glGetIntegerv },
		{ "glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog },
		{ "glGetProgramiv", (uintptr_t)&glGetProgramiv },
		{ "glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog },
		{ "glGetShaderiv", (uintptr_t)&glGetShaderiv },
		{ "glGetString", (uintptr_t)&glGetString },
		{ "glGetUniformLocation", (uintptr_t)&glGetUniformLocation },
		{ "glHint", (uintptr_t)&glHint },
		{ "glLightModelxv", (uintptr_t)&glLightModelxv },
		{ "glLightx", (uintptr_t)&ret0 },
		{ "glLightxv", (uintptr_t)&glLightxv },
		{ "glLineWidth", (uintptr_t)&glLineWidth },
		{ "glLinkProgram", (uintptr_t)&glLinkProgram },
		{ "glLoadIdentity", (uintptr_t)&glLoadIdentity },
		{ "glLoadMatrixf", (uintptr_t)&glLoadMatrixf },
		{ "glLoadMatrixx", (uintptr_t)&glLoadMatrixx },
		{ "glMaterialx", (uintptr_t)&ret0 },
		{ "glMaterialxv", (uintptr_t)&glMaterialxv },
		{ "glMatrixMode", (uintptr_t)&glMatrixMode },
		{ "glNormalPointer", (uintptr_t)&glNormalPointer },
		{ "glOrthof", (uintptr_t)&glOrthof },
		{ "glPixelStorei", (uintptr_t)&ret0 },
		{ "glPolygonOffset", (uintptr_t)&glPolygonOffset },
		{ "glPopMatrix", (uintptr_t)&glPopMatrix },
		{ "glPushMatrix", (uintptr_t)&glPushMatrix },
		{ "glReadPixels", (uintptr_t)&glReadPixels },
		{ "glRenderbufferStorage", (uintptr_t)&glRenderbufferStorage },
		{ "glRotatef", (uintptr_t)&glRotatef },
		{ "glScalef", (uintptr_t)&glScalef },
		{ "glScissor", (uintptr_t)&glScissor },
		{ "glShadeModel", (uintptr_t)&glShadeModel },
		{ "glShaderSource", (uintptr_t)&glShaderSource },
		{ "glStencilFunc", (uintptr_t)&glStencilFunc },
		{ "glStencilFuncSeparate", (uintptr_t)&glStencilFuncSeparate },
		{ "glStencilMask", (uintptr_t)&glStencilMask },
		{ "glStencilOp", (uintptr_t)&glStencilOp },
		{ "glStencilOpSeparate", (uintptr_t)&glStencilOpSeparate },
		{ "glTexCoordPointer", (uintptr_t)&glTexCoordPointer },
		{ "glTexEnvx", (uintptr_t)&glTexEnvx },
		{ "glTexEnvxv", (uintptr_t)&glTexEnvxv },
		{ "glTexImage2D", (uintptr_t)&glTexImage2D },
		{ "glTexParameterf", (uintptr_t)&glTexParameterf },
		{ "glTexParameteri", (uintptr_t)&glTexParameteri },
		{ "glTexSubImage2D", (uintptr_t)&glTexSubImage2D },
		{ "glTranslatef", (uintptr_t)&glTranslatef },
		{ "glUniform1f", (uintptr_t)&glUniform1f },
		{ "glUniform1fv", (uintptr_t)&glUniform1fv },
		{ "glUniform1i", (uintptr_t)&glUniform1i },
		{ "glUniform1iv", (uintptr_t)&glUniform1iv },
		{ "glUniform2f", (uintptr_t)&glUniform2f },
		{ "glUniform2fv", (uintptr_t)&glUniform2fv },
		{ "glUniform2iv", (uintptr_t)&glUniform2iv },
		{ "glUniform3f", (uintptr_t)&glUniform3f },
		{ "glUniform3fv", (uintptr_t)&glUniform3fv },
		{ "glUniform3iv", (uintptr_t)&glUniform3iv },
		{ "glUniform4f", (uintptr_t)&glUniform4f },
		{ "glUniform4fv", (uintptr_t)&glUniform4fv },
		{ "glUniform4iv", (uintptr_t)&glUniform4iv },
		{ "glUniformMatrix2fv", (uintptr_t)&glUniformMatrix2fv },
		{ "glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv },
		{ "glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv },
		{ "glUseProgram", (uintptr_t)&glUseProgram },
		{ "glVertexAttrib4f", (uintptr_t)&glVertexAttrib4f },
		{ "glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer },
		{ "glVertexPointer", (uintptr_t)&glVertexPointer },
		{ "glViewport", (uintptr_t)&glViewport },


		// EGL
		{ "eglBindAPI", (uintptr_t)&eglBindAPI },
		{ "eglChooseConfig", (uintptr_t)&eglChooseConfig },
		{ "eglCreateContext", (uintptr_t)&eglCreateContext },
		{ "eglCreateWindowSurface", (uintptr_t)&eglCreateWindowSurface },
		{ "eglDestroyContext", (uintptr_t)&eglDestroyContext },
		{ "eglDestroySurface", (uintptr_t)&eglDestroySurface },
		{ "eglGetConfigAttrib", (uintptr_t)&eglGetConfigAttrib },
		{ "eglGetDisplay", (uintptr_t)&eglGetDisplay },
		{ "eglGetError", (uintptr_t)&eglGetError },
		{ "eglGetProcAddress", (uintptr_t)&eglGetProcAddress },
		{ "eglInitialize", (uintptr_t)&eglInitialize },
		{ "eglMakeCurrent", (uintptr_t)&eglMakeCurrent },
		{ "eglQuerySurface", (uintptr_t)&eglQuerySurface },
		{ "eglSwapBuffers", (uintptr_t)&eglSwapBuffers },
		{ "eglTerminate", (uintptr_t)&eglTerminate },


		// Pthread
		{ "pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_soloader },
		{ "pthread_attr_init", (uintptr_t) &pthread_attr_init_soloader },
		{ "pthread_attr_setdetachstate", (uintptr_t) &pthread_attr_setdetachstate_soloader },
		{ "pthread_attr_setstacksize", (uintptr_t) &pthread_attr_setstacksize_soloader },
		{ "pthread_cond_broadcast", (uintptr_t) &pthread_cond_broadcast_soloader },
		{ "pthread_cond_destroy", (uintptr_t) &pthread_cond_destroy_soloader },
		{ "pthread_cond_init", (uintptr_t) &pthread_cond_init_soloader },
		{ "pthread_cond_signal", (uintptr_t) &pthread_cond_signal_soloader },
		{ "pthread_cond_timedwait", (uintptr_t) &pthread_cond_timedwait_soloader },
		{ "pthread_cond_wait", (uintptr_t) &pthread_cond_wait_soloader },
		{ "pthread_create", (uintptr_t) &pthread_create_soloader },
		{ "pthread_detach", (uintptr_t) &pthread_detach_soloader },
		{ "pthread_equal", (uintptr_t) &pthread_equal_soloader },
		{ "pthread_exit", (uintptr_t)&pthread_exit },
		{ "pthread_getschedparam", (uintptr_t) &pthread_getschedparam_soloader },
		{ "pthread_getspecific", (uintptr_t)&pthread_getspecific },
		{ "pthread_join", (uintptr_t) &pthread_join_soloader },
		{ "pthread_key_create", (uintptr_t)&pthread_key_create },
		{ "pthread_key_delete", (uintptr_t)&pthread_key_delete },
		{ "pthread_kill", (uintptr_t)&pthread_kill_soloader },
		{ "pthread_mutex_destroy", (uintptr_t) &pthread_mutex_destroy_soloader },
		{ "pthread_mutex_init", (uintptr_t) &pthread_mutex_init_soloader },
		{ "pthread_mutex_lock", (uintptr_t) &pthread_mutex_lock_soloader },
		{ "pthread_mutex_trylock", (uintptr_t) &pthread_mutex_trylock_soloader },
		{ "pthread_mutex_unlock", (uintptr_t) &pthread_mutex_unlock_soloader },
		{ "pthread_mutexattr_destroy", (uintptr_t) &pthread_mutexattr_destroy_soloader },
		{ "pthread_mutexattr_init", (uintptr_t) &pthread_mutexattr_init_soloader },
		{ "pthread_mutexattr_settype", (uintptr_t) &pthread_mutexattr_settype_soloader },
		{ "pthread_once", (uintptr_t)&pthread_once_soloader },
		{ "pthread_self", (uintptr_t) &pthread_self_soloader },
		{ "pthread_setname_np", (uintptr_t) &pthread_setname_np_soloader },
		{ "pthread_setschedparam", (uintptr_t) &pthread_setschedparam_soloader },
		{ "pthread_setspecific", (uintptr_t)&pthread_setspecific },
		{ "pthread_sigmask", (uintptr_t)&ret0 },

		{ "sem_destroy", (uintptr_t) &sem_destroy_soloader },
		{ "sem_getvalue", (uintptr_t) &sem_getvalue_soloader },
		{ "sem_init", (uintptr_t) &sem_init_soloader },
		{ "sem_post", (uintptr_t) &sem_post_soloader },
		{ "sem_timedwait", (uintptr_t) &sem_timedwait_soloader },
		{ "sem_trywait", (uintptr_t) &sem_trywait_soloader },
		{ "sem_wait", (uintptr_t) &sem_wait_soloader },

		{ "sched_get_priority_max", (uintptr_t)&sched_get_priority_max },
		{ "sched_get_priority_min", (uintptr_t)&sched_get_priority_min },
		{ "sched_yield", (uintptr_t)&sched_yield },


		// wchar, wctype
		{ "mbtowc", (uintptr_t)&mbtowc },
		{ "btowc", (uintptr_t)&btowc },
		{ "iswalpha", (uintptr_t)&iswalpha },
		{ "iswcntrl", (uintptr_t)&iswcntrl },
		{ "iswctype", (uintptr_t)&iswctype },
		{ "iswdigit", (uintptr_t)&iswdigit },
		{ "iswdigit", (uintptr_t)&iswdigit },
		{ "iswlower", (uintptr_t)&iswlower },
		{ "iswprint", (uintptr_t)&iswprint },
		{ "iswpunct", (uintptr_t)&iswpunct },
		{ "iswspace", (uintptr_t)&iswspace },
		{ "iswupper", (uintptr_t)&iswupper },
		{ "iswxdigit", (uintptr_t)&iswxdigit },
		{ "towlower", (uintptr_t)&towlower },
		{ "towupper", (uintptr_t)&towupper },
		{ "wcrtomb", (uintptr_t)&wcrtomb },
		{ "wcscasecmp", (uintptr_t)&wcscasecmp },
		{ "wcscmp", (uintptr_t)&wcscmp },
		{ "wcscoll", (uintptr_t)&wcscoll },
		{ "wcsftime", (uintptr_t)&wcsftime },
		{ "wcslcat", (uintptr_t)&wcslcat },
		{ "wcslcpy", (uintptr_t)&wcslcpy },
		{ "wcslen", (uintptr_t)&wcslen },
		{ "wcsncasecmp", (uintptr_t)&wcsncasecmp },
		{ "wcsncpy", (uintptr_t)&wcsncpy },
		{ "wcsxfrm", (uintptr_t)&wcsxfrm },
		{ "wctob", (uintptr_t)&wctob },
		{ "wctype", (uintptr_t)&wctype },
		{ "wmemchr", (uintptr_t)&wmemchr },
		{ "wmemcmp", (uintptr_t)&wmemcmp },
		{ "wmemcpy", (uintptr_t)&wmemcpy },
		{ "wmemmove", (uintptr_t)&wmemmove },
		{ "wmemset", (uintptr_t)&wmemset },
		{ "mbrlen", (uintptr_t)&mbrlen },
		{ "mbrtowc", (uintptr_t)&mbrtowc },


		// libdl
		{ "dlclose", (uintptr_t)&ret0 },
		{ "dlerror", (uintptr_t)&ret0 },
		{ "dlopen", (uintptr_t)&ret1 },
		{ "dlsym", (uintptr_t)&dlsym_fake },


		// Errno
		{ "__errno", (uintptr_t)&__errno_soloader },
		{ "strerror", (uintptr_t)&strerror_soloader },
		{ "strerror_r", (uintptr_t)&strerror_r_soloader },
		

		// Strings
		{ "memchr", (uintptr_t)&memchr },
		{ "memrchr", (uintptr_t)&memrchr },
		{ "strcasecmp", (uintptr_t)&strcasecmp },
		{ "strcat", (uintptr_t)&strcat },
		{ "strchr", (uintptr_t)&strchr },
		{ "strcmp", (uintptr_t)&strcmp },
		{ "strcoll", (uintptr_t)&strcoll },
		{ "strcpy", (uintptr_t)&strcpy },
		{ "strcspn", (uintptr_t)&strcspn },
		{ "strdup", (uintptr_t)&strdup },
		{ "strlcat", (uintptr_t)&strlcat },
		{ "strlcpy", (uintptr_t)&strlcpy },
		{ "strlen", (uintptr_t)&strlen },
		{ "strncasecmp", (uintptr_t)&strncasecmp },
		{ "strncat", (uintptr_t)&strncat },
		{ "strncmp", (uintptr_t)&strncmp },
		{ "strncpy", (uintptr_t)&strncpy },
		{ "strnlen", (uintptr_t)&strnlen },
		{ "strpbrk", (uintptr_t)&strpbrk },
		{ "strrchr", (uintptr_t)&strrchr },
		{ "strspn", (uintptr_t)&strspn },
		{ "strstr", (uintptr_t)&strstr },
		{ "strtok", (uintptr_t)&strtok },
		{ "strtok_r", (uintptr_t)&strtok_r },
		{ "strxfrm", (uintptr_t)&strxfrm },
		

		// Syscalls
		{ "syscall", (uintptr_t)&syscall },
		{ "sysconf", (uintptr_t)&ret0 },
		{ "system", (uintptr_t)&system },


		// Time
		{ "clock", (uintptr_t)&clock },
		//{ "clock_getres", (uintptr_t)&clock_getres },
		{ "clock_gettime", (uintptr_t)&clock_gettime },
		{ "difftime", (uintptr_t)&difftime },
		{ "gettimeofday", (uintptr_t)&gettimeofday },
		{ "gmtime", (uintptr_t)&gmtime },
		{ "gmtime_r", (uintptr_t)&gmtime_r },
		{ "localtime", (uintptr_t)&localtime },
		{ "localtime_r", (uintptr_t)&localtime_r },
		{ "mktime", (uintptr_t)&mktime },
		//{ "nanosleep", (uintptr_t)&nanosleep },
		{ "strftime", (uintptr_t)&strftime },
		{ "time", (uintptr_t)&time },
		{ "tzset", (uintptr_t)&tzset },


		// Temp
		{ "mkstemp", (uintptr_t)&mkstemp },
		{ "mktemp", (uintptr_t)&mktemp },
		{ "tmpfile", (uintptr_t)&tmpfile },
		{ "tmpnam", (uintptr_t)&tmpnam },


		// stdlib
		{ "abort", (uintptr_t)&abort },
		{ "atof", (uintptr_t)&atof },
		{ "atoi", (uintptr_t)&atoi },
		{ "atol", (uintptr_t)&atol },
		{ "atoll", (uintptr_t)&atoll },
		{ "exit", (uintptr_t)&exit_soloader },
		{ "lrand48", (uintptr_t)&lrand48 },
		{ "prctl", (uintptr_t)&ret0 },
		{ "sleep", (uintptr_t)&sleep },
		{ "srand48", (uintptr_t)&srand48 },
		{ "strtod", (uintptr_t)&strtod },
		{ "strtof", (uintptr_t)&strtof },
		{ "strtoimax", (uintptr_t)&strtoimax },
		{ "strtol", (uintptr_t)&strtol },
		{ "strtold", (uintptr_t)&strtold },
		{ "strtoll", (uintptr_t)&strtoll },
		{ "strtoul", (uintptr_t)&strtoul },
		{ "strtoull", (uintptr_t)&strtoull },
		{ "strtoumax", (uintptr_t)&strtoumax },
		{ "usleep", (uintptr_t)&usleep },

		#ifdef USE_SCELIBC_IO
			{ "qsort", (uintptr_t)&sceLibcBridge_qsort },
			{ "rand", (uintptr_t)&sceLibcBridge_rand },
			{ "srand", (uintptr_t)&sceLibcBridge_srand },
		#else
			{ "qsort", (uintptr_t)&qsort },
			{ "rand", (uintptr_t)&rand },
			{ "srand", (uintptr_t)&srand },
		#endif


		// Env
		{ "getenv", (uintptr_t)&getenv_soloader },
		{ "setenv", (uintptr_t)&setenv_soloader },


		// Jmp
		{ "setjmp", (uintptr_t)&setjmp }, // TODO: May have different struct size?
		{ "longjmp", (uintptr_t)&longjmp }, // TODO: May have different struct size?


		// Signals
		{ "bsd_signal", (uintptr_t)&signal },
		{ "raise", (uintptr_t)&raise },
		{ "sigaction", (uintptr_t)&sigaction },
		
		
		// Locale
		{ "setlocale", (uintptr_t)&setlocale },


		// zlib
		{ "crc32", (uintptr_t)&crc32 },
		{ "gzopen", (uintptr_t)&gzopen },
		{ "gzgets", (uintptr_t)&gzgets },
		{ "gzclose", (uintptr_t)&gzclose },
		{ "compressBound", (uintptr_t)&compressBound },
		{ "compress", (uintptr_t)&compress },
		{ "uncompress", (uintptr_t)&uncompress },
		{ "deflateInit_", (uintptr_t)&deflateInit_ },
		{ "deflate", (uintptr_t)&deflate },
		{ "deflateEnd", (uintptr_t)&deflateEnd },
		{ "inflateInit_", (uintptr_t)&inflateInit_ },
		{ "inflate", (uintptr_t)&inflate },
		{ "inflateEnd", (uintptr_t)&inflateEnd },
		{ "inflateInit2_", (uintptr_t)&inflateInit2_ },
};

void resolve_imports(so_module* mod) {
	__sF_fake[0] = *stdin;
	__sF_fake[1] = *stdout;
	__sF_fake[2] = *stderr;

	so_resolve(mod, default_dynlib, sizeof(default_dynlib), 0);
}
