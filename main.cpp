#include <iostream>
#include "ObElfReader.h"
#include "ElfRebuilder.h"
#include "FDebug.h"
//#include <getopt.h> // getopt.h is not available on Windows
#include <stdio.h>

#ifdef __SO64__
#define TARGET_NAME "SoFixer64"
#else
#define TARGET_NAME "SoFixer32"
#endif

// getopt.h is not available on Windows, so these are not used with MSVC
// const char* short_options = "hdm:s:o:b:"; 
// const struct option long_options[] = {      
//         {"help", 0, NULL, 'h'},
//         {"debug", 0, NULL, 'd'},
//         {"memso", 1, NULL, 'm'},
//         {"source", 1, NULL, 's'},
//         {"baseso", 1, NULL, 'b'},
//         {"output", 1, NULL, 'o'},
//         {nullptr, 0, nullptr, 0}
// };
void useage();


bool main_loop(int argc, char* argv[]) {
    // int c; // Variable 'c' is unused due to getopt removal

    ObElfReader elf_reader;

    std::string source, output, baseso, modules_info_file;
    // getopt is not available on Windows, so we'll have to manually parse or use a different method
    // For now, let's assume fixed arguments or implement a simple parser if necessary.
    // The original getopt loop is commented out.
    /*
    while((c = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (c) {
            case 'd':
                FLOGI("Use debug mode");
                break;
            case 's':
                source = optarg;
                break;
            case 'o':
                output = optarg;
                break;
            case 'b':
                baseso = optarg;
                break;
            case 'm': {
                auto is16Bit = [](const char* c) {
                    auto len = strlen(c);
                    if(len > 2) {
                        if(c[0] == '0' & c[1] == 'x') return true;
                    }
                    bool is10bit = true;
                    for(auto i = 0; i < len; i++) {
                        if((c[i] > 'a' && c[i] < 'f') ||
                           (c[i] > 'A' && c[i] < 'F')) {
                            is10bit = false;
                        }
                    }
                    return !is10bit;
                };
#ifndef __SO64__
                auto base = strtoul(optarg, 0, is16Bit(optarg) ? 16: 10);
#else
                auto base = strtoull(optarg, 0, is16Bit(optarg) ? 16: 10);
#endif
                elf_reader.setDumpSoBaseAddr(base);
            }
                break;
            default:
                return false;
        }
    }
    */

    // Manual argument parsing for Windows (simple example)
    // This is a placeholder and needs to be more robust for real use.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-s" || arg == "--source") && i + 1 < argc) {
            source = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output = argv[++i];
        } else if ((arg == "-b" || arg == "--baseso") && i + 1 < argc) {
            baseso = argv[++i];
        } else if ((arg == "-m" || arg == "--memso") && i + 1 < argc) {
            const char* mem_arg = argv[++i];
            auto is16Bit = [](const char* c_str) {
                auto len = strlen(c_str);
                if(len > 2) {
                    if(c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X')) return true;
                }
                bool is_hex = false;
                for(size_t j = (c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X')) ? 2 : 0; j < len; j++) {
                    if(isxdigit(c_str[j])) {
                        is_hex = true;
                    } else if (is_hex) { // if we already found hex digits, non-hex means not purely hex. 0xa1b is hex, 0xa1bS is not.
                        return false;
                    } else if (!isdigit(c_str[j])) { // if not hex and not digit, then not a number we handle here
                        return false;
                    }
                }
                return is_hex; // true if all (relevant) chars are hex digits
            };
#ifndef __SO64__
            auto base_addr = strtoul(mem_arg, nullptr, is16Bit(mem_arg) ? 16: 10);
#else
            auto base_addr = strtoull(mem_arg, nullptr, is16Bit(mem_arg) ? 16: 10);
#endif
            elf_reader.setDumpSoBaseAddr(base_addr);
        } else if ((arg == "-l" || arg == "--modules-info") && i + 1 < argc) {
            modules_info_file = argv[++i];
        } else if (arg == "-d" || arg == "--debug") {
             FLOGI("Use debug mode");
        } else if (arg == "-h" || arg == "--help") {
            useage();
            return true; // Assuming help implies successful exit
        }
    }

    if (source.empty() || output.empty()) {
        FLOGE("Source and output file paths are required.");
        useage();
        return false;
    }


    auto file = fopen(source.c_str(), "rb");
    if(nullptr == file) {
        FLOGE("source so file cannot found!!!");
        return false;
    }
#ifdef _WIN32
    auto fd = _fileno(file); // Use _fileno on Windows
#elif __LARGE64_FILES
    auto fd = file->_file; // This might be specific to some older systems/compilers
#else
    auto fd = fileno(file);
#endif

    FLOGI("start to rebuild elf file");
    if (!elf_reader.setSource(source.c_str())) {
        FLOGE("unable to open source file");
        return false;
    }
    if (!baseso.empty()) {
        elf_reader.setBaseSoName(baseso.c_str());
    }
    if (!modules_info_file.empty()) {
        elf_reader.SetModulesInfoPath(modules_info_file);
    }

    if(!elf_reader.Load()) {
        FLOGE("source so file is invalid");
        return false;
    }

    ElfRebuilder elf_rebuilder(&elf_reader);
    if(!elf_rebuilder.Rebuild()) {
        FLOGE("error occured in rebuilding elf file");
        return false;
    }
    fclose(file);

    if (!output.empty()) {
        file = fopen(output.c_str(), "wb+");
        if(nullptr == file) {
            FLOGE("output so file cannot write !!!");
            return false;
        }
        fwrite(elf_rebuilder.getRebuildData(), 1, elf_rebuilder.getRebuildSize(),  file);
        fclose(file);
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (main_loop(argc, argv)) {
        FLOGI("Done!!!");
        return 0;
    }
    useage();
    return -1;
}

void useage() {
    FLOGI(TARGET_NAME "v2.1 author F8LEFT(currwin)");
    FLOGI("Useage: SoFixer <option(s)> -s sourcefile -o generatefile");
    FLOGI(" try rebuild shdr with phdr");
    FLOGI(" Options are:");

    FLOGI("  -d --debug                                 Show debug info");
    FLOGI("  -m --memso memBaseAddr(16bit format)       the memory address x which the source so is dump from");
    FLOGI("  -s --source sourceFilePath                 Source file path");
    FLOGI("  -b --baseso baseFilePath                   Original so file path.(used to get base information)(experimental)");
    FLOGI("  -o --output generateFilePath               Generate file path");
    FLOGI("  -h --help                                  Display this information");
    FLOGI("  -l --modules-info modulesInfoFilePath      Path to the loaded_modules_info.txt file");
}
