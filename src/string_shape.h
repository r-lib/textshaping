#pragma once

#ifndef NO_HARFBUZZ_FRIBIDI

#include "cpp11/protect.hpp"
#include <numeric>
#include <functional>
#include <algorithm>
#include <systemfonts.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>
#include <set>
#include <cstdint>
#include <hb.h>
#include "utils.h"
#include "cache_lru.h"

static const uint32_t EMPTY_CHAR = 0xffffffff; // Largest possible value. Unlikely any font would use that
static const uint32_t SPACER_CHAR = 0xffffffff - 1; // Second largest possible value. Also unlikely any font would use that

struct ShapeID {
  size_t string_hash;
  size_t embed_hash;
  std::string font;
  unsigned int index;
  double size;
  double tracking;

  inline ShapeID() : string_hash(0), embed_hash(0), font(""), index(0), size(0.0), tracking(0.0) {}
  inline ShapeID(const ShapeID& shape) :
    string_hash(shape.string_hash),
    embed_hash(shape.embed_hash),
    font(shape.font),
    index(shape.index),
    size(shape.size),
    tracking(shape.tracking) {}

  inline bool operator==(const ShapeID &other) const {
    return string_hash == other.string_hash &&
           embed_hash == other.embed_hash &&
           index == other.index &&
           size == other.size &&
           font == other.font &&
           tracking == other.tracking;
  }
};

struct BidiID {
  size_t string_hash;
  int direction;

  inline bool operator==(const BidiID &other) const {
    return string_hash == other.string_hash && direction == other.direction;
  }
};

