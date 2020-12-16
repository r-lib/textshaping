#ifndef NO_HARFBUZZ_FRIBIDI

#include <hb-ft.h>
#include "string_shape.h"
#include "string_bidi.h"
#include <systemfonts.h>
#include <systemfonts-ft.h>
#include <algorithm>

UTF_UCS HarfBuzzShaper::utf_converter = UTF_UCS();
LRU_Cache<std::string, std::vector<int> > HarfBuzzShaper::bidi_cache = {1000};
LRU_Cache<ShapeID, ShapeInfo> HarfBuzzShaper::shape_cache = {1000};
std::vector<unsigned int> HarfBuzzShaper::glyph_id = {};
std::vector<unsigned int> HarfBuzzShaper::glyph_cluster = {};
std::vector<unsigned int> HarfBuzzShaper::string_id = {};
std::vector<int32_t> HarfBuzzShaper::x_pos = {};
std::vector<int32_t> HarfBuzzShaper::y_pos = {};
std::vector<int32_t> HarfBuzzShaper::x_mid = {};
std::vector<int32_t> HarfBuzzShaper::x_advance = {};
std::vector<int32_t> HarfBuzzShaper::x_offset = {};
std::vector<int32_t> HarfBuzzShaper::left_bear = {};
std::vector<int32_t> HarfBuzzShaper::right_bear = {};
std::vector<int32_t> HarfBuzzShaper::top_extend = {};
std::vector<int32_t> HarfBuzzShaper::bottom_extend = {};
std::vector<int32_t> HarfBuzzShaper::ascenders = {};
std::vector<int32_t> HarfBuzzShaper::descenders = {};
std::vector<bool> HarfBuzzShaper::may_break = {};
std::vector<bool> HarfBuzzShaper::must_break = {};
ShapeID HarfBuzzShaper::last_shape_id = {};
ShapeID HarfBuzzShaper::temp_shape_id = {};
ShapeInfo HarfBuzzShaper::last_shape_info = {};

bool HarfBuzzShaper::shape_string(const char* string, const char* fontfile,
                                  int index, double size, double res, double lineheight,
                                  int align, double hjust, double vjust, double width,
                                  double tracking, double ind, double hang, double before,
                                  double after) {
  reset();

  int error = 0;
  FT_Face face = get_cached_face(fontfile, index, size, res, &error);
  if (error != 0) {
    error_code = error;
    return false;
  }
  hb_font_t *font = hb_ft_font_create(face, NULL);

  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  std::vector<int> embeddings = {};

  if (n_chars > 1) {
    std::string key(string);
    if (!bidi_cache.get(key, embeddings)) {
      embeddings = get_bidi_embeddings(utc_string, n_chars);
      bidi_cache.add(key, embeddings);
    }
  } else {
    embeddings.push_back(0);
  }

  max_width = width;
  indent = ind;
  pen_x = indent;
  hanging = hang;
  space_before = before;
  space_after = after;
  cur_tracking = tracking;

  cur_res = res;
  cur_lineheight = lineheight;
  cur_align = align;
  cur_hjust = hjust;
  cur_vjust = vjust;

  int start = 0;
  for (size_t i = 0; i < embeddings.size(); ++i) {
    if (i == embeddings.size() - 1 || embeddings[i] != embeddings[i + 1]) {
      hb_buffer_reset(buffer);
      hb_buffer_add_utf32(buffer, utc_string, n_chars, start, i - start + 1);
      hb_buffer_guess_segment_properties(buffer);

      bool success = shape_glyphs(font, utc_string + start, i - start + 1);
      if (!success) {
        return false;
      }

      start = i + 1;
    }
  }

  hb_font_destroy(font);
  //FT_Done_Face(face);
  return true;
}

