#include "R_ext/Arith.h"
#include "cpp11/logicals.hpp"
#include "cpp11/protect.hpp"
#define R_NO_REMAP

#include "string_metrics.h"
#include "string_shape.h"
#include "hb_shaper.h"
#include <systemfonts-ft.h>

#define CPP11_PARTIAL
#include <cpp11/declarations.hpp>
#include <cpp11/data_frame.hpp>
#include <cpp11/named_arg.hpp>

#include "utils.h"

using namespace cpp11;

#ifdef NO_HARFBUZZ_FRIBIDI

list get_string_shape_c(strings string, integers id, strings path, integers index,
                        list_of<list> features, list_of<list> variations,
                        doubles size, doubles res,
                        doubles lineheight, integers align, doubles hjust,
                        doubles vjust, doubles width, doubles tracking,
                        doubles indent, doubles hanging, doubles space_before,
                        doubles space_after, integers direction,
                        list_of<integers> soft_wrap, list_of<integers> hard_wrap) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  writable::data_frame string_df({
    "string"_nm = writable::logicals(),
    "width"_nm = writable::logicals(),
    "height"_nm = writable::logicals(),
    "left_bearing"_nm = writable::logicals(),
    "right_bearing"_nm = writable::logicals(),
    "top_bearing"_nm = writable::logicals(),
    "bottom_bearing"_nm = writable::logicals(),
    "left_border"_nm = writable::logicals(),
    "top_border"_nm = writable::logicals(),
    "pen_x"_nm = writable::logicals(),
    "pen_y"_nm = writable::logicals()
  });
  string_df.attr("class") = writable::strings({"tbl_df", "tbl", "data.frame"});

  writable::data_frame info_df({
    "glyph"_nm = writable::logicals(),
    "index"_nm = writable::logicals(),
    "metric_id"_nm = writable::logicals(),
    "string_id"_nm = writable::logicals(),
    "x_offset"_nm = writable::logicals(),
    "y_offset"_nm = writable::logicals(),
    "x_midpoint"_nm = writable::logicals()
  });
  info_df.attr("class") = writable::strings({"tbl_df", "tbl", "data.frame"});

  return writable::list({
    "shape"_nm = info_df,
    "metrics"_nm = string_df
  });
}

doubles get_line_width_c(strings string, strings path, integers index, doubles size,
                         doubles res, logicals include_bearing, list_of<list> features,
                         list_of<list> variations) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  return {};
}

int ts_string_width(const char* string, FontSettings font_info, double size,
                    double res, int include_bearing, double* width) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  *width = 0.0;
  return 0;
}

int ts_string_shape(const char* string, FontSettings font_info, double size,
                    double res, std::vector<textshaping::Point>& loc, std::vector<uint32_t>& id,
                    std::vector<int>& cluster, std::vector<unsigned int>& font,
                    std::vector<FontSettings>& fallbacks,
                    std::vector<double>& fallback_scaling) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  loc.clear();
  id.clear();
  cluster.clear();
  font.clear();
  fallbacks.clear();
  return 0;
}

int ts_string_shape_old(const char* string, FontSettings font_info, double size,
                         double res, double* x, double* y, int* id, int* n_glyphs,
                         unsigned int max_length) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  *n_glyphs = 0;
  return 0;
}

int ts_string_width2(const char* string, FontSettings2 font_info, double size,
                     double res, int include_bearing, double* width) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  *width = 0.0;
  return 0;
}

int ts_string_shape2(const char* string, FontSettings2 font_info, double size,
                     double res, std::vector<textshaping::Point>& loc, std::vector<uint32_t>& id,
                     std::vector<int>& cluster, std::vector<unsigned int>& font,
                     std::vector<FontSettings2>& fallbacks,
                     std::vector<double>& fallback_scaling) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  loc.clear();
  id.clear();
  cluster.clear();
  font.clear();
  fallbacks.clear();
  return 0;
}

#else

