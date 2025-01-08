//#include "hb_shaper.h"
//
//#ifndef NO_HARFBUZZ_FRIBIDI
//
//void init_hb_shaper(DllInfo* dll) {
//
//}
//
//void unload_hb_shaper(DllInfo *dll) {
//
//}
//
//#else
//
//static HarfBuzzShaper* hb_shaper;
//
//HarfBuzzShaper& get_hb_shaper() {
//  return *hb_shaper;
//}
//
//void init_hb_shaper(DllInfo* dll) {
//  hb_shaper = new HarfBuzzShaper();
//}
//
//void unload_hb_shaper(DllInfo *dll) {
//  delete hb_shaper;
//}
//
//#endif
