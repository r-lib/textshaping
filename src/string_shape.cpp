#ifndef NO_HARFBUZZ_FRIBIDI

#include <climits>
#include <cmath> // for std::abs
#include <iterator>
#include <list>
#include <vector>

#include "hb_ft_compat.h"
#include <ft2build.h>
#include FT_MULTIPLE_MASTERS_H
#include <hb-ot.h>
#include "string_shape.h"
#include "string_bidi.h"
#include <systemfonts.h>
#include <systemfonts-ft.h>
#include <algorithm>

// Does the codepoint belong to a script that can only be typeset vertically
// (UAX#50). Such scripts make direction = "auto" resolve to top-to-bottom
static inline bool char_is_required_vertical(uint32_t cp) {
  return (cp >= 0x1800 && cp <= 0x18AF) ||  // Mongolian (incl. Manchu, Sibe, etc.)
         (cp >= 0xA840 && cp <= 0xA87F);    // Phags-pa
}

// Vertical orientation lookups, generated from the UAX#50 data file
// (Vertical_Orientation.txt): vo_r_ranges is the R (rotated) class, i.e.
// scripts and characters that are never typeset upright, and vo_tr_ranges
// is the Tr (transformed) class, i.e. characters that require a typographic
// vertical glyph (brackets, quotes, dashes) and fall back to rotation
#include "vert_orientation.inc"

static inline bool in_ranges(const std::pair<uint32_t, uint32_t>* ranges, size_t n, uint32_t cp) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (cp < ranges[mid].first) {
      hi = mid;
    } else if (cp > ranges[mid].second) {
      lo = mid + 1;
    } else {
      return true;
    }
  }
  return false;
}

// Does the codepoint belong to the UAX#50 R class (never typeset upright).
// Inside vertical text these runs are either rotated 90 degrees or set
// upright with vert/vrt2 substitutions
static inline bool char_is_horizontal_only(uint32_t cp) {
  return in_ranges(vo_r_ranges, sizeof(vo_r_ranges) / sizeof(vo_r_ranges[0]), cp);
}

// Does the codepoint belong to the UAX#50 Tr class (requires a typographic
// vertical glyph via vert/vrt2, with rotated display as fallback)
static inline bool char_is_tr(uint32_t cp) {
  return in_ranges(vo_tr_ranges, sizeof(vo_tr_ranges) / sizeof(vo_tr_ranges[0]), cp);
}

// Does the codepoint have a strong RTL direction (Hebrew, Arabic, and
// friends). Used to give horizontal-only runs inside vertical text their own
// direction when they are rotated, since the bidi algorithm is bypassed
// there
static inline bool char_is_rtl_strong(uint32_t cp) {
  return (cp >= 0x0590 && cp <= 0x07FF) ||  // Hebrew, Arabic, Syriac, Thaana, NKo, Samaritan, Mandaic
         (cp >= 0x08A0 && cp <= 0x08FF) ||  // Arabic extended-A
         (cp >= 0xFB1D && cp <= 0xFB4F) ||  // Hebrew presentation forms
         (cp >= 0xFB50 && cp <= 0xFDFF) ||  // Arabic presentation forms-A
         (cp >= 0xFE70 && cp <= 0xFEFF);    // Arabic presentation forms-B
}

// JLREQ kinsoku shori: characters that may not start a line (closing
// brackets, sentence punctuation, prolonged sound marks, middle dots,
// iteration marks, small kana). When wrapping is forced these are allowed
// to hang past the line end instead
static inline bool char_is_line_start_prohibited(uint32_t cp) {
  switch (cp) {
  case 0x3001: case 0x3002:              // 、。
  case 0xFF0C: case 0xFF0E:              // ，．
  case 0x3009: case 0x300A:              // 〉》
  case 0x300D: case 0x300F:              // 」』
  case 0x3011: case 0x3015:              // 】〕
  case 0x3017: case 0x3019: case 0x301B: // 〗〙〛
  case 0xFF09: case 0xFF5D:              // ）｝
  case 0x30FC: case 0x30A0:              // ー゠
  case 0x2014: case 0x2015:              // —―
  case 0xFF01: case 0xFF1F:              // ！？
  case 0x30FB: case 0xFF1A: case 0xFF1B: // ・：；
  case 0x2025: case 0x2026:              // ‥…
  case 0x3005: case 0x303B:              // 々〻
  case 0x309D: case 0x309E:              // ゝゞ
  case 0x30FD: case 0x30FE:              // ヽヾ
  case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049: // ぁぃぅぇぉ
  case 0x3063:                          // っ
  case 0x3083: case 0x3085: case 0x3087: case 0x308E: // ゃゅょゎ
  case 0x3095: case 0x3096:              // ゕゖ
  case 0x30A1: case 0x30A3: case 0x30A5: case 0x30A7: case 0x30A9: // ァィゥェォ
  case 0x30C3:                          // ッ
  case 0x30E3: case 0x30E5: case 0x30E7: case 0x30EE: // ャュョヮ
  case 0x30F5: case 0x30F6:              // ヵヶ
    return true;
  }
  if (cp >= 0x31F0 && cp <= 0x31FF) return true; // katakana phonetic extensions (small)
  return false;
}

// Fullwidth punctuation whose trailing blank (the empty half of its em
// advance) may collapse at line end, per JLREQ line-end spacing rules
static inline bool char_is_trailing_collapsible(uint32_t cp) {
  switch (cp) {
  case 0x3001: case 0x3002:              // 、。
  case 0xFF0C: case 0xFF0E:              // ，．
  case 0xFF01: case 0xFF1F:              // ！？
  case 0xFF1A: case 0xFF1B:              // ：；
  case 0x30FB:                           // ・
  case 0x3009: case 0x300A:              // 〉》
  case 0x300D: case 0x300F:              // 」』
  case 0x3011: case 0x3015:              // 】〕
  case 0x3017: case 0x3019: case 0x301B: // 〗〙〛
  case 0xFF09: case 0xFF5D:              // ）｝
    return true;
  }
  return false;
}

// Characters that may not end a line (opening brackets). When wrapping is
// forced these are pulled down to the next line
static inline bool char_is_line_end_prohibited(uint32_t cp) {
  switch (cp) {
  case 0x3008: case 0x300A:              // 〈《
  case 0x300C: case 0x300E:              // 「『
  case 0x3010: case 0x3014:              // 【〔
  case 0x3016: case 0x3018: case 0x301A: // 〖〘〚
  case 0x301D:                           // 〝
  case 0xFF08: case 0xFF3B: case 0xFF5B: // （［｛
    return true;
  }
  return false;
}

// Does the font provide real vertical metrics (vhea table or vertical
// origins), i.e. can it be shaped upright in vertical text with proper
// advances, or must vertical presentation be faked by rotation
static bool face_has_vertical_metrics(hb_face_t* face) {
  hb_blob_t* blob = hb_face_reference_table(face, HB_TAG('v', 'h', 'e', 'a'));
  unsigned int len = hb_blob_get_length(blob);
  hb_blob_destroy(blob);
  if (len > 0) return true;
  blob = hb_face_reference_table(face, HB_TAG('V', 'O', 'R', 'G'));
  len = hb_blob_get_length(blob);
  hb_blob_destroy(blob);
  return len > 0;
}

// Does the font's GSUB table contain a given feature
static bool face_has_feature(hb_face_t* face, hb_tag_t tag) {
  unsigned int count = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GSUB, 0, 0, nullptr);
  if (count == 0) return false;
  std::vector<hb_tag_t> tags(count);
  hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GSUB, 0, &count, tags.data());
  for (unsigned int i = 0; i < count; ++i) {
    if (tags[i] == tag) return true;
  }
  return false;
}

// Does the font's GSUB table contain vert or vrt2 substitutions, i.e. can it
// set horizontal-script glyphs upright in vertical text
static bool face_has_vert_subs(hb_face_t* face) {
  return face_has_feature(face, HB_TAG('v', 'e', 'r', 't')) ||
         face_has_feature(face, HB_TAG('v', 'r', 't', '2'));
}

UTF_UCS HarfBuzzShaper::utf_converter = UTF_UCS();
LRU_Cache<BidiID, std::vector<int> > HarfBuzzShaper::bidi_cache = {1000};
LRU_Cache<ShapeID, ShapeInfo> HarfBuzzShaper::shape_cache = {1000};

// Clamp a double into int32 range; non-finite values become 0 (converting
// out-of-range doubles to int32 is undefined behaviour)
static inline int32_t clamp_i32(double v) {
  if (!std::isfinite(v)) return 0;
  if (v > (double) INT32_MAX) return INT32_MAX;
  if (v < (double) INT32_MIN) return INT32_MIN;
  return (int32_t) v;
}

FT_Face get_cached_or_new_face(const FontSettings2& font, double size, double res, int* error) {
  if (ft_compat) {
    return systemfonts::ver2::get_cached_face(font, size, res, error);
  }
  const char* fontfile = font.file;
  int index = font.index;
  FT_Face new_face = nullptr;
  FT_Error err = FT_New_Face(ft.library, fontfile, index, &new_face);
  if (err != 0) {
    err = FT_New_Face(ft.library, fontfile, 0, &new_face);
    if (err != 0) {
      *error = err;
      return nullptr;
    }
  }

  if (FT_IS_SCALABLE(new_face)) {
    err = FT_Set_Char_Size(new_face, 0, size * 64, res, res);
    if (err != 0) {
      *error = err;
      FT_Done_Face(new_face);
      return nullptr;
    }
  } else {
    if (new_face->num_fixed_sizes == 0) {
      *error = 23;
      FT_Done_Face(new_face);
      return nullptr;
    }
    int best_match = 0;
    int diff = 1e6;
    int scaled_size = 64 * size * res / 72;
    int largest_size = 0;
    int largest_ind = -1;
    bool found_match = false;
    for (int i = 0; i < new_face->num_fixed_sizes; ++i) {
      if (new_face->available_sizes[i].size > largest_size) {
        largest_size = new_face->available_sizes[i].size;
        largest_ind = i;
      }
      int ndiff = new_face->available_sizes[i].size - scaled_size;
      if (ndiff >= 0 && ndiff < diff) {
        best_match = i;
        diff = ndiff;
        found_match = true;
      }
    }
    if (!found_match && scaled_size >= largest_size) {
      best_match = largest_ind;
    }

    err = FT_Select_Size(new_face, best_match);
    if (err != 0) {
      *error = err;
      FT_Done_Face(new_face);
      return nullptr;
    }
  }
  if (font.n_axes > 0 && font.axes != nullptr && font.coords != nullptr) {
    FT_MM_Var* variations = nullptr;
    if (FT_Get_MM_Var(new_face, &variations) == 0 && variations != nullptr) {
      std::vector<FT_Fixed> coords(variations->num_axis);
      for (FT_UInt i = 0; i < variations->num_axis; ++i) {
        coords[i] = variations->axis[i].def;
      }
      for (int i = 0; i < font.n_axes; ++i) {
        FT_ULong tag = (FT_ULong) font.axes[i];
        for (FT_UInt j = 0; j < variations->num_axis; ++j) {
          if (variations->axis[j].tag == tag) {
            coords[j] = (FT_Fixed) font.coords[i];
            break;
          }
        }
      }
      FT_Set_Var_Design_Coordinates(new_face, coords.size(), coords.data());
      FT_Done_MM_Var(ft.library, variations);
    }
  }
  return new_face;
}

