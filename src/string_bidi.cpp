#include "string_bidi.h"
#include <cstdint>

#ifndef NO_HARFBUZZ_FRIBIDI

std::vector<int> get_bidi_embeddings(const uint32_t* string, int n_chars) {
  return {};
}

#else

#include <fribidi.h>

std::vector<int> get_bidi_embeddings(const std::vector<uint32_t>& string, int& direction) {
  FriBidiCharType base_direction = direction == 0 ? FRIBIDI_TYPE_ON : (direction == 1 ? FRIBIDI_TYPE_LTR : FRIBIDI_TYPE_RTL);
  std::vector<FriBidiLevel> embedding_levels(string.size());

  FriBidiLevel max_level = fribidi_log2vis(string.data(), string.size(), &base_direction,
                                           nullptr, nullptr, nullptr,
                                           embedding_levels.data());
  (void) max_level; //shut up compiler about unused variables

  direction = FRIBIDI_IS_RTL(base_direction) ? 2 : 1;

  return {embedding_levels.begin(), embedding_levels.end()};
}

#endif
