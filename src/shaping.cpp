#include "shaping.h"
#include <systemfonts.h>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

[[cpp11::register]]
void test() {
  std::string text("This is a test");

  hb_buffer_t *buf;
  buf = hb_buffer_create();
  hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);

  hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
  hb_buffer_set_language(buf, hb_language_from_string("en", -1));

  FT_Library ft_library;
  FT_Init_FreeType(&ft_library);
  FT_Face face;
  FT_New_Face(ft_library, "/System/Library/Fonts/Helvetica.ttc", 0, &face);
  FT_Set_Char_Size(face, 0, 1000, 0, 0);
  hb_font_t *font = hb_ft_font_create_referenced(face);

  hb_shape(font, buf, NULL, 0);

  unsigned int glyph_count;
  hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(buf, &glyph_count);
  hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

  for (int i = 0; i < glyph_count; ++i) {
    Rprintf("ID: %i, x_off: %f, y_off: %f, x_adv: %f, y_adv: %f\n",
            glyph_info[i].codepoint,
            glyph_pos[i].x_offset / 64.0,
            glyph_pos[i].y_offset / 64.0,
            glyph_pos[i].x_advance / 64.0,
            glyph_pos[i].y_advance / 64.0);
  }

  hb_buffer_destroy(buf);
  hb_font_destroy(font);

  FT_Done_Face(face);
  FT_Done_FreeType(ft_library);
}
