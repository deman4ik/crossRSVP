#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

class HalFile {
 public:
  HalFile() = default;
  ~HalFile() { close(); }

  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  bool open(const char* path, const char* mode) {
    close();
    file_ = std::fopen(path, mode);
    return file_ != nullptr;
  }

  int read(void* buffer, const size_t count) {
    if (!file_) return -1;
    return static_cast<int>(std::fread(buffer, 1, count, file_));
  }

  size_t write(const void* buffer, const size_t count) {
    return file_ ? std::fwrite(buffer, 1, count, file_) : 0;
  }

  void flush() {
    if (file_) std::fflush(file_);
  }

  bool close() {
    if (!file_) return true;
    const bool closed = std::fclose(file_) == 0;
    file_ = nullptr;
    return closed;
  }

 private:
  std::FILE* file_ = nullptr;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool exists(const char* path) const {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
  }

  bool remove(const char* path) { return std::remove(path) == 0; }

  bool rename(const char* from, const char* to) {
    ++renameCount_;
    for (auto failure = renameFailures_.begin(); failure != renameFailures_.end(); ++failure) {
      if (failure->first == from && failure->second == to) {
        renameFailures_.erase(failure);
        return false;
      }
    }
    return std::rename(from, to) == 0;
  }

  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    if (path == failedReadOpen_) {
      failedReadOpen_.clear();
      return false;
    }
    return file.open(path.c_str(), "rb");
  }

  bool openFileForWrite(const char*, const std::string& path, HalFile& file) {
    return file.open(path.c_str(), "wb");
  }

  void failRename(std::string from, std::string to) {
    renameFailures_.emplace_back(std::move(from), std::move(to));
  }

  void failNextReadOpen(std::string path) { failedReadOpen_ = std::move(path); }

  void resetTestState() {
    renameFailures_.clear();
    failedReadOpen_.clear();
    renameCount_ = 0;
  }

  uint32_t renameCount() const { return renameCount_; }

 private:
  std::vector<std::pair<std::string, std::string>> renameFailures_;
  std::string failedReadOpen_;
  uint32_t renameCount_ = 0;
};

#define Storage HalStorage::getInstance()
