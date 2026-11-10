#pragma once

#include <hb-ft.h>

// hb_ft_font_create_referenced() should always be available when harfbuzz is
// built with FreeType support, but some systems ship a libharfbuzz without it
// or with headers that don't match the library. configure link-tests the
// symbol and defines TEXTSHAPING_NO_HB_FT_REFERENCED when it is missing, in
// which case we replicate its semantics (the hb_font_t keeps the FT_Face
// alive) with the older two-argument hb_ft_font_create().
static inline hb_font_t* ts_hb_ft_font_create_referenced(FT_Face ft_face) {
#ifdef TEXTSHAPING_NO_HB_FT_REFERENCED
  FT_Reference_Face(ft_face);
  return hb_ft_font_create(ft_face, (hb_destroy_func_t) FT_Done_Face);
#else
  return hb_ft_font_create_referenced(ft_face);
#endif
}
