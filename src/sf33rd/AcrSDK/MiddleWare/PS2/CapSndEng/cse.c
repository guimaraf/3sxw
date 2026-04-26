#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/cse.h"
#include "common.h"
#include "port/debug/debug_log.h"
#include "port/sound/spu.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlMemMap.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlRpcQueue.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlSndDrv.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlTSB.h"

#include <assert.h>
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>

static CSE_SYSWORK cseSysWork __attribute__((aligned(16)));

s32 cseInitSndDrv() {
    u32 i;

    mlSeInitSndDrv();
    mlTsbInit();
    cseSysWork.InitializeFlag = 1;
    cseSysWork.Counter = 0;

    for (i = 0; i < SPUBANKID_MAX; i++) {
        cseSysWork.SpuBankId[i] = -1;
    }

    return 0;
}

s32 cseExecServer() {
    const bool record_timing = DebugLog_IsEnabled();
    const Uint64 start_ns = record_timing ? SDL_GetTicksNS() : 0;

    if (cseSysWork.InitializeFlag == 1) {
        mlTsbExecServer();
        cseSysWork.Counter++;
        if (record_timing) {
            DebugLog_RecordCseExecServer((double)(SDL_GetTicksNS() - start_ns) / 1e6);
        }
        return 0;
    }

    printf("[EE]");
    printf("(DBG)");
    //"System not initialized\n"
    printf("システムが初期化されていない\n");
    if (record_timing) {
        DebugLog_RecordCseExecServer((double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }
    return -1;
}

s32 cseTsbRequest(u16 bank, u16 code, s32 NumArgSets, ...) {
    const bool record_timing = DebugLog_IsEnabled();
    const Uint64 start_ns = record_timing ? SDL_GetTicksNS() : 0;
    s32 rtpc[10] = {};
    s32 i;
    s32 cmd;
    s32 prm;

    va_list vlist;
    va_start(vlist, NumArgSets);

    for (i = 0; i < NumArgSets; i++) {
        cmd = va_arg(vlist, int);
        prm = va_arg(vlist, int);

        if (cmd < 10) {
            rtpc[cmd] += prm;
        }
    }

    va_end(vlist);

    const s32 result = mlTsbRequest(bank, code, rtpc);

    if (record_timing) {
        DebugLog_RecordCseTsbRequest((double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }

    return result;
}

s32 cseSendBd2SpuWithId(void* ee_addr, u32 size, u32 bank, u32 id) {
    const bool record_timing = DebugLog_IsEnabled();
    const Uint64 start_ns = record_timing ? SDL_GetTicksNS() : 0;
    u32 uploaded_size = 0;
    CSE_SPUID_PARAM param = {};
    bank &= 0xF;

    if (cseSysWork.SpuBankId[bank] != id) {
        cseSysWork.SpuBankId[bank] = id;
        param.cmd = 0x30000000;
        param.e_addr = (uintptr_t)ee_addr;
        param.s_addr = mlMemMapGetBankAddr(bank);
        param.size = size;

        SPU_Upload(param.s_addr, ee_addr, size);
        uploaded_size = size;
    }

    if (record_timing && uploaded_size > 0) {
        DebugLog_RecordCseSendBdToSpu(uploaded_size, (double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }

    return 0;
}

u32 cseGetIdStoredBd(u32 bank) {
    return cseSysWork.SpuBankId[bank & 0xF];
}

s32 cseMemMapSetPhdAddr(u32 bank, void* addr) {
    return mlMemMapSetPhdAddr(bank, addr);
}

s32 cseTsbSetBankAddr(u32 bank, SoundEvent* addr) {
    return mlTsbSetBankAddr(bank, addr);
}

s32 cseSeStopAll() {
    mlTsbStopAll();
    mlSeStopAll();
    return 0;
}

s32 cseSysSetMasterVolume(s32 vol) {
    return mlSysSetMasterVolume(vol);
}

s32 cseSysSetMono(u32 mono_sw) {
    return mlSysSetMono(mono_sw);
}