struct EmbedInfo {
  std::vector<size_t> glyph_id;
  std::vector<size_t> glyph_cluster;
  std::vector<size_t> string_id;
  std::vector<int32_t> x_advance;
  std::vector<int32_t> y_advance;
  std::vector<int32_t> x_offset;
  std::vector<int32_t> y_offset;
  std::vector<int32_t> x_bear;
  std::vector<int32_t> y_bear;
  std::vector<int32_t> width;
  std::vector<int32_t> height;
  std::vector<int32_t> ascenders;
  std::vector<int32_t> descenders;
  std::vector<bool> is_blank;
  std::vector<bool> may_break;
  std::vector<bool> may_stretch;
  std::vector<unsigned int> font;
  std::vector<FontSettings> fallbacks;
  std::vector<double> fallback_size;
  std::vector<double> fallback_scaling;
  size_t embedding_level;
  int32_t full_width;
  bool terminates_paragraph;
  void add(const EmbedInfo& other, bool check = true) {
    if (check && embedding_level != other.embedding_level) {
      cpp11::stop("Unable to merge embeddings of different levels");
    }
    if (check && terminates_paragraph) {
      cpp11::stop("Can't combine embeddings past termination point");
    }
    glyph_id.insert(glyph_id.end(), other.glyph_id.begin(), other.glyph_id.end());
    glyph_cluster.insert(glyph_cluster.end(), other.glyph_cluster.begin(), other.glyph_cluster.end());
    string_id.insert(string_id.end(), other.string_id.begin(), other.string_id.end());
    x_advance.insert(x_advance.end(), other.x_advance.begin(), other.x_advance.end());
    y_advance.insert(y_advance.end(), other.y_advance.begin(), other.y_advance.end());
    x_offset.insert(x_offset.end(), other.x_offset.begin(), other.x_offset.end());
    y_offset.insert(y_offset.end(), other.y_offset.begin(), other.y_offset.end());
    x_bear.insert(x_bear.end(), other.x_bear.begin(), other.x_bear.end());
    y_bear.insert(y_bear.end(), other.y_bear.begin(), other.y_bear.end());
    width.insert(width.end(), other.width.begin(), other.width.end());
    height.insert(height.end(), other.height.begin(), other.height.end());
    ascenders.insert(ascenders.end(), other.ascenders.begin(), other.ascenders.end());
    descenders.insert(descenders.end(), other.descenders.begin(), other.descenders.end());
    is_blank.insert(is_blank.end(), other.is_blank.begin(), other.is_blank.end());
    may_break.insert(may_break.end(), other.may_break.begin(), other.may_break.end());
    may_stretch.insert(may_stretch.end(), other.may_stretch.begin(), other.may_stretch.end());
    size_t current_font_size = font.size();
    font.insert(font.end(), other.font.begin(), other.font.end());
    std::for_each(font.begin() + current_font_size, font.end(), [this](unsigned int& f) { f+=this->fallbacks.size();});
    fallbacks.insert(fallbacks.end(), other.fallbacks.begin(), other.fallbacks.end());
    fallback_size.insert(fallback_size.end(), other.fallback_size.begin(), other.fallback_size.end());
    fallback_scaling.insert(fallback_scaling.end(), other.fallback_scaling.begin(), other.fallback_scaling.end());
    full_width += other.full_width;
    terminates_paragraph = other.terminates_paragraph;
  }
  void split(size_t from, size_t to, EmbedInfo& into) {
    into.embedding_level = embedding_level;
    into.terminates_paragraph = false;

    into.glyph_id.insert(into.glyph_id.end(), glyph_id.begin() + from, glyph_id.begin() + to);
    glyph_id.erase(glyph_id.begin() + from, glyph_id.begin() + to);
    into.glyph_cluster.insert(into.glyph_cluster.end(), glyph_cluster.begin() + from, glyph_cluster.begin() + to);
    glyph_cluster.erase(glyph_cluster.begin() + from, glyph_cluster.begin() + to);
    into.string_id.insert(into.string_id.end(), string_id.begin() + from, string_id.begin() + to);
    string_id.erase(string_id.begin() + from, string_id.begin() + to);
    into.x_advance.insert(into.x_advance.end(), x_advance.begin() + from, x_advance.begin() + to);
    x_advance.erase(x_advance.begin() + from, x_advance.begin() + to);
    into.y_advance.insert(into.y_advance.end(), y_advance.begin() + from, y_advance.begin() + to);
    y_advance.erase(y_advance.begin() + from, y_advance.begin() + to);
    into.x_offset.insert(into.x_offset.end(), x_offset.begin() + from, x_offset.begin() + to);
    x_offset.erase(x_offset.begin() + from, x_offset.begin() + to);
    into.y_offset.insert(into.y_offset.end(), y_offset.begin() + from, y_offset.begin() + to);
    y_offset.erase(y_offset.begin() + from, y_offset.begin() + to);
    into.x_bear.insert(into.x_bear.end(), x_bear.begin() + from, x_bear.begin() + to);
    x_bear.erase(x_bear.begin() + from, x_bear.begin() + to);
    into.y_bear.insert(into.y_bear.end(), y_bear.begin() + from, y_bear.begin() + to);
    y_bear.erase(y_bear.begin() + from, y_bear.begin() + to);
    into.width.insert(into.width.end(), width.begin() + from, width.begin() + to);
    width.erase(width.begin() + from, width.begin() + to);
    into.height.insert(into.height.end(), height.begin() + from, height.begin() + to);
    height.erase(height.begin() + from, height.begin() + to);
    into.ascenders.insert(into.ascenders.end(), ascenders.begin() + from, ascenders.begin() + to);
    ascenders.erase(ascenders.begin() + from, ascenders.begin() + to);
    into.descenders.insert(into.descenders.end(), descenders.begin() + from, descenders.begin() + to);
    descenders.erase(descenders.begin() + from, descenders.begin() + to);
    into.is_blank.insert(into.is_blank.end(), is_blank.begin() + from, is_blank.begin() + to);
    is_blank.erase(is_blank.begin() + from, is_blank.begin() + to);
    into.may_break.insert(into.may_break.end(), may_break.begin() + from, may_break.begin() + to);
    may_break.erase(may_break.begin() + from, may_break.begin() + to);
    into.may_stretch.insert(into.may_stretch.end(), may_stretch.begin() + from, may_stretch.begin() + to);
    may_stretch.erase(may_stretch.begin() + from, may_stretch.begin() + to);
    into.font.insert(into.font.end(), font.begin() + from, font.begin() + to);
    font.erase(font.begin() + from, font.begin() + to);
    into.fallbacks.insert(into.fallbacks.end(), fallbacks.begin(), fallbacks.end());
    into.fallback_size.insert(into.fallback_size.end(), fallback_size.begin(), fallback_size.end());
    into.fallback_scaling.insert(into.fallback_scaling.end(), fallback_scaling.begin(), fallback_scaling.end());
    into.full_width = std::accumulate(into.x_advance.begin(), into.x_advance.end(), int32_t(0));
    full_width -= into.full_width;
  }
  uint32_t pop() {
    uint32_t cluster;
    if (embedding_level % 2 == 0) {
      cluster = glyph_cluster.back();
      glyph_id.pop_back();
      glyph_cluster.pop_back();
      string_id.pop_back();
      x_advance.pop_back();
      y_advance.pop_back();
      x_offset.pop_back();
      y_offset.pop_back();
      x_bear.pop_back();
      y_bear.pop_back();
      width.pop_back();
      height.pop_back();
      ascenders.pop_back();
      descenders.pop_back();
      is_blank.pop_back();
      may_break.pop_back();
      may_stretch.pop_back();
      font.pop_back();
    } else {
      // This is not efficient but we would generally only do this on small embeddings
      cluster = glyph_cluster.front();
      glyph_id.erase(glyph_id.begin());
      glyph_cluster.erase(glyph_cluster.begin());
      string_id.erase(string_id.begin());
      x_advance.erase(x_advance.begin());
      y_advance.erase(y_advance.begin());
      x_offset.erase(x_offset.begin());
      y_offset.erase(y_offset.begin());
      x_bear.erase(x_bear.begin());
      y_bear.erase(y_bear.begin());
      width.erase(width.begin());
      height.erase(height.begin());
      ascenders.erase(ascenders.begin());
      descenders.erase(descenders.begin());
      is_blank.erase(is_blank.begin());
      may_break.erase(may_break.begin());
      may_stretch.erase(may_stretch.begin());
      font.erase(font.begin());
    }
    return cluster;
  }
};
struct ShapeInfo {
  size_t run_start;
  size_t run_end;
  FontSettings font_info;
  unsigned int index;
  double size;
  double res;
  double tracking;
  std::vector<EmbedInfo> embeddings;

