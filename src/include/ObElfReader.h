#ifndef SOFIXER_OBELFREADER_H
#define SOFIXER_OBELFREADER_H

#include "ElfReader.h" // It inherits from ElfReader

class ElfRebuilder; // Forward declaration

class ObElfReader : public ElfReader {
public:
    ObElfReader(); // Default constructor if simple, or define in .cpp
    ~ObElfReader() override;

    bool Load() override; // Overrides ElfReader::Load

    void setDumpSoBaseAddr(Elf_Addr base) { dump_so_base_ = base; }
    Elf_Addr getDumpSoBaseAddr() const { return dump_so_base_; }

    void setBaseSoName(const char* name);

    bool haveDynamicSectionInLoadableSegment();

    // Methods that seem to be missing based on errors
    bool FindSymbol(const char *symbol_name, Elf_Sym *sym, int *symidx);
    const char* GetString(Elf_Word index);
    Elf_Addr CallConstructors();
    const Elf_Phdr* GetPhdr(int num); // This will use ElfReader::phdr_table_ and phdr_mmap_
    const Elf_Ehdr* GetEhdr();      // This will use ElfReader::header_
    Elf_Word GetSymbolNum();
    const Elf_Sym* GetSymbol(Elf_Word index);
    const Elf_Sym* GetSymbolTable();
    const char* GetStringTable();
    // GetLoadBias() is in ElfReader as public load_bias()
    uint8_t* GetMapSegment(int segment_num);

protected:
    void GetDynamicSection(Elf_Dyn** dynamic, size_t* dynamic_count, Elf_Word* dynamic_flags) override;

private:
    void FixDumpSoPhdr();
    bool LoadDynamicSectionFromBaseSource();
    void ApplyDynamicSection();
    bool ParseDynamicSegment(); // Helper to populate dynsym_, dynstr_, etc.

    Elf_Addr dump_so_base_ = 0;
    const char* baseso_ = nullptr;

    void* dynamic_sections_ = nullptr; // This is for the externally loaded .dynamic
    size_t dynamic_count_ = 0;
    Elf_Word dynamic_flags_ = 0;

    // Direct pointers and sizes from the loaded library's dynamic segment
    Elf_Sym* dynsym_ = nullptr;
    const char* dynstr_ = nullptr;
    Elf_Word dynsym_count_ = 0; // Number of symbols in .dynsym
    Elf_Word dynstr_size_ = 0;  // Size of .dynstr
    Elf_Word syment_ = 0; // Size of one symbol entry

    friend class ElfRebuilder;
};

#endif //SOFIXER_OBELFREADER_H 