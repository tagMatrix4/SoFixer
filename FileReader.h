//===------------------------------------------------------------*- C++ -*-===//
//
//                     Created by F8LEFT on 2021/1/5.
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#ifndef SOFIXER_FILEREADER_H
#define SOFIXER_FILEREADER_H

#include "macros.h"
#include "FDebug.h"
#include <cerrno>
#include <cstdio>
#include <cstring>

class FileReader {
public:
    FileReader(const char* name): source(name){}
    ~FileReader() {
        Close();
    }
    bool Open() {
        if (IsValid()) {
            return false;
        }
        fp = fopen(source, "rb");
        if (fp == nullptr) {
            return false;
        }
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        return true;
    }
    bool Close() {
        if (IsValid()) {
            auto err = fclose(fp);
            fp = nullptr;
            return err == 0;
        }
        return false;
    }
    bool IsValid() {
        return fp != nullptr;
    }
    const char* getSource() {
        return source;
    }
    size_t Read(void *addr, size_t len, long long offset = -1) {
        if (offset >= 0) {
            fseek(fp, offset, SEEK_SET);
        }
#ifdef _MSC_VER
        auto rc = fread(addr, 1, len, fp);
#else
        auto rc = TEMP_FAILURE_RETRY(fread(addr, 1, len, fp));
#endif

        if (rc == 0 && ferror(fp)) { // Check for actual read error
            FLOGE("can't read file \"%s\": %s", source, strerror(errno));
            return -1; // Indicate error explicitly if possible, or stick to size_t context
        }
        // fread returns number of items read. If it's less than len, it could be EOF or error.
        if (rc != len && !feof(fp)) { 
            FLOGE("\"%s\" has no enough data at %x:%zx, or read error. Read %zu, expected %zu", source, offset, len, rc, len);
            // Return rc as it's the number of bytes successfully read
        } else if (rc != len && feof(fp)) {
            FLOGE("\"%s\" hit EOF. Read %zu, expected %zu at %x:%zx", source, rc, len, offset, len);
        }
        return rc;
    }
    long FileSize() {
        return file_size;
    }
private:
    FILE* fp = nullptr;
    const char* source = nullptr;
    long file_size;
};

#endif //SOFIXER_FILEREADER_H