std::vector< std::vector<FontFeature> > create_font_features(list_of<list> features) {
  std::vector< std::vector<FontFeature> > res;

  for (R_xlen_t i = 0; i < features.size(); ++i) {
    res.emplace_back();
    strings tags = as_cpp<strings>(features[i][0]);
    integers vals = as_cpp<integers>(features[i][1]);
    if (tags.size() != vals.size()) {
      cpp11::stop("Feature tags and settings must be of the same length");
    }
    for (R_xlen_t j = 0; j < tags.size(); ++j) {
      const char* f = Rf_translateCharUTF8(tags[j]);
      size_t len = strlen(f);
      if (len > 4) {
        cpp11::stop("OpenType feature tags must be at most 4 characters");
      }
      // Shorter tags are padded with spaces per the OpenType convention
      char tag[4] = {' ', ' ', ' ', ' '};
      memcpy(tag, f, len);
      res.back().push_back({{tag[0], tag[1], tag[2], tag[3]}, vals[j]});
    }
  }

  return res;
}

void create_font_variations(list_of<list> variations,
                            std::vector< std::vector<int> >& axes,
                            std::vector< std::vector<int> >& coords) {
  for (R_xlen_t i = 0; i < variations.size(); ++i) {
    list variation = variations[i];
    axes.emplace_back(as_cpp< std::vector<int> >(variation["axis"]));
    coords.emplace_back(as_cpp< std::vector<int> >(variation["value"]));
    if (axes.back().size() != coords.back().size()) {
      cpp11::stop("`axis` and `value` in a variation must be of the same length");
    }
  }
}

std::vector<FontSettings2> create_font_settings(strings path, integers index,
                                                std::vector< std::vector<FontFeature> >& features,
                                                std::vector< std::vector<int> >& axes,
                                                std::vector< std::vector<int> >& coords) {
  std::vector<FontSettings2> res;

  if (path.size() != index.size() || path.size() != features.size() ||
      path.size() != (R_xlen_t) axes.size() || path.size() != (R_xlen_t) coords.size()) {
    cpp11::stop("`path`, `index`, `features`, and `variations` must all be of the same length");
  }

  for (R_xlen_t i = 0; i < path.size(); ++i) {
    res.emplace_back();
    strncpy(res.back().file, Rf_translateCharUTF8(path[i]), PATH_MAX);
    res.back().file[PATH_MAX] = '\0';
    res.back().index = index[i];
    res.back().features = features[i].data();
    res.back().n_features = features[i].size();
    res.back().axes = axes[i].data();
    res.back().coords = coords[i].data();
    res.back().n_axes = axes[i].size();
  }

  return res;
}

