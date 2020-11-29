#pragma once

#include <cstdint>

extern "C" int32_t LoadFileToMem(const char *relativefilepath, char **fileOut, uint32_t *sizeOut);
extern "C" uint32_t load_loader_elf_from_sd(unsigned char *baseAddress, const char *relativePath);

