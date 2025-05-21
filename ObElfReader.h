//===------------------------------------------------------------*- C++ -*-===//
//
//                     Created by F8LEFT on 2021/1/5.
//===----------------------------------------------------------------------===//
// ElfReader for Obfuscated so file
//===----------------------------------------------------------------------===//
#ifndef SOFIXER_OBELFREADER_H
#define SOFIXER_OBELFREADER_H

#include "ElfReader.h"
#include <string> // Required for std::string
#include <map>    // Required for std::map

class ElfRebuilder;

class ObElfReader: public ElfReader {
public:
    ~ObElfReader() override;
    // the phdr informaiton in dumped so may be incorrect,
    // try to fix it
    void FixDumpSoPhdr();

    bool Load() override;
    bool LoadDynamicSectionFromBaseSource();

    void setDumpSoBaseAddr(Elf_Addr base) { dump_so_base_ = base; }

    void setBaseSoName(const char* name) {
        baseso_ = name;
    }

    void SetModulesInfoPath(const std::string& path) { // New setter
        modules_info_path_ = path;
    }

//    void GetDynamicSection(Elf_Dyn** dynamic, size_t* dynamic_count, Elf_Word* dynamic_flags) override;
    bool haveDynamicSectionInLoadableSegment();

    const std::map<std::string, Elf_Addr>& GetLoadedModulesMap() const { // Getter for ElfRebuilder
        return loaded_modules_map_;
    }

private:
    void ApplyDynamicSection();
    bool ParseModulesInfo(); // New private method

    Elf_Addr dump_so_base_ = 0;

    const char* baseso_ = nullptr;

    void* dynamic_sections_ = nullptr;
    size_t dynamic_count_ = 0;
    Elf_Word dynamic_flags_ = 0;

    std::string modules_info_path_; // New member
    std::map<std::string, Elf_Addr> loaded_modules_map_; // New member

    friend class ElfRebuilder;

};


#endif //SOFIXER_OBELFREADER_H
