#ifndef FILEREADER_H_
#define FILEREADER_H_

#include <stdio.h>
#include <string> // 确保包含 <string>
#include "macros.h"
#include "elf.h"

#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h> // For _stat64
#endif

class FileReader {
public:
    explicit FileReader(const std::string& source_path);
    ~FileReader();

    bool Read(Elf_Off offset, void *buf, size_t size);
    bool ReadAll(void *buf, size_t size);
    long long GetSize();
    const char *GetSourcePathCStr() const; // 改回原始名称，确保一致性
    const std::string& GetSourcePath() const; // 新增一个返回 std::string 引用的方法

protected:
    FILE *fp_;
    std::string source_path_; // 使用 source_path_
    long long file_size_;

private:
    DISALLOW_COPY_AND_ASSIGN(FileReader);
};

#endif // FILEREADER_H_ 