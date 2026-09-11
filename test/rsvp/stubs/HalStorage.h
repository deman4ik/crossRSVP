#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class HalFile {
  friend class HalStorage;

 public:
  HalFile() = default;
  ~HalFile() { close(); }

  HalFile(HalFile&& other) noexcept
      : file_(other.file_), path_(std::move(other.path_)), directoryIterator_(std::move(other.directoryIterator_)) {
    other.file_ = nullptr;
    other.path_.clear();
  }
  HalFile& operator=(HalFile&& other) noexcept {
    if (this == &other) return *this;
    close();
    file_ = other.file_;
    path_ = std::move(other.path_);
    directoryIterator_ = std::move(other.directoryIterator_);
    other.file_ = nullptr;
    other.path_.clear();
    return *this;
  }

  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  bool open(const char* path, const char* mode) {
    close();
    file_ = std::fopen(path, mode);
    if (file_) path_ = path;
    return file_ != nullptr;
  }

  int read(void* buffer, const size_t count) {
    if (!file_) return -1;
    return static_cast<int>(std::fread(buffer, 1, count, file_));
  }
  size_t fileSize() const { return file_ ? std::filesystem::file_size(path_) : 0; }

  size_t write(const void* buffer, const size_t count) { return file_ ? std::fwrite(buffer, 1, count, file_) : 0; }

  void flush() {
    if (file_) std::fflush(file_);
  }

  bool close() {
    const bool closed = !file_ || std::fclose(file_) == 0;
    file_ = nullptr;
    path_.clear();
    directoryIterator_.reset();
    return closed;
  }

  size_t getName(char* name, const size_t length) {
    if (!name || length == 0 || path_.empty()) return 0;
    const std::string entryName = std::filesystem::path(path_).filename().string();
    const size_t copied = std::min(entryName.size(), length - 1);
    std::memcpy(name, entryName.data(), copied);
    name[copied] = '\0';
    return copied;
  }

  bool isDirectory() const { return !path_.empty() && std::filesystem::is_directory(path_); }

  HalFile openNextFile() {
    if (!directoryIterator_ || *directoryIterator_ == std::filesystem::directory_iterator{}) return {};
    const auto entry = **directoryIterator_;
    ++*directoryIterator_;
    return HalFile(entry.path().string());
  }

  explicit operator bool() const { return file_ != nullptr || !path_.empty(); }

 private:
  explicit HalFile(std::string path) : path_(std::move(path)) {
    if (std::filesystem::is_directory(path_)) {
      directoryIterator_ = std::make_unique<std::filesystem::directory_iterator>(path_);
    }
  }

  std::FILE* file_ = nullptr;
  std::string path_;
  std::unique_ptr<std::filesystem::directory_iterator> directoryIterator_;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool exists(const char* path) const { return std::filesystem::exists(path); }

  bool mkdir(const char* path, bool = true) {
    std::error_code error;
    return std::filesystem::create_directories(path, error) || std::filesystem::exists(path);
  }

  bool remove(const char* path) {
    for (auto failure = removeFailures_.begin(); failure != removeFailures_.end(); ++failure) {
      if (*failure == path) {
        removeFailures_.erase(failure);
        return false;
      }
    }
    return std::remove(path) == 0;
  }

  bool removeDir(const char* path) {
    std::error_code error;
    const auto removed = std::filesystem::remove_all(path, error);
    return !error && removed > 0;
  }

  HalFile open(const char* path) {
    if (!std::filesystem::is_directory(path)) return {};
    return HalFile(std::string(path));
  }

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

  bool openFileForWrite(const char*, const std::string& path, HalFile& file) { return file.open(path.c_str(), "wb"); }

  void failRename(std::string from, std::string to) { renameFailures_.emplace_back(std::move(from), std::move(to)); }

  void failRemove(std::string path) { removeFailures_.push_back(std::move(path)); }

  void failNextReadOpen(std::string path) { failedReadOpen_ = std::move(path); }

  void resetTestState() {
    renameFailures_.clear();
    removeFailures_.clear();
    failedReadOpen_.clear();
    renameCount_ = 0;
  }

  uint32_t renameCount() const { return renameCount_; }

 private:
  std::vector<std::pair<std::string, std::string>> renameFailures_;
  std::vector<std::string> removeFailures_;
  std::string failedReadOpen_;
  uint32_t renameCount_ = 0;
};

#define Storage HalStorage::getInstance()
