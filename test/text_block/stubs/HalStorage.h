#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

class HalFile {
 public:
  bool open(const char* path, const char* mode) {
    file_ = std::fopen(path, mode);
    return file_ != nullptr;
  }
  bool seek(long offset) { return file_ != nullptr && std::fseek(file_, offset, SEEK_SET) == 0; }
  int read(uint8_t* buffer, size_t count) { return file_ ? static_cast<int>(std::fread(buffer, 1, count, file_)) : -1; }
  int read(void* buffer, size_t count) { return read(static_cast<uint8_t*>(buffer), count); }
  size_t write(const uint8_t* buffer, size_t count) { return file_ ? std::fwrite(buffer, 1, count, file_) : 0; }
  size_t write(const void* buffer, size_t count) { return write(static_cast<const uint8_t*>(buffer), count); }
  bool close() {
    if (file_ == nullptr) return true;
    const bool ok = std::fclose(file_) == 0;
    file_ = nullptr;
    return ok;
  }
  ~HalFile() { close(); }

 private:
  std::FILE* file_ = nullptr;
};
