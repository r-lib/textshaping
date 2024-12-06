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

bool HarfBuzzShaper::shape_string(const char* string, FontSettings& font_info,
                                  double size, double res, double lineheight,
                                  int align, double hjust, double vjust, double width,
                                  double tracking, double ind, double hang, double before,
                                  double after, bool spacer) {
  reset();

  max_width = width + 1; // To prevent rounding errors
  indent = ind;
  hanging = hang;
  space_before = before;
  space_after = after;

  cur_res = res;
  cur_lineheight = lineheight;
  cur_align = align;
  cur_hjust = hjust;
  cur_vjust = vjust;

  return add_string(string, font_info, size, tracking, spacer);
}

bool HarfBuzzShaper::add_string(const char* string, FontSettings& font_info,
                                double size, double tracking, bool spacer) {
  if (spacer) {
    return add_spacer(font_info, size, tracking);
  }

  error_code = 0;
  shape_infos.push_back(shape_text_run(string, font_info, size, cur_res, tracking));

  if (error_code != 0) {
    shape_infos.pop_back();
    return false;
  }
  //FT_Done_Face(face);
  return true;
}
bool HarfBuzzShaper::add_spacer(FontSettings& font_info, double height, double width) {
  width *= 64.0 / 72.0;
  int32_t ascend = height * 64.0 * cur_res / 72.0;
  int32_t descend = 0;
#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
#else
  int error = 0;
  hb_font_t *font = hb_ft_font_create(get_cached_face(font_info.file, font_info.index, height, cur_res, &error), NULL);
  if (error != 0) {
    Rprintf("Failed to get face: %s, %i\n", font_info.file, font_info.index);
    error_code = error;
  } else {
    hb_font_extents_t fextent;
    hb_font_get_h_extents(font, &fextent);
    ascend = fextent.ascender;
    descend = fextent.descender;
  }
#endif
  FontSettings dummy_font = {"", 0, NULL, 0};
  ShapeInfo info = {
    {0}, // glyph_id
    {0}, // glyph_cluster
    {int32_t(width)}, // x_advance
    {0}, // y_advance
    {0}, // x_offset
    {0}, // y_offset
    {0}, // x_bear
    {int32_t(ascend)}, // y_bear
    {int32_t(width)}, // width
    {int32_t(ascend - descend)}, // height
    {int32_t(ascend)}, // ascenders
    {descend}, // descenders
    {false}, // may_break
    {false}, // must_break
    {false}, // may_stretch
    {0}, // font
    {dummy_font}, // fallbacks
    {height}, // fallback_size
    {-1}, // fallback_scaling
    true // ltr
  };
  shape_infos.push_back(info);
  return true;
}

