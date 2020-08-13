#pragma once

#define R_NO_REMAP

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <stdlib.h>
#include <stdint.h>

// Calculate the width of a string based on a fontfile, index, size, and
// resolution. Writes it to width, and returns 0 if successful
static inline int ts_string_width(const char* string, const char* fontfile, int index,
                                  double size, double res, int include_bearing,
                                  double* width) {
  static int (*p_string_width)(const char*, const char*, int, double, double, int, double*) = NULL;
  if (p_string_width == NULL) {
    p_string_width = (int (*)(const char*, const char*, int, double, double, int, double*)) R_GetCCallable("textshaper", "string_width");
  }
  return p_string_width(string, fontfile, index, size, res, include_bearing, width);
}
// Calculate glyph positions and id in the font file for a string based on a
// fontfile, index, size, and resolution, and writes it to the x and y arrays.
// Returns 0 if successful.
static inline int ts_string_shape(const char* string, const char* fontfile, int index,
                                  double size, double res, double* x, double* y,
                                  int* id, int* n_glyphs, unsigned int max_length) {
  static int (*p_string_shape)(const char*, const char*, int, double, double, double*, double*, int*, int*, unsigned int) = NULL;
  if (p_string_shape == NULL) {
    p_string_shape = (int (*)(const char*, const char*, int, double, double, double*, double*, int*, int*, unsigned int)) R_GetCCallable("textshaper", "string_shape");
  }
  return p_string_shape(string, fontfile, index, size, res, x, y, id, n_glyphs, max_length);
}
