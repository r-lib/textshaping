#ifndef NO_HARFBUZZ_FRIBIDI

#include <climits>
#include <iterator>
#include <list>
#include <vector>

#include <hb-ft.h>
#include "string_shape.h"
#include "string_bidi.h"
#include <systemfonts.h>
#include <systemfonts-ft.h>
#include <algorithm>

UTF_UCS HarfBuzzShaper::utf_converter = UTF_UCS();
LRU_Cache<BidiID, std::vector<int> > HarfBuzzShaper::bidi_cache = {1000};
LRU_Cache<ShapeID, ShapeInfo> HarfBuzzShaper::shape_cache = {1000};

bool HarfBuzzShaper::shape_string(const char* string, FontSettings& font_info,
                                  double size, double res, double lineheight,
                                  int align, double hjust, double vjust, double width,
                                  double tracking, double ind, double hang, double before,
                                  double after, bool spacer, int direction,
                                  std::vector<int>& soft_wrap,
                                  std::vector<int>& hard_wrap) {
  reset();

  // Set global settings
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

  dir = direction;

  return add_string(string, font_info, size, tracking, spacer, soft_wrap, hard_wrap);
}

bool HarfBuzzShaper::add_string(const char* string, FontSettings& font_info,
                                double size, double tracking, bool spacer,
                                std::vector<int>& soft_wrap,
                                std::vector<int>& hard_wrap) {
  if (spacer) {
    return add_spacer(font_info, size, tracking);
  }

  size_t run_start = full_string.size();

  // Convert string to UTC and add it to the global string
  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  if (n_chars == 0) {
    // Empty run - we treat is as a 0-width spacer to capture the font ascend and descend
    return add_spacer(font_info, size, 0);
  }

  full_string.insert(full_string.end(), utc_string, utc_string + n_chars);

  size_t run_end = full_string.size();

  unsigned int index = shape_infos.size();

  // Add precalculated soft and hard breeak points to the sets
  for (auto iter = soft_wrap.begin(); iter != soft_wrap.end(); ++iter) {
    soft_break.insert(run_start + (*iter) - 1);
  }
  for (auto iter = hard_wrap.begin(); iter != hard_wrap.end(); ++iter) {
    hard_break.insert(run_start + (*iter) - 1);
  }

  // Add run info to list
  shape_infos.emplace_back(run_start, run_end, font_info, index, size, cur_res, tracking);

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
  ShapeInfo info;
  info.index = shape_infos.size();
  info.run_start = info.run_end = full_string.size();

  // A spacer is encoded as an "unknown glyph" with ascend giving the height and
  //  width/x-advance giving the width
  info.font_info = font_info;
  info.embeddings.push_back({
    {SPACER_CHAR}, // glyph_id
    {0}, // glyph_cluster
    {info.index}, // string_id
    {int32_t(width)}, // x_advance
    {0}, // y_advance
    {0}, // x_offset
    {0}, // y_offset
    {0}, // x_bear
    {ascend}, // y_bear
    {int32_t(width)}, // width
    {ascend - descend}, // height
    {ascend}, // ascenders
    {descend}, // descenders
    {false}, // is_blank
    {false}, // may_break
    {false}, // may_stretch
    {0}, // font
    {dummy_font}, // fallbacks
    {height}, // fallback_size
    {-1}, // fallback_scaling
    0, // ltr
    int32_t(width), // full_width
    false // Not a line breaker in itself
  });
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
  uint32_t break_char;
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
    line = get_next_line_at_width(max_width - cur_line_indent, final_embeddings, hard_break, break_char);
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
        if (iter->glyph_id[i] != SPACER_CHAR) {
          glyph_id.push_back(iter->glyph_id[i]);
          glyph_cluster.push_back(iter->glyph_cluster[i]);
          fontfile.push_back(iter->fallbacks[iter->font[i]].file);
          fontindex.push_back(iter->fallbacks[iter->font[i]].index);
          fontsize.push_back(iter->fallback_size[iter->font[i]]);
          advance.push_back(iter->x_advance[i]);
          ascender.push_back(iter->ascenders[i]);
          descender.push_back(iter->descenders[i]);

          string_id.push_back(iter->string_id[i]);
          line_id.push_back(line_width.size() - 1);
          x_pos.push_back(pen_x + iter->x_offset[i]);

          may_stretch.push_back(iter->may_stretch[i]);
        }


        line_ascend = std::max(line_ascend, iter->ascenders[i]);

        // Only needed for first line
        if (line_width.size() == 1) {
          top_bearing = std::max(top_bearing, iter->y_bear[i]);
        }
        // Only needed for last line
        if (final_embeddings.empty()) {
          bottom_bearing = std::min(bottom_bearing, iter->height[i] + iter->y_bear[i]);
        }

        // If first character record left bearing
        if (first_char) {
          line_left_bear.back() = iter->x_bear[i];
        }

        // Advance pen to next position
        pen_x += iter->x_advance[i];

        // Continuously update this info
        if (!iter->is_blank[i]) {
          line_right_bear.back() = iter->x_advance[i] - iter->x_bear[i] - iter->width[i];
          line_width.back() = pen_x;
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
        if (iter->glyph_id[i] != SPACER_CHAR) {
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

    // Reset pen to next line
    if (!final_embeddings.empty() || hard_break) {
      cur_line_indent = hard_break ? indent : hanging;
      pen_x = 0;
      if (hard_break) pen_y -= space_after + space_before;
    }
  }

  if (hard_break) {
    // Last line ended with a line break. We move pen_y down based on last glyph size
    size_t last_glyph = line.back().embedding_level % 2 == 0 ? line.back().glyph_id.size() : 0;
    int32_t line_height = (line.back().ascenders[last_glyph] - line.back().descenders[last_glyph]) * cur_lineheight;
    pen_y -= line_height;
    bottom_bearing += line_height;
    pen_x = indent;
    line_left_extra = 0;
    line_width.push_back(pen_x);
  }

  // If rtl, the pen is placed at the left
  if (!ltr) pen_x = -line_left_extra;

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
  return true;
}

void HarfBuzzShaper::reset() {
  full_string.clear();
  bidi_embedding.clear();
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
  // Find bidi embeddings and determine the overall direction of the text
  if (full_string.size() > 1) {
    // If we have more than one char we find bidi embeddings
    // We append the direction to the end in the cache so we can read it back
    BidiID key = {vector_hash(full_string.begin(), full_string.end()), direction};
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
    if (iter->embeddings.size() == 0) { // avoid shaping spacers
      shape_text_run(*iter, ltr);
    } else {
      // We adopt the embedding level of the prior embedding for spacers
      int level = all_embeddings.size() == 0 ? (ltr ? 0 : 1) : all_embeddings.back().embedding_level;
      iter->embeddings[0].embedding_level = level;
    }
    iter->add_index(i++);
    all_embeddings.insert(all_embeddings.end(), std::make_move_iterator(iter->embeddings.begin()), std::make_move_iterator(iter->embeddings.end()));
    iter->embeddings.clear();
  }

  // Shortcut for simplest case
  if (all_embeddings.size() == 1) return all_embeddings;

  // Reverse ordering of consecutive embeddings in rtl
  // This is needed to keep their internal order during shaping
  // Further collapses all consequtive runs into one embedding to simplify the final shapping operation
  auto run_start = all_embeddings.begin();
  int run_embed_level = run_start->embedding_level;
  std::list<EmbedInfo> final_embeddings;
  for (auto iter = std::next(run_start); iter != all_embeddings.end(); ++iter) {
    int cur_embed_level = iter->embedding_level;

    if (std::prev(iter)->terminates_paragraph || run_embed_level != cur_embed_level) {
      if (run_embed_level % 2 != 0) {
        std::reverse(run_start, iter);
      }
      run_embed_level = cur_embed_level;

      for (auto iter2 = std::next(run_start); iter2 != iter; ++iter2) {
        run_start->add(*iter2);
      }
      final_embeddings.push_back(std::move(*run_start));
      run_start = iter;
    }
  }
  if (run_embed_level % 2 != 0) std::reverse(run_start, all_embeddings.end());
  for (auto iter2 = std::next(run_start); iter2 != all_embeddings.end(); ++iter2) {
    run_start->add(*iter2);
  }
  final_embeddings.push_back(std::move(*run_start));

  return final_embeddings;
}

void HarfBuzzShaper::shape_text_run(ShapeInfo &text_run, bool ltr) {
  int n_features = text_run.font_info.n_features;
  std::vector<hb_feature_t> features(n_features);
  ShapeID run_id;
  if (n_features == 0) {
    // No features. This is a simple string and if we have already seen it it may be in the cache
    run_id.string_hash = vector_hash(full_string.begin() + text_run.run_start, full_string.begin() + text_run.run_end);
    run_id.embed_hash = vector_hash(bidi_embedding.begin() + text_run.run_start, bidi_embedding.begin() + text_run.run_end);
    run_id.font.assign(text_run.font_info.file);
    run_id.index = text_run.font_info.index;
    run_id.size = text_run.size * text_run.res;
    run_id.tracking = text_run.tracking;
    if (shape_cache.get(run_id, text_run)) {
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
  std::vector<FontSettings> fallback = {text_run.font_info};
  std::vector<double> fallback_size;
  std::vector<double> fallback_scaling;

  // Get scaling and sizing info for the default font
  if (!get_font_sizing(fallback.back(), text_run.size, text_run.res, fallback_size, fallback_scaling)) {
    return;
  }

  size_t n_chars = text_run.run_end - text_run.run_start;

  if (n_chars == 0) {
    // Empty string. We record the sizing of the font for use when calculating line height
    EmbedInfo embedding;
    embedding.ascenders.push_back(0);
    embedding.descenders.push_back(0);
#if HB_VERSION_MAJOR < 2 && HB_VERSION_MINOR < 2
#else
    int error = 0;
    hb_font_t *font = hb_ft_font_create(get_cached_face(text_run.font_info.file, text_run.font_info.index, text_run.size, text_run.res, &error), NULL);
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", text_run.font_info.file, text_run.font_info.index);
      error_code = error;
    } else {
      hb_font_extents_t fextent;
      hb_font_get_h_extents(font, &fextent);
      embedding.ascenders[0] = fextent.ascender;
      embedding.descenders[0] = fextent.descender;
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
    // These will be recorded as negative embeddings
    // This depends on the font so we can't cache this as part of the embedding
    std::vector<int> emoji_embeddings = {};
    emoji_embeddings.resize(n_chars);
    detect_emoji_embedding(full_string.data() + text_run.run_start, n_chars, emoji_embeddings.data(), text_run.font_info.file, text_run.font_info.index);
    bool emoji_font_added = false;
    for (int i = 0; i < n_chars; ++i) {
      if (emoji_embeddings[i] == 1) {
        bidi_embedding[i] *= -1;
        if (!emoji_font_added) {
          // Add the system emoji font to fallbacks
          fallback.push_back(locate_font_with_features("emoji", 0, 0));
          if (!get_font_sizing(fallback.back(), text_run.size, text_run.res, fallback_size, fallback_scaling)) {
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
    if (terminal || i == text_run.run_end || bidi_embedding[i] != bidi_embedding[i - 1]) {
      bool success = shape_embedding(
        embedding_start,
        i,
        features,
        bidi_embedding[embedding_start],
        text_run,
        fallback,
        fallback_size,
        fallback_scaling
      );
      if (!success) return;
      if (!text_run.embeddings.empty() && terminal) {
        text_run.embeddings.back().terminates_paragraph = terminal;
      }
      embedding_start = i;
    }
  }

  if (n_features == 0) {
    // If simply shape add to cache
    shape_cache.add(run_id, text_run);
  }
  //FT_Done_Face(face);
  return;
}


EmbedInfo HarfBuzzShaper::shape_single_line(const char* string, FontSettings& font_info, double size, double res) {
  // Fast version when we know we don't need word wrap and alignment
  // Used by graphics devices

  reset();

  int n_chars = 0;
  const uint32_t* utc_string = utf_converter.convert_to_ucs(string, n_chars);

  full_string = {utc_string, utc_string + n_chars};

  std::vector<ShapeInfo> shapes = {ShapeInfo(0, full_string.size(), font_info, 0, size, res, 0)};
  int direction = 0;
  auto final_embeddings = combine_embeddings(shapes, direction);

  if (final_embeddings.size() == 0) return EmbedInfo();

  rearrange_embeddings(final_embeddings);

  // Combined all embeddings into one
  for (auto iter = std::next(final_embeddings.begin()); iter != final_embeddings.end(); ++iter) {
    final_embeddings.front().add(*iter);
  }
  return final_embeddings.front();
}

bool HarfBuzzShaper::shape_embedding(unsigned int start, unsigned int end,
                                     std::vector<hb_feature_t>& features,
                                     int dir, ShapeInfo& shape_info,
                                     std::vector<FontSettings>& fallbacks,
                                     std::vector<double>& fallback_sizes,
                                     std::vector<double>& fallback_scales) {
  unsigned int embedding_size = end - start;

  if (embedding_size < 1) {
    return true;
  }

  int error = 0;
  // Load main font (emoji if dir is negative)
  // Shouldn't be able to fail as we have already tried to load it in the calling function
  FT_Face face = get_cached_face(
    fallbacks[dir < 0 ? 1 : 0].file,
    fallbacks[dir < 0 ? 1 : 0].index,
    shape_info.size,
    shape_info.res,
    &error
  );
  if (error) {
    return false;
  }

  hb_font_t *font = hb_ft_font_create(face, NULL);

  // Do a first run of shaping. Hopefully it's enough
  unsigned int n_glyphs = 0;
  hb_buffer_reset(buffer);
  hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start, embedding_size);
  hb_buffer_guess_segment_properties(buffer);
  hb_buffer_set_direction(buffer, dir % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
  hb_glyph_info_t *glyph_info = NULL;

  hb_shape(font, buffer, features.data(), features.size());

  hb_glyph_position_t *glyph_pos = NULL;
  glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

  if (n_glyphs == 0) {
    hb_font_destroy(font);
    return true;
  }

  shape_info.embeddings.emplace_back();
  shape_info.embeddings.back().embedding_level = std::abs(dir);
  shape_info.embeddings.back().terminates_paragraph = false;
  shape_info.embeddings.back().full_width = 0;
  bool embed_is_ltr = dir % 2 == 0;

  // See if all characters hav been found in the default font
  unsigned int current_font = dir < 0 ? 1 : 0;
  std::vector<unsigned int> char_font(embedding_size, current_font);
  bool needs_fallback = false;
  bool any_resolved = false;
  annotate_fallbacks(current_font + 1, 0, char_font, glyph_info, n_glyphs, needs_fallback, any_resolved, embed_is_ltr, start);

  if (!needs_fallback) { // Short route - use existing shaping
    glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
    fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start, shape_info, fallback_sizes, fallback_scales);
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
    unsigned int fallback_start = 0, fallback_end = 0;
    bool found = fallback_cluster(current_font, char_font, 0, fallback_start, fallback_end);
    if (!found) {
      // This shouldn't happen but oh well. We give up on fallbacks and shape with what we got
      break;
    }
    int error = 0;
    bool using_new = false;
    font = load_fallback(current_font, start + fallback_start, start + fallback_end, error, using_new, shape_info, fallbacks, fallback_sizes, fallback_scales);
    if (!using_new) {
      // We don't need to have success if we are trying an existing font
      any_resolved = true;
    }
    if (error != 0) {
      Rprintf("Failed to get face: %s, %i\n", fallbacks[current_font].file, fallbacks[current_font].index);
      error_code = error;
      shape_info.embeddings.pop_back();
      return false;
    }
    do {
      // Go through not yet resolved chars and see if the current fallback can resolve it
      hb_buffer_reset(buffer);
      hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + fallback_start, fallback_end - fallback_start);
      hb_buffer_guess_segment_properties(buffer);
      hb_buffer_set_direction(buffer, dir % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
      hb_shape(font, buffer, features.data(), features.size());
      glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);

      if (n_glyphs > 0) {
        bool needs_fallback_2 = false;
        bool any_resolved_2 = false;
        annotate_fallbacks(current_font + 1, fallback_start, char_font, glyph_info, n_glyphs, needs_fallback_2, any_resolved_2, embed_is_ltr, start);
        if (needs_fallback_2) needs_fallback = true;
        if (any_resolved_2) any_resolved = true;
      }

      found = fallback_cluster(current_font, char_font, fallback_end, fallback_start, fallback_end);
    } while (found);

    hb_font_destroy(font);
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
        FT_Face face = get_cached_face(fallbacks[current_font].file,
                                       fallbacks[current_font].index,
                                       shape_info.size, shape_info.res, &error);
        if (error != 0) {
          Rprintf("Failed to get face: %s, %i\n", fallbacks[current_font].file, fallbacks[current_font].index);
          error_code = error;
          return false;
        }

        font = hb_ft_font_create(face, NULL);

        hb_buffer_reset(buffer);
        hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + text_run_start, i - text_run_start);
        hb_buffer_guess_segment_properties(buffer);
        hb_buffer_set_direction(buffer, dir % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
        hb_shape(font, buffer, features.data(), features.size());
        glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
        glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
        fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start + text_run_start, shape_info, fallback_sizes, fallback_scales);
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
        FT_Face face = get_cached_face(fallbacks[current_font].file,
                                       fallbacks[current_font].index,
                                       shape_info.size, shape_info.res, &error);
        if (error != 0) {
          Rprintf("Failed to get face: %s, %i\n", fallbacks[current_font].file, fallbacks[current_font].index);
          error_code = error;
          return false;
        }

        font = hb_ft_font_create(face, NULL);

        hb_buffer_reset(buffer);
        hb_buffer_add_utf32(buffer, full_string.data(), full_string.size(), start + i, text_run_end - i);
        hb_buffer_guess_segment_properties(buffer);
        hb_buffer_set_direction(buffer, dir % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
        hb_shape(font, buffer, features.data(), features.size());
        glyph_info = hb_buffer_get_glyph_infos(buffer, &n_glyphs);
        glyph_pos = hb_buffer_get_glyph_positions(buffer, &n_glyphs);
        fill_shape_info(glyph_info, glyph_pos, n_glyphs, font, current_font, start + i, shape_info, fallback_sizes, fallback_scales);
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
                                          unsigned int end, int& error, bool& new_added,
                                          ShapeInfo& shape_info,
                                          std::vector<FontSettings>& fallbacks,
                                          std::vector<double>& fallback_sizes,
                                          std::vector<double>& fallback_scales) {
  new_added = false;
  // Font should only be able to be maximally the size of fallbacks
  if (font >= fallbacks.size()) {
    int n_conv = 0;
    const char* fallback_string = utf_converter.convert_to_utf(full_string.data() + start, end - start, n_conv);
    fallbacks.push_back(
      get_fallback(fallback_string,
                   fallbacks[0].file,
                   fallbacks[0].index)
    );
    new_added = true;
  }
  FT_Face face = get_font_sizing(fallbacks[font], shape_info.size, shape_info.res, fallback_sizes, fallback_scales);

  return hb_ft_font_create(face, NULL);
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
                                     std::vector<double>& fallback_scales) {
  double scaling = fallback_scales[font_id];
  if (scaling < 0) scaling = 1.0;

  double tracking = shape_info.tracking * fallback_sizes[font_id] / 1000;


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

  int new_size = embedding.glyph_id.size() + n_glyphs;
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
    embedding.x_offset.push_back(glyph_pos[i].x_offset * scaling);
    embedding.y_offset.push_back(glyph_pos[i].y_offset * scaling);
    embedding.x_advance.push_back(glyph_pos[i].x_advance * scaling + tracking);
    embedding.y_advance.push_back(glyph_pos[i].y_advance * scaling);
    embedding.full_width += embedding.x_advance.back();

    hb_font_get_glyph_extents(font, glyph_info[i].codepoint, &extent);
    embedding.x_bear.push_back(extent.x_bearing * scaling);
    embedding.y_bear.push_back(extent.y_bearing * scaling);
    embedding.width.push_back(extent.width * scaling);
    embedding.height.push_back(extent.height * scaling);

    embedding.ascenders.push_back(ascend * scaling);
    embedding.descenders.push_back(descend * scaling);

    embedding.font.push_back(font_id);
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

inline FT_Face HarfBuzzShaper::get_font_sizing(FontSettings& font_info, double size, double res, std::vector<double>& sizes, std::vector<double>& scales) {
  int error = 0;
  FT_Face face = get_cached_face(font_info.file, font_info.index, size, res, &error);
  if (error != 0) {
    Rprintf("Failed to get face: %s, %i\n", font_info.file, font_info.index);
    error_code = error;
    return nullptr;
  }
  double scaling = FT_IS_SCALABLE(face) ? -1 : size * 64.0 * res / 72.0 / face->size->metrics.height;
  double fscaling = family_scaling(face->family_name);
  scales.push_back(scaling * fscaling);
  sizes.push_back(size * fscaling);
  return face;
}

void HarfBuzzShaper::insert_hyphen(EmbedInfo& embedding, size_t where) {
  int error = 0;
  // Load main font (emoji if dir is negative)
  // Shouldn't be able to fail as we have already tried to load it in the calling function
  FT_Face face = get_cached_face(
    embedding.fallbacks[embedding.font[where]].file,
    embedding.fallbacks[embedding.font[where]].index,
    embedding.fallback_size[embedding.font[where]],
    shape_infos[0].res,
    &error
  );
  if (error) {
    return;
  }

  double scaling = embedding.fallback_scaling[embedding.font[where]];
  if (scaling < 0) scaling = 1.0;

  hb_font_t *font = hb_ft_font_create(face, NULL);
  hb_codepoint_t glyph = 0;
  hb_bool_t found = hb_font_get_glyph(font, 8208, 0, &glyph); // True hyphen;
  if (!found) found = hb_font_get_glyph(font, 45, 0, &glyph); // Hyphen minus

  if (!found) return; // No hyphen-like glyph in font

  embedding.glyph_id[where] = glyph;

  hb_position_t x = hb_font_get_glyph_h_advance(font, glyph);
  hb_position_t y = 0;

  embedding.x_advance[where] = x * scaling;
  if (embedding.glyph_cluster[where] > 0) {
    hb_font_get_glyph_kerning_for_direction(font, full_string[embedding.glyph_cluster[where] - 1], glyph, embedding.embedding_level % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL, &x, &y);
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
        if (force) {
          // If forcing we need to consume at least one glyph
          break_pos = std::max(i - 1, size_t(1));
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
        if (force) {
          // If forcing we need to consume at least one glyph
          break_pos = std::min(i, embedding.glyph_id.size() - 2);
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
  static std::vector<std::list<EmbedInfo>::iterator> embed_stack(125); // Max nesting level allowed by ICU bidi algo

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

std::list<EmbedInfo> HarfBuzzShaper::get_next_line_at_width(int32_t width, std::list<EmbedInfo>& all_embeddings, bool& hard_break, uint32_t& break_char) {
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
      break_char = has_break->pop();
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
          bool is_shy = full_string[iter->glyph_cluster[break_at]] == 173;
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
      break_char = std::prev(iter)->pop();
      line.splice(line.begin(), all_embeddings, all_embeddings.begin(), iter);
    }
  }
  rearrange_embeddings(line);
  return line;
}

void HarfBuzzShaper::do_alignment(bool ltr) {
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
      if (may_stretch[i] && i-1 < x_pos.size() && index == line_id[i+1]) {
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
      cum_move += (width - line_width[index]) / (n_glyphs[index]-1);
    }
    pen_x += cum_move;
    // Update line width of all stretched lines to match the full width of the textbox
    for (size_t i = 0; i < line_width.size(); ++i) {
      if (n_glyphs[i] != 0) line_width[i] = width;
    }
  }
}

#endif