bool HarfBuzzShaper::finish_string() {
  if (shape_infos.empty()) {
    return true;
  }
  bool vertical = false;

  if (vertical) {
    pen_y = indent;
    pen_x = 0;
  } else {
    pen_x = indent;
    pen_y = 0;
  }

  bool first_char = true;
  bool first_line = true;
  int cur_line = 0;
  int32_t max_descend = 0;
  int32_t max_ascend = 0;
  int32_t max_top_extend = 0;
  int32_t max_bottom_extend = 0;
  int32_t last_max_descend = 0;
  int breaktype = 0;
  int32_t cur_linewidth = 0;
  int32_t cur_left_bear = 0;
  int32_t cur_right_bear = 0;
  size_t last_line_start = 0;

  for (size_t j = 0; j < shape_infos.size(); ++j) {
    ShapeInfo& cur_shape = shape_infos[j];
    bool last_shape = j == shape_infos.size() - 1;
    size_t start = cur_shape.ltr ? 0 : cur_shape.glyph_id.size();

    while (true) {
      size_t end;
      bool last;
      if (cur_shape.glyph_id.size() == 0) {
        end = 0;
        last = last_shape;
        max_ascend = std::max(max_ascend, cur_shape.ascenders[0]);
        max_descend = std::min(max_descend, cur_shape.descenders[0]);
      } else {
        end = fill_out_width(start, max_width - pen_x, j, breaktype);
        last = last_shape && end == (cur_shape.ltr ? cur_shape.glyph_id.size() : 0);
        for (size_t i = std::min(start, end); i < std::max(start, end); ++i) {
          glyph_id.push_back(cur_shape.glyph_id[i]);
          glyph_cluster.push_back(cur_shape.glyph_cluster[i]);
          fontfile.push_back(cur_shape.fallbacks[cur_shape.font[i]].file);
          fontindex.push_back(cur_shape.fallbacks[cur_shape.font[i]].index);
          fontsize.push_back(cur_shape.fallback_size[cur_shape.font[i]]);
          advance.push_back(cur_shape.x_advance[i]);
          ascender.push_back(cur_shape.ascenders[i]);
          descender.push_back(cur_shape.descenders[i]);

          string_id.push_back(j);
          line_id.push_back(cur_line);
          x_pos.push_back(pen_x + cur_shape.x_offset[i]);
          y_pos.push_back(pen_y + cur_shape.y_offset[i]);

          must_break.push_back(cur_shape.must_break[i]);
          may_stretch.push_back(cur_shape.may_stretch[i]);

          if (vertical) {
            pen_y += cur_shape.y_advance[i];
            if (first_char) cur_left_bear = cur_shape.y_bear[i];
            if (!cur_shape.may_break[i] || (cur_shape.ltr && first_char)) {
              cur_linewidth += pen_y;
              cur_right_bear = cur_shape.y_bear[i] + cur_shape.height[i];
            }

            // TODO: Awaiting knowledge on glyph metrics returned with vertical layouts
            max_ascend = std::max(max_ascend, cur_shape.ascenders[i]);
            max_top_extend = std::max(max_top_extend, cur_shape.y_bear[i]);
            max_descend = std::max(max_descend, cur_shape.descenders[i]);
            max_bottom_extend = std::max(max_bottom_extend, cur_shape.height[i] + cur_shape.y_bear[i]);


          } else {
            pen_x += cur_shape.x_advance[i];
            if (first_char) cur_left_bear = cur_shape.x_bear[i];
            if ((!cur_shape.may_break[i] && !cur_shape.must_break[i]) || (cur_shape.ltr && first_char)) {
              cur_linewidth = pen_x;
              cur_right_bear = cur_shape.x_advance[i] - cur_shape.x_bear[i] - cur_shape.width[i];
            }

            max_ascend = std::max(max_ascend, cur_shape.ascenders[i]);
            max_top_extend = std::max(max_top_extend, cur_shape.y_bear[i]);
            max_descend = std::min(max_descend, cur_shape.descenders[i]);
            max_bottom_extend = std::min(max_bottom_extend, cur_shape.height[i] + cur_shape.y_bear[i]);
          }

          if (!cur_shape.may_break[i]) first_char = false;
        }
      }

      if (breaktype != 0 || last) {
        line_width.push_back(cur_linewidth);
        line_left_bear.push_back(cur_left_bear);
        line_right_bear.push_back(cur_right_bear);

        if (first_line) {
          top_border = max_ascend + space_before;
          top_bearing = top_border - max_top_extend;
        }

        double line_height = first_line ? 0 : (max_ascend - last_max_descend) * cur_lineheight;
        for (size_t k = last_line_start; k < y_pos.size(); ++k) {
          if (vertical) {
            x_pos[k] -= line_height;
          } else {
            y_pos[k] -= line_height;
          }
        }
        if (vertical) {
          pen_x -= line_height + (breaktype == 2 ? space_after + (last ? 0 : space_before) : 0);
          pen_y = last ? (breaktype != 0 ? 0 : pen_y) : (breaktype == 1 ? hanging : indent);

        } else {
          pen_y -= line_height + (breaktype == 2 ? space_after + (last ? 0 : space_before) : 0);
          pen_x = last ? (breaktype != 0 ? 0 : pen_x) : (breaktype == 1 ? hanging : indent);
        }

        last_max_descend = max_descend;
        cur_linewidth = 0;
        cur_left_bear = 0;
        cur_right_bear = 0;
        first_line = false;
        cur_line++;
        first_char = true;
        if (!last) { // We need this for calculating box dimensions
          max_ascend = 0;
          max_descend = 0;
          max_top_extend = 0;
          max_bottom_extend = 0;
        } else if (breaktype == 2) {
          pen_y -= (max_ascend - last_max_descend) * cur_lineheight + space_before;
          max_bottom_extend = 0;
        }
      }
      if (breaktype != 0) {
        last_line_start = y_pos.size();
      }
      if (end == 0 || end == cur_shape.glyph_id.size()) {
        break;
      }
      start = end;
    }
  }

  int32_t bottom = pen_y + max_descend - space_after;
  height = top_border - bottom;
  bottom_bearing = max_bottom_extend - max_descend + space_after;
  int max_width_ind = std::max_element(line_width.begin(), line_width.end()) - line_width.begin();
  width = max_width < 0 ? line_width[max_width_ind] : max_width;
  if (cur_align == 1 || cur_align == 2) {
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      int32_t lwd = line_width[index];
      x_pos[i] = cur_align == 1 ? x_pos[i] + width/2 - lwd/2 : x_pos[i] + width - lwd;
    }
    pen_x = cur_align == 1 ? pen_x + width/2 - line_width.back()/2 : pen_x + width - line_width.back();
  }
  if (cur_align == 3 || cur_align == 4 || cur_align == 5) {
    std::vector<size_t> n_stretches(line_width.size(), 0);
    std::vector<bool> no_stretch(line_width.size(), false);
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      no_stretch[index] = no_stretch[index] || index == line_width.size() - 1 || must_break[i];
      if (may_stretch[i] && i-1 < x_pos.size() && index == line_id[i+1]) {
        n_stretches[index]++;
      }
    }
    int32_t cum_move = 0;
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      int32_t lwd = line_width[index];
      if (no_stretch[index]) {
        if (cur_align == 4) {
          x_pos[i] = x_pos[i] + width/2 - lwd/2;
        } else if (cur_align == 5) {
          x_pos[i] = x_pos[i] + width - lwd;
        }
        continue;
      }
      if (i == 0 || line_id[i-1] != index) {
        cum_move = 0;
      }
      x_pos[i] += cum_move;
      if (n_stretches[index] == 0) {
        if (cur_align == 4) {
          x_pos[i] = x_pos[i] + width/2 - lwd/2;
        } else if (cur_align == 5) {
          x_pos[i] = x_pos[i] + width - lwd;
        }
      }
      if (may_stretch[i]) {
        cum_move += (width - line_width[index]) / n_stretches[index];
      }
    }
    pen_x += cum_move;
    if (no_stretch.back() || n_stretches.back() == 0) {
      if (cur_align == 4) {
        pen_x += width/2 - line_width.back()/2;
      } else if (cur_align == 5) {
        pen_x += width - line_width.back();
      }
    }
    for (size_t i = 0; i < line_width.size(); ++i) {
      if (!no_stretch[i] && n_stretches[i] != 0) line_width[i] = width;
    }
  }
  if (cur_align == 6) {
    std::vector<size_t> n_glyphs(line_width.size(), 0);
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      if (!must_break[i] && (i == x_pos.size()-1 || index == line_id[i+1])) {
        n_glyphs[index]++;
      }
    }
    int32_t cum_move = 0;
    for (size_t i = 0; i < x_pos.size(); ++i) {
      int index = line_id[i];
      if (i == 0 || line_id[i-1] != index) {
        cum_move = 0;
      }
      x_pos[i] += cum_move;
      cum_move += (width - line_width[index]) / (n_glyphs[index]-1);
    }
    pen_x += cum_move;
    for (size_t i = 0; i < line_width.size(); ++i) {
      if (n_glyphs[i] != 0) line_width[i] = width;
    }
  }
  double width_diff = width - line_width[max_width_ind];
  if (cur_align == 1) {
    width_diff /= 2;
  }
  left_bearing = cur_align == 0 || cur_align == 3 || cur_align == 5 ? *std::min_element(line_left_bear.begin(), line_left_bear.end()) : line_left_bear[max_width_ind] + width_diff;
  right_bearing = cur_align == 2 || cur_align == 4 || cur_align == 5 ? *std::min_element(line_right_bear.begin(), line_right_bear.end()) : line_right_bear[max_width_ind] + width_diff;

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
  return true;
}

