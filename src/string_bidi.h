#pragma once

#include <fribidi/fribidi.h>
#include <vector>

std::vector<int> get_bidi_embeddings(const uint32_t* string, int n_chars);