  inline ShapeInfo() : run_start(0), run_end(0), font_info(), index(0), size(0), res(0), tracking(0), embeddings({}) {}
  inline ShapeInfo(size_t _run_start, size_t _run_end, FontSettings& _font_info, unsigned int _index, double _size, double _res, double _tracking) :
    run_start(_run_start),
    run_end(_run_end),
    font_info(_font_info),
    index(_index),
    size(_size),
    res(_res),
    tracking(_tracking),
    embeddings({}) {}

  void add_index(unsigned int ind) {
    index = ind;
    for (auto iter = embeddings.begin(); iter != embeddings.end(); ++iter) {
      iter->string_id.clear();
      for (auto iter2 = iter->glyph_id.begin(); iter2 != iter->glyph_id.end(); ++iter2) {
        iter->string_id.push_back(ind);
      }
    }
  }
};

template<typename Iterator>
inline size_t vector_hash(Iterator begin, Iterator end) {
  typedef typename std::iterator_traits<Iterator>::value_type Type;
  size_t answer = end - begin;
  for (auto iter = begin; iter != end; ++iter) {
    uint32_t x = *iter;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    answer ^= x + 0x9e3779b9 + (answer << 6) + (answer >> 2);
  }
  return answer;
}
namespace std {
template <>
struct hash<ShapeID> {
  size_t operator()(const ShapeID & x) const {
    return x.string_hash ^
      x.embed_hash ^
      std::hash<std::string>()(x.font) ^
      std::hash<unsigned int>()(x.index) ^
      std::hash<double>()(x.size) ^
      std::hash<double>()(x.tracking);
  }
};

template <>
struct hash<BidiID> {
  size_t operator()(const BidiID & x) const {
    return x.string_hash ^ std::hash<int>()(x.direction);
  }
};
}

class HarfBuzzShaper {
public:
  HarfBuzzShaper() :
  // Public
  glyph_id(),
  glyph_cluster(),
  fontfile(),
  fontindex(),
  fontsize(),
  string_id(),
  x_pos(),
  y_pos(),
  advance(),
  ascender(),
  descender(),
  line_must_break(),
  width(0),
  height(0),
  left_bearing(0),
  right_bearing(0),
  top_bearing(0),
  bottom_bearing(0),
  top_border(0),
  left_border(0),
  pen_x(0),
  pen_y(0),
  error_code(0),
  dir(0),
  // Private
  full_string(),
  bidi_embedding(),
  soft_break(),
  hard_break(),
  cur_lineheight(0.0),
  cur_align(0),
  cur_string(0),
  cur_hjust(0.0),
  cur_vjust(0.0),
  cur_res(0.0),
  shape_infos(),
  may_stretch(),
  line_left_bear(),
  line_right_bear(),
  line_width(),
  line_id(),
  top(0),
  bottom(0),
  ascend(0),
  descend(0),
  max_width(0),
  indent(0),
  hanging(0),
  space_before(0),
  space_after(0)
  {
    buffer = hb_buffer_create();
  };
  ~HarfBuzzShaper() {
    hb_buffer_destroy(buffer);
  };

