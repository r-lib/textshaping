#include "hb_shaper.h"

static HarfBuzzShaper* hb_shaper;

HarfBuzzShaper& get_hb_shaper() {
  return *hb_shaper;
}

void init_hb_shaper(DllInfo* dll) {
  hb_shaper = new HarfBuzzShaper();
}

void unload_hb_shaper(DllInfo *dll) {
  delete hb_shaper;
}