bool HarfBuzzShaper::shape_string(const char* string, FontSettings2& font_info,
                                  double size, double res, double lineheight,
                                  int align, double hjust, double vjust, double width,
                                  double tracking, double ind, double hang, double before,
                                  double after, bool spacer, int direction, int mixed,
                                  std::vector<int>& wrap) {
  reset();

  // Set global settings
  if (width < 0 || !std::isfinite(width)) {
    max_width = -1; // No width limit
  } else {
    max_width = clamp_i32(width + 1); // +1 to prevent rounding errors
  }
  indent = clamp_i32(ind);
  hanging = clamp_i32(hang);
  space_before = clamp_i32(before);
  space_after = clamp_i32(after);

  cur_res = res;
  cur_lineheight = lineheight;
  cur_align = align;
  cur_hjust = hjust;
  cur_vjust = vjust;

  dir = direction;
  cur_mixed = mixed;
  // The R-level API allows auto-detection of vertical text; the
  // graphics-device C API never reaches this function
  vertical_allowed = true;

  return add_string(string, font_info, size, tracking, spacer, wrap);
}

bool HarfBuzzShaper::add_string(const char* string, FontSettings2& font_info,
                                double size, double tracking, bool spacer,
                                std::vector<int>& wrap) {
  if (spacer) {
    return add_spacer(font_info, size, tracking);
  }

  size_t run_start = full_string.size();

  // Convert string to UTC and add it to the global string
  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  if (n_chars == 0) {
    // Empty run - we treat is as a 0-width spacer to capture the font ascend and descend
    return add_spacer(font_info, size, 0, EMPTY_CHAR);
  }

  full_string.insert(full_string.end(), utc_string, utc_string + n_chars);

  size_t run_end = full_string.size();

  unsigned int index = shape_infos.size();

  // Add precalculated soft and hard break points to the sets
  for (auto iter = wrap.begin(); iter != wrap.end(); ++iter) {
    // Guard against out-of-range positions (e.g. NA) coming from R
    if (*iter < 1 || (size_t)(run_start + *iter) > full_string.size()) continue;
    int break_pos = run_start + (*iter) - 1;
    if (glyph_is_linebreak(full_string[break_pos])) {
      hard_break.insert(break_pos);
    } else {
      soft_break.insert(break_pos);
    }
  }

  // Add run info to list
  shape_infos.emplace_back(run_start, run_end, font_info, index, size, cur_res, tracking);

  return true;
}

bool HarfBuzzShaper::add_spacer(FontSettings2& font_info, double height, double width, uint32_t filler) {
  width *= 64.0 / 72.0;
  int32_t ascend = clamp_i32(height * 64.0 * cur_res / 72.0);
  int32_t descend = 0;
#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
#else
  int error = 0;
  FT_Face face = get_cached_or_new_face(font_info, height, cur_res, &error);
  if (error != 0) {
    Rprintf("Failed to get face: %s, %i\n", font_info.file, font_info.index);
    error_code = error;
  } else {
    hb_font_t *font = ts_hb_ft_font_create_referenced(face);
    hb_font_extents_t fextent;
    hb_font_get_h_extents(font, &fextent);
    ascend = fextent.ascender;
    descend = fextent.descender;
    hb_font_destroy(font);
    FT_Done_Face(face);
  }
#endif
  FontSettings2 dummy_font;
  ShapeInfo info;
  info.index = shape_infos.size();
  info.run_start = info.run_end = full_string.size();

  // A spacer is encoded as an "unknown glyph" with ascend giving the height and
  //  width/x-advance giving the width
  info.font_info = font_info;
  // EmbedInfo is not an aggregate under C++11 (it has default member
  // initializers), so it must be built field by field
  EmbedInfo spacer_embed;
  spacer_embed.glyph_id.push_back(filler);
  spacer_embed.glyph_cluster.push_back(0);
  spacer_embed.string_id.push_back(info.index);
  spacer_embed.x_advance.push_back(clamp_i32(width));
  spacer_embed.y_advance.push_back(0);
  spacer_embed.x_offset.push_back(0);
  spacer_embed.y_offset.push_back(0);
  spacer_embed.x_bear.push_back(0);
  spacer_embed.y_bear.push_back(ascend);
  spacer_embed.width.push_back(clamp_i32(width));
  spacer_embed.height.push_back(ascend - descend);
  spacer_embed.em_width.push_back(clamp_i32(width));
  spacer_embed.ascenders.push_back(ascend);
  spacer_embed.descenders.push_back(descend);
  spacer_embed.rotation.push_back(0.0);
  spacer_embed.is_blank.push_back(false);
  spacer_embed.may_break.push_back(false);
  spacer_embed.may_stretch.push_back(false);
  spacer_embed.font.push_back(0);
  spacer_embed.fallbacks.push_back(dummy_font);
  spacer_embed.fallback_size.push_back(height);
  spacer_embed.fallback_scaling.push_back(-1);
  // embedding_level = 0 (ltr)
  spacer_embed.full_width = clamp_i32(width);
  // Not a line breaker in itself
  spacer_embed.terminates_paragraph = false;
  info.embeddings.push_back(spacer_embed);
  shape_infos.push_back(info);
  return true;
}

bool HarfBuzzShaper::finish_string() {
  if (shape_infos.empty()) {
    return true;
  }

  pen_x = 0;
  pen_y = -space_before;
  int32_t cur_line_indent = indent;
  int32_t previous_line_descend = 0;
  int32_t line_ascend = 0;
  bool hard_break = false;
  int32_t break_height = 0;
  int32_t line_left_extra = 0;
  std::list<EmbedInfo> line;

  auto final_embeddings = combine_embeddings(shape_infos, dir);
  bool ltr = dir != 2;
  // If alignment depends on direction update the alignment now
  if (cur_align == 7) cur_align = ltr ? 0 : 2;
  if (cur_align == 8) cur_align = ltr ? 3 : 5;

  // Lay out one line at a time
  while (!final_embeddings.empty()) {
    // Retrieve the next line from the embedding list
    line = get_next_line_at_width(max_width - cur_line_indent, final_embeddings, hard_break, break_height);
    bool first_char = true;

    // If ltr indent goes to the left
    if (ltr) pen_x += cur_line_indent;

    // Initialise line stats
    line_width.push_back(pen_x);
    line_left_bear.push_back(0);
    line_right_bear.push_back(0);
    line_must_break.push_back(hard_break);
    size_t line_start_index = x_pos.size();
    line_left_extra = 0;
    uint32_t last_cp = 0;
    int32_t lead_collapse = 0;

    for (auto iter = line.begin(); iter != line.end(); ++iter) {
      for (size_t i = 0; i < iter->glyph_id.size(); ++i) {
        // If rtl we need to treat preceding blank glyphs like we treat terminal
        // blank glyphs in ltr. We record how long we are in the line and move it
        // all backwards once we hit the first non-blank
        if (first_char && !ltr) {
          if (iter->is_blank[i]) {
            line_left_extra = pen_x;
          } else {
            for (size_t j = line_start_index; j < x_pos.size(); ++j) {
              x_pos[j] -= line_left_extra;
            }
            pen_x = 0;
          }
        }
        // Collapse the leading blank of an opening bracket at line start
        // (JLREQ): the empty half of the em advance hangs outside the line
        // box and the whole line shifts towards the edge
        if (first_char && !iter->is_blank[i] && ltr) {
          int32_t cluster = iter->glyph_cluster[i];
          uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
          if (char_is_line_end_prohibited(cp)) {
            lead_collapse = iter->x_offset[i] + iter->x_bear[i];
            if (lead_collapse > 0) {
              pen_x -= lead_collapse;
            } else {
              lead_collapse = 0;
            }
          }
        }
        // Collapse the leading blank of an opening bracket following
        // closing punctuation (JLREQ): the facing half-em blanks overlap,
        // leaving a single half em of separation
        if (!first_char && !iter->is_blank[i] && ltr && last_cp != 0 &&
            char_is_trailing_collapsible(last_cp)) {
          int32_t cluster = iter->glyph_cluster[i];
          uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
          if (char_is_line_end_prohibited(cp)) {
            int32_t lead = iter->x_offset[i] + iter->x_bear[i];
            if (lead > 0) pen_x -= lead;
          }
        }
        if (iter->glyph_id[i] != EMPTY_CHAR) { // Avoid adding made up glyph info for empty text runs
          glyph_id.push_back(iter->glyph_id[i]);
          glyph_cluster.push_back(iter->glyph_cluster[i]);
          fontfile.push_back(iter->fallbacks[iter->font[i]].file);
          fontindex.push_back(iter->fallbacks[iter->font[i]].index);
          fontsize.push_back(iter->fallback_size[iter->font[i]]);
          advance.push_back(iter->x_advance[i]);
          ascender.push_back(iter->ascenders[i]);
          descender.push_back(iter->descenders[i]);
          rotation.push_back(iter->rotation[i]);
          x_bear.push_back(iter->x_bear[i]);
          y_bear.push_back(iter->y_bear[i]);
          glyph_width.push_back(iter->width[i]);
          em_width.push_back(iter->em_width[i]);

          string_id.push_back(iter->string_id[i]);
          line_id.push_back(line_width.size() - 1);
          x_pos.push_back(pen_x + iter->x_offset[i]);

          may_stretch.push_back(iter->may_stretch[i]);
        }


        line_ascend = std::max(line_ascend, iter->ascenders[i]);

        // Only needed for first line
        if (line_width.size() == 1) {
          top_bearing = std::max(top_bearing, iter->y_offset[i] + iter->y_bear[i]);
        }
        // Only needed for last line
        if (final_embeddings.empty()) {
          bottom_bearing = std::min(bottom_bearing, iter->y_offset[i] + iter->height[i] + iter->y_bear[i]);
        }

        // If first character record left bearing
        if (first_char) {
          line_left_bear.back() = iter->x_offset[i] + iter->x_bear[i] - lead_collapse;
        }

        // Advance pen to next position
        pen_x += iter->x_advance[i];

        // Continuously update this info
        if (!iter->is_blank[i]) {
          line_right_bear.back() = iter->x_advance[i] - iter->x_offset[i] - iter->x_bear[i] - iter->width[i];
          line_width.back() = pen_x;
          int32_t cluster = iter->glyph_cluster[i];
          last_cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
        }

        // Set to false at the first non-blank glyph
        if (!iter->is_blank[i]) first_char = false;
      }
    }
    // We now know the height of the line. Update pen_y and record y_pos
    // Also update max_descend for next line to use
    pen_y -= (line_ascend - previous_line_descend) * (line_width.size() == 1 ? 1 : cur_lineheight);
    for (auto iter = line.begin(); iter != line.end(); ++iter) {
      for (size_t i = 0; i < iter->glyph_id.size(); ++i) {
        if (iter->glyph_id[i] != EMPTY_CHAR) { // Avoid adding made up glyph info for empty text runs
          y_pos.push_back(pen_y + iter->y_offset[i]);
        }
        previous_line_descend = std::min(previous_line_descend, iter->descenders[i]);
      }
    }

    // If first line, normalise top_bearing to 0
    if (line_width.size() == 1) top_bearing = -pen_y - top_bearing;

    // If rtl indent goes to the right
    if (!ltr) {
      line_width.back() = pen_x + cur_line_indent;
    }

    // Collapse the trailing blank of fullwidth punctuation at line end
    // (JLREQ): the empty half of the em advance hangs outside the line box,
    // so alignment and justification use the ink edge
    if (ltr && last_cp != 0 && char_is_trailing_collapsible(last_cp) && line_right_bear.back() > 0) {
      line_width.back() -= line_right_bear.back();
      line_right_bear.back() = 0;
    }

    // Reset pen to next line
    if (!final_embeddings.empty() || hard_break) {
      cur_line_indent = hard_break ? indent : hanging;
      pen_x = 0;
      line_ascend = 0;
      if (hard_break) pen_y -= space_after + space_before;
    }
  }

  if (hard_break) {
    // Last line ended with a line break. We move pen_y down based on last glyph size
    int32_t line_height = break_height * cur_lineheight;
    size_t n_last_glyphs = line.back().ascenders.size();
    if (n_last_glyphs > 0) {
      size_t last_glyph = line.back().embedding_level % 2 == 0 ? n_last_glyphs - 1 : 0;
      line_height = (line.back().ascenders[last_glyph] - line.back().descenders[last_glyph]) * cur_lineheight;
    }
    pen_y -= line_height;
    bottom_bearing += line_height;
    pen_x = indent;
    line_left_extra = 0;
    line_width.push_back(pen_x);
  }

  // If rtl, the pen is placed at the left
  if (!ltr) pen_x = -line_left_extra;

  if (line_width.empty()) {
    // Shaping produced no glyphs at all (e.g. all runs failed to shape)
    line_width.push_back(pen_x);
    line_left_bear.push_back(0);
    line_right_bear.push_back(0);
  }

  // Calculate overall box stats
  bottom_bearing += space_after - previous_line_descend;
  int32_t bottom = pen_y + previous_line_descend - space_after;
  int max_width_ind = std::max_element(line_width.begin(), line_width.end()) - line_width.begin();
  width = max_width < 0 ? line_width[max_width_ind] : max_width;
  height = -bottom;

  do_alignment(ltr);

  // Figure out additional space to left or right to add to bearing
  double width_diff = width - line_width[max_width_ind];
  if (cur_align == 1) {
    width_diff /= 2;
  }
  left_bearing = cur_align == 0 || cur_align == 3 || cur_align == 5 ? *std::min_element(line_left_bear.begin(), line_left_bear.end()) : line_left_bear[max_width_ind] + width_diff;
  right_bearing = cur_align == 2 || cur_align == 4 || cur_align == 5 ? *std::min_element(line_right_bear.begin(), line_right_bear.end()) : line_right_bear[max_width_ind] + width_diff;

  // Move glyphs and borders according to overall box justification
  left_border = - cur_hjust * width;
  pen_x += left_border;
  for (size_t i = 0; i < x_pos.size(); ++i) {
    x_pos[i] += left_border;
  }
  for (size_t i = 0; i < x_pos.size(); ++i) {
    y_pos[i] += - bottom - cur_vjust * height;
  }
  top_border += - bottom - cur_vjust * height;
  pen_y += - bottom - cur_vjust * height;

  // For vertical text everything has been laid out in a transformed space
  // where the main (vertical) axis is x and columns stack along -y. Rotate
  // all positions and metrics into real space: ttb maps (x, y) -> (y, -x) so
  // columns progress right-to-left, btt maps (x, y) -> (-y, x) so columns
  // progress left-to-right
  if (dir == 3 || dir == 4) {
    bool ttb = dir == 3;
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int32_t tx = x_pos[i];
      int32_t ty = y_pos[i];
      if (ttb) {
        x_pos[i] = ty;
        y_pos[i] = -tx;
      } else {
        x_pos[i] = -ty;
        y_pos[i] = tx;
        // The btt mapping mirrors the cross axis while the glyphs stay
        // upright, so the origin of upright glyphs must be moved to mirror
        // the em box rather than the ink box — this keeps each glyph's
        // designed placement within its em box consistent between ttb and
        // btt. The main axis correction accounts for the bearings flipping
        // around the origin. Rotated runs need no correction: their +90
        // rotation flips the reference edge the same way
        if (rotation[i] == 0.0) {
          x_pos[i] -= em_width[i];
          y_pos[i] += 2 * x_bear[i] + glyph_width[i];
        }
      }
    }
    int32_t t_pen_x = pen_x;
    int32_t t_pen_y = pen_y;
    pen_x = ttb ? t_pen_y : -t_pen_y;
    pen_y = ttb ? -t_pen_x : t_pen_x;
    if (!ttb && !em_width.empty() && rotation.back() == 0.0) {
      // Move the pen's cross-axis position like the origin of a hypothetical
      // next glyph so it stays consistent with the corrected upright glyph
      // positions. The main-axis coordinate is an advance position, not an
      // origin, and maps point-wise. Rotated runs need no correction, as in
      // the glyph loop above
      pen_x -= em_width.back();
    }

    int32_t t_width = width;
    int32_t t_height = height;
    int32_t t_left_border = left_border;
    int32_t t_top_border = top_border;
    int32_t t_left_bearing = left_bearing;
    int32_t t_right_bearing = right_bearing;
    int32_t t_top_bearing = top_bearing;
    int32_t t_bottom_bearing = bottom_bearing;

    width = t_height;
    height = t_width;
    if (ttb) {
      // real x = t y, so the real left edge is the t-space bottom edge and
      // the real right edge is the t-space top edge; real y = -t x, so the
      // real top edge is the t-space left edge
      left_border = t_top_border - t_height;
      top_border = -t_left_border;
      left_bearing = t_bottom_bearing;
      right_bearing = t_top_bearing;
      top_bearing = t_left_bearing;
      bottom_bearing = t_right_bearing;
    } else {
      left_border = -t_top_border;
      top_border = t_left_border + t_width;
      left_bearing = t_top_bearing;
      right_bearing = t_bottom_bearing;
      top_bearing = t_right_bearing;
      bottom_bearing = t_left_bearing;
    }
  }

  // Propagate any font/shaping failure recorded while building this
  // paragraph instead of silently returning partial output
  return error_code == 0;
}

