/****************************************************************************
 * Copyright (C) 2018-2020 Maschell
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

#include <cstdint>
#include <coreinit/dynload.h>
#include <coreinit/screen.h>

#include "utils/ElfUtils.h"
#include "utils/logger.h"
#include "dynamic.h"
#include "kernel.h"
#include <whb/log_udp.h>
#include <coreinit/filesystem.h>
#include <whb/sdcard.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>
#include <vpad/input.h>
#include <vector>
#include <map>
#include <utils/StringTools.h>

std::map<std::string, std::string> get_all_payloads(const char *relativefilepath);

std::vector<std::string> readDirFull(const char *base_path, const char *path, FSCmdBlock &cmd, FSClient &client, int filter);

std::string PayloadSelectionScreen(const std::map<std::string, std::string> &payloads);

extern "C" void __init_wut();
extern "C" void __fini_wut();

extern "C" uint32_t start_wrapper(int argc, char **argv) {
    doKernelSetup();
    InitFunctionPointers();

    __init_wut();

    WHBLogUdpInit();

    DEBUG_FUNCTION_LINE("Hello from payload");

    VPADReadError err;
    VPADStatus vpad_data;
    VPADRead(VPAD_CHAN_0, &vpad_data, 1, &err);

    uint32_t btn = vpad_data.hold | vpad_data.trigger;

    std::map<std::string, std::string> payloads = get_all_payloads("wiiu/payloads");

    uint32_t entryPoint = 0;

    std::string payload_path = "wiiu/payloads/default/payload.elf";
    if ((btn & VPAD_BUTTON_B) == VPAD_BUTTON_B) {
        payload_path = PayloadSelectionScreen(payloads);
        DEBUG_FUNCTION_LINE("Selected %s", payload_path.c_str());
    }

    entryPoint = load_loader_elf_from_sd(nullptr, payload_path.c_str());

    if (entryPoint != 0) {
        DEBUG_FUNCTION_LINE("loaded payload entrypoint at %08X", entryPoint);
    } else {
        DEBUG_FUNCTION_LINE("failed to load elf");
    }

    WHBLogUdpDeinit();

    __fini_wut();

    revertKernelHook();

    return entryPoint;
}
extern "C" int _start(int argc, char **argv) {
    uint32_t entryPoint = start_wrapper(argc, argv);
    int res = -1;
    if (entryPoint != 0) {
        res = ((int (*)(int, char **)) entryPoint)(argc, argv);
    }
    return res;
}

std::map<std::string, std::string> get_all_payloads(const char *relativefilepath) {
    std::map<std::string, std::string> result;
    const char *sdRootPath = "";
    std::vector<std::string> payload_folders;
    std::vector<std::string> files_inFolder;
    bool clientAdded = false;
    if (!WHBMountSdCard()) {
        DEBUG_FUNCTION_LINE("Failed to mount SD Card...");
        goto exit;
    }

    FSCmdBlock cmd;
    FSClient client;

    sdRootPath = WHBGetSdCardMountPath();

    FSAddClient(&client, -1);
    clientAdded = true;

    FSInitCmdBlock(&cmd);

    payload_folders = readDirFull(sdRootPath, relativefilepath, cmd, client, 1);

    for (auto &e : payload_folders) {
        DEBUG_FUNCTION_LINE("Reading path %s", e.c_str());
        files_inFolder = readDirFull(sdRootPath, e.c_str(), cmd, client, 2);

        for (auto &child : files_inFolder) {
            if (StringTools::EndsWith(child, "payload.elf")) {
                std::vector<std::string> folders = StringTools::stringSplit(e, "/");
                std::string folder_name = e;
                if (folders.size() > 1) {
                    folder_name = folders.at(folders.size() - 1);
                }

                DEBUG_FUNCTION_LINE("%s is valid!", folder_name.c_str());
                result[folder_name] = child;
                break;
            }
        }
    }

    exit:
    WHBUnmountSdCard();
    if (clientAdded) {
        FSDelClient(&client, -1);
    }
    return result;
}

std::vector<std::string> readDirFull(const char *base_path, const char *path, FSCmdBlock &cmd, FSClient &client, int filter) {
    std::string full_dir_path = StringTools::strfmt("%s/%s", base_path, path);
    FSStatus status;
    FSDirectoryHandle handle;
    std::vector<std::string> result;
    if (FSOpenDir(&client, &cmd, full_dir_path.c_str(), &handle, -1) == FS_STATUS_OK) {
        FSDirectoryEntry entry;
        while (true) {
            status = FSReadDir(&client, &cmd, handle, &entry, -1);
            if (status < 0) {
                break;
            }
            std::string filepath = StringTools::strfmt("%s/%s", path, entry.name);

            if (entry.info.flags & FS_STAT_DIRECTORY && filter <= 1) {
                result.push_back(filepath);
            } else if (filter == 0 || filter == 2) {
                result.push_back(filepath);
            }
        }
    } else {
        DEBUG_FUNCTION_LINE("Failed to open dir %s", path);
    }
    return result;
}


std::string PayloadSelectionScreen(const std::map<std::string, std::string> &payloads) {
    int32_t screen_buf0_size = 0;

    // Init screen and screen buffers
    OSScreenInit();
    screen_buf0_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    OSScreenSetBufferEx(SCREEN_TV, (void *) 0xF4000000);
    OSScreenSetBufferEx(SCREEN_DRC, (void *) (0xF4000000 + screen_buf0_size));

    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);

    // Clear screens
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    VPADStatus vpad_data;
    VPADReadError error;
    int selected = 0;
    while (true) {
        // Clear screens
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);

        int pos = 0;
        int i = 0;
        for (auto const&[key, val] : payloads) {
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
    int i = 0;
    for (auto const&[key, val] : payloads) {
        if (i == selected) {
            return val;
        }
        i++;
    }
    return "";
}