bool HarfBuzzShaper::add_string(const char* string, const char* fontfile,
                                int index, double size, double tracking) {
  cur_string++;
  int error = 0;
  FT_Face face = get_cached_face(fontfile, index, size, cur_res, &error);
  if (error != 0) {
    error_code = error;
    return false;
  }
  hb_font_t *font = hb_ft_font_create(face, NULL);

  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  std::vector<int> embeddings = {};

  if (n_chars > 1) {
    std::string key(string);
    if (!bidi_cache.get(key, embeddings)) {
      embeddings = get_bidi_embeddings(utc_string, n_chars);
      bidi_cache.add(key, embeddings);
    }
  } else {
    embeddings.push_back(0);
  }

  cur_tracking = tracking;

  int start = 0;
  for (size_t i = 0; i < embeddings.size(); ++i) {
    if (i == embeddings.size() - 1 || embeddings[i] != embeddings[i + 1]) {
      hb_buffer_reset(buffer);
      hb_buffer_add_utf32(buffer, utc_string, n_chars, start, i - start + 1);
      hb_buffer_guess_segment_properties(buffer);

      bool success = shape_glyphs(font, utc_string + start, i - start + 1);
      if (!success) {
        return false;
      }

      start = i + 1;
    }
  }

  hb_font_destroy(font);
  //FT_Done_Face(face);
  return true;
}

bool HarfBuzzShaper::finish_string() {
  if (glyph_id.size() == 0) {
    return true;
  }
  bool first_char = true;
  bool first_line = true;
  pen_x += indent;
  int last_space = -1;
  int32_t last_nonspace_width = 0;
  int32_t last_nonspace_bear = 0;
  int cur_line = 0;
  double line_height = 0;
  size_t glyph_counter = 0;
  int32_t max_descend = 0;
  int32_t max_ascend = 0;
  int32_t max_top_extend = 0;
  int32_t max_bottom_extend = 0;
  int32_t last_max_descend = 0;
  bool no_break_last = true;

  for (unsigned int i = 0; i < glyph_id.size(); ++i) {
    bool linebreak = must_break[i];
    bool might_break = may_break[i];
    bool last = i == glyph_id.size() - 1;

    bool soft_wrap = false;

    if (might_break || linebreak) {
      last_space = i;
      if (no_break_last) {
        last_nonspace_width = pen_x;
        last_nonspace_bear = i == 0 ? 0 : right_bear[i - 1];
      }
    }
    no_break_last = !might_break;

    // Calculate top and bottom extend and ascender/descender


    // Soft wrapping?
    if (max_width > 0 && !first_char && pen_x + x_advance[i] > max_width && !might_break && !linebreak) {
      // Rewind to last breaking char and set the soft_wrap flag
      i = last_space >= 0 ? last_space : i - 1;
      x_pos.resize(i + 1);
      x_mid.resize(i + 1);
      line_id.resize(i + 1);
      soft_wrap = true;
      last = false;
    } else {
      // No soft wrap, record pen position
      x_pos.push_back(pen_x + x_offset[i]);
      x_mid.push_back(x_advance[i] / 2);
      line_id.push_back(cur_line);
    }

    // If last char update terminal line info
    if (last) {
      last_nonspace_width = pen_x + x_advance[i];
      last_nonspace_bear = right_bear[i];
    }
    if (first_char) {
      line_left_bear.push_back(left_bear[i]);
      pen_y -= space_before;
    }

    // Handle new lines
    if (linebreak || soft_wrap || last) {
      // Record and reset line dim info
      line_right_bear.push_back(last_nonspace_bear);
      line_width.push_back(last_nonspace_width);
      last_nonspace_bear = 0;
      last_nonspace_width = 0;
      last_space = -1;
      no_break_last = true;

      // Calculate line dimensions
      for (size_t j = glyph_counter; j < x_pos.size(); ++j) {
        if (max_ascend < ascenders[j]) {
          max_ascend = ascenders[j];
        }
        if (max_top_extend < top_extend[j]) {
          max_top_extend = top_extend[j];
        }
        if (max_descend > descenders[j]) {
          max_descend = descenders[j];
        }
        if (max_bottom_extend > bottom_extend[j]) {
          max_bottom_extend = bottom_extend[j];
        }
      }

      // Move pen based on indent and line height
      line_height = (max_ascend - last_max_descend) * cur_lineheight;
      if (last) {
        pen_x = (linebreak || soft_wrap) ? 0 : pen_x + x_advance[i];
      } else {
        pen_x = soft_wrap ? hanging : indent;
      }
      pen_y = first_line ? 0 : pen_y - line_height;
      bottom -= line_height;
      // Fill up y_pos based on calculated pen position
      for (; glyph_counter < x_pos.size(); ++glyph_counter) {
        y_pos.push_back(pen_y);
      }
      // Move pen_y further down based on paragraph spacing
      // TODO: Add per string paragraph spacing
      if (linebreak) {
        pen_y -= space_after;
        if (last) {
          pen_y -= line_height;
          bottom -= line_height;
        }
      }
      if (first_line) {
        top_border = max_ascend;
        top_bearing = top_border - max_top_extend;
      }
      // Reset flags and counters
      last_max_descend = max_descend;
      if (!last) {
        max_ascend = 0;
        max_descend = 0;
        max_top_extend = 0;
        max_bottom_extend = 0;
        first_line = false;
        cur_line++;
        first_char = true;
      }
    } else {
      // No line break - advance the pen
      pen_x += x_advance[i];
      first_char = false;
    }
  }
  height = top_border - bottom - max_descend;
  bottom_bearing = max_bottom_extend - max_descend;
  int max_width_ind = std::max_element(line_width.begin(), line_width.end()) - line_width.begin();
  width = max_width < 0 ? line_width[max_width_ind] : max_width;
  if (cur_align != 0) {
    for (unsigned int i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      int lwd = line_width[index];
      x_pos[i] = cur_align == 1 ? x_pos[i] + width/2 - lwd/2 : x_pos[i] + width - lwd;
    }
  }
  double width_diff = width - line_width[max_width_ind];
  if (cur_align == 1) {
    width_diff /= 2;
  }
  left_bearing = cur_align == 0 ? *std::min_element(line_left_bear.begin(), line_left_bear.end()) : line_left_bear[max_width_ind] + width_diff;
  right_bearing = cur_align == 2 ? *std::min_element(line_right_bear.begin(), line_right_bear.end()) : line_right_bear[max_width_ind] + width_diff;
  if (cur_hjust != 0.0) {
    left_border = - cur_hjust * width;
    pen_x += left_border;
    for (unsigned int i = 0; i < x_pos.size(); ++i) {
      x_pos[i] += left_border;
    }
  }
  if (cur_vjust != 1.0) {
    int32_t just_height = top_border - pen_y;
    for (unsigned int i = 0; i < x_pos.size(); ++i) {
      y_pos[i] += - pen_y - cur_vjust * just_height;
    }
    top_border += - pen_y - cur_vjust * just_height;
    pen_y += - pen_y - cur_vjust * just_height;
  }
  return true;
}