void HarfBuzzShaper::reset() {
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
  line_left_bear.clear();
  line_right_bear.clear();
  line_width.clear();
  line_id.clear();
  must_break.clear();
  may_stretch.clear();
  shape_infos.clear();

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

ShapeInfo HarfBuzzShaper::shape_text_run(const char* string, FontSettings& font_info,
                                         double size, double res, double tracking) {
  int n_features = font_info.n_features;
  std::vector<hb_feature_t> features(n_features);
  ShapeInfo text_run;
  ShapeID run_id;
  if (n_features == 0) {
    run_id.string.assign(string);
    run_id.font.assign(font_info.file);
    run_id.index = font_info.index;
    run_id.size = size * res;
    run_id.tracking = tracking;
    if (shape_cache.get(run_id, text_run)) {
      return text_run;
    }
  } else {
    for (int i = 0; i < n_features; ++i) {
      const char* tag = font_info.features[i].feature;
      features[i].tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
      features[i].value = font_info.features[i].setting;
      features[i].start = 0;
      features[i].end = -1;
    }
  }

  text_run.fallbacks.push_back(font_info);

  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  if (n_chars == 0) {
    text_run.ascenders.push_back(0);
    text_run.descenders.push_back(0);
#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
#else
    int error = 0;
    hb_font_t *font = hb_ft_font_create(get_cached_face(font_info.file, font_info.index, size, res, &error), NULL);
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", font_info.file, font_info.index);
      error_code = error;
    } else {
      hb_font_extents_t fextent;
      hb_font_get_h_extents(font, &fextent);
      text_run.ascenders[0] = fextent.ascender;
      text_run.descenders[0] = fextent.descender;
    }
#endif
    return text_run;
  }

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

  bool may_have_emoji = false;
  for (int i = 0; i < n_chars; ++i) {
    if (utc_string[i] >= 8205) {
      may_have_emoji = true;
      break;
    }
  }
  if (may_have_emoji) {
    std::vector<int> emoji_embeddings = {};
    emoji_embeddings.resize(n_chars);
    detect_emoji_embedding(utc_string, n_chars, emoji_embeddings.data(), font_info.file, font_info.index);
    bool emoji_font_added = false;
    for (int i = 0; i < n_chars; ++i) {
      if (emoji_embeddings[i] == 1) {
        embeddings[i] = 2;
        if (!emoji_font_added) {
          text_run.fallbacks.push_back(locate_font_with_features("emoji", 0, 0));
          emoji_font_added = true;
        }
      }
    }
  }

  size_t embedding_start = 0;
  for (size_t i = 1; i <= embeddings.size(); ++i) {
    if (i == embeddings.size() || embeddings[i] != embeddings[i - 1]) {
      bool success = shape_embedding(utc_string, embedding_start, i, n_chars, size, res, tracking, features, embeddings[embedding_start] == 2, text_run);
      if (!success) return ShapeInfo();
      embedding_start = i;
    }
  }

  if (n_features == 0) {
    shape_cache.add(run_id, text_run);
  }
  //FT_Done_Face(face);
  return text_run;
}

