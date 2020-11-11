#include "string_bidi.h"

#ifdef NO_HARFBUZZ_FRIBIDI

std::vector<int> get_bidi_embeddings(const uint32_t* string, int n_chars) {
  return {};
}

#else

#include <fribidi.h>

std::vector<int> get_bidi_embeddings(const uint32_t* string, int n_chars) {
  FriBidiCharType base_direction = FRIBIDI_TYPE_ON;
  std::vector<FriBidiLevel> embedding_levels(n_chars);

  FriBidiLevel max_level = fribidi_log2vis(string, n_chars, &base_direction,
                                           nullptr, nullptr, nullptr,
                                           embedding_levels.data());
  (void) max_level; //shut up compiler about unused variables

  return {embedding_levels.begin(), embedding_levels.end()};
}

#endif