  std::vector<unsigned int> glyph_id;
  std::vector<unsigned int> glyph_cluster;
  std::vector<std::string> fontfile;
  std::vector<unsigned int> fontindex;
  std::vector<double> fontsize;
  std::vector<unsigned int> string_id;
  std::vector<int32_t> x_pos;
  std::vector<int32_t> y_pos;
  std::vector<int32_t> advance;
  std::vector<int32_t> ascender;
  std::vector<int32_t> descender;
  std::vector<bool> line_must_break;
  int32_t width;
  int32_t height;
  int32_t left_bearing;
  int32_t right_bearing;
  int32_t top_bearing;
  int32_t bottom_bearing;
  int32_t top_border;
  int32_t left_border;
  int32_t pen_x;
  int32_t pen_y;

  int error_code;
  int dir;

  bool shape_string(const char* string, FontSettings& font_info,
                    double size, double res, double lineheight,
                    int align, double hjust, double vjust, double width,
                    double tracking, double ind, double hang, double before,
                    double after, bool spacer, int direction,
                    std::vector<int>& soft_wrap, std::vector<int>& hard_wrap);
  bool add_string(const char* string, FontSettings& font_info,
                  double size, double tracking, bool spacer,
                  std::vector<int>& soft_wrap, std::vector<int>& hard_wrap);
  bool add_spacer(FontSettings& font_info, double height, double width, uint32_t filler = SPACER_CHAR);
  bool finish_string();

  void shape_text_run(ShapeInfo &text_run, bool ltr);
  EmbedInfo shape_single_line(const char* string, FontSettings& font_info, double size, double res);

private:
  std::vector<uint32_t> full_string;
  std::vector<int> bidi_embedding;
  static UTF_UCS utf_converter;
  static LRU_Cache<BidiID, std::vector<int> > bidi_cache;
  static LRU_Cache<ShapeID, ShapeInfo> shape_cache;
  std::set<int> soft_break;
  std::set<int> hard_break;
  hb_buffer_t *buffer;
  double cur_lineheight;
  int cur_align;
  unsigned int cur_string;
  double cur_hjust;
  double cur_vjust;
  double cur_res;
  std::vector<ShapeInfo> shape_infos;
  std::vector<bool> may_stretch;
  std::vector<int32_t> line_left_bear;
  std::vector<int32_t> line_right_bear;
  std::vector<int32_t> line_width;
  std::vector<int32_t> line_id;

  int32_t top;
  int32_t bottom;
  int32_t ascend;
  int32_t descend;
  int32_t max_width;
  int32_t indent;
  int32_t hanging;
  int32_t space_before;
  int32_t space_after;

  void reset();
  std::list<EmbedInfo> combine_embeddings(std::vector<ShapeInfo>& shapes, int& direction);
  bool shape_embedding(unsigned int start, unsigned int end, std::vector<hb_feature_t>& features,
                       int dir, ShapeInfo& shape_info, std::vector<FontSettings>& fallbacks,
                       std::vector<double>& fallback_sizes, std::vector<double>& fallback_scales);
  hb_font_t* load_fallback(unsigned int font, unsigned int start, unsigned int end,
                           int& error, bool& new_added, ShapeInfo& shape_info,
                           std::vector<FontSettings>& fallbacks,
                           std::vector<double>& fallback_sizes,
                           std::vector<double>& fallback_scales);
  bool fallback_cluster(unsigned int font, std::vector<unsigned int>& char_font,
                        unsigned int from, unsigned int& start, unsigned int& end);
  void annotate_fallbacks(unsigned int font, unsigned int offset,
                          std::vector<unsigned int>& char_font,
                          hb_glyph_info_t* glyph_info, unsigned int n_glyphs,
                          bool& needs_fallback, bool& any_resolved, bool ltr,
                          unsigned int string_offset);
  void fill_shape_info(hb_glyph_info_t* glyph_info, hb_glyph_position_t* glyph_pos,
                       unsigned int n_glyphs, hb_font_t* font, unsigned int font_id,
                       unsigned int cluster_offset, ShapeInfo& shape_info,
                       std::vector<double>& fallback_sizes,
                       std::vector<double>& fallback_scales);
  void fill_glyph_info(EmbedInfo& embedding);
  FT_Face get_font_sizing(FontSettings& font_info, double size, double res, std::vector<double>& sizes, std::vector<double>& scales);
  void insert_hyphen(EmbedInfo& embedding, size_t where);
  bool has_valid_break(const EmbedInfo& embedding, int32_t width, size_t& break_pos, bool force);
  void rearrange_embeddings(std::list<EmbedInfo>& line);
  std::list<EmbedInfo> get_next_line_at_width(int32_t width, std::list<EmbedInfo>& all_embeddings, bool& hard_break, uint32_t& break_char);
  void do_alignment(bool ltr);

