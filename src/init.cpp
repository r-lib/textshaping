#include <cpp11/R.hpp>
#include "hb_shaper.h"

extern "C" void R_unload_textshaping2(DllInfo *dll) {
  unload_hb_shaper(dll);
}