bool HarfBuzzShaper::single_line_shape(const char* string, FontSettings font_info,
                                       double size, double res) {
  int n_features = font_info.n_features;
  std::vector<hb_feature_t> features(n_features);
  if (n_features == 0) {
    temp_shape_id.string.assign(string);
    temp_shape_id.font.assign(font_info.file);
    temp_shape_id.index = font_info.index;
    temp_shape_id.size = size * res;
    if (temp_shape_id == last_shape_id) {
      return true;
    }
    if (shape_cache.get(temp_shape_id, last_shape_info)) {
      last_shape_id.string.swap(temp_shape_id.string);
      last_shape_id.font.swap(temp_shape_id.font);
      last_shape_id.index = temp_shape_id.index;
      last_shape_id.size = temp_shape_id.size;
      return true;
    }
  } else {
    for (int i = 0; i < n_features; ++i) {
      const char* tag = font_info.features[i].feature;
      features[i].tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
      features[i].value = font_info.features[i].setting;
      features[i].start = 0;
      features[i].end = -1;
    }
    // Reset temp id so we don't haphazardly use a shaping with features
    temp_shape_id.string.clear();
    temp_shape_id.font.clear();
    temp_shape_id.index = 0;
    temp_shape_id.size = 0;
  }

  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  std::vector<int> embeddings = {};

  if (n_chars > 1) {
    std::string key(string);
    if (!bidi_cache.get(key, embeddings)) {
      embeddings = get_bidi_embeddings(utc_string, n_chars);
      bidi_cache.add(key, embeddings);
    }
  } else {
    embeddings.push_back(0);
  }

  last_shape_info.x_pos.clear();
  last_shape_info.glyph_id.clear();
  last_shape_info.width = 0;
  last_shape_info.font.clear();
  last_shape_info.fallbacks.clear();
  last_shape_info.fallbacks.push_back(font_info);

  size_t embedding_start = 0;
  for (size_t i = 1; i <= embeddings.size(); ++i) {
    if (i == embeddings.size() || embeddings[i] != embeddings[i - 1]) {
      shape_embedding(utc_string, embedding_start, i, n_chars, size, res, features);
      embedding_start = i;
    }
  }

  shape_cache.add(temp_shape_id, last_shape_info);
  last_shape_id.string.swap(temp_shape_id.string);
  last_shape_id.font.swap(temp_shape_id.font);
  last_shape_id.index = temp_shape_id.index;
  last_shape_id.size = temp_shape_id.size;
  //FT_Done_Face(face);
  return true;
}