void HarfBuzzShaper::reset() {
  full_string.clear();
  bidi_embedding.clear();
  vert_class.clear();
  vertical_allowed = false;
  cur_mixed = 0;
  emoji_flag.clear();
  glyph_id.clear();
  glyph_cluster.clear();
  fontfile.clear();
  fontindex.clear();
  fontsize.clear();
  string_id.clear();
  x_pos.clear();
  y_pos.clear();
  advance.clear();
  ascender.clear();
  descender.clear();
  rotation.clear();
  x_bear.clear();
  y_bear.clear();
  glyph_width.clear();
  em_width.clear();
  line_left_bear.clear();
  line_right_bear.clear();
  line_width.clear();
  line_id.clear();
  line_must_break.clear();
  may_stretch.clear();
  shape_infos.clear();
  soft_break.clear();
  hard_break.clear();

  pen_x = 0;
  pen_y = 0;

  top = 0;
  bottom = 0;
  ascend = 0;
  descend = 0;

  left_bearing = 0;
  right_bearing = 0;
  top_bearing = 0;
  bottom_bearing = 0;
  width = 0;
  height = 0;
  top_border = 0;
  left_border = 0;

  cur_string = 0;

  error_code = 0;
  dir = 0;

  cur_lineheight = 0.0;
  cur_align = 0;
  cur_hjust = 0.0;
  cur_vjust = 0.0;
  cur_res = 0.0;
  top = 0;
  bottom = 0;
  max_width = 0;
  indent = 0;
  hanging = 0;
  space_before = 0;
  space_after = 0;
}

std::list<EmbedInfo> HarfBuzzShaper::combine_embeddings(std::vector<ShapeInfo>& shapes, int& direction) {
  // Auto direction detection: strings containing required-vertical scripts
  // (e.g. Mongolian) resolve to top-to-bottom; scripts that can go both ways
  // (e.g. Japanese) stay horizontal unless the user asks for vertical
  if (direction == 0 && vertical_allowed) {
    for (size_t i = 0; i < full_string.size(); ++i) {
      if (char_is_required_vertical(full_string[i])) {
        direction = 3;
        break;
      }
    }
  }

  bool vertical = direction == 3 || direction == 4;

  // Find bidi embeddings and determine the overall direction of the text
  if (vertical) {
    // Vertical text is not passed through the bidi algorithm. All embeddings
    // stay at level 0 and runs are instead itemized by whether their script
    // is horizontal-only
    bidi_embedding.assign(full_string.size(), 0);
    vert_class.resize(full_string.size());
    if (cur_mixed == 4) {
      // Sideways: everything is shaped horizontally as a single run and
      // rotated into the column, including scripts that would normally be
      // upright (text-orientation: sideways)
      std::fill(vert_class.begin(), vert_class.end(), 1);
    } else {
      for (size_t i = 0; i < full_string.size(); ++i) {
        if (char_is_horizontal_only(full_string[i])) {
          vert_class[i] = 1;
        } else if (char_is_tr(full_string[i])) {
          // UAX#50 Tr (brackets, quotes, dashes): upright via vert when the
          // font provides it, rotated like R otherwise
          vert_class[i] = 3;
        } else {
          vert_class[i] = 0;
        }
      }
    }
    if (cur_mixed == 2 || cur_mixed == 3) {
      // Tate-chu-yoko: maximal runs of up to 4 characters are set
      // horizontally, upright in the column. "tate" combines ASCII digit
      // runs only, "combine" any horizontal-only run (text-combine-upright:
      // all). Longer runs stay rotated
      size_t n = full_string.size();
      size_t i = 0;
      while (i < n) {
        bool combinable = cur_mixed == 3
          ? vert_class[i] == 1
          : (full_string[i] >= 0x30 && full_string[i] <= 0x39);
        if (combinable) {
          size_t j = i;
          if (cur_mixed == 3) {
            while (j < n && vert_class[j] == 1) ++j;
          } else {
            while (j < n && full_string[j] >= 0x30 && full_string[j] <= 0x39) ++j;
          }
          if (j - i <= 4) {
            std::fill(vert_class.begin() + i, vert_class.begin() + j, 2);
          }
          i = j;
        } else {
          ++i;
        }
      }
    }
  } else if (!full_string.empty()) {
    // If we have more than one char we find bidi embeddings
    // We append the direction to the end in the cache so we can read it back
    BidiID key;
    key.string_hash = vector_hash(full_string.begin(), full_string.end());
    key.direction = direction;
    key.string = full_string;
    if (!bidi_cache.get(key, bidi_embedding)) {
      bidi_embedding = get_bidi_embeddings(full_string, direction);
      bidi_embedding.push_back(direction);
      bidi_cache.add(key, bidi_embedding);
      bidi_embedding.pop_back();
    } else {
      direction = bidi_embedding.back();
      bidi_embedding.pop_back();
    }
  } else {
    bidi_embedding = std::vector<int>(full_string.size(), 0);
  }

  // If size < 2 we haven't done any guessing and we assume ltr (it doesn't matter anyway)
  bool ltr = direction != 2;

  // Shape all embeddings and collect them in a single vector
  std::list<EmbedInfo> all_embeddings;
  unsigned int i = 0;
  for (auto iter = shapes.begin(); iter != shapes.end(); ++iter) {
    if (iter->embeddings.empty()) { // avoid shaping spacers
      shape_text_run(*iter, ltr);
    } else {
      // We adopt the embedding level of the prior embedding for spacers
      int level = all_embeddings.empty() ? (ltr ? 0 : 1) : all_embeddings.back().embedding_level;
      iter->embeddings[0].embedding_level = level;
    }
    iter->add_index(i++);
    all_embeddings.insert(all_embeddings.end(), std::make_move_iterator(iter->embeddings.begin()), std::make_move_iterator(iter->embeddings.end()));
    iter->embeddings.clear();
  }

  // Shortcut for simplest case
  if (all_embeddings.size() <= 1) return all_embeddings;

  // Reverse ordering of consecutive embeddings in rtl
  // This is needed to keep their internal order during shaping
  // Further collapses all consequtive runs into one embedding to simplify the final shaping operation
  // rtl runs are merged in reverse order rather than reversing the list first,
  // so a paragraph-terminating embedding stays the last one merged
  auto run_start = all_embeddings.begin();
  int run_embed_level = run_start->embedding_level;
  std::list<EmbedInfo> final_embeddings;
  for (auto iter = std::next(run_start); iter != all_embeddings.end(); ++iter) {
    int cur_embed_level = iter->embedding_level;

    if (std::prev(iter)->terminates_paragraph || run_embed_level != cur_embed_level) {
      if (run_embed_level % 2 != 0) {
        EmbedInfo merged;
        merged.embedding_level = run_embed_level;
        auto rev = iter;
        do {
          --rev;
          merged.add(*rev, false);
        } while (rev != run_start);
        merged.terminates_paragraph = std::prev(iter)->terminates_paragraph;
        final_embeddings.push_back(std::move(merged));
      } else {
        for (auto iter2 = std::next(run_start); iter2 != iter; ++iter2) {
          run_start->add(*iter2);
        }
        final_embeddings.push_back(std::move(*run_start));
      }
      run_embed_level = cur_embed_level;
      run_start = iter;
    }
  }
  if (run_embed_level % 2 != 0) {
    EmbedInfo merged;
    merged.embedding_level = run_embed_level;
    auto rev = all_embeddings.end();
    do {
      --rev;
      merged.add(*rev, false);
    } while (rev != run_start);
    merged.terminates_paragraph = all_embeddings.back().terminates_paragraph;
    final_embeddings.push_back(std::move(merged));
  } else {
    for (auto iter2 = std::next(run_start); iter2 != all_embeddings.end(); ++iter2) {
      run_start->add(*iter2);
    }
    final_embeddings.push_back(std::move(*run_start));
  }

  return final_embeddings;
}

