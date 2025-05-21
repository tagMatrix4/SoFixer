//===------------------------------------------------------------*- C++ -*-===//
//
//                     Created by F8LEFT on 2021/1/5.
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include "ObElfReader.h"

#include <vector>
#include <algorithm>
#include <fstream>      // For std::ifstream
#include <sstream>      // For std::stringstream
#include <string>       // For std::string related functions like getline, find, substr, stoull

void ObElfReader::FixDumpSoPhdr() {
    // some shell will release data between loadable phdr(s), just load all memory data
    if (dump_so_base_ != 0) {
        std::vector<Elf_Phdr*> loaded_phdrs;
        for (auto i = 0; i < phdr_num_; i++) {
            auto phdr = &phdr_table_[i];
            if(phdr->p_type != PT_LOAD) continue;
            loaded_phdrs.push_back(phdr);
        }
        std::sort(loaded_phdrs.begin(), loaded_phdrs.end(),
                  [](Elf_Phdr * first, Elf_Phdr * second) {
                      return first->p_vaddr < second->p_vaddr;
                  });
        if (!loaded_phdrs.empty()) {
            for (size_t i = 0, total = loaded_phdrs.size(); i < total; i++) {
                auto phdr = loaded_phdrs[i];
                if (i != total - 1) {
                    // to next loaded segament
                    auto nphdr = loaded_phdrs[i+1];
                    phdr->p_memsz = nphdr->p_vaddr - phdr->p_vaddr;
                } else {
                    // to the file end
                    phdr->p_memsz = file_size - phdr->p_vaddr;
                }
                phdr->p_filesz = phdr->p_memsz;
            }
        }
    }

    auto phdr = phdr_table_;
    for(auto i = 0; i < phdr_num_; i++) {
        phdr->p_paddr = phdr->p_vaddr;
        phdr->p_filesz = phdr->p_memsz;     // expend filesize to memsiz
        phdr->p_offset = phdr->p_vaddr;     // since elf has been loaded. just expand file data to dump memory data
//            phdr->p_flags = 0                 // TODO fix flags by PT_TYPE
        phdr++;
    }
}

bool ObElfReader::ParseModulesInfo() {
    if (modules_info_path_.empty()) {
        FLOGD("Modules info path is not set. Skipping parsing.");
        return true; // Not an error if path is not provided
    }

    std::ifstream info_file(modules_info_path_);
    if (!info_file.is_open()) {
        FLOGE("Failed to open modules info file: %s", modules_info_path_.c_str());
        return false;
    }

    FLOGI("Parsing modules info file: %s", modules_info_path_.c_str());
    std::string line;
    // Skip header line
    if (std::getline(info_file, line) && line.rfind("#", 0) != 0) {
        // If the first line is not a comment, reset to read it as data
        info_file.clear();
        info_file.seekg(0);
    } else if (line.rfind("#", 0) == 0) {
        FLOGD("Skipped header line: %s", line.c_str());
    } else if (info_file.eof()){
        FLOGW("Modules info file is empty or contains only a header that was consumed by getline: %s", modules_info_path_.c_str());
        return true; // Empty file after header is not an error for parsing itself.
    }

    while (std::getline(info_file, line)) {
        if (line.empty() || line[0] == '#') { // Skip empty lines or comments
            continue;
        }
        std::stringstream ss(line);
        std::string module_name_str;
        std::string load_address_str;

        if (std::getline(ss, module_name_str, ',') && std::getline(ss, load_address_str)) {
            try {
                Elf_Addr load_address = std::stoull(load_address_str, nullptr, 16);
                loaded_modules_map_[module_name_str] = load_address;
                FLOGD("Loaded module from info: %s -> 0x%llx", module_name_str.c_str(), load_address);
            } catch (const std::invalid_argument& ia) {
                (void)ia; // Mark as used
                FLOGE("Invalid address format in modules info file for %s: %s. Line: %s", module_name_str.c_str(), load_address_str.c_str(), line.c_str());
            } catch (const std::out_of_range& oor) {
                (void)oor; // Mark as used
                FLOGE("Address out of range in modules info file for %s: %s. Line: %s", module_name_str.c_str(), load_address_str.c_str(), line.c_str());
            }
        } else {
            FLOGW("Malformed line in modules info file: %s", line.c_str());
        }
    }

    if (info_file.bad()) {
        FLOGE("Error reading modules info file: %s", modules_info_path_.c_str());
        return false;
    }
    FLOGI("Finished parsing modules info. Loaded %zu entries.", loaded_modules_map_.size());
    return true;
}