list get_string_shape_c(strings string, integers id, strings path, integers index,
                        list_of<list> features, list_of<list> variations,
                        doubles size, doubles res,
                        doubles lineheight, integers align, doubles hjust,
                        doubles vjust, doubles width, doubles tracking,
                        doubles indent, doubles hanging, doubles space_before,
                        doubles space_after, integers direction,
                        list_of<integers> soft_wrap, list_of<integers> hard_wrap) {
  int n_strings = string.size();

  // Return Columns
  writable::integers glyph, glyph_id, metric_id, string_id, fontindex;
  writable::doubles x_offset, y_offset, widths, heights, left_bearings, right_bearings,
    top_bearings, bottom_bearings, left_border, top_border, pen_x, pen_y, fontsize,
    advance, ascender, descender;
  writable::strings fontpath, str;
  writable::logicals ltr;

  if (n_strings != 0) {
    if (n_strings != id.size() ||
        n_strings != path.size() ||
        n_strings != index.size() ||
        n_strings != features.size() ||
        n_strings != size.size() ||
        n_strings != res.size() ||
        n_strings != lineheight.size() ||
        n_strings != align.size() ||
        n_strings != hjust.size() ||
        n_strings != vjust.size() ||
        n_strings != width.size() ||
        n_strings != tracking.size() ||
        n_strings != indent.size() ||
        n_strings != hanging.size() ||
        n_strings != space_before.size() ||
        n_strings != space_after.size() ||
        n_strings != direction.size() ||
        n_strings != soft_wrap.size() ||
        n_strings != hard_wrap.size()
    ) {
      cpp11::stop("All input must be the same size");
    }
    auto all_features = create_font_features(features);
    std::vector< std::vector<int> > all_axes, all_coords;
    create_font_variations(variations, all_axes, all_coords);
    auto fonts = create_font_settings(path, index, all_features, all_axes, all_coords);

    // Shape the text
    int cur_id = id[0] == NA_INTEGER ? 0 : id[0] - 1; // make sure it differs from first
    bool success = false;

    HarfBuzzShaper& shaper = get_hb_shaper();
    std::vector<int> soft, hard;
    for (int i = 0; i < n_strings; ++i) {
      const char* this_string = Rf_translateCharUTF8(string[i]);
      int this_id = id[i];
      soft.assign(soft_wrap[i].begin(), soft_wrap[i].end());
      hard.assign(hard_wrap[i].begin(), hard_wrap[i].end());
      if (cur_id == this_id) {
        success = shaper.add_string(this_string, fonts[i], size[i], tracking[i], cpp11::is_na(string[i]), soft, hard);

        if (!success) {
          cpp11::stop("Failed to shape string (%s) with font file (%s) with freetype error %i", this_string, Rf_translateCharUTF8(path[i]), shaper.error_code);
        }
      } else {
        cur_id = this_id;
        success = shaper.shape_string(this_string, fonts[i], size[i], res[i],
                                      lineheight[i], align[i], hjust[i], vjust[i],
                                      width[i] * 64.0, tracking[i], indent[i] * 64.0,
                                      hanging[i] * 64.0, space_before[i] * 64.0,
                                      space_after[i] * 64.0, cpp11::is_na(string[i]),
                                      direction[i], soft, hard);

        if (!success) {
          cpp11::stop("Failed to shape string (%s) with font file (%s) with freetype error %i", this_string, Rf_translateCharUTF8(STRING_ELT(path, i)), shaper.error_code);
        }
      }
      bool store_string = i == n_strings - 1 || cur_id != INTEGER(id)[i + 1];
      if (store_string) {
        success = shaper.finish_string();
        if (!success) {
          cpp11::stop("Failed to finalise string shaping");
        }
        int n_glyphs = shaper.glyph_id.size();
        for (int j = 0; j < n_glyphs; j++) {
          glyph.push_back((int) shaper.glyph_cluster[j] + 1);
          glyph_id.push_back((int) (shaper.glyph_id[j] == SPACER_CHAR ? R_NaInt : shaper.glyph_id[j]));
          metric_id.push_back(pen_x.size() + 1);
          string_id.push_back(shaper.string_id[j] + 1);
          x_offset.push_back(double(shaper.x_pos[j]) / 64.0);
          y_offset.push_back(double(shaper.y_pos[j]) / 64.0);
          fontpath.push_back(shaper.fontfile[j]);
          fontindex.push_back((int) shaper.fontindex[j]);
          fontsize.push_back(shaper.fontsize[j]);
          advance.push_back(double(shaper.advance[j]) / 64.0);
          ascender.push_back(double(shaper.ascender[j]) / 64.0);
          descender.push_back(double(shaper.descender[j]) / 64.0);
        }
        widths.push_back(double(shaper.width) / 64.0);
        heights.push_back(double(shaper.height) / 64.0);
        left_bearings.push_back(double(shaper.left_bearing) / 64.0);
        right_bearings.push_back(double(shaper.right_bearing) / 64.0);
        top_bearings.push_back(double(shaper.top_bearing) / 64.0);
        bottom_bearings.push_back(double(shaper.bottom_bearing) / 64.0);
        left_border.push_back(double(shaper.left_border) / 64.0);
        top_border.push_back(double(shaper.top_border) / 64.0);
        pen_x.push_back(double(shaper.pen_x) / 64.0);
        pen_y.push_back(double(shaper.pen_y) / 64.0);
        ltr.push_back(shaper.dir == 1);
      }
    }
    str = writable::strings(pen_x.size());
  }

  writable::data_frame string_df({
    "string"_nm = str,
    "width"_nm = widths,
    "height"_nm = heights,
    "left_bearing"_nm = left_bearings,
    "right_bearing"_nm = right_bearings,
    "top_bearing"_nm = top_bearings,
    "bottom_bearing"_nm = bottom_bearings,
    "left_border"_nm = left_border,
    "top_border"_nm = top_border,
    "pen_x"_nm = pen_x,
    "pen_y"_nm = pen_y,
    "ltr"_nm = ltr
  });
  string_df.attr("class") = writable::strings({"tbl_df", "tbl", "data.frame"});

  writable::data_frame info_df({
    "glyph"_nm = glyph,
    "index"_nm = glyph_id,
    "metric_id"_nm = metric_id,
    "string_id"_nm = string_id,
    "x_offset"_nm = x_offset,
    "y_offset"_nm = y_offset,
    "font_path"_nm = fontpath,
    "font_index"_nm = fontindex,
    "font_size"_nm = fontsize,
    "advance"_nm = advance,
    "ascender"_nm = ascender,
    "descender"_nm = descender
  });
  info_df.attr("class") = writable::strings({"tbl_df", "tbl", "data.frame"});

  return writable::list({
    "shape"_nm = info_df,
    "metrics"_nm = string_df
  });
}

