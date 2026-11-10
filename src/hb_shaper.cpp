#include "hb_shaper.h"

#ifdef NO_HARFBUZZ_FRIBIDI

void init_hb_shaper(DllInfo* dll) {

}

void unload_hb_shaper(DllInfo *dll) {

}

#else

static HarfBuzzShaper* hb_shaper = nullptr;

HarfBuzzShaper& get_hb_shaper() {
  // Recreate lazily so callers holding C-callable pointers don't hit a
  // use-after-free after a package unload/reload cycle
  if (hb_shaper == nullptr) {
    hb_shaper = new HarfBuzzShaper();
  }
  return *hb_shaper;
}

void init_hb_shaper(DllInfo* dll) {
  if (hb_shaper == nullptr) {
    hb_shaper = new HarfBuzzShaper();
  }
}

void unload_hb_shaper(DllInfo *dll) {
  delete hb_shaper;
  hb_shaper = nullptr;
}

#endif