bool HarfBuzzShaper::shape_embedding(const uint32_t* string, unsigned start,
                                     unsigned end, unsigned int string_length,
                                     double size, double res, double tracking,
                                     std::vector<hb_feature_t>& features,
                                     bool emoji, ShapeInfo& shape_info) {
  unsigned int embedding_size = end - start;

  if (embedding_size < 1) {
    return true;
  }

  int error = 0;
  FT_Face face = get_cached_face(shape_info.fallbacks[0].file,
                                 shape_info.fallbacks[0].index,
                                 size, res, &error);

  if (error != 0) {
    Rprintf("Failed to get face: %s, %i\n", shape_info.fallbacks[0].file, shape_info.fallbacks[0].index);
    error_code = error;
    return false;
  }

  if (shape_info.fallback_scaling.empty()) {
    double scaling = FT_IS_SCALABLE(face) ? -1 : size * 64.0 * res / 72.0 / face->size->metrics.height;
    double fscaling = family_scaling(face->family_name);
    shape_info.fallback_scaling.push_back(scaling * fscaling);
    shape_info.fallback_size.push_back(size * fscaling);
  }

  if (emoji) {
    face = get_cached_face(shape_info.fallbacks[1].file,
                           shape_info.fallbacks[1].index,
                           size, res, &error);
    if (error != 0) {
      Rprintf("Failed to get emoji face: %s, %i\n", shape_info.fallbacks[1].file, shape_info.fallbacks[1].index);
      error_code = error;
      return false;
    }

    if (shape_info.fallback_scaling.size() == 1) {
      double scaling = FT_IS_SCALABLE(face) ? -1 : size * 64.0 * res / 72.0 / face->size->metrics.height;
      double fscaling = family_scaling(face->family_name);
      shape_info.fallback_scaling.push_back(scaling * fscaling);
      shape_info.fallback_size.push_back(size * fscaling);
    }
  }

  hb_font_t *font = hb_ft_font_create(face, NULL);

  unsigned int n_glyphs = 0;
  hb_buffer_reset(buffer);
  hb_buffer_add_utf32(buffer, string, string_length, start, embedding_size);
  hb_buffer_guess_segment_properties(buffer);
  hb_glyph_info_t *glyph_info = NULL;
  glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

  hb_shape(font, buffer, features.data(), features.size());

  hb_glyph_position_t *glyph_pos = NULL;
  glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

  if (n_glyphs == 0) {
    hb_font_destroy(font);
    return true;
  }

  shape_info.ltr = true;
  for (unsigned int i = 1; i < n_glyphs; ++i) {
    if (glyph_info[i - 1].cluster == glyph_info[i].cluster) {
      continue;
    }
    shape_info.ltr = glyph_info[i - 1].cluster < glyph_info[i].cluster;
    break;
  }

  unsigned int current_font = emoji ? 1 : 0;
  std::vector<unsigned int> char_font(embedding_size, current_font);
  bool needs_fallback = false;
  bool any_resolved = false;
  annotate_fallbacks(current_font + 1, 0, char_font, glyph_info, n_glyphs, needs_fallback, any_resolved, shape_info.ltr, start);

  if (!needs_fallback) { // Short route - use existing shaping
    glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
    fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start, shape_info, tracking);
    fill_glyph_info(string, start + embedding_size, shape_info);
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
    font = load_fallback(current_font, string, start + fallback_start, start + fallback_end, error, size, res, using_new, shape_info);
    if (!using_new) {
      // We don't need to have success if we are trying an existing font
      any_resolved = true;
    }
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", shape_info.fallbacks[current_font].file, shape_info.fallbacks[current_font].index);
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
        annotate_fallbacks(current_font + 1, fallback_start, char_font, glyph_info, n_glyphs, needs_fallback_2, any_resolved_2, shape_info.ltr, start);
        if (needs_fallback_2) needs_fallback = true;
        if (any_resolved_2) any_resolved = true;
      }

      found = fallback_cluster(current_font, char_font, fallback_end, fallback_start, fallback_end);
    } while (found);

    hb_font_destroy(font);
  }

  // Make sure char_font does not point to non-existing fonts
  for (size_t i = 0; i < char_font.size(); ++i) {
    if (char_font[i] >= shape_info.fallbacks.size()) {
      char_font[i] = 0;
    }
  }

  if (shape_info.ltr) {
    current_font = char_font[0];
    unsigned int text_run_start = 0;
    for (unsigned int i = 1; i <= embedding_size; ++i) {
      if (i == embedding_size || char_font[i] != current_font) {
        int error = 0;
        FT_Face face = get_cached_face(shape_info.fallbacks[current_font].file,
                                       shape_info.fallbacks[current_font].index,
                                       size, res, &error);
        if (error != 0) {
          Rprintf("Failed to get face: %s, %i\n", shape_info.fallbacks[current_font].file, shape_info.fallbacks[current_font].index);
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
        fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start + text_run_start, shape_info, tracking);
        fill_glyph_info(string, start + i, shape_info);
        hb_font_destroy(font);

        if (i < embedding_size) {
          current_font = char_font[i];
          if (current_font >= shape_info.fallbacks.size()) current_font = 0;
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
        FT_Face face = get_cached_face(shape_info.fallbacks[current_font].file,
                                       shape_info.fallbacks[current_font].index,
                                       size, res, &error);
        if (error != 0) {
          Rprintf("Failed to get face: %s, %i\n", shape_info.fallbacks[current_font].file, shape_info.fallbacks[current_font].index);
          error_code = error;
          return false;
        }

        font = hb_ft_font_create(face, NULL);

        hb_buffer_reset(buffer);
        hb_buffer_add_utf32(buffer, string, string_length, start + i, text_run_end - i);
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(font, buffer, features.data(), features.size());
        glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
        glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
        fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start + i, shape_info, tracking);
        fill_glyph_info(string, start + text_run_end, shape_info);
        hb_font_destroy(font);

        if (i > 0) {
          current_font = char_font[i - 1];
          if (current_font >= shape_info.fallbacks.size()) current_font = 0;
          text_run_end = i;
        }
      }
    }
  }

  return true;
}

