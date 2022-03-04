#include "utils/logger.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utils/logger.h>

#include "elf_abi.h"

int32_t LoadFileToMem(const char *filepath, uint8_t **inbuffer, uint32_t *size) {
    //! always initialze input
    *inbuffer = NULL;
    if (size) {
        *size = 0;
    }

    int32_t iFd = open(filepath, O_RDONLY);
    if (iFd < 0) {
        return -1;
    }

    uint32_t filesize = lseek(iFd, 0, SEEK_END);
    lseek(iFd, 0, SEEK_SET);

    uint8_t *buffer = (uint8_t *) memalign(0x40, (filesize + 0x3F) & ~(0x3F));
    if (buffer == NULL) {
        close(iFd);
        return -2;
    }

    uint32_t blocksize = 0x20000;
    uint32_t done      = 0;
    int32_t readBytes  = 0;

    while (done < filesize) {
        if (done + blocksize > filesize) {
            blocksize = filesize - done;
        }
        readBytes = read(iFd, buffer + done, blocksize);
        if (readBytes <= 0)
            break;
        done += readBytes;
    }

    close(iFd);

    if (done != filesize) {
        free(buffer);
        buffer = NULL;
        return -3;
    }

    *inbuffer = buffer;

    //! size is optional input
    if (size) {
        *size = filesize;
    }

    return filesize;
}

static void InstallMain(void *data_elf);

static bool CheckElfLoadedBetween(void *data_elf, uint32_t start_address, uint32_t end_address);

static unsigned int get_section(void *data, const char *name, unsigned int *size, unsigned int *addr, int fail_on_not_found);

uint32_t load_loader_elf_from_sd(unsigned char *baseAddress, const char *relativePath) {
    char *elf_data    = NULL;
    uint32_t fileSize = 0;
    if (LoadFileToMem(relativePath, &elf_data, &fileSize) < 0) {
        return 0;
    }

    if (!CheckElfLoadedBetween(elf_data, 0x00800000, 0x00FD0000)) {
        return 0;
    }
    InstallMain(elf_data);

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *) elf_data;

    uint32_t res = ehdr->e_entry;

    free((void *) elf_data);

    return res;
}

static bool CheckElfSectionLoadedBetween(void *data_elf, const char *name, uint32_t start_address, uint32_t end_address) {
    unsigned int target_addr = 0;
    unsigned int len         = 0;
    if (get_section(data_elf, name, &len, &target_addr, 0) > 0) {
        if (target_addr < start_address || target_addr + len > end_address) {
            DEBUG_FUNCTION_LINE("ERROR: target_addr (%08X) < start_address (%08X) || target_addr + len (%08X) > end_address (%08X)", target_addr, start_address, target_addr + len, end_address);
            return false;
        }
    }
    return true;
}

static bool CheckElfLoadedBetween(void *data_elf, uint32_t start_address, uint32_t end_address) {
    if (!CheckElfSectionLoadedBetween(data_elf, ".text", start_address, end_address)) {
        DEBUG_FUNCTION_LINE("ERROR: The .text would be loaded into a invalid location.");
        return false;
    }
    if (!CheckElfSectionLoadedBetween(data_elf, ".rodata", start_address, end_address)) {
        DEBUG_FUNCTION_LINE("ERROR: The .rodata would be loaded into a invalid location.");
        return false;
    }
    if (!CheckElfSectionLoadedBetween(data_elf, ".data", start_address, end_address)) {
        DEBUG_FUNCTION_LINE("ERROR: The .data would be loaded into a invalid location.");
        return false;
    }
    if (!CheckElfSectionLoadedBetween(data_elf, ".bss", start_address, end_address)) {
        DEBUG_FUNCTION_LINE("ERROR: The .bss would be loaded into a invalid location.");
        return false;
    }
    return true;
}

static unsigned int get_section(void *data, const char *name, unsigned int *size, unsigned int *addr, int fail_on_not_found) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *) data;

    if (!data || !IS_ELF(*ehdr) || (ehdr->e_type != ET_EXEC) || (ehdr->e_machine != EM_PPC)) {
        OSFatal("Invalid elf file");
    }

    Elf32_Shdr *shdr = (Elf32_Shdr *) ((uint32_t) data + ehdr->e_shoff);
    int i;
    for (i = 0; i < ehdr->e_shnum; i++) {
        const char *section_name = ((const char *) data) + shdr[ehdr->e_shstrndx].sh_offset + shdr[i].sh_name;
        if (strcmp(section_name, name) == 0) {
            if (addr) {
                *addr = shdr[i].sh_addr;
            }
            if (size) {
                *size = shdr[i].sh_size;
            }
            return shdr[i].sh_offset;
        }
    }

    if (fail_on_not_found) {
        OSFatal((char *) name);
    }

    return 0;
}

/* ****************************************************************** */
/*                         INSTALL MAIN CODE                          */
/* ****************************************************************** */
static void InstallMain(void *data_elf) {
    // get .text section
    unsigned int main_text_addr = 0;
    unsigned int main_text_len  = 0;
    unsigned int section_offset = get_section(data_elf, ".text", &main_text_len, &main_text_addr, 1);
    unsigned char *main_text    = (unsigned char *) ((uint32_t) data_elf + section_offset);
    /* Copy main .text to memory */
    if (section_offset > 0) {
        DEBUG_FUNCTION_LINE("Copy section to %08X from %08X (size: %d)", main_text_addr, main_text, main_text_len);
        memcpy((void *) (main_text_addr), (void *) main_text, main_text_len);
        DCFlushRange((void *) main_text_addr, main_text_len);
        ICInvalidateRange((void *) main_text_addr, main_text_len);
    }


    // get the .rodata section
    unsigned int main_rodata_addr = 0;
    unsigned int main_rodata_len  = 0;
    section_offset                = get_section(data_elf, ".rodata", &main_rodata_len, &main_rodata_addr, 0);
    if (section_offset > 0) {
        unsigned char *main_rodata = (unsigned char *) ((uint32_t) data_elf + section_offset);
        /* Copy main rodata to memory */
        memcpy((void *) (main_rodata_addr), (void *) main_rodata, main_rodata_len);
        DCFlushRange((void *) main_rodata_addr, main_rodata_len);
        ICInvalidateRange((void *) main_rodata_addr, main_rodata_len);
    }

    // get the .data section
    unsigned int main_data_addr = 0;
    unsigned int main_data_len  = 0;
    section_offset              = get_section(data_elf, ".data", &main_data_len, &main_data_addr, 0);
    if (section_offset > 0) {
        unsigned char *main_data = (unsigned char *) ((uint32_t) data_elf + section_offset);
        /* Copy main data to memory */
        memcpy((void *) (main_data_addr), (void *) main_data, main_data_len);
        DCFlushRange((void *) main_data_addr, main_data_len);
        ICInvalidateRange((void *) main_data_addr, main_data_len);
    }

    // get the .bss section
    unsigned int main_bss_addr = 0;
    unsigned int main_bss_len  = 0;
    section_offset             = get_section(data_elf, ".bss", &main_bss_len, &main_bss_addr, 0);
    if (section_offset > 0) {
        unsigned char *main_bss = (unsigned char *) ((uint32_t) data_elf + section_offset);
        /* Copy main data to memory */
        memcpy((void *) (main_bss_addr), (void *) main_bss, main_bss_len);
        DCFlushRange((void *) main_bss_addr, main_bss_len);
        ICInvalidateRange((void *) main_bss_addr, main_bss_len);
    }
}