void HarfBuzzShaper::shape_text_run(ShapeInfo &text_run, bool ltr) {
  int n_features = text_run.font_info.n_features;
  int n_axes = text_run.font_info.n_axes;
  std::vector<hb_feature_t> features(n_features);
  ShapeID run_id;
  if (n_features == 0 && n_axes == 0) {
    // No features. This is a simple string and if we have already seen it it may be in the cache
    run_id.string_hash = vector_hash(full_string.begin() + text_run.run_start, full_string.begin() + text_run.run_end);
    run_id.embed_hash = vert_class.empty()
      ? vector_hash(bidi_embedding.begin() + text_run.run_start, bidi_embedding.begin() + text_run.run_end)
      : vector_hash(vert_class.begin() + text_run.run_start, vert_class.begin() + text_run.run_end);
    run_id.font.assign(text_run.font_info.file);
    run_id.index = text_run.font_info.index;
    run_id.size = text_run.size * text_run.res;
    run_id.font_size = text_run.size;
    run_id.tracking = text_run.tracking;
    run_id.direction = dir;
    run_id.mixed = cur_mixed;
    run_id.string.assign(full_string.begin() + text_run.run_start, full_string.begin() + text_run.run_end);
    if (vert_class.empty()) {
      run_id.embedding.assign(bidi_embedding.begin() + text_run.run_start, bidi_embedding.begin() + text_run.run_end);
    } else {
      run_id.embedding.assign(vert_class.begin() + text_run.run_start, vert_class.begin() + text_run.run_end);
    }
    size_t run_start = text_run.run_start;
    size_t run_end = text_run.run_end;
    if (shape_cache.get(run_id, text_run)) {
      // Clusters are stored relative to the run start. Offset them to this
      // paragraph and recompute break info, which depends on paragraph-level
      // soft/hard break positions
      for (auto iter = text_run.embeddings.begin(); iter != text_run.embeddings.end(); ++iter) {
        for (auto cl = iter->glyph_cluster.begin(); cl != iter->glyph_cluster.end(); ++cl) {
          *cl += run_start;
        }
        iter->is_blank.clear();
        iter->may_break.clear();
        iter->may_stretch.clear();
        fill_glyph_info(*iter);
      }
      text_run.run_start = run_start;
      text_run.run_end = run_end;
      return;
    }
  } else {
    // There are features. We parse them into the correct format
    for (int i = 0; i < n_features; ++i) {
      const char* tag = text_run.font_info.features[i].feature;
      features[i].tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
      features[i].value = text_run.font_info.features[i].setting;
      features[i].start = 0;
      features[i].end = -1;
    }
  }

  // Structures to keep font info in
  std::vector<FontSettings2> fallback = {text_run.font_info};
  std::vector<double> fallback_size;
  std::vector<double> fallback_scaling;

  // Get scaling and sizing info for the default font
  if (!get_font_sizing(fallback.back(), text_run.size, text_run.res, fallback_size, fallback_scaling, true)) {
    return;
  }

  size_t n_chars = text_run.run_end - text_run.run_start;

  // The emoji flag must cover the whole paragraph for the embedding split
  // loop below, even when this run contains no emoji
  if (emoji_flag.size() < full_string.size()) {
    emoji_flag.resize(full_string.size(), 0);
  }

  if (n_chars == 0) {
    // Empty string. We record the sizing of the font for use when calculating line height
    EmbedInfo embedding;
    embedding.ascenders.push_back(0);
    embedding.descenders.push_back(0);
#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
#else
    int error = 0;
    FT_Face face = get_cached_or_new_face(text_run.font_info, text_run.size, text_run.res, &error);
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", text_run.font_info.file, text_run.font_info.index);
      error_code = error;
    } else {
      hb_font_t *font = ts_hb_ft_font_create_referenced(face);
      hb_font_extents_t fextent;
      hb_font_get_h_extents(font, &fextent);
      embedding.ascenders[0] = fextent.ascender;
      embedding.descenders[0] = fextent.descender;
      hb_font_destroy(font);
      FT_Done_Face(face);
    }
#endif
    text_run.embeddings.push_back(embedding);
    return;
  }

  // Heuristic check to see if the string might contain emoji chars
  bool may_have_emoji = false;
  for (int i = text_run.run_start; i < text_run.run_end; ++i) {
    if (full_string[i] >= 8205) {
      may_have_emoji = true;
      break;
    }
  }

  if (may_have_emoji) {
    // If it may have emojis go through string and detect any
    // These are recorded in a separate flag since the embedding level can't
    // encode it (negating level 0 is a no-op)
    // This depends on the font so we can't cache this as part of the embedding
    std::vector<int> emoji_embeddings = {};
    emoji_embeddings.resize(n_chars);
    systemfonts::ver2::detect_emoji_embedding(full_string.data() + text_run.run_start, n_chars, emoji_embeddings.data(), text_run.font_info);
    bool emoji_font_added = false;
    for (int i = 0; i < n_chars; ++i) {
      // Older systemfonts versions only know 0/1 so a hard emoji (1) must
      // keep its meaning. 2 marks soft (text-presentation-default) emoji that
      // should only use the emoji font as a last resort
      if (emoji_embeddings[i] != 0) {
        emoji_flag[text_run.run_start + i] = emoji_embeddings[i];
        if (!emoji_font_added) {
          // Add the system emoji font to fallbacks
          fallback.push_back(systemfonts::ver2::locate_font("emoji", 0, 0, 0, nullptr, nullptr, 0));
          if (!get_font_sizing(fallback.back(), text_run.size, text_run.res, fallback_size, fallback_scaling, true)) {
            return;
          }
          emoji_font_added = true;
        }
      }
    }
  }

  size_t embedding_start = text_run.run_start;
  for (size_t i = text_run.run_start + 1; i <= text_run.run_end; ++i) {
    // Shape all embeddings separately
    bool terminal = glyph_must_hard_break(i - 1);
    if (terminal || i == text_run.run_end || bidi_embedding[i] != bidi_embedding[i - 1] ||
        emoji_flag[i] != emoji_flag[i - 1] ||
        (!vert_class.empty() && vert_class[i] != vert_class[i - 1])) {
      bool success = shape_embedding(
        embedding_start,
        i,
        features,
        bidi_embedding[embedding_start],
        emoji_flag[embedding_start],
        text_run,
        fallback,
        fallback_size,
        fallback_scaling
      );
      if (!success) return;
      if (!text_run.embeddings.empty() && terminal) {
        EmbedInfo& emb = text_run.embeddings.back();
        emb.terminates_paragraph = terminal;
        // The line-break glyph is last in ltr and first in rtl
        emb.full_width -= emb.embedding_level % 2 == 0 ? emb.x_advance.back() : emb.x_advance.front();
      }
      embedding_start = i;
    }
  }

  if (n_features == 0 && n_axes == 0) {
    // If simply shape add to cache
    // Store a copy with clusters relative to the run start so the entry can
    // be reused when the same run appears at a different paragraph position
    ShapeInfo cache_copy = text_run;
    for (auto iter = cache_copy.embeddings.begin(); iter != cache_copy.embeddings.end(); ++iter) {
      for (auto cl = iter->glyph_cluster.begin(); cl != iter->glyph_cluster.end(); ++cl) {
        *cl -= text_run.run_start;
      }
    }
    shape_cache.add(run_id, std::move(cache_copy));
  }
  //FT_Done_Face(face);
  return;
}


EmbedInfo HarfBuzzShaper::shape_single_line(const char* string, FontSettings2& font_info, double size, double res,
                                            int direction, int mixed, bool allow_vertical) {
  // Fast version when we know we don't need word wrap and alignment
  // Used by graphics devices

  reset();

  dir = direction;
  cur_mixed = mixed;
  vertical_allowed = allow_vertical;

  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  full_string = {utc_string, utc_string + n_chars};

  std::vector<ShapeInfo> shapes = {ShapeInfo(0, full_string.size(), font_info, 0, size, res, 0)};
  if (shapes.empty()) return EmbedInfo();
  auto final_embeddings = combine_embeddings(shapes, dir);

  if (final_embeddings.empty()) return EmbedInfo();

  rearrange_embeddings(final_embeddings);

  // Combine all embeddings into one
  for (auto iter = std::next(final_embeddings.begin()); iter != final_embeddings.end(); ++iter) {
    final_embeddings.front().add(*iter, false);
  }
  return final_embeddings.front();
}

