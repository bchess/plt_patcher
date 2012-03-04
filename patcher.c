#define _GNU_SOURCE
#include <link.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>

typedef struct PatchFunctionData
{
    char * libName;
    char * functionName;
    void * newFunction;
    int result;
} PatchFunctionData;

int unprotect(void * ptr, ssize_t size)
{
    ssize_t offset = ((ssize_t)ptr) % 4096;
    ssize_t start = ((ssize_t)ptr) - offset;
    return mprotect((void *)start, size + offset, PROT_READ | PROT_EXEC);
}

typedef struct DynamicHeader
{
    Elf64_Dyn * header;
    Elf64_Dyn * end;
} DynamicHeader;

DynamicHeader dynamicHeader(struct dl_phdr_info * info)
{
    int i = 0;
    DynamicHeader h = {NULL,NULL};
    for( i = 0; i != info->dlpi_phnum; ++i ) {
        if( info->dlpi_phdr[i].p_type == PT_DYNAMIC ) {
            h.header = (Elf64_Dyn *)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
            h.end = (Elf64_Dyn *)((char *)h.header + info->dlpi_phdr[i].p_memsz);
            return h;
        }
    }
    return h;
}

Elf64_Shdr * findSectionHeaderByAddress(struct dl_phdr_info * info, void * address)
{
    Elf64_Ehdr * elfHeader = (Elf64_Ehdr *)info->dlpi_addr;
    Elf64_Shdr * shdr = (Elf64_Shdr *)(elfHeader->e_shoff + info->dlpi_addr);
    Elf64_Shdr * shdrEnd = shdr + elfHeader->e_shnum;
    for( ; shdr != shdrEnd; ++shdr ) {
        int err = unprotect(shdr, sizeof(Elf64_Shdr));
        if( err == 0 && shdr->sh_addr + info->dlpi_addr == (ssize_t)address ) {
            return shdr;
        }
    }
    return NULL;
}

Elf64_Shdr * findSectionHeaderByIndex(struct dl_phdr_info * info, int index)
{
    Elf64_Ehdr * elfHeader = (Elf64_Ehdr *)info->dlpi_addr;
    Elf64_Shdr * shdr = (Elf64_Shdr *)(elfHeader->e_shoff + info->dlpi_addr);
    return shdr + index;
}

void * sectionData(DynamicHeader header, int sectionType)
{
    void * result = NULL;
    int i = 0;
    for( ; header.header != header.end; ++header.header ) {
        if( header.header->d_tag == sectionType ) {
            result = (void *)header.header->d_un.d_ptr;
        }
        ++i;
    } 
    return result;
}

static int phdrCallback(struct dl_phdr_info * libInfo, size_t size, void *data_)
{
    PatchFunctionData * data = data_;
    char * libStart = (char *)libInfo->dlpi_addr;
    if( libStart == 0 )
        return 0;
    //printf("%s\n", libInfo->dlpi_name);
    if( strcmp(libInfo->dlpi_name, data->libName) != 0 )
        return 0; // Not the right lib

    // Find the dynamic header section
    DynamicHeader dHeader = dynamicHeader(libInfo);

    // .rela.plt has the offset into the GOT where the function will jump to
    Elf64_Rela * relTable = sectionData(dHeader, DT_JMPREL);
    if( relTable == 0 )
        return 0;
    Elf64_Shdr * relSection = findSectionHeaderByAddress(libInfo, relTable);
    if( relSection == 0 ) 
        return 0;
    ssize_t relTableSize = (ssize_t)sectionData(dHeader, DT_PLTRELSZ) / sizeof(*relTable);

    // .dynsym can map from a relTable's symbol to a symbol name
    Elf64_Sym * symTable = sectionData(dHeader, DT_SYMTAB);
    Elf64_Shdr * symSection = findSectionHeaderByIndex(libInfo, relSection->sh_link);

    // the string table can give us the actual string of the symbol
    Elf64_Shdr * strSection = findSectionHeaderByIndex(libInfo, symSection->sh_link);
    char * stringTable = strSection->sh_offset + libStart;

    int i;
    for( i = 0; i != relTableSize; ++i ) {
        Elf64_Rela * rel = relTable + i;
        Elf64_Sym * sym = symTable + ELF64_R_SYM(rel->r_info);
        char * symName = stringTable + sym->st_name;
        if( strcmp(symName, data->functionName) == 0 ) {
            ssize_t * gotAddr = (ssize_t *)(rel->r_offset + libInfo->dlpi_addr);
            *gotAddr = (ssize_t)data->newFunction;
            data->result = 0;
            break; // Success!
        }
    }

    return 0;
}

int patchFunctionForLib(char * libName, char * functionName, void * newFunction)
{
    PatchFunctionData data = {libName, functionName, newFunction, -1};
    dl_iterate_phdr(phdrCallback, &data);
    return data.result;
}

