#define R_NO_REMAP

#include "string_metrics.h"
#include "string_shape.h"

#include <cpp11/declarations.hpp>
#include "utils.h"

using namespace cpp11;

list get_string_shape_c(strings string, integers id, strings path, integers index,
                        doubles size, doubles res, doubles lineheight, integers align,
                        doubles hjust, doubles vjust, doubles width, doubles tracking,
                        doubles indent, doubles hanging, doubles space_before,
                        doubles space_after) {
  BEGIN_CPP11
  int n_strings = string.size();
  bool one_path = path.size() == 1;
  const char* first_path = Rf_translateCharUTF8(path[0]);
  int first_index = index[0];
  bool one_size = size.size() == 1;
  double first_size = size[0];
  bool one_res = res.size() == 1;
  double first_res = res[0];
  bool one_lht = lineheight.size() == 1;
  double first_lht = lineheight[0];
  bool one_align = align.size() == 1;
  int first_align = align[0];
  bool one_hjust = hjust.size() == 1;
  double first_hjust = hjust[0];
  bool one_vjust = vjust.size() == 1;
  double first_vjust = vjust[0];
  bool one_width = width.size() == 1;
  double first_width = width[0] * 64;
  bool one_tracking = tracking.size() == 1;
  double first_tracking = tracking[0];
  bool one_indent = indent.size() == 1;
  double first_indent = indent[0] * 64;
  bool one_hanging = hanging.size() == 1;
  double first_hanging = hanging[0] * 64;
  bool one_before = space_before.size() == 1;
  double first_before = space_before[0] * 64;
  bool one_after = space_after.size() == 1;
  double first_after = space_after[0] * 64;

  // Return Columns
  writable::integers glyph, glyph_id, metric_id, string_id;
  writable::doubles x_offset, y_offset, x_midpoint, widths, heights, left_bearings, right_bearings, top_bearings, bottom_bearings, left_border, top_border, pen_x, pen_y;

  // Shape the text
  int cur_id = id[0] - 1; // make sure it differs from first
  bool success = false;

  HarfBuzzShaper shaper;
  for (int i = 0; i < n_strings; ++i) {
    const char* this_string = Rf_translateCharUTF8(string[i]);
    int this_id = id[i];
    if (cur_id == this_id) {
      success = shaper.add_string(this_string,
                                  one_path ? first_path : Rf_translateCharUTF8(path[i]),
                                  one_path ? first_index : index[i],
                                  one_size ? first_size : size[i],
                                  one_tracking ? first_tracking : tracking[i]);
      if (!success) {
        Rf_error("Failed to shape string (%s) with font file (%s) with freetype error %i", this_string, Rf_translateCharUTF8(path[i]), shaper.error_code);
      }
    } else {
      cur_id = this_id;
      success = shaper.shape_string(this_string,
                                    one_path ? first_path : Rf_translateCharUTF8(path[i]),
                                    one_path ? first_index : index[i],
                                    one_size ? first_size : size[i],
                                    one_res ? first_res : res[i],
                                    one_lht ? first_lht : lineheight[i],
                                    one_align ? first_align : align[i],
                                    one_hjust ? first_hjust : hjust[i],
                                    one_vjust ? first_vjust : vjust[i],
                                    one_width ? first_width : width[i] * 64,
                                    one_tracking ? first_tracking : tracking[i],
                                    one_indent ? first_indent : indent[i] * 64,
                                    one_hanging ? first_hanging : hanging[i] * 64,
                                    one_before ? first_before : space_before[i] * 64,
                                    one_after ? first_after : space_after[i] * 64);
      if (!success) {
        Rf_error("Failed to shape string (%s) with font file (%s) with freetype error %i", this_string, Rf_translateCharUTF8(STRING_ELT(path, i)), shaper.error_code);
      }
    }
    bool store_string = i == n_strings - 1 || cur_id != INTEGER(id)[i + 1];
    if (store_string) {
      success = shaper.finish_string();
      if (!success) {
        Rf_error("Failed to finalise string shaping");
      }
      int n_glyphs = shaper.glyph_id.size();
      for (int j = 0; j < n_glyphs; j++) {
        glyph.push_back((int) shaper.glyph_cluster[j]);
        glyph_id.push_back((int) shaper.glyph_id[j]);
        metric_id.push_back(pen_x.size() + 1);
        string_id.push_back(shaper.string_id[j] + 1);
        x_offset.push_back(double(shaper.x_pos[j]) / 64.0);
        y_offset.push_back(double(shaper.y_pos[j]) / 64.0);
        x_midpoint.push_back(double(shaper.x_mid[j]) / 64.0);
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
    }
  }
  writable::strings str(pen_x.size());

  writable::data_frame string_df({
    "string"_nm = (SEXP) str,
    "width"_nm = (SEXP) widths,
    "height"_nm = (SEXP) heights,
    "left_bearing"_nm = (SEXP) left_bearings,
    "right_bearing"_nm = (SEXP) right_bearings,
    "top_bearing"_nm = (SEXP) top_bearings,
    "bottom_bearing"_nm = (SEXP) bottom_bearings,
    "left_border"_nm = (SEXP) left_border,
    "top_border"_nm = (SEXP) top_border,
    "pen_x"_nm = (SEXP) pen_x,
    "pen_y"_nm = (SEXP) pen_y
  });
  string_df.attr("class") = writable::strings({"tbl_df", "tbl", "data.frame"});

  writable::data_frame info_df({
    "glyph"_nm = (SEXP) glyph,
    "index"_nm = (SEXP) glyph_id,
    "metric_id"_nm = (SEXP) metric_id,
    "string_id"_nm = (SEXP) string_id,
    "x_offset"_nm = (SEXP) x_offset,
    "y_offset"_nm = (SEXP) y_offset,
    "x_midpoint"_nm = (SEXP) x_midpoint
  });
  info_df.attr("class") = writable::strings({"tbl_df", "tbl", "data.frame"});

  return writable::list({
    "shape"_nm = info_df,
    "metrics"_nm = string_df
  });
  END_CPP11
}

doubles get_line_width_c(strings string, strings path, integers index, doubles size,
                         doubles res, logicals include_bearing) {
  BEGIN_CPP11
  int n_strings = string.size();
  bool one_path = path.size() == 1;
  const char* first_path = Rf_translateCharUTF8(path[0]);
  int first_index = index[0];
  bool one_size = size.size() == 1;
  double first_size = size[0];
  bool one_res = res.size() == 1;
  double first_res = res[0];
  bool one_bear = include_bearing.size() == 1;
  int first_bear = include_bearing[0];

  writable::doubles widths;
  int error = 1;
  double width = 0;

  for (int i = 0; i < n_strings; ++i) {
    error = string_width(
      Rf_translateCharUTF8(string[i]),
      one_path ? first_path : Rf_translateCharUTF8(path[i]),
      one_path ? first_index : index[i],
      one_size ? first_size : size[i],
      one_res ? first_res : res[i],
      one_bear ? first_bear : include_bearing[0],
      &width
    );
    if (error) {
      Rf_error("Failed to calculate width of string (%s) with font file (%s) with freetype error %i", Rf_translateCharUTF8(string[i]), Rf_translateCharUTF8(path[i]), error);
    }
    widths.push_back(width);
  }

  return widths;
  END_CPP11
}

int string_width(const char* string, const char* fontfile, int index,
                 double size, double res, int include_bearing, double* width) {
  BEGIN_CPP11
  HarfBuzzShaper shaper;
  bool success = shaper.single_line_shape(
    string, fontfile, index, size, res
  );

  if (!success) {
    return shaper.error_code;
  }

  int32_t width_tmp = shaper.last_shape_info.width;
  if (!include_bearing) {
    width_tmp -= shaper.last_shape_info.left_bearing;
    width_tmp -= shaper.last_shape_info.right_bearing;
  }
  *width = double(width_tmp) / 64.0;

  END_CPP11_NO_RETURN
  return 0;
}

int string_shape(const char* string, const char* fontfile, int index,
                 double size, double res, double* x, double* y, int* id, int* n_glyphs,
                 unsigned int max_length) {
  BEGIN_CPP11
  HarfBuzzShaper shaper;
  bool success = shaper.single_line_shape(
    string, fontfile, index, size, res
  );
  if (!success) {
    return shaper.error_code;
  }
  *n_glyphs = max_length < shaper.last_shape_info.x_pos.size() ? max_length : shaper.last_shape_info.x_pos.size();
  for (unsigned int i = 0; i < *n_glyphs; ++i) {
    x[i] = double(shaper.last_shape_info.x_pos[i]) / 64.0;
    y[i] = 0.0;
    id[i] = shaper.last_shape_info.glyph_id[i];
  }

  END_CPP11_NO_RETURN
  return 0;
}

void export_string_metrics(DllInfo* dll) {
  R_RegisterCCallable("textshaping", "string_width", (DL_FUNC)string_width);
  R_RegisterCCallable("textshaping", "string_shape", (DL_FUNC)string_shape);
}