bool HarfBuzzShaper::shape_embedding(unsigned int start, unsigned int end,
                                     std::vector<hb_feature_t>& features,
                                     int dir, int emoji, ShapeInfo& shape_info,
                                     std::vector<FontSettings2>& fallbacks,
                                     std::vector<double>& fallback_sizes,
                                     std::vector<double>& fallback_scales) {
  unsigned int embedding_size = end - start;

  if (embedding_size < 1) {
    return true;
  }

  int error = 0;
  // Load main font (emoji font if flagged as hard emoji)
  // Shouldn't be able to fail as we have already tried to load it in the calling function
  FT_Face face = get_cached_or_new_face(
    fallbacks[emoji == 1 ? 1 : 0],
    shape_info.size,
    shape_info.res,
    &error
  );
  if (error) {
    return false;
  }

  hb_font_t *font = ts_hb_ft_font_create_referenced(face);
  FT_Done_Face(face);

  // The cross-axis centerline of a vertical column is defined by the primary
  // font of the run (the vertical equivalent of sharing one baseline), so
  // that glyphs resolved through fallback fonts align with the rest of the
  // column
  double run_middle = 0.0;
  if (!vert_class.empty()) {
    hb_font_extents_t prim_extent;
    if (hb_font_get_h_extents(font, &prim_extent)) {
      double prim_scaling = fallback_scales[emoji == 1 ? 1 : 0];
      if (prim_scaling < 0) prim_scaling = 1.0;
      run_middle = (prim_extent.ascender + prim_extent.descender) * prim_scaling / 2.0;
    }
  }

  // Determine the HarfBuzz direction to shape this run with, and whether the
  // renderer should rotate the glyphs afterwards (mixed vertical text)
  hb_direction_t run_dir;
  double run_rotation = 0.0;
  bool run_tate = !vert_class.empty() && vert_class[start] == 2;
  // Tate-chu-yoko runs get the font's narrower digit forms when available:
  // qwid (quarter width) for runs of 4 digits, twid (third width) for 3,
  // and hwid (half width) otherwise, so the run fills the em at full size.
  // Explicit user settings of hwid/twid/qwid take precedence
  std::vector<hb_feature_t> tate_features;
  bool all_digits = run_tate;
  if (run_tate) {
    for (unsigned int i = start; i < end; ++i) {
      if (full_string[i] < 0x30 || full_string[i] > 0x39) {
        all_digits = false;
        break;
      }
    }
  }
  if (all_digits) {
    tate_features = features;
    bool has_narrow = false;
    for (size_t i = 0; i < features.size(); ++i) {
      if (features[i].tag == HB_TAG('h', 'w', 'i', 'd') ||
          features[i].tag == HB_TAG('t', 'w', 'i', 'd') ||
          features[i].tag == HB_TAG('q', 'w', 'i', 'd')) {
        has_narrow = true;
        break;
      }
    }
    if (!has_narrow) {
      hb_tag_t narrow = HB_TAG('h', 'w', 'i', 'd');
      size_t n_digits = end - start;
      hb_face_t* face = hb_font_get_face(font);
      if (n_digits >= 4 && face_has_feature(face, HB_TAG('q', 'w', 'i', 'd'))) {
        narrow = HB_TAG('q', 'w', 'i', 'd');
      } else if (n_digits == 3 && face_has_feature(face, HB_TAG('t', 'w', 'i', 'd'))) {
        narrow = HB_TAG('t', 'w', 'i', 'd');
      }
      hb_feature_t feat;
      feat.tag = narrow;
      feat.value = 1;
      feat.start = 0;
      feat.end = (unsigned int) -1;
      tate_features.push_back(feat);
    }
  }
  const std::vector<hb_feature_t>& active_features = all_digits ? tate_features : features;
  if (run_tate) {
    // Tate-chu-yoko digit run: shape horizontally, stay upright in the
    // column. The horizontal-to-vertical remapping happens per glyph in
    // fill_shape_info
    run_dir = HB_DIRECTION_LTR;
  } else if (vert_class.empty()) {
    run_dir = dir % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL;
  } else if (vert_class[start] == 0) {
    run_dir = HB_DIRECTION_TTB;
    // Required-vertical scripts (e.g. Mongolian) in a font without vertical
    // metrics can't be shaped upright: HarfBuzz would synthesize 1em
    // advances and centered origins, tearing the script apart. These fonts
    // are designed for "shape horizontally, rotate the line" presentation
    // The run may start with neutral characters (e.g. spaces), so scan the
    // whole run for required-vertical script
    bool req_vertical = false;
    for (unsigned int i = start; i < end; ++i) {
      if (char_is_required_vertical(full_string[i])) {
        req_vertical = true;
        break;
      }
    }
    if (req_vertical && !face_has_vertical_metrics(hb_font_get_face(font))) {
      run_dir = HB_DIRECTION_LTR;
      run_rotation = this->dir == 4 ? 90.0 : -90.0;
    }
  } else if (vert_class[start] == 3) {
    // UAX#50 Tr: use the typographic vertical glyph when the font provides
    // vert/vrt2, otherwise fall back to rotating the code chart glyph
    // 90 degrees like an R-class run
    if (face_has_vert_subs(hb_font_get_face(font))) {
      run_dir = HB_DIRECTION_TTB;
    } else {
      run_dir = HB_DIRECTION_LTR;
      run_rotation = this->dir == 4 ? 90.0 : -90.0;
    }
  } else {
    // Horizontal-only script inside vertical text. Set upright using
    // vert/vrt2 substitutions if requested and the font supports it,
    // otherwise shape horizontally and let the renderer rotate the run
    // 90 degrees (clockwise for ttb)
    bool upright = cur_mixed == 1 && face_has_vert_subs(hb_font_get_face(font));
    if (upright) {
      run_dir = HB_DIRECTION_TTB;
    } else {
      // The bidi algorithm is bypassed for vertical text, so detect the
      // run's own direction from its first strong character to keep rotated
      // RTL scripts (Arabic, Hebrew) in reading order
      run_dir = HB_DIRECTION_LTR;
      for (unsigned int i = start; i < end; ++i) {
        uint32_t cp = full_string[i];
        if (char_is_rtl_strong(cp)) {
          run_dir = HB_DIRECTION_RTL;
          break;
        }
        if ((cp >= 0x0041 && cp <= 0x007A) ||  // Basic Latin letters
            (cp >= 0x00C0 && cp <= 0x02AF) ||  // Latin-1 supplement & extended
            (cp >= 0x0370 && cp <= 0x052F)) {  // Greek, Cyrillic
          break;
        }
      }
      run_rotation = this->dir == 4 ? 90.0 : -90.0;
    }
  }

  // Upright vertical runs get vertical kana forms when the font provides
  // them. HarfBuzz enables vert/vrt2 automatically for vertical direction
  // but not vkna. Explicit user setting takes precedence
  std::vector<hb_feature_t> vert_features;
  if (run_dir == HB_DIRECTION_TTB) {
    bool has_vkna = false;
    for (size_t i = 0; i < active_features.size(); ++i) {
      if (active_features[i].tag == HB_TAG('v', 'k', 'n', 'a')) {
        has_vkna = true;
        break;
      }
    }
    if (!has_vkna) {
      vert_features = active_features;
      hb_feature_t vkna;
      vkna.tag = HB_TAG('v', 'k', 'n', 'a');
      vkna.value = 1;
      vkna.start = 0;
      vkna.end = (unsigned int) -1;
      vert_features.push_back(vkna);
    }
  }
  const std::vector<hb_feature_t>& shape_features = vert_features.empty() ? active_features : vert_features;

  // Do a first run of shaping. Hopefully it's enough
  unsigned int n_glyphs = 0;
  hb_buffer_reset(buffer);
  hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start, embedding_size);
  hb_buffer_guess_segment_properties(buffer);
  hb_buffer_set_direction(buffer, run_dir);
  hb_glyph_info_t *glyph_info = NULL;

  hb_shape(font, buffer, shape_features.data(), shape_features.size());

  hb_glyph_position_t *glyph_pos = NULL;
  glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

  if (n_glyphs == 0) {
    hb_font_destroy(font);
    return true;
  }

  shape_info.embeddings.emplace_back();
  shape_info.embeddings.back().embedding_level = dir;
  shape_info.embeddings.back().terminates_paragraph = false;
  shape_info.embeddings.back().full_width = 0;
  bool embed_is_ltr = dir % 2 == 0;

  // See if all characters hav been found in the default font
  unsigned int current_font = emoji == 1 ? 1 : 0;
  std::vector<unsigned int> char_font(embedding_size, current_font);
  bool needs_fallback = false;
  bool any_resolved = false;
  // For soft emoji the emoji font (index 1) is only tried as a last resort,
  // so unresolved characters are first directed at the regular fallbacks
  // starting at index 2
  annotate_fallbacks(emoji == 2 ? 2 : current_font + 1, embedding_size, char_font, glyph_info, n_glyphs, needs_fallback, any_resolved, embed_is_ltr, start);

  if (!needs_fallback) { // Short route - use existing shaping
    glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
    fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start, shape_info, fallback_sizes, fallback_scales, fallbacks, run_dir, run_rotation, run_middle, run_tate);
    fill_glyph_info(shape_info.embeddings.back());
    hb_font_destroy(font);
    shape_info.embeddings.back().fallbacks = fallbacks;
    shape_info.embeddings.back().fallback_size = fallback_sizes;
    shape_info.embeddings.back().fallback_scaling = fallback_scales;
    return true;
  }
  hb_font_destroy(font);

  // Need to reset this in case the first annotation didn't find any hits in the first font
  any_resolved = true;
  // We need to figure out the font for each character, then redo shaping using that
  while (needs_fallback && any_resolved) {
    needs_fallback = false;
    any_resolved = false;

    // Find the next fallback font to try out (starting with default)
    ++current_font;
    if (emoji == 2 && current_font == 1) {
      // Skip the emoji font slot for soft emoji; it is tried last
      ++current_font;
    }
    unsigned int fallback_start = 0, fallback_end = 0;
    bool found = fallback_cluster(current_font, char_font, 0, fallback_start, fallback_end);
    if (!found) {
      // This shouldn't happen but oh well. We give up on fallbacks and shape with what we got
      break;
    }
    bool using_new = false;
    font = load_fallback(current_font, start + fallback_start, start + fallback_end, using_new, shape_info, fallbacks, fallback_sizes, fallback_scales);
    if (font == nullptr) {
      shape_info.embeddings.pop_back();
      return false;
    }
    if (!using_new) {
      // We don't need to have success if we are trying an existing font
      any_resolved = true;
    }
    do {
      // Go through not yet resolved chars and see if the current fallback can resolve it
      hb_buffer_reset(buffer);
      hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + fallback_start, fallback_end - fallback_start);
      hb_buffer_guess_segment_properties(buffer);
      hb_buffer_set_direction(buffer, run_dir);
      hb_shape(font, buffer, shape_features.data(), shape_features.size());
      glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

      if (n_glyphs > 0) {
        bool needs_fallback_2 = false;
        bool any_resolved_2 = false;
        annotate_fallbacks(current_font + 1, fallback_end, char_font, glyph_info, n_glyphs, needs_fallback_2, any_resolved_2, embed_is_ltr, start);
        if (needs_fallback_2) needs_fallback = true;
        if (any_resolved_2) any_resolved = true;
      }

      found = fallback_cluster(current_font, char_font, fallback_end, fallback_start, fallback_end);
    } while (found);

    hb_font_destroy(font);
  }

  // For soft emoji, characters left unresolved by the regular fallback chain
  // get a final attempt with the emoji font
  if (emoji == 2 && fallbacks.size() > 1) {
    bool any_unresolved = false;
    for (size_t i = 0; i < char_font.size(); ++i) {
      if (char_font[i] >= fallbacks.size()) {
        char_font[i] = 1;
        any_unresolved = true;
      }
    }
    if (any_unresolved) {
      bool using_new = false;
      unsigned int fallback_start = 0, fallback_end = 0;
      bool found = fallback_cluster(1, char_font, 0, fallback_start, fallback_end);
      if (found) {
        font = load_fallback(1, start + fallback_start, start + fallback_end, using_new, shape_info, fallbacks, fallback_sizes, fallback_scales);
        if (font == nullptr) {
          shape_info.embeddings.pop_back();
          return false;
        }
        do {
          // Go through not yet resolved chars and see if the emoji font can resolve it
          hb_buffer_reset(buffer);
          hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + fallback_start, fallback_end - fallback_start);
          hb_buffer_guess_segment_properties(buffer);
          hb_buffer_set_direction(buffer, run_dir);
          hb_shape(font, buffer, shape_features.data(), shape_features.size());
          glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

          if (n_glyphs > 0) {
            bool needs_fallback_2 = false;
            bool any_resolved_2 = false;
            // Characters the emoji font can't render either are marked with
            // an out-of-range font so they end up as missing glyphs below
            annotate_fallbacks(fallbacks.size(), fallback_end, char_font, glyph_info, n_glyphs, needs_fallback_2, any_resolved_2, embed_is_ltr, start);
          }

          found = fallback_cluster(1, char_font, fallback_end, fallback_start, fallback_end);
        } while (found);
        hb_font_destroy(font);
      }
    }
  }

  // Make sure char_font does not point to non-existing fonts
  for (size_t i = 0; i < char_font.size(); ++i) {
    if (char_font[i] >= fallbacks.size()) {
      char_font[i] = 0;
    }
  }

  // The following two blocks only differ in the direction of operation
  if (embed_is_ltr) {
    current_font = char_font[0];
    unsigned int text_run_start = 0;
    for (unsigned int i = 1; i <= embedding_size; ++i) {
      if (i == embedding_size || char_font[i] != current_font) {
        // Move on until we hit a new font. Then shape everything up until current point
        // and add to embedding struct
        int error = 0;
        FT_Face face = get_cached_or_new_face(fallbacks[current_font],
                                              shape_info.size, shape_info.res, &error);
        if (error != 0) {
          Rprintf("Failed to get face: %s, %i\n", fallbacks[current_font].file, fallbacks[current_font].index);
          error_code = error;
          shape_info.embeddings.pop_back();
          return false;
        }

        font = ts_hb_ft_font_create_referenced(face);
        FT_Done_Face(face);

        hb_buffer_reset(buffer);
        hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + text_run_start, i - text_run_start);
        hb_buffer_guess_segment_properties(buffer);
        hb_buffer_set_direction(buffer, run_dir);
        hb_shape(font, buffer, shape_features.data(), shape_features.size());
        glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
        glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
        fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start + text_run_start, shape_info, fallback_sizes, fallback_scales, fallbacks, run_dir, run_rotation, run_middle, run_tate);
        fill_glyph_info(shape_info.embeddings.back());
        hb_font_destroy(font);

        if (i < embedding_size) {
          current_font = char_font[i];
          if (current_font >= fallbacks.size()) current_font = 0;
          text_run_start = i;
        }
      }
    }
  } else {
    current_font = char_font.back();
    int text_run_end = embedding_size;
    for (int i = text_run_end - 1; i >= 0; --i) {
      if (i <= 0 || char_font[i - 1] != current_font) {
        int error = 0;
        FT_Face face = get_cached_or_new_face(fallbacks[current_font],
                                              shape_info.size, shape_info.res, &error);
        if (error != 0) {
          Rprintf("Failed to get face: %s, %i\n", fallbacks[current_font].file, fallbacks[current_font].index);
          error_code = error;
          shape_info.embeddings.pop_back();
          return false;
        }

        font = ts_hb_ft_font_create_referenced(face);
        FT_Done_Face(face);

        hb_buffer_reset(buffer);
        hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + i, text_run_end - i);
        hb_buffer_guess_segment_properties(buffer);
        hb_buffer_set_direction(buffer, run_dir);
        hb_shape(font, buffer, shape_features.data(), shape_features.size());
        glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
        glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
        fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start + i, shape_info, fallback_sizes, fallback_scales, fallbacks, run_dir, run_rotation, run_middle, run_tate);
        fill_glyph_info(shape_info.embeddings.back());
        hb_font_destroy(font);

        if (i > 0) {
          current_font = char_font[i - 1];
          if (current_font >= fallbacks.size()) current_font = 0;
          text_run_end = i;
        }
      }
    }
  }

  shape_info.embeddings.back().fallbacks = fallbacks;
  shape_info.embeddings.back().fallback_size = fallback_sizes;
  shape_info.embeddings.back().fallback_scaling = fallback_scales;

  return true;
}

