#include "string_bidi.h"
#include <cstdint>

#ifdef NO_HARFBUZZ_FRIBIDI

std::vector<int> get_bidi_embeddings(const std::vector<uint32_t>& string, int& direction) {
  return {};
}

#else

#include <fribidi.h>
#include <algorithm>
#include <climits>

std::vector<int> get_bidi_embeddings(const std::vector<uint32_t>& string, int& direction) {
  // fribidi_log2vis takes an int length; fall back to neutral embeddings for
  // absurdly long input rather than narrowing
  if (string.size() > (size_t) INT_MAX) {
    direction = direction == 2 ? 2 : 1;
    return std::vector<int>(string.size(), direction == 2 ? 1 : 0);
  }

  FriBidiCharType base_direction = direction == 0 ? FRIBIDI_TYPE_ON : (direction == 1 ? FRIBIDI_TYPE_LTR : FRIBIDI_TYPE_RTL);
  std::vector<FriBidiLevel> embedding_levels(string.size());

  FriBidiLevel max_level = fribidi_log2vis(string.data(), (int) string.size(), &base_direction,
                                           nullptr, nullptr, nullptr,
                                           embedding_levels.data());

  if (max_level == 0) {
    // Analysis failed; fall back to neutral embeddings in the requested
    // direction rather than returning uninitialized levels
    direction = direction == 2 ? 2 : 1;
    std::fill(embedding_levels.begin(), embedding_levels.end(),
              direction == 2 ? 1 : 0);
  } else {
    direction = FRIBIDI_IS_RTL(base_direction) ? 2 : 1;
  }

  return {embedding_levels.begin(), embedding_levels.end()};
}

#endif
