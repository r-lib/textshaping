#pragma once

#include <vector>
#include <cstdint>

std::vector<int> get_bidi_embeddings(const uint32_t* string, int n_chars);