// Load a font from the fallback vector with the correct sizing etc
hb_font_t*  HarfBuzzShaper::load_fallback(unsigned int font, unsigned int start,
                                          unsigned int end, bool& new_added,
                                          ShapeInfo& shape_info,
                                          std::vector<FontSettings2>& fallbacks,
                                          std::vector<double>& fallback_sizes,
                                          std::vector<double>& fallback_scales) {
  new_added = false;
  // Font should only be able to be maximally the size of fallbacks
  if (font >= fallbacks.size()) {
    int n_conv = 0;
    const char* fallback_string = utf_converter.convert_to_utf(full_string.data() + start, end - start, n_conv);
    fallbacks.push_back(
      systemfonts::ver2::get_fallback(fallback_string, fallbacks[0])
    );
    new_added = true;
  }
  FT_Face face = nullptr;
  if (new_added) {
    face = get_font_sizing(fallbacks[font], shape_info.size, shape_info.res, fallback_sizes, fallback_scales);
  } else {
    // Existing font; its sizing has already been recorded in lockstep with
    // the fallbacks vector, so don't push new entries
    int error = 0;
    face = get_cached_or_new_face(fallbacks[font], shape_info.size, shape_info.res, &error);
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", fallbacks[font].file, fallbacks[font].index);
      error_code = error;
      return nullptr;
    }
  }

  if (!face) {
    return nullptr;
  }
  hb_font_t* hbfont = ts_hb_ft_font_create_referenced(face);
  FT_Done_Face(face);
  return hbfont;
}

// Find the next run of text for a specific fallback font
bool HarfBuzzShaper::fallback_cluster(unsigned int font, std::vector<unsigned int>& char_font, unsigned int from, unsigned int& start, unsigned int& end) {
  bool has_cluster = false;
  for (unsigned int i = from; i < char_font.size(); ++i) {
    if (char_font[i] == font) {
      start = i;
      has_cluster = true;
      break;
    }
  }
  for (unsigned int i = start + 1; i <= char_font.size(); ++i) {
    if (i == char_font.size() || char_font[i] != font) {
      end = i;
      break;
    }
  }
  return has_cluster;
}

// Find areas in the string that aren't covered by current font
void HarfBuzzShaper::annotate_fallbacks(unsigned int font, unsigned int buffer_end,
                                        std::vector<unsigned int>& char_font,
                                        hb_glyph_info_t* glyph_info,
                                        unsigned int n_glyphs,
                                        bool& needs_fallback, bool& any_resolved,
                                        bool ltr, unsigned int string_offset) {
  unsigned int current_cluster = glyph_info[0].cluster;
  unsigned int cluster_start = 0;

  for (unsigned int i = 1; i <= n_glyphs; ++i) {
    if (i == n_glyphs || glyph_info[i].cluster != current_cluster) {
      unsigned int next_cluster;
      if (ltr) {
        next_cluster = i < n_glyphs ? glyph_info[i].cluster : buffer_end + string_offset;
      } else {
        next_cluster = cluster_start > 0 ? glyph_info[cluster_start - 1].cluster : buffer_end + string_offset;
      }
      bool has_glyph = true;
      for (unsigned int j = cluster_start; j < i; ++j) {
        if (glyph_info[j].codepoint == 0 && !glyph_must_hard_break(current_cluster)) {
          has_glyph = false;
        }
      }
      if (!has_glyph) {
        needs_fallback = true;
        for (unsigned int j = current_cluster; j < next_cluster; ++j) {
          char_font[j - string_offset] = font;
        }
      } else {
        any_resolved = true;
      }
      if (i < n_glyphs) {
        current_cluster = glyph_info[i].cluster;
        cluster_start = i;
      }
    }
  }
}