void HarfBuzzShaper::reset() {
  glyph_id.clear();
  glyph_cluster.clear();
  string_id.clear();
  x_pos.clear();
  y_pos.clear();
  x_mid.clear();
  x_advance.clear();
  x_offset.clear();
  left_bear.clear();
  right_bear.clear();
  top_extend.clear();
  bottom_extend.clear();
  line_left_bear.clear();
  line_right_bear.clear();
  line_width.clear();
  line_id.clear();
  ascenders.clear();
  descenders.clear();
  may_break.clear();
  must_break.clear();

  pen_x = 0;
  pen_y = 0;

  top = 0;
  bottom = 0;
  ascend = 0;
  descend = 0;

  left_bearing = 0;
  right_bearing = 0;
  width = 0;
  height = 0;
  top_border = 0;
  left_border = 0;

  cur_string = 0;
}

bool HarfBuzzShaper::shape_glyphs(hb_font_t *font, const uint32_t *string, unsigned int n_chars) {
  hb_shape(font, buffer, NULL, 0);

  unsigned int n_glyphs = 0;
  hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
  hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);

  if (n_glyphs == 0) return true;

  glyph_id.reserve(n_glyphs);
  glyph_cluster.reserve(n_glyphs);
  string_id.reserve(n_glyphs);
  x_pos.reserve(n_glyphs);
  y_pos.reserve(n_glyphs);

#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
  ascend = 0;
  descend = 0;
#else
  hb_font_extents_t extent;
  hb_font_get_h_extents(font, &extent);

  ascend = extent.ascender;
  descend = extent.descender;
#endif

  for (unsigned int i = 0; i < n_glyphs; ++i) {
    unsigned int cluster = glyph_info[i].cluster;
    glyph_cluster.push_back(cluster);
    glyph_id.push_back(glyph_info[i].codepoint);

    if (cluster < n_chars) {
      may_break.push_back(glyph_is_breaker(string[cluster]));
      must_break.push_back(glyph_is_linebreak(string[cluster]));
    } else {
      may_break.push_back(false);
      must_break.push_back(false);
    }

    string_id.push_back(cur_string);

    hb_glyph_extents_t extent;
    hb_font_get_glyph_extents(font, glyph_info[i].codepoint, &extent);

    int32_t x_off = glyph_pos[i].x_offset;
    int32_t x_adv = glyph_pos[i].x_advance;
    x_advance.push_back(x_adv + cur_tracking);
    left_bear.push_back(extent.x_bearing);
    right_bear.push_back(x_adv - (extent.x_bearing + extent.width));
    top_extend.push_back(extent.y_bearing);
    bottom_extend.push_back(extent.height + extent.y_bearing);
    ascenders.push_back(ascend);
    descenders.push_back(descend);
    if (i == 0) {
      x_offset.push_back(0);
    } else {
      x_offset.push_back(x_off);
    }
  }
  return true;
}