  inline double family_scaling(const char* family) {
    if (strcmp("Apple Color Emoji", family) == 0) {
      return 1.3;
    } else if (strcmp("Noto Color Emoji", family) == 0) {
      return 1.175;
    }
    return 1;
  }

  inline bool glyph_is_linebreak(int32_t id) const {
    switch (id) {
    case 10: return true;    // Line feed
    case 11: return true;    // Vertical tab
    case 12: return true;    // Form feed
    case 13: return true;    // Carriage return
    case 133: return true;   // Next line
    case 8232: return true;  // Line Separator
    case 8233: return true;  // Paragraph Separator
    }
    return false;
  }

  inline bool glyph_is_breaker(int32_t id) const {
    switch (id) {
    case 9: return true;     // Horizontal tab
    case 32: return true;    // Space
    case 45: return true;    // Hyphen-minus
    case 173: return true;   // Soft hyphen
    case 5760: return true;  // Ogham Space Mark
    case 6158: return true;  // Mongolian Vowel Separator
    case 8192: return true;  // En Quad
    case 8193: return true;  // Em Quad
    case 8194: return true;  // En Space
    case 8195: return true;  // Em Space
    case 8196: return true;  // Three-Per-Em Space
    case 8197: return true;  // Four-Per-Em Space
    case 8198: return true;  // Six-Per-Em Space
    case 8200: return true;  // Punctuation Space
    case 8201: return true;  // Thin Space
    case 8202: return true;  // Hair Space
    case 8203: return true;  // Zero Width Space
    case 8204: return true;  // Zero Width Non-Joiner
    case 8205: return true;  // Zero Width Joiner
    case 8208: return true;  // Hyphen
    case 8287: return true;  // Medium Mathematical Space
    case 12288: return true; // Ideographic Space
    }
    return false;
  }

  inline bool glyph_is_blank(int32_t id) const {
    switch (id) {
    case 9: return true;     // Horizontal tab
    case 10: return true;    // Line feed
    case 11: return true;    // Vertical tab
    case 12: return true;    // Form feed
    case 13: return true;    // Carriage return
    case 32: return true;    // Space
    case 133: return true;   // Next line
    case 5760: return true;  // Ogham Space Mark
    case 6158: return true;  // Mongolian Vowel Separator
    case 8192: return true;  // En Quad
    case 8193: return true;  // Em Quad
    case 8194: return true;  // En Space
    case 8195: return true;  // Em Space
    case 8196: return true;  // Three-Per-Em Space
    case 8197: return true;  // Four-Per-Em Space
    case 8198: return true;  // Six-Per-Em Space
    case 8200: return true;  // Punctuation Space
    case 8201: return true;  // Thin Space
    case 8202: return true;  // Hair Space
    case 8203: return true;  // Zero Width Space
    case 8204: return true;  // Zero Width Non-Joiner
    case 8205: return true;  // Zero Width Joiner
    case 8232: return true;  // Line Separator
    case 8233: return true;  // Paragraph Separator
    case 8287: return true;  // Medium Mathematical Space
    case 12288: return true; // Ideographic Space
    }
    return false;
  }

  inline bool glyph_may_stretch(int32_t id) const {
    switch (id) {
    case 32: return true;    // Space
    }
    return false;
  }

  inline bool glyph_may_soft_break(int index) const {
    return soft_break.find(index) != soft_break.end();
  }

  inline bool glyph_must_hard_break(int index) const {
    return hard_break.find(index) != hard_break.end();
  }
};

#endif