// Add shaping info to the embedding structure
void HarfBuzzShaper::fill_shape_info(hb_glyph_info_t* glyph_info,
                                     hb_glyph_position_t* glyph_pos,
                                     unsigned int n_glyphs, hb_font_t* font,
                                     unsigned int font_id,
                                     unsigned int cluster_offset,
                                     ShapeInfo& shape_info,
                                     std::vector<double>& fallback_sizes,
                                     std::vector<double>& fallback_scales,
                                     std::vector<FontSettings2>& fallbacks,
                                     hb_direction_t hb_dir, double rotation, double run_middle,
                                     bool tate) {
  double scaling = fallback_scales[font_id];
  if (scaling < 0) scaling = 1.0;

  double tracking = shape_info.tracking * fallback_sizes[font_id] / 1000;

  // Tate-chu-yoko: the run is shaped horizontally but laid out upright in
  // the vertical column. The whole run occupies a main-axis extent equal to
  // its horizontal width (charged to the first glyph so line breaking and
  // the pen see the right total), and the glyphs march along the cross axis
  // centered on the column centerline. Runs wider than the em box are
  // scaled down to fit, registered as a scaled copy of the font so the
  // renderer draws the digits smaller as well
  std::vector<double> tate_adv;
  double tate_total = 0.0;
  double tate_scale = 1.0;
  // Main-axis extent of the run: never less than one em, so a single
  // (half-width) glyph gets a full em slot like its CJK neighbours instead
  // of crowding them
  double tate_slot = 0.0;
  unsigned int out_font = font_id;
  if (tate) {
    tate_adv.reserve(n_glyphs);
    for (unsigned int i = 0; i < n_glyphs; ++i) {
      double a = glyph_pos[i].x_advance * scaling + tracking;
      tate_adv.push_back(a);
      tate_total += a;
    }
    // glyph_pos is in 26.6 fixed point, so the em box includes the *64
    double em = fallback_sizes[font_id] * shape_info.res / 72.0 * 64.0 * scaling;
    if (em > 0 && tate_total > em) {
      tate_scale = em / tate_total;
      tate_total = em;
      for (double& a : tate_adv) a *= tate_scale;
      out_font = fallbacks.size();
      fallbacks.push_back(fallbacks[font_id]);
      fallback_sizes.push_back(fallback_sizes[font_id] * tate_scale);
      fallback_scales.push_back(fallback_scales[font_id]);
    }
    tate_slot = std::max(tate_total, em);
  }
  double tate_pos = run_middle - tate_total / 2.0;
  // The btt mirror flips the cross axis, which would reverse the digit
  // sequence — assign cross slots from the far end so they come out
  // left-to-right after mirroring
  bool tate_reverse = tate && dir == 4;
  if (tate_reverse) tate_pos += tate_total;

  // Vertically shaped runs are remapped into the horizontal layout space
  // used by the rest of the shaper: the vertical advance becomes the x
  // advance and the cross axis becomes y. Positions are rotated back into
  // real space at the end of finish_string. All vertical runs are shaped
  // top-to-bottom (see shape_embedding) so only that case is handled here
  bool vertical = hb_dir == HB_DIRECTION_TTB || hb_dir == HB_DIRECTION_BTT;


#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
  ascend = 0;
  descend = 0;
#else
  hb_font_extents_t fextent;
  hb_font_get_h_extents(font, &fextent);

  ascend = fextent.ascender;
  descend = fextent.descender;
#endif

  hb_glyph_extents_t extent;

  EmbedInfo& embedding = shape_info.embeddings.back();

  size_t new_size = embedding.glyph_id.size() + n_glyphs;
  embedding.glyph_id.reserve(new_size);
  embedding.glyph_cluster.reserve(new_size);
  embedding.x_offset.reserve(new_size);
  embedding.y_offset.reserve(new_size);
  embedding.x_advance.reserve(new_size);
  embedding.y_advance.reserve(new_size);
  embedding.x_bear.reserve(new_size);
  embedding.y_bear.reserve(new_size);
  embedding.width.reserve(new_size);
  embedding.height.reserve(new_size);
  embedding.ascenders.reserve(new_size);
  embedding.descenders.reserve(new_size);
  embedding.font.reserve(new_size);

  for (unsigned int i = 0; i < n_glyphs; ++i) {
    embedding.glyph_id.push_back(glyph_info[i].codepoint);
    embedding.glyph_cluster.push_back(glyph_info[i].cluster);

    double x_off = glyph_pos[i].x_offset * scaling;
    double y_off = glyph_pos[i].y_offset * scaling;
    double x_adv = glyph_pos[i].x_advance * scaling;
    double y_adv = glyph_pos[i].y_advance * scaling;

    hb_font_get_glyph_extents(font, glyph_info[i].codepoint, &extent);
    // y_bearing is the top of the ink box and height is negative (downward)
    double x_b = extent.x_bearing * scaling;
    double y_b = extent.y_bearing * scaling;
    double w = extent.width * scaling;
    double h = extent.height * scaling;

    if (tate) {
      // Upright in real space like TTB-shaped glyphs, so bearings transpose
      // the same way. The em box is centered in the run's main-axis extent
      // using the font's horizontal extents
      double em_height = (ascend - descend) * scaling * tate_scale;
      embedding.em_width.push_back(x_adv * tate_scale);
      // All glyphs in the run share one main-axis position, but the pen has
      // already advanced past the run's full extent after the first glyph —
      // pull subsequent glyphs back by the same amount
      embedding.x_offset.push_back((tate_slot - em_height) / 2.0 + ascend * scaling * tate_scale - y_off * tate_scale - (i == 0 ? 0.0 : tate_slot));
      if (tate_reverse) tate_pos -= tate_adv[i];
      embedding.y_offset.push_back(tate_pos + x_off * tate_scale);
      embedding.x_advance.push_back(i == 0 ? tate_slot : 0.0);
      embedding.y_advance.push_back(0.0);
      embedding.x_bear.push_back(-y_b * tate_scale);
      embedding.width.push_back(-h * tate_scale);
      embedding.y_bear.push_back((x_b + w) * tate_scale);
      embedding.height.push_back(-w * tate_scale);
      if (!tate_reverse) tate_pos += tate_adv[i];
    } else if (vertical) {
      // Tracking goes on the main axis, i.e. the vertical advance (negative
      // going down for ttb)
      y_adv -= tracking;
      // Center the glyph's advance box on the column centerline defined by
      // the primary font's middle (run_middle is 0 for horizontal text).
      // The VORG table has no x coordinate — the vertical origin's x is the
      // center of the advance by definition — so h_adv/2 is the spec-
      // sanctioned cross position. (The main axis is different: hb's
      // y_offset already carries the VORG origin Y, so per-glyph vertical
      // placement — e.g. lowercase fullwidth letters sitting optically
      // centered — is honoured through x_offset = -y_off above.) Glyphs
      // with vertical substitutions (e.g. corner punctuation) keep their
      // designed placement inside the em box
      double h_adv = hb_font_get_glyph_h_advance(font, glyph_info[i].codepoint) * scaling;
      embedding.em_width.push_back(h_adv);
      embedding.x_offset.push_back(-y_off);
      embedding.y_offset.push_back(run_middle - h_adv / 2.0);
      embedding.x_advance.push_back(-y_adv);
      embedding.y_advance.push_back(x_adv);
      embedding.x_bear.push_back(-y_b);
      embedding.width.push_back(-h);
      embedding.y_bear.push_back(x_b + w);
      embedding.height.push_back(-w);
    } else {
      embedding.em_width.push_back(x_adv);
      x_adv += tracking;
      embedding.x_offset.push_back(x_off);
      embedding.y_offset.push_back(y_off);
      embedding.x_advance.push_back(x_adv);
      embedding.y_advance.push_back(y_adv);
      embedding.x_bear.push_back(x_b);
      embedding.y_bear.push_back(y_b);
      embedding.width.push_back(w);
      embedding.height.push_back(h);
    }
    embedding.full_width += embedding.x_advance.back();

    embedding.rotation.push_back(rotation);

    embedding.ascenders.push_back(ascend * scaling);
    embedding.descenders.push_back(descend * scaling);

    embedding.font.push_back(tate ? out_font : font_id);
  }
}

// Add line breaking/stretching info to embedding structure
void HarfBuzzShaper::fill_glyph_info(EmbedInfo& embedding) {
  for (size_t i = embedding.is_blank.size(); i < embedding.glyph_cluster.size(); ++i) {
    int32_t cluster = embedding.glyph_cluster[i];
    if (cluster < full_string.size()) {
      embedding.is_blank.push_back(glyph_is_blank(full_string[cluster]));
      embedding.may_break.push_back(glyph_may_soft_break(cluster));
      embedding.may_stretch.push_back(glyph_may_stretch(full_string[cluster]));
    } else {
      embedding.is_blank.push_back(false);
      embedding.may_break.push_back(false);
      embedding.may_stretch.push_back(false);
    }
  }
}

inline FT_Face HarfBuzzShaper::get_font_sizing(FontSettings2& font_info, double size, double res, std::vector<double>& sizes, std::vector<double>& scales, bool deref) {
  int error = 0;
  FT_Face face = get_cached_or_new_face(font_info, size, res, &error);
  if (error != 0) {
    Rprintf("Failed to get face: %s, %i\n", font_info.file, font_info.index);
    error_code = error;
    return nullptr;
  }
  double scaling = -1;
  if (!FT_IS_SCALABLE(face) && face->size != nullptr && face->size->metrics.height != 0) {
    scaling = size * 64.0 * res / 72.0 / face->size->metrics.height;
  }
  double fscaling = family_scaling(face->family_name);
  scales.push_back(scaling * fscaling);
  sizes.push_back(size * fscaling);
  if (deref) {
    FT_Done_Face(face);
    // The returned pointer is released in this case and must only be used
    // as a success indicator, never dereferenced
  }
  return face;
}

void HarfBuzzShaper::insert_hyphen(EmbedInfo& embedding, size_t where) {
  int error = 0;
  // Load main font (emoji if dir is negative)
  // Shouldn't be able to fail as we have already tried to load it in the calling function
  FT_Face face = get_cached_or_new_face(
    embedding.fallbacks[embedding.font[where]],
    embedding.fallback_size[embedding.font[where]],
    shape_infos[0].res,
    &error
  );
  if (error) {
    return;
  }

  double scaling = embedding.fallback_scaling[embedding.font[where]];
  if (scaling < 0) scaling = 1.0;

  hb_font_t *font = ts_hb_ft_font_create_referenced(face);
  FT_Done_Face(face);
  hb_codepoint_t glyph = 0;
  hb_bool_t found = hb_font_get_glyph(font, 8208, 0, &glyph); // True hyphen;
  if (!found) found = hb_font_get_glyph(font, 45, 0, &glyph); // Hyphen minus

  if (!found) { // No hyphen-like glyph in font
    hb_font_destroy(font);
    return;
  }

  embedding.glyph_id[where] = glyph;

  hb_position_t x = hb_font_get_glyph_h_advance(font, glyph);
  hb_position_t y = 0;

  embedding.x_advance[where] = x * scaling;
  // Kerning is against the preceding glyph (in visual order), not codepoint
  size_t prev = embedding.embedding_level % 2 == 0
    ? (where == 0 ? embedding.glyph_id.size() : where - 1)
    : where + 1;
  hb_codepoint_t prev_glyph = prev < embedding.glyph_id.size() ? embedding.glyph_id[prev] : 0;
  if (prev_glyph != 0 && embedding.glyph_cluster[where] > 0) {
    hb_font_get_glyph_kerning_for_direction(font, prev_glyph, glyph, embedding.embedding_level % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL, &x, &y);
  } else {
    x = 0;
  }
  embedding.x_offset[where] = x * scaling;
  embedding.y_offset[where] = y * scaling;

  hb_glyph_extents_t extent;
  hb_font_get_glyph_extents(font, glyph, &extent);
  embedding.x_bear[where] = extent.x_bearing * scaling;
  embedding.y_bear[where] = extent.y_bearing * scaling;
  embedding.width[where] = extent.width * scaling;
  embedding.height[where] = extent.height * scaling;

  hb_font_destroy(font);
}

