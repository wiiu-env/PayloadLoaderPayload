#pragma once

#include <cstdint>

extern "C" int32_t LoadFileToMem(const char *filepath, uint8_t **inbuffer, uint32_t *size);
extern "C" uint32_t load_loader_elf_from_sd(unsigned char *baseAddress, const char *relativePath);