doubles get_line_width_c(strings string, strings path, integers index, doubles size,
                         doubles res, logicals include_bearing, list_of<list> features,
                         list_of<list> variations) {
  int n_strings = string.size();
  writable::doubles widths;
  if (n_strings != 0) {
    if (n_strings != path.size() ||
        n_strings != index.size() ||
        n_strings != features.size() ||
        n_strings != size.size() ||
        n_strings != res.size() ||
        n_strings != include_bearing.size()
    ) {
      cpp11::stop("All input must be the same size");
    }

    auto all_features = create_font_features(features);
    std::vector< std::vector<int> > all_axes, all_coords;
    create_font_variations(variations, all_axes, all_coords);
    auto fonts = create_font_settings(path, index, all_features, all_axes, all_coords);

    int error = 1;
    double width = 0;

    for (int i = 0; i < n_strings; ++i) {
      error = ts_string_width2(
        Rf_translateCharUTF8(string[i]),
        fonts[i],
        size[i],
        res[i],
        include_bearing[i] == TRUE ? 1 : 0,
        &width
      );
      if (error) {
        cpp11::stop("Failed to calculate width of string (%s) with font file (%s) with freetype error %i", Rf_translateCharUTF8(string[i]), Rf_translateCharUTF8(path[i]), error);
      }
      widths.push_back(width);
    }
  }

  return widths;
}

int ts_string_width(const char* string, FontSettings font_info, double size,
                    double res, int include_bearing, double* width) {
  return ts_string_width2(string, FontSettings2(font_info), size, res, include_bearing, width);
}

int ts_string_width2(const char* string, FontSettings2 font_info, double size,
                     double res, int include_bearing, double* width) {
  BEGIN_CPP11
  HarfBuzzShaper& shaper = get_hb_shaper();
  shaper.error_code = 0;
  EmbedInfo string_shape = shaper.shape_single_line(string, font_info, size, res);


  if (shaper.error_code != 0) {
    return shaper.error_code;
  }

  int32_t width_tmp = 0;
  for (size_t i = 0; i < string_shape.glyph_id.size(); ++i) {
    width_tmp += string_shape.x_advance[i];
  }

  if (!include_bearing && !string_shape.glyph_id.empty()) {
    width_tmp -= string_shape.x_bear[0];
    width_tmp -= string_shape.x_advance.back() - string_shape.x_bear.back() - string_shape.width.back();
  }
  *width = double(width_tmp) / 64.0;

  END_CPP11_NO_RETURN
  return 0;
}

int ts_string_shape(const char* string, FontSettings font_info, double size,
                    double res, std::vector<textshaping::Point>& loc, std::vector<uint32_t>& id,
                    std::vector<int>& cluster, std::vector<unsigned int>& font,
                    std::vector<FontSettings>& fallbacks,
                    std::vector<double>& fallback_scaling) {
  std::vector<FontSettings2> fallbacks2;
  int result = ts_string_shape2(string, FontSettings2(font_info), size, res, loc, id, cluster, font, fallbacks2, fallback_scaling);
  fallbacks.clear();
  for (size_t i = 0; i < fallbacks2.size(); ++i) {
    // Avoid the FontSettings constructor: its strncpy triggers
    // -Wstringop-truncation (bound equals destination size) on GCC
    fallbacks.emplace_back();
    FontSettings& fs = fallbacks.back();
    size_t len = strlen(fallbacks2[i].file);
    if (len > PATH_MAX) len = PATH_MAX;
    memcpy(fs.file, fallbacks2[i].file, len);
    fs.file[len] = '\0';
    fs.index = fallbacks2[i].index;
    fs.features = fallbacks2[i].features;
    fs.n_features = fallbacks2[i].n_features;
  }
  return result;
}

