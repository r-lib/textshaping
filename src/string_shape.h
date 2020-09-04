#pragma once

#include <systemfonts.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>
#include <cstdint>
#include <hb.h>
#include "utils.h"
#include "cache_lru.h"

struct ShapeID {
  std::string string;
  std::string font;
  unsigned int index;
  double size;

  inline ShapeID() : string(""), font(""), index(0), size(0.0) {}
  inline ShapeID(std::string _string, std::string _font, unsigned int _index, double _size) :
    string(_string),
    font(_font),
    index(_index),
    size(_size) {}
  inline ShapeID(const ShapeID& shape) :
    string(shape.string),
    font(shape.font),
    index(shape.index),
    size(shape.size) {}

  inline bool operator==(const ShapeID &other) const {
    return (index == other.index &&
            size == other.size &&
            string == other.string &&
            font == other.font);
  }
};
struct ShapeInfo {
  std::vector<unsigned int> glyph_id;
  std::vector<int32_t> x_pos;
  int32_t width;
  int32_t left_bearing;
  int32_t right_bearing;
};
namespace std {
template <>
struct hash<ShapeID> {
  size_t operator()(const ShapeID & x) const {
    return std::hash<std::string>()(x.string) ^
      std::hash<std::string>()(x.font) ^
      std::hash<unsigned int>()(x.index) ^
      std::hash<double>()(x.size);
  }
};
}

class HarfBuzzShaper {
public:
  HarfBuzzShaper() :
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
  cur_lineheight(0.0),
  cur_align(0),
  cur_string(0),
  cur_hjust(0.0),
  cur_vjust(0.0),
  cur_res(0.0),
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

  static std::vector<unsigned int> glyph_id;
  static std::vector<unsigned int> glyph_cluster;
  static std::vector<unsigned int> string_id;
  static std::vector<int32_t> x_pos;
  static std::vector<int32_t> y_pos;
  static std::vector<int32_t> x_mid;
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
  static ShapeInfo last_shape_info;

  int error_code;

  bool shape_string(const char* string, const char* fontfile, int index,
                    double size, double res, double lineheight,
                    int align, double hjust, double vjust, double width,
                    double tracking, double ind, double hang, double before,
                    double after);
  bool add_string(const char* string, const char* fontfile, int index,
                  double size, double tracking);
  bool finish_string();


  bool single_line_shape(const char* string, FontSettings font_info, double size,
                         double res);

private:
  static UTF_UCS utf_converter;
  static LRU_Cache<std::string, std::vector<int> > bidi_cache;
  static LRU_Cache<ShapeID, ShapeInfo> shape_cache;
  static ShapeID last_shape_id;
  static ShapeID temp_shape_id;
  hb_buffer_t *buffer;
  double cur_lineheight;
  int cur_align;
  unsigned int cur_string;
  double cur_hjust;
  double cur_vjust;
  double cur_res;
  double cur_tracking;
  static std::vector<int32_t> x_advance;
  static std::vector<int32_t> x_offset;
  static std::vector<int32_t> left_bear;
  static std::vector<int32_t> right_bear;
  static std::vector<int32_t> top_extend;
  static std::vector<int32_t> bottom_extend;
  static std::vector<int32_t> ascenders;
  static std::vector<int32_t> descenders;
  static std::vector<bool> may_break;
  static std::vector<bool> must_break;
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
  bool shape_glyphs(hb_font_t *font, const uint32_t *string, unsigned int n_chars);

  inline bool glyph_is_linebreak(int id) {
    switch (id) {
    case 10: return true;
    case 11: return true;
    case 12: return true;
    case 13: return true;
    case 133: return true;
    case 8232: return true;
    case 8233: return true;
    }
    return false;
  }

  inline bool glyph_is_breaker(int id) {
    switch (id) {
    case 9: return true;
    case 32: return true;
    case 5760: return true;
    case 6158: return true;
    case 8192: return true;
    case 8193: return true;
    case 8194: return true;
    case 8195: return true;
    case 8196: return true;
    case 8197: return true;
    case 8198: return true;
    case 8200: return true;
    case 8201: return true;
    case 8202: return true;
    case 8203: return true;
    case 8204: return true;
    case 8205: return true;
    case 8287: return true;
    case 12288: return true;
    }
    return false;
  }
};
