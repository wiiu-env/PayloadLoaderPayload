/****************************************************************************
 * Copyright (C) 2018-2022 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#include "dynamic.h"
#include "kernel.h"
#include "utils/DirList.h"
#include "utils/ElfUtils.h"
#include "utils/logger.h"
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <coreinit/filesystem.h>
#include <coreinit/memexpheap.h>
#include <coreinit/screen.h>
#include <cstdint>
#include <malloc.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <utils/StringTools.h>
#include <vector>
#include <vpad/input.h>

std::map<std::string, std::string> get_all_payloads(const char *relativefilepath);

std::string PayloadSelectionScreen(const std::map<std::string, std::string> &payloads);

extern "C" void init_wut();
extern "C" void fini_wut();
MEMExpHeapBlock *memory_start = nullptr;

extern "C" uint32_t start_wrapper(int argc, char **argv) {
    doKernelSetup();
    InitFunctionPointers();

    // Save last entry on mem2 heap to detect leaked memory
    MEMHeapHandle mem2_heap_handle = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);
    auto heap                      = (MEMExpHeap *) mem2_heap_handle;
    memory_start                   = heap->usedList.tail;

    init_wut();

    initLogging();

    DEBUG_FUNCTION_LINE_VERBOSE("Hello from payload.elf multiloader");

    VPADStatus vpadStatus{};
    VPADReadError vpadError = VPAD_READ_UNINITIALIZED;
    int btn                 = 0;
    do {
        if (VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError) > 0 && vpadError == VPAD_READ_SUCCESS) {
            btn = vpadStatus.trigger | vpadStatus.hold;
        } else {
            OSSleepTicks(OSMillisecondsToTicks(1));
        }
    } while (vpadError == VPAD_READ_NO_SAMPLES);


    std::string payload_path = "wiiu/payloads/default/payload.elf";

    uint32_t entryPoint = 0;
    if ((btn & VPAD_BUTTON_B) != VPAD_BUTTON_B) {
        entryPoint = load_loader_elf_from_sd(nullptr, payload_path.c_str());
    }

    if (!entryPoint) {
        std::map<std::string, std::string> payloads = get_all_payloads("wiiu/payloads");
        payload_path                                = PayloadSelectionScreen(payloads);
        DEBUG_FUNCTION_LINE_VERBOSE("Selected %s", payload_path.c_str());
        entryPoint = load_loader_elf_from_sd(nullptr, payload_path.c_str());
    }

    if (entryPoint != 0) {
        DEBUG_FUNCTION_LINE("Loaded payload entrypoint at %08X", entryPoint);
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to load: %s", payload_path.c_str());
    }

    deinitLogging();

    fini_wut();

    revertKernelHook();

    return entryPoint;
}

extern "C" struct _reent *__syscall_getreent(void) {
    return _impure_ptr;
}

extern "C" int _start(int argc, char **argv) {
    uint32_t entryPoint = start_wrapper(argc, argv);

    MEMHeapHandle mem2_heap_handle = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);
    auto heap                      = (MEMExpHeap *) mem2_heap_handle;
    // free leaked memory
    if (memory_start) {
        int leak_count = 0;
        while (true) {
            MEMExpHeapBlock *memory_end = heap->usedList.tail;
            if (memory_end == memory_start) {
                break;
            }
            auto mem_ptr = &memory_end[1]; // &memory_end + sizeof(MEMExpHeapBlock);
            free(mem_ptr);
            leak_count++;
        }
        DEBUG_FUNCTION_LINE_INFO("Freed %d leaked memory blocks\n", leak_count);
    }

    int res = -1;
    if (entryPoint != 0) {
        res = ((int (*)(int, char **)) entryPoint)(argc, argv);
    }
    _Exit(0);
    return res;
}

std::map<std::string, std::string> get_all_payloads(const char *relativefilepath) {
    std::map<std::string, std::string> result;
    std::vector<std::string> payload_folders;
    std::vector<std::string> files_inFolder;

    DirList dirList(std::string("fs:/vol/external01/") + relativefilepath, nullptr, DirList::Dirs, 1);
    dirList.SortList();

    for (int i = 0; i < dirList.GetFilecount(); i++) {
        auto curDirName = dirList.GetFilename(i);
        auto curPath    = dirList.GetFilepath(i);
        DEBUG_FUNCTION_LINE("Reading path %s", curDirName);

        auto payloadPath = std::string(curPath) + "/payload.elf";
        auto fd          = fopen(payloadPath.c_str(), "r");
        if (fd) {
            fclose(fd);
            result[curDirName] = payloadPath;
        }
    }

    return result;
}

std::string PayloadSelectionScreen(const std::map<std::string, std::string> &payloads) {
    // Init screen and screen buffers
    OSScreenInit();
    uint32_t screen_buf0_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t screen_buf1_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
    auto *screenBuffer        = (uint8_t *) memalign(0x100, screen_buf0_size + screen_buf1_size);
    OSScreenSetBufferEx(SCREEN_TV, (void *) screenBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, (void *) (screenBuffer + screen_buf0_size));

    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);

    // Clear screens
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    VPADStatus vpad_data;
    VPADReadError error;
    int selected       = 0;
    std::string header = "Please choose your payload:";
    while (true) {
        // Clear screens
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);

        int pos = 0;

        OSScreenPutFontEx(SCREEN_TV, 0, pos, header.c_str());
        OSScreenPutFontEx(SCREEN_DRC, 0, pos, header.c_str());

        pos += 2;

        int i = 0;
        for (auto const &[key, val] : payloads) {
            std::string text = StringTools::strfmt("%s %s", i == selected ? "> " : "  ", key.c_str());
            OSScreenPutFontEx(SCREEN_TV, 0, pos, text.c_str());
            OSScreenPutFontEx(SCREEN_DRC, 0, pos, text.c_str());
            i++;
            pos++;
        }

        VPADRead(VPAD_CHAN_0, &vpad_data, 1, &error);
        if (vpad_data.trigger == VPAD_BUTTON_A) {
            break;
        }

        if (vpad_data.trigger == VPAD_BUTTON_UP) {
            selected--;
            if (selected < 0) {
                selected = 0;
            }
        } else if (vpad_data.trigger == VPAD_BUTTON_DOWN) {
            selected++;
            if ((uint32_t) selected >= payloads.size()) {
                selected = payloads.size() - 1;
            }
        }

        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);

        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    free(screenBuffer);

    int i = 0;
    for (auto const &[key, val] : payloads) {
        if (i == selected) {
            return val;
        }
        i++;
    }
    return "";
}