bool HarfBuzzShaper::has_valid_break(const EmbedInfo& embedding, int32_t width, size_t& break_pos, bool force) {
  bool has_break = false;
  int32_t w = 0;
  if (embedding.embedding_level % 2 == 0) { // ltr
    break_pos = 0;
    for (size_t i = 0; i < embedding.glyph_id.size(); ++i) {
      // If the glyph is blank we don't care if it extends beyond the max width
      if (embedding.is_blank[i] && embedding.may_break[i]) {
        break_pos = i;
        has_break = true;
      }
      w += embedding.x_advance[i];
      if (w > width) {
        // Compression: the trailing blank of collapsible punctuation need
        // not fit — if the glyph's ink fits it may end the line here, with
        // the blank hanging off (it collapses at the line end anyway)
        if (embedding.may_break[i]) {
          int32_t cluster = embedding.glyph_cluster[i];
          uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
          if (char_is_trailing_collapsible(cp)) {
            int32_t trail = embedding.x_advance[i] - embedding.x_offset[i] - embedding.x_bear[i] - embedding.width[i];
            if (trail > 0 && w - trail <= width) {
              break_pos = i;
              return true;
            }
          }
        }
        if (force) {
          // Kinsoku shori (prototype): line-start-prohibited punctuation
          // hangs past the line end (buried style) rather than starting the
          // next line. The overhang is limited to about two em (twice the
          // advance of the first hung glyph)
          int32_t hang_limit = 0;
          int32_t hung_w = 0;
          size_t j = i;
          while (j < embedding.glyph_id.size()) {
            int32_t cluster = embedding.glyph_cluster[j];
            uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
            if (!char_is_line_start_prohibited(cp)) break;
            int32_t adv = embedding.x_advance[j];
            if (hang_limit == 0) hang_limit = 2 * adv;
            if (hung_w + adv > hang_limit) break;
            hung_w += adv;
            if (embedding.may_break[j]) {
              break_pos = j;
              return true;
            }
            ++j;
          }
          // If forcing we need to consume at least one glyph
          break_pos = i == 0 ? 0 : i - 1;
          // Kinsoku shori: an opening bracket may not end the line — pull
          // it (and any run of them) down to the next line
          while (break_pos > 0) {
            int32_t cluster = embedding.glyph_cluster[break_pos];
            uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
            if (!char_is_line_end_prohibited(cp)) break;
            --break_pos;
          }
          return true;
        }
        return has_break;
      }
      // If it isn't blank we wait
      if (!embedding.is_blank[i] && embedding.may_break[i]) {
        break_pos = i;
        has_break = true;
      }
    }
  } else {
    for (size_t i = embedding.glyph_id.size(); i > 0; --i) {
      // If the glyph is blank we don't care if it extends beyond the max width
      if (embedding.is_blank[i - 1] && embedding.may_break[i - 1]) {
        break_pos = i - 1;
        has_break = true;
      }
      w += embedding.x_advance[i - 1];
      if (w > width) {
        // Compression (mirror of the ltr branch): the trailing blank of
        // collapsible punctuation need not fit — if the glyph's ink fits
        // it may end the line here with the blank hanging off
        if (embedding.may_break[i - 1]) {
          int32_t cluster = embedding.glyph_cluster[i - 1];
          uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
          if (char_is_trailing_collapsible(cp)) {
            int32_t trail = embedding.x_advance[i - 1] - embedding.x_offset[i - 1] - embedding.x_bear[i - 1] - embedding.width[i - 1];
            if (trail > 0 && w - trail <= width) {
              break_pos = i - 1;
              return true;
            }
          }
        }
        if (force) {
          // Kinsoku shori (mirror of the ltr branch): line-start-prohibited
          // punctuation hangs past the line end instead of starting the
          // next line. The line's end is its left edge in rtl, so hanging
          // extends towards lower indices. The overhang is limited to
          // about two em (twice the advance of the first hung glyph)
          int32_t hang_limit = 0;
          int32_t hung_w = 0;
          size_t j = i - 1;
          while (true) {
            int32_t cluster = embedding.glyph_cluster[j];
            uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
            if (!char_is_line_start_prohibited(cp)) break;
            int32_t adv = embedding.x_advance[j];
            if (hang_limit == 0) hang_limit = 2 * adv;
            if (hung_w + adv > hang_limit) break;
            hung_w += adv;
            if (embedding.may_break[j]) {
              break_pos = j;
              return true;
            }
            if (j == 0) break;
            --j;
          }
          // If forcing we need to consume at least one glyph
          break_pos = std::min(i, embedding.glyph_id.size() - 1);
          // An opening bracket may not end the line — pull it down to the
          // next line. The line ends at its left edge in rtl, i.e. at the
          // glyph with the lowest kept index
          while (break_pos + 1 < embedding.glyph_id.size()) {
            int32_t cluster = embedding.glyph_cluster[break_pos];
            uint32_t cp = cluster < (int32_t) full_string.size() ? full_string[cluster] : 0;
            if (!char_is_line_end_prohibited(cp)) break;
            ++break_pos;
          }
          return true;
        }
        return has_break;
      }
      // If it isn't blank we wait
      if (!embedding.is_blank[i - 1] && embedding.may_break[i - 1]) {
        break_pos = i - 1;
        has_break = true;
      }
    }
  }
  return has_break;
}

void HarfBuzzShaper::rearrange_embeddings(std::list<EmbedInfo>& line) {
  static std::vector<std::list<EmbedInfo>::iterator> embed_stack(128); // Max nesting level allowed by ICU bidi algo

  if (line.size() < 2) return; // Nothing to do

  embed_stack[0] = line.begin();
  size_t current_embed = 0;
  for (auto iter = line.begin(); iter != line.end(); ++iter) {
    size_t cur_embed_level = iter->embedding_level;
    // Nothing to do here but also shouldn't happen
    if (cur_embed_level == current_embed) continue;

    if (cur_embed_level > current_embed) {
      // We fill the stack with current position up to the embeddings level
      while (current_embed != cur_embed_level) {
        ++current_embed;
        if (current_embed >= embed_stack.size()) {
          embed_stack.resize(current_embed + 1);
        }
        embed_stack[current_embed] = iter;
      }
    } else {
      // We are exiting a run. Need to reverse everything above
      while (current_embed != cur_embed_level) {
        std::reverse(embed_stack[current_embed], iter);
        --current_embed;
      }
    }
  }
  // We need to reverse until we hit 0
  while (current_embed != 0) {
    std::reverse(embed_stack[current_embed], line.end());
    --current_embed;
  }
}

std::list<EmbedInfo> HarfBuzzShaper::get_next_line_at_width(int32_t width, std::list<EmbedInfo>& all_embeddings, bool& hard_break, int32_t& break_height) {
  std::list<EmbedInfo> line;

  if (width < 0) { // No limit on width
    auto has_break = all_embeddings.end();
    for (auto iter = all_embeddings.begin(); iter != all_embeddings.end(); ++iter) {
      if (iter->terminates_paragraph) {
        has_break = iter;
        break;
      }
    }
    if (has_break == all_embeddings.end()) {
      // No line breaks. Use the full embedding
      line.swap(all_embeddings);
      hard_break = false;
    } else {
      line.splice(line.begin(), all_embeddings, all_embeddings.begin(), std::next(has_break));
      hard_break = true;
      break_height = has_break->pop();
    }
  } else {
    auto iter = all_embeddings.begin();
    int32_t cum_width = 0;
    while (cum_width <= width && iter != all_embeddings.end()) {
      cum_width += iter->full_width;
      ++iter;
      if (std::prev(iter)->terminates_paragraph) break;
    }
    if (cum_width > width) {
      // Last embedding needs to be cut if possible
      hard_break = false;
      auto orig_iter = iter;
      auto orig_width = cum_width;
      size_t break_at = 0;
      size_t from, to;
      bool force = false;
      while (true) {
        --iter;
        cum_width -= iter->full_width;
        // Search for a break in the embedding that satisfies our width constraint
        if (has_valid_break(*iter, width - cum_width, break_at, force)) {
          if (iter->embedding_level % 2 == 0) {
            from = 0;
            to = break_at + 1;
          } else {
            from = break_at;
            to = iter->glyph_id.size();
          }
          // Did we break at soft hyphen?
          bool is_shy = iter->glyph_cluster[break_at] < full_string.size() &&
                        full_string[iter->glyph_cluster[break_at]] == 173;
          // We found one. Split the first part into half tail and assemble line
          line.splice(line.begin(), all_embeddings, all_embeddings.begin(), iter);
          line.emplace_back();
          iter->split(from, to, line.back());
          if (is_shy) { // Substitute soft hyphen with hyphen
            size_t at = line.back().embedding_level % 2 == 0 ? line.back().glyph_id.size() - 1 : 0;
            insert_hyphen(line.back(), at);
          }
          break;
        }
        // We shouldn't hit the below condition since force == true should find a break immediately
        if (iter == all_embeddings.begin() && force) break;

        if (iter == all_embeddings.begin()) {
          // We didn't find a break. Reset and set force = true
          iter = orig_iter;
          cum_width = orig_width;
          force = true;
        }
      }
      if (line.empty()) {
        // If we end here we completely failed to fit a single glyph to the line
        cpp11::stop("Failed to wrap lines");
      }
    } else if (iter == all_embeddings.end()) {
      hard_break = all_embeddings.back().terminates_paragraph;
      line.swap(all_embeddings); // It all fits on the line
    } else {
      // It got stopped by a hard line break
      hard_break = true;
      break_height = std::prev(iter)->pop();
      line.splice(line.begin(), all_embeddings, all_embeddings.begin(), iter);
    }
  }
  rearrange_embeddings(line);
  return line;
}

void HarfBuzzShaper::do_alignment(bool ltr) {
  if (x_pos.empty()) return; // Nothing to align (e.g. empty string)
  if (cur_align == 1 || cur_align == 2) {
    // Standard center or right justify
    // Move x_pos based on the linewidth of the line and the full width of the text
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      int32_t lwd = line_width[index];
      x_pos[i] = cur_align == 1 ? x_pos[i] + width/2 - lwd/2 : x_pos[i] + width - lwd;
    }
    // Do the same with pen position
    pen_x = cur_align == 1 ? pen_x + width/2 - line_width.back()/2 : pen_x + width - line_width.back();
  }
  if (cur_align == 3 || cur_align == 4 || cur_align == 5) {
    // Justified alignment
    std::vector<size_t> n_stretches(line_width.size(), 0);
    std::vector<bool> no_stretch(line_width.size(), false);
    for (size_t i = 0; i < x_pos.size(); ++i) {
      // Loop through all glyphs to figure out number of stretches per line and if
      // stretches are allowed (not for last line or lines with forced linebreak)
      int index = line_id[i];
      no_stretch[index] = no_stretch[index] || index == line_width.size() - 1 || line_must_break[index];
      if (may_stretch[i] && i+1 < x_pos.size() && index == line_id[i+1]) {
        n_stretches[index]++;
      }
    }
    int32_t cum_move = 0;
    for (size_t i = 0; i < x_pos.size(); ++i) {
      // Loop through glyphs, spreading them out
      int index = line_id[i];
      int32_t lwd = line_width[index];
      if (no_stretch[index] || n_stretches[index] == 0) {
        // If line may not stretch instead move it according to alignment
        if (cur_align == 4) {
          x_pos[i] = x_pos[i] + width/2 - lwd/2;
        } else if (cur_align == 5) {
          x_pos[i] = x_pos[i] + width - lwd;
        }
        continue;
      }
      if (i == 0 || line_id[i-1] != index) {
        // New line, reset stretch counter
        cum_move = 0;
      }
      // Move x_pos according to the cummulative move counter
      x_pos[i] += cum_move;
      if (may_stretch[i]) {
        // If white space the counter gets increased
        cum_move += (width - line_width[index]) / n_stretches[index];
      }
    }
    // Update pen_x to match position of last line
    if (ltr) {
      // If last line is empty ignore cum_move
      pen_x += line_id.back() == line_width.size() - 1 ? cum_move : 0;
    }
    if (no_stretch.back() || n_stretches.back() == 0) {
      if (cur_align == 4) {
        pen_x += width/2 - line_width.back()/2;
      } else if (cur_align == 5) {
        pen_x += width - line_width.back();
      }
    }
    // Update line width of all stretched lines to match the full width of the textbox
    for (size_t i = 0; i < line_width.size(); ++i) {
      if (!no_stretch[i] && n_stretches[i] != 0) line_width[i] = width;
    }
  }
  if (cur_align == 6) {
    // Distribute glyphs evenly over line
    std::vector<size_t> n_glyphs(line_width.size(), 0);
    for (size_t i = 0; i < x_pos.size(); ++i) {
      // Count number of glyphs on each line (not including carriage return)
      int index = line_id[i];
      if (!line_must_break[index] && (i == x_pos.size()-1 || index == line_id[i+1])) {
        n_glyphs[index]++;
      }
    }
    // Spread out glyphs according to an accumulating spread factor
    int32_t cum_move = 0;
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      if (i == 0 || line_id[i-1] != index) {
        cum_move = 0;
      }
      x_pos[i] += cum_move;
      if (n_glyphs[index] > 1) {
        cum_move += (width - line_width[index]) / int32_t(n_glyphs[index] - 1);
      }
    }
    pen_x += cum_move;
    // Update line width of all stretched lines to match the full width of the textbox
    for (size_t i = 0; i < line_width.size(); ++i) {
      if (n_glyphs[i] != 0) line_width[i] = width;
    }
  }
}

#endif