hb_font_t*  HarfBuzzShaper::load_fallback(unsigned int font, const uint32_t* string, unsigned int start, unsigned int end, int& error, double size, double res, bool& new_added, ShapeInfo& shape_info) {

  new_added = false;
  if (font >= shape_info.fallbacks.size()) {
    int n_conv = 0;
    const char* fallback_string = utf_converter.convert_to_utf(string + start, end - start, n_conv);
    shape_info.fallbacks.push_back(
      get_fallback(fallback_string,
                   shape_info.fallbacks[0].file,
                   shape_info.fallbacks[0].index)
    );
    new_added = true;
  }
  FT_Face face = get_cached_face(shape_info.fallbacks[font].file,
                                 shape_info.fallbacks[font].index,
                                 size, res, &error);

  if (error != 0) {
    return NULL;
  }

  if (font >= shape_info.fallback_scaling.size()) {
    double scaling = FT_IS_SCALABLE(face) ? -1 : size * 64.0 * res / 72.0 / face->size->metrics.height;
    double fscaling = family_scaling(face->family_name);
    shape_info.fallback_scaling.push_back(scaling * fscaling);
    shape_info.fallback_size.push_back(size * fscaling);
  }

  return hb_ft_font_create(face, NULL);
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

void HarfBuzzShaper::annotate_fallbacks(unsigned int font, unsigned int offset,
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
        next_cluster = i < n_glyphs ? glyph_info[i].cluster : char_font.size() + string_offset;
      } else {
        next_cluster = cluster_start > 0 ? glyph_info[cluster_start - 1].cluster : char_font.size() + string_offset;
      }
      bool has_glyph = true;
      for (unsigned int j = cluster_start; j < i; ++j) {
        if (glyph_info[j].codepoint == 0) {
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

void HarfBuzzShaper::fill_shape_info(hb_glyph_info_t* glyph_info,
                                     hb_glyph_position_t* glyph_pos,
                                     unsigned int n_glyphs, hb_font_t* font,
                                     unsigned int font_id,
                                     unsigned int cluster_offset,
                                     ShapeInfo& shape_info, int32_t tracking) {
  double scaling = shape_info.fallback_scaling[font_id];
  if (scaling < 0) scaling = 1.0;

  tracking *= shape_info.fallback_size[font_id] / 1000;


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

  int new_size = shape_info.glyph_id.size() + n_glyphs;
  shape_info.glyph_id.reserve(new_size);
  shape_info.glyph_cluster.reserve(new_size);
  shape_info.x_offset.reserve(new_size);
  shape_info.y_offset.reserve(new_size);
  shape_info.x_advance.reserve(new_size);
  shape_info.y_advance.reserve(new_size);
  shape_info.x_bear.reserve(new_size);
  shape_info.y_bear.reserve(new_size);
  shape_info.width.reserve(new_size);
  shape_info.height.reserve(new_size);
  shape_info.ascenders.reserve(new_size);
  shape_info.descenders.reserve(new_size);
  shape_info.font.reserve(new_size);

  for (unsigned int i = 0; i < n_glyphs; ++i) {
    shape_info.glyph_id.push_back(glyph_info[i].codepoint);
    shape_info.glyph_cluster.push_back(glyph_info[i].cluster);
    shape_info.x_offset.push_back(glyph_pos[i].x_offset * scaling);
    shape_info.y_offset.push_back(glyph_pos[i].y_offset * scaling);
    shape_info.x_advance.push_back(glyph_pos[i].x_advance * scaling + tracking);
    shape_info.y_advance.push_back(glyph_pos[i].y_advance * scaling);

    hb_font_get_glyph_extents(font, glyph_info[i].codepoint, &extent);
    shape_info.x_bear.push_back(extent.x_bearing * scaling);
    shape_info.y_bear.push_back(extent.y_bearing * scaling);
    shape_info.width.push_back(extent.width * scaling);
    shape_info.height.push_back(extent.height * scaling);

    shape_info.ascenders.push_back(ascend * scaling);
    shape_info.descenders.push_back(descend * scaling);

    shape_info.font.push_back(font_id);

  }
}

void HarfBuzzShaper::fill_glyph_info(const uint32_t* string, int end,
                                     ShapeInfo& shape_info) {
  for (size_t i = shape_info.must_break.size(); i < shape_info.glyph_cluster.size(); ++i) {
    int32_t cluster = shape_info.glyph_cluster[i];
    if (cluster < end) {
      shape_info.must_break.push_back(glyph_is_linebreak(string[cluster]));
      shape_info.may_break.push_back(glyph_is_breaker(string[cluster]));
      shape_info.may_stretch.push_back(glyph_may_stretch(string[cluster]));
    } else {
      shape_info.must_break.push_back(false);
      shape_info.may_break.push_back(false);
      shape_info.may_stretch.push_back(false);
    }
  }
}

size_t HarfBuzzShaper::fill_out_width(size_t from, int32_t max,
                                      size_t shape, int& breaktype) {
  int32_t w = 0;
  size_t last_possible_break = from;
  bool has_break = false;
  breaktype = 0;
  if (shape_infos[shape].ltr) {
    for (size_t i = from; i < shape_infos[shape].glyph_id.size(); ++i) {
      if (shape_infos[shape].must_break[i]) {
        breaktype = 2;
        return i + 1;
      }

      if (max < 0) continue;

      if (shape_infos[shape].may_break[i]) {
        last_possible_break = i;
        has_break = true;
      }
      w += shape_infos[shape].x_advance[i];
      if (w > max) {
        breaktype = 1;
        return has_break ? last_possible_break + 1 : i;
      }
    }
    size_t next_shape = shape + 1;
    while (next_shape < shape_infos.size()) {
      for (size_t i = 0; i < shape_infos[next_shape].glyph_id.size(); ++i) {
        if (shape_infos[next_shape].must_break[i] || shape_infos[next_shape].may_break[i]) {
          return shape_infos[shape].glyph_id.size();
        }
        w += shape_infos[next_shape].x_advance[i];
        if (w > max) {
          breaktype = has_break ? 1 : 0;
          return has_break ? last_possible_break + 1 : shape_infos[shape].glyph_id.size();
        }
      }
      next_shape++;
    }
    last_possible_break = shape_infos[shape].glyph_id.size();
  } else {
    for (size_t i = from - 1; i >= 0; --i) {
      if (shape_infos[shape].must_break[i]) {
        breaktype = 2;
        return i + 1;
      }

      if (max < 0) continue;

      if (shape_infos[shape].may_break[i]) {
        last_possible_break = i;
        has_break = true;
      }
      w += shape_infos[shape].x_advance[i];
      if (w > max) {
        breaktype = 1;
        return has_break ? last_possible_break : i + 1;
      }
    }
    size_t next_shape = shape + 1;
    while (next_shape < shape_infos.size()) {
      for (size_t i = shape_infos[next_shape].glyph_id.size() - 1; i >= 0; --i) {
        if (shape_infos[next_shape].must_break[i] || shape_infos[shape].may_break[i]) {
          return 0;
        }
        w += shape_infos[next_shape].x_advance[i];
        if (w > max) {
          breaktype = has_break ? 1 : 0;
          return has_break ? last_possible_break : 0;
        }
      }
      next_shape++;
    }
    last_possible_break = 0;
  }
  return last_possible_break;
}

#endif