bool ObElfReader::Load() {
    // try open
    if (!ReadElfHeader() || !VerifyElfHeader() || !ReadProgramHeader()) {
        FLOGE("Failed basic ELF header processing.");
        return false;
    }

    if (!ParseModulesInfo()) { // Parse modules info early
        FLOGE("Failed to parse modules info file.");
        // Decide if this is a fatal error. For now, let's continue but log it.
    }

    // Attempt to determine dump_so_base_ if not set by user
    if (dump_so_base_ == 0 && name_ != nullptr && !loaded_modules_map_.empty()) {
        std::string current_so_name = name_;
        size_t last_slash = current_so_name.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            current_so_name = current_so_name.substr(last_slash + 1);
        }
        // Remove "dump_" prefix if it exists from the Java side
        if (current_so_name.rfind("dump_", 0) == 0) {
            current_so_name = current_so_name.substr(5);
        }

        auto it = loaded_modules_map_.find(current_so_name);
        if (it != loaded_modules_map_.end()) {
            dump_so_base_ = it->second;
            FLOGI("Automatically determined dump_so_base_ for %s from modules info: 0x%llx", 
                  current_so_name.c_str(), dump_so_base_);
        } else {
            FLOGW("Could not find base address for %s in modules info. Dump base remains 0.", 
                  current_so_name.c_str());
            // Try to find it with "dump_" prefix if it wasn't removed properly before, or if that was the name in the file
            std::string prefixed_name = "dump_" + current_so_name;
            it = loaded_modules_map_.find(prefixed_name);
            if (it != loaded_modules_map_.end()){
                 dump_so_base_ = it->second;
                 FLOGI("Automatically determined dump_so_base_ for %s (as %s) from modules info: 0x%llx", 
                     current_so_name.c_str(), prefixed_name.c_str(), dump_so_base_);
            } else {
                 FLOGW("Still could not find base address for %s or %s in modules info. Dump base remains 0.", 
                     current_so_name.c_str(), prefixed_name.c_str());
            }
        }
    }
    if (dump_so_base_ == 0) {
         FLOGW("dump_so_base_ is 0. Relocations for the main SO might not be fully correct unless it was loaded at 0 by chance.");
    } else {
         FLOGI("Using dump_so_base_: 0x%llx for relocations.", dump_so_base_);
    }

    FixDumpSoPhdr();

    bool has_base_dynamic_info = false;
    size_t base_dynamic_size = 0;
    if (!haveDynamicSectionInLoadableSegment()) {
        // try to get dynamic information from base so file.
        // TODO fix bug in dynamic section rebuild.
        LoadDynamicSectionFromBaseSource();
        has_base_dynamic_info = dynamic_sections_ != nullptr;
        if (has_base_dynamic_info) {
            base_dynamic_size = dynamic_count_ * sizeof(Elf_Dyn);
        }
    } else {
        FLOGI("dynamic segment have been found in loadable segment, "
              "argument baseso will be ignored.");
    }

    if (!ReserveAddressSpace(static_cast<uint32_t>(base_dynamic_size)) ||
        !LoadSegments() ||
        !FindPhdr()) {
        return false;
    }
    if (has_base_dynamic_info) {
        // Copy dynamic information to the end of the file.
        ApplyDynamicSection();
    }

    ApplyPhdrTable();

    return true;
}

//void ObElfReader::GetDynamicSection(Elf_Dyn **dynamic, size_t *dynamic_count, Elf_Word *dynamic_flags) {
//    if (dynamic_sections_ == nullptr) {
//        ElfReader::GetDynamicSection(dynamic, dynamic_count, dynamic_flags);
//        return;
//    }
//    *dynamic = reinterpret_cast<Elf_Dyn*>(dynamic_sections_);
//    if (dynamic_count) {
//        *dynamic_count = dynamic_count_;
//    }
//    if (dynamic_flags) {
//        *dynamic_flags = dynamic_flags_;
//    }
//    return;
//}

ObElfReader::~ObElfReader() {
    if (dynamic_sections_ != nullptr) {
        delete [](uint8_t*)dynamic_sections_;
    }
}

bool ObElfReader::LoadDynamicSectionFromBaseSource() {
    if (baseso_ == nullptr) {
        FLOGD("Base SO name is not set. Cannot load dynamic section from base source.");
        return false;
    }
    ElfReader base_reader;

    // if base so is provided, load dynamic section from base so
    if (!base_reader.setSource(baseso_) ||
        !base_reader.ReadElfHeader() ||
        !base_reader.VerifyElfHeader() ||
        !base_reader.ReadProgramHeader()) {
        FLOGE("Unable to parse base so file, is it correct?");
        return false;
    }
    const Elf_Phdr * phdr_table_ = base_reader.phdr_table_;
    const Elf_Phdr * phdr_limit = phdr_table_ + base_reader.phdr_num_;
    const Elf_Phdr * phdr;

    for (phdr = phdr_table_; phdr < phdr_limit; phdr++) {
        if (phdr->p_type != PT_DYNAMIC) {
            continue;
        }

        dynamic_sections_ = new uint8_t [phdr->p_memsz];
        base_reader.source_->Read(dynamic_sections_, phdr->p_memsz, phdr->p_offset);

        dynamic_count_ = (unsigned)(phdr->p_memsz / sizeof(Elf_Dyn));
        dynamic_flags_ = phdr->p_flags;
        return true;
    }

    return false;
}

void ObElfReader::ApplyDynamicSection() {
    if (dynamic_sections_ == nullptr)
        return;
    uint8_t * wbuf_start = load_start_ + load_size_;
    size_t dynamic_size = dynamic_count_ * sizeof(Elf_Dyn);
    if (pad_size_ < dynamic_size)
        return;
    // copy directly
    memcpy(wbuf_start, dynamic_sections_, dynamic_size);
    // fix phdr header
    for (auto p = phdr_table_, pend = phdr_table_+ phdr_num_; p < pend; p++) {
        if (p->p_type == PT_DYNAMIC) {
            p->p_vaddr = wbuf_start - load_bias_;
            p->p_paddr = p->p_vaddr;
            p->p_offset = p->p_vaddr;

            p->p_memsz = dynamic_size;
            p->p_filesz = p->p_memsz;
            break;
        }
    }
}

bool ObElfReader::haveDynamicSectionInLoadableSegment() {
    Elf_Addr min_vaddr, max_vaddr;
    phdr_table_get_load_size(phdr_table_, phdr_num_, &min_vaddr, &max_vaddr);

    const Elf_Phdr* phdr = phdr_table_;
    const Elf_Phdr* phdr_limit = phdr + phdr_num_;

    for (phdr = phdr_table_; phdr < phdr_limit; phdr++) {
        if (phdr->p_type != PT_DYNAMIC) {
            continue;
        }
        if (phdr->p_vaddr > min_vaddr && (phdr->p_vaddr + phdr->p_memsz) < max_vaddr) {
            return true;
        }
        break;
    }
    return false;
}

