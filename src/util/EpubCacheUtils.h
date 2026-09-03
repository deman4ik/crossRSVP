#pragma once

#include <string>

// Clears derived EPUB cache entries while preserving durable reader state.
bool clearEpubDerivedCache(const std::string& cachePath);