bool HarfBuzzShaper::shape_embedding(const uint32_t* string, unsigned start,
                                     unsigned end, unsigned int string_length,
                                     double size, double res,
                                     std::vector<hb_feature_t>& features) {
  unsigned int embedding_size = end - start;

  if (embedding_size < 1) {
    return true;
  }

  int error = 0;
  FT_Face face = get_cached_face(last_shape_info.fallbacks[0].file,
                                 last_shape_info.fallbacks[0].index,
                                 size, res, &error);
  if (error != 0) {
    Rprintf("Failed to get face: %s, %i\n", last_shape_info.fallbacks[0].file, last_shape_info.fallbacks[0].index);
    error_code = error;
    return false;
  }
  hb_font_t *font = hb_ft_font_create(face, NULL);

  unsigned int n_glyphs = 0;
  hb_buffer_reset(buffer);
  hb_buffer_add_utf32(buffer, string, string_length, start, embedding_size);
  hb_buffer_guess_segment_properties(buffer);

  hb_shape(font, buffer, features.data(), features.size());

  hb_glyph_info_t *glyph_info = NULL;
  hb_glyph_position_t *glyph_pos = NULL;
  glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

  if (n_glyphs == 0) {
    hb_font_destroy(font);
    return true;
  }

  unsigned int current_font = 0;
  std::vector<unsigned int> char_font(embedding_size, current_font);
  bool needs_fallback = false;
  bool any_resolved = false;
  annotate_fallbacks(current_font + 1, 0, char_font, glyph_info, n_glyphs, needs_fallback, any_resolved);

  if (!needs_fallback) { // Short route - use existing shaping
    glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
    fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, 0);
    hb_font_destroy(font);
    return true;
  }
  hb_font_destroy(font);

  // Need to reset this in case the first annotation didn't find any hits in the first font
  any_resolved = true;
  // We need to figure out the font for each character, then redo shaping using that
  while (needs_fallback && any_resolved) {
    needs_fallback = false;
    any_resolved = false;

    ++current_font;
    unsigned int fallback_start = 0, fallback_end = 0;
    bool found = fallback_cluster(current_font, char_font, 0, fallback_start, fallback_end);
    if (!found) {
      // Think about handling this unlikely scenario;
      return false;
    }
    int error = 0;
    bool using_new = false;
    font = load_fallback(current_font, string, start + fallback_start, start + fallback_end, error, size, res, using_new);
    if (!using_new) {
      // We don't need to have success if we are trying an existing font
      any_resolved = true;
    }
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", last_shape_info.fallbacks[current_font].file, last_shape_info.fallbacks[current_font].index);
      error_code = error;
      return false;
    }
    do {
      hb_buffer_reset(buffer);
      hb_buffer_add_utf32(buffer, string, string_length, start + fallback_start, fallback_end - fallback_start);
      hb_buffer_guess_segment_properties(buffer);
      hb_shape(font, buffer, features.data(), features.size());
      glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

      if (n_glyphs > 0) {
        bool needs_fallback_2 = false;
        bool any_resolved_2 = false;
        annotate_fallbacks(current_font + 1, fallback_start, char_font, glyph_info, n_glyphs, needs_fallback_2, any_resolved_2);
        if (needs_fallback_2) needs_fallback = true;
        if (any_resolved_2) any_resolved = true;
      }
      hb_font_destroy(font);

      found = fallback_cluster(current_font, char_font, fallback_end, fallback_start, fallback_end);
    } while (found);
  }

  current_font = char_font[0];
  unsigned int text_run_start = 0;
  for (unsigned int i = 1; i <= embedding_size; ++i) {
    if (i == embedding_size || char_font[i] != current_font) {
      int error = 0;
      FT_Face face = get_cached_face(last_shape_info.fallbacks[current_font].file,
                                     last_shape_info.fallbacks[current_font].index,
                                     size, res, &error);
      if (error != 0) {
        Rprintf("Failed to get face: %s, %i\n", last_shape_info.fallbacks[current_font].file, last_shape_info.fallbacks[current_font].index);
        error_code = error;
        return false;
      }
      font = hb_ft_font_create(face, NULL);

      hb_buffer_reset(buffer);
      hb_buffer_add_utf32(buffer, string, string_length, start + text_run_start, i - text_run_start);
      hb_buffer_guess_segment_properties(buffer);
      hb_shape(font, buffer, features.data(), features.size());
      glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
      glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
      fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font);
      hb_font_destroy(font);

      if (i < embedding_size) {
        current_font = char_font[i];
        if (current_font >= last_shape_info.fallbacks.size()) current_font = 0;
        text_run_start = i;
      }
    }
  }

  return true;
}