int ts_string_shape2(const char* string, FontSettings2 font_info, double size,
                     double res, std::vector<textshaping::Point>& loc, std::vector<uint32_t>& id,
                     std::vector<int>& cluster, std::vector<unsigned int>& font,
                     std::vector<FontSettings2>& fallbacks,
                     std::vector<double>& fallback_scaling) {
  BEGIN_CPP11
  HarfBuzzShaper& shaper = get_hb_shaper();
  shaper.error_code = 0;
  loc.clear();
  id.clear();
  cluster.clear();
  font.clear();
  fallbacks.clear();
  fallback_scaling.clear();
  EmbedInfo string_shape = shaper.shape_single_line(string, font_info, size, res);

  if (shaper.error_code != 0) {
    return shaper.error_code;
  }

  size_t n_glyphs = string_shape.glyph_id.size();
  int32_t x = 0;
  int32_t y = 0;
  for (size_t i = 0; i < n_glyphs; ++i) {
    loc.push_back({
      double(x + string_shape.x_offset[i]) / 64.0,
      double(y + string_shape.y_offset[i]) / 64.0
    });
    x += string_shape.x_advance[i];
    y += string_shape.y_advance[i];
  }
  id.assign(string_shape.glyph_id.begin(), string_shape.glyph_id.end());
  cluster.assign(string_shape.glyph_cluster.begin(), string_shape.glyph_cluster.end());
  font.assign(string_shape.font.begin(), string_shape.font.end());
  fallbacks.assign(string_shape.fallbacks.begin(), string_shape.fallbacks.end());
  fallback_scaling.assign(string_shape.fallback_scaling.begin(), string_shape.fallback_scaling.end());

  END_CPP11_NO_RETURN
  return 0;
}
int ts_string_shape_old(const char* string, FontSettings font_info, double size,
                        double res, double* x, double* y, int* id, int* n_glyphs,
                        unsigned int max_length) {
  int result = 0;
  BEGIN_CPP11
  std::vector<textshaping::Point> _loc;
  std::vector<uint32_t> _id;
  std::vector<int> _cluster;
  std::vector<unsigned int> _font;
  std::vector<FontSettings2> _fallbacks;
  std::vector<double> _fallback_scaling;
  result = ts_string_shape2(string, FontSettings2(font_info), size, res, _loc, _id, _cluster, _font, _fallbacks, _fallback_scaling);

  if (result == 0) {
    *n_glyphs = max_length > _loc.size() ? _loc.size() : max_length;
    for (int i = 0; i < *n_glyphs; ++i) {
      x[i] = _loc[i].x;
      y[i] = _loc[i].y;
      id[i] = (int) _id[i];
    }
  }

  END_CPP11_NO_RETURN
  return result;
}

#endif

bool get_systemfont_cache_compat() {
  // Called directly so this also works in NO_HARFBUZZ_FRIBIDI builds, where
  // the ft_compat constant from string_shape.h isn't available
  return systemfonts::ver2::check_ft_version();
}

void export_string_metrics(DllInfo* dll) {
  R_RegisterCCallable("textshaping", "ts_string_width", (DL_FUNC)ts_string_width);
  R_RegisterCCallable("textshaping", "ts_string_shape_new", (DL_FUNC)ts_string_shape);
  R_RegisterCCallable("textshaping", "ts_string_shape", (DL_FUNC)ts_string_shape_old);
  R_RegisterCCallable("textshaping", "ts_string_width2", (DL_FUNC)ts_string_width2);
  R_RegisterCCallable("textshaping", "ts_string_shape2", (DL_FUNC)ts_string_shape2);
}
