#pragma once

#include <cpp11/R.hpp>
#include "string_shape.h"

HarfBuzzShaper& get_hb_shaper();

[[cpp11::init]]
void init_hb_shaper(DllInfo* dll);

void unload_hb_shaper(DllInfo *dll);