void HarfBuzzShaper::annotate_fallbacks(unsigned int font, unsigned int offset,
                                        std::vector<unsigned int>& char_font,
                                        hb_glyph_info_t* glyph_info,
                                        unsigned int n_glyphs,
                                        bool& needs_fallback, bool& any_resolved) {
  unsigned int current_cluster = glyph_info[0].cluster;
  unsigned int cluster_start = 0;

  for (unsigned int i = 1; i <= n_glyphs; ++i) {
    if (i == n_glyphs || glyph_info[i].cluster != current_cluster) {
      bool has_glyph = true;
      for (unsigned int j = cluster_start; j < i; ++j) {
        if (glyph_info[j].codepoint == 0) {
          has_glyph = false;
        }
      }
      if (!has_glyph) {
        needs_fallback = true;
        for (unsigned int j = cluster_start; j < i; ++j) {
          char_font[j + offset] = font;
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

void HarfBuzzShaper::fill_shape_info(hb_glyph_info_t* glyph_info,
                                     hb_glyph_position_t* glyph_pos,
                                     unsigned int n_glyphs, hb_font_t* font,
                                     unsigned int font_id) {
  hb_glyph_extents_t extent;
  int32_t x = last_shape_info.width;
  for (unsigned int i = 0; i < n_glyphs; ++i) {
    if (last_shape_info.x_pos.size() == 0)  {
      hb_font_get_glyph_extents(font, glyph_info[i].codepoint, &extent);
      last_shape_info.left_bearing = extent.x_bearing;
    }
    last_shape_info.x_pos.push_back(x + glyph_pos[i].x_offset);
    last_shape_info.glyph_id.push_back(glyph_info[i].codepoint);
    last_shape_info.font.push_back(font_id);
    x += glyph_pos[i].x_advance;
  }
  last_shape_info.width = x;
  hb_font_get_glyph_extents(font, glyph_info[n_glyphs - 1].codepoint, &extent);
  last_shape_info.right_bearing = glyph_pos[n_glyphs - 1].x_advance - (extent.x_bearing + extent.width);
}

hb_font_t*  HarfBuzzShaper::load_fallback(unsigned int font, const uint32_t* string, unsigned int start, unsigned int end, int& error, double size, double res, bool& new_added) {

  new_added = false;
  if (font >= last_shape_info.fallbacks.size()) {
    int n_conv = 0;
    const char* fallback_string = utf_converter.convert_to_utf(string + start, end - start, n_conv);
    last_shape_info.fallbacks.push_back(
      get_fallback(fallback_string,
                   last_shape_info.fallbacks[0].file,
                   last_shape_info.fallbacks[0].index)
    );
    new_added = true;
  }
  FT_Face face = get_cached_face(last_shape_info.fallbacks[font].file,
                                 last_shape_info.fallbacks[font].index,
                                 size, res, &error);
  if (error == 0) {
    return hb_ft_font_create(face, NULL);
  } else {
    return NULL;
  }
}

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

#endif
