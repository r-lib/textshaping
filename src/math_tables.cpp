#include <cpp11.hpp>
#include <hb.h>
#include <hb-ot.h>
#include <systemfonts.h>
#include <systemfonts-ft.h>
#include <vector>


// Helper to load font and create HarfBuzz font object
// Uses HarfBuzz blob API directly for MATH table access
static hb_font_t* load_math_font(const char* font_path, int index, double size, double res) {
  // Load font directly through HarfBuzz (not FreeType) for proper MATH table access
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(font_path);
  if (!blob) {
    cpp11::stop("Failed to load font file: %s", font_path);
  }

  hb_face_t* face = hb_face_create(blob, index);
  hb_blob_destroy(blob);

  if (face == hb_face_get_empty()) {
    cpp11::stop("Failed to create font face: %s", font_path);
  }

  hb_font_t* hb_font = hb_font_create(face);
  hb_face_destroy(face);  // font keeps a reference

  // Set font scale: size in points * 64 (26.6 fixed point)
  int scale = static_cast<int>(size * 64);
  hb_font_set_scale(hb_font, scale, scale);

  return hb_font;
}

// Check if a font has MATH table data
[[cpp11::register]]
bool has_math_table(cpp11::strings font_path, cpp11::integers index) {
  if (font_path.size() == 0) {
    cpp11::stop("font_path must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;

  hb_font_t* font = load_math_font(path, idx, 12.0, 72.0);
  hb_face_t* face = hb_font_get_face(font);

  bool has_math = hb_ot_math_has_data(face);

  hb_font_destroy(font);

  return has_math;
}

// Names for all MATH constants
static const char* math_constant_names[] = {
  "ScriptPercentScaleDown",
  "ScriptScriptPercentScaleDown",
  "DelimitedSubFormulaMinHeight",
  "DisplayOperatorMinHeight",
  "MathLeading",
  "AxisHeight",
  "AccentBaseHeight",
  "FlattenedAccentBaseHeight",
  "SubscriptShiftDown",
  "SubscriptTopMax",
  "SubscriptBaselineDropMin",
  "SuperscriptShiftUp",
  "SuperscriptShiftUpCramped",
  "SuperscriptBottomMin",
  "SuperscriptBaselineDropMax",
  "SubSuperscriptGapMin",
  "SuperscriptBottomMaxWithSubscript",
  "SpaceAfterScript",
  "UpperLimitGapMin",
  "UpperLimitBaselineRiseMin",
  "LowerLimitGapMin",
  "LowerLimitBaselineDropMin",
  "StackTopShiftUp",
  "StackTopDisplayStyleShiftUp",
  "StackBottomShiftDown",
  "StackBottomDisplayStyleShiftDown",
  "StackGapMin",
  "StackDisplayStyleGapMin",
  "StretchStackTopShiftUp",
  "StretchStackBottomShiftDown",
  "StretchStackGapAboveMin",
  "StretchStackGapBelowMin",
  "FractionNumeratorShiftUp",
  "FractionNumeratorDisplayStyleShiftUp",
  "FractionDenominatorShiftDown",
  "FractionDenominatorDisplayStyleShiftDown",
  "FractionNumeratorGapMin",
  "FractionNumDisplayStyleGapMin",
  "FractionRuleThickness",
  "FractionDenominatorGapMin",
  "FractionDenomDisplayStyleGapMin",
  "SkewedFractionHorizontalGap",
  "SkewedFractionVerticalGap",
  "OverbarVerticalGap",
  "OverbarRuleThickness",
  "OverbarExtraAscender",
  "UnderbarVerticalGap",
  "UnderbarRuleThickness",
  "UnderbarExtraDescender",
  "RadicalVerticalGap",
  "RadicalDisplayStyleVerticalGap",
  "RadicalRuleThickness",
  "RadicalExtraAscender",
  "RadicalKernBeforeDegree",
  "RadicalKernAfterDegree",
  "RadicalDegreeBottomRaisePercent"
};

static const int NUM_MATH_CONSTANTS = 56;

// Get all MATH table constants for a font
[[cpp11::register]]
cpp11::writable::list get_math_constants(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0) {
    cpp11::stop("font_path must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);
  hb_face_t* face = hb_font_get_face(font);

  if (!hb_ot_math_has_data(face)) {
    hb_font_destroy(font);
    cpp11::stop("Font does not have MATH table: %s", path);
  }

  cpp11::writable::list result;
  cpp11::writable::strings names;
  cpp11::writable::doubles values;

  for (int i = 0; i < NUM_MATH_CONSTANTS; i++) {
    hb_position_t value = hb_ot_math_get_constant(
      font,
      static_cast<hb_ot_math_constant_t>(i)
    );
    names.push_back(math_constant_names[i]);
    values.push_back(static_cast<double>(value));
  }

  result.push_back(cpp11::named_arg("name") = names);
  result.push_back(cpp11::named_arg("value") = values);

  hb_font_destroy(font);

  // Convert to data frame
  result.attr("class") = "data.frame";
  result.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -NUM_MATH_CONSTANTS});

  return result;
}

// Get glyph variants for stretching (e.g., large parentheses, radicals)
[[cpp11::register]]
cpp11::writable::list get_math_glyph_variants(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::integers glyph_id,
    cpp11::logicals vertical,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0 || glyph_id.size() == 0) {
    cpp11::stop("font_path and glyph_id must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  int glyph = glyph_id[0];
  bool is_vertical = vertical.size() > 0 ? (bool)vertical[0] : true;
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);
  hb_face_t* face = hb_font_get_face(font);

  if (!hb_ot_math_has_data(face)) {
    hb_font_destroy(font);
    cpp11::stop("Font does not have MATH table: %s", path);
  }

  hb_direction_t direction = is_vertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;

  // First call to get count
  unsigned int count = 0;
  hb_ot_math_get_glyph_variants(font, glyph, direction, 0, &count, nullptr);

  cpp11::writable::integers variant_glyphs;
  cpp11::writable::doubles variant_advances;

  if (count > 0) {
    std::vector<hb_ot_math_glyph_variant_t> variants(count);
    hb_ot_math_get_glyph_variants(font, glyph, direction, 0, &count, variants.data());

    for (unsigned int i = 0; i < count; i++) {
      variant_glyphs.push_back(static_cast<int>(variants[i].glyph));
      variant_advances.push_back(static_cast<double>(variants[i].advance));
    }
  }

  hb_font_destroy(font);

  cpp11::writable::list result;
  result.push_back(cpp11::named_arg("glyph") = variant_glyphs);
  result.push_back(cpp11::named_arg("advance") = variant_advances);

  int n = variant_glyphs.size();
  if (n > 0) {
    result.attr("class") = "data.frame";
    result.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -n});
  } else {
    result.attr("class") = "data.frame";
    result.attr("row.names") = cpp11::writable::integers();
  }

  return result;
}

// Get glyph assembly parts for constructing large symbols
[[cpp11::register]]
cpp11::writable::list get_math_glyph_assembly(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::integers glyph_id,
    cpp11::logicals vertical,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0 || glyph_id.size() == 0) {
    cpp11::stop("font_path and glyph_id must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  int glyph = glyph_id[0];
  bool is_vertical = vertical.size() > 0 ? (bool)vertical[0] : true;
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);
  hb_face_t* face = hb_font_get_face(font);

  if (!hb_ot_math_has_data(face)) {
    hb_font_destroy(font);
    cpp11::stop("Font does not have MATH table: %s", path);
  }

  hb_direction_t direction = is_vertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;

  // First call to get count
  unsigned int count = 0;
  hb_position_t italics_correction = 0;
  hb_ot_math_get_glyph_assembly(font, glyph, direction, 0, &count, nullptr, &italics_correction);

  cpp11::writable::integers part_glyphs;
  cpp11::writable::doubles start_connectors;
  cpp11::writable::doubles end_connectors;
  cpp11::writable::doubles full_advances;
  cpp11::writable::logicals is_extender;

  if (count > 0) {
    std::vector<hb_ot_math_glyph_part_t> parts(count);
    hb_ot_math_get_glyph_assembly(font, glyph, direction, 0, &count, parts.data(), &italics_correction);

    for (unsigned int i = 0; i < count; i++) {
      part_glyphs.push_back(static_cast<int>(parts[i].glyph));
      start_connectors.push_back(static_cast<double>(parts[i].start_connector_length));
      end_connectors.push_back(static_cast<double>(parts[i].end_connector_length));
      full_advances.push_back(static_cast<double>(parts[i].full_advance));
      is_extender.push_back((parts[i].flags & HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER) != 0);
    }
  }

  hb_font_destroy(font);

  cpp11::writable::list result;
  result.push_back(cpp11::named_arg("glyph") = part_glyphs);
  result.push_back(cpp11::named_arg("start_connector") = start_connectors);
  result.push_back(cpp11::named_arg("end_connector") = end_connectors);
  result.push_back(cpp11::named_arg("full_advance") = full_advances);
  result.push_back(cpp11::named_arg("is_extender") = is_extender);
  result.attr("italics_correction") = cpp11::writable::doubles({static_cast<double>(italics_correction)});

  int n = part_glyphs.size();
  if (n > 0) {
    result.attr("class") = "data.frame";
    result.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -n});
  } else {
    result.attr("class") = "data.frame";
    result.attr("row.names") = cpp11::writable::integers();
  }

  return result;
}

// Get italics correction for a glyph (used for positioning scripts)
[[cpp11::register]]
double get_math_italics_correction(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::integers glyph_id,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0 || glyph_id.size() == 0) {
    cpp11::stop("font_path and glyph_id must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  int glyph = glyph_id[0];
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);

  hb_position_t correction = hb_ot_math_get_glyph_italics_correction(font, glyph);

  hb_font_destroy(font);

  return static_cast<double>(correction);
}

// Get top accent attachment point for a glyph
[[cpp11::register]]
double get_math_top_accent_attachment(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::integers glyph_id,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0 || glyph_id.size() == 0) {
    cpp11::stop("font_path and glyph_id must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  int glyph = glyph_id[0];
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);

  hb_position_t attachment = hb_ot_math_get_glyph_top_accent_attachment(font, glyph);

  hb_font_destroy(font);

  return static_cast<double>(attachment);
}

// Get minimum connector overlap for glyph assembly
[[cpp11::register]]
double get_math_min_connector_overlap(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::logicals vertical,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0) {
    cpp11::stop("font_path must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  bool is_vertical = vertical.size() > 0 ? (bool)vertical[0] : true;
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);

  hb_direction_t direction = is_vertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;
  hb_position_t overlap = hb_ot_math_get_min_connector_overlap(font, direction);

  hb_font_destroy(font);

  return static_cast<double>(overlap);
}

// Check if a glyph is an extended shape (large operator)
[[cpp11::register]]
bool is_math_glyph_extended_shape(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::integers glyph_id
) {
  if (font_path.size() == 0 || glyph_id.size() == 0) {
    cpp11::stop("font_path and glyph_id must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  int glyph = glyph_id[0];

  hb_font_t* font = load_math_font(path, idx, 12.0, 72.0);
  hb_face_t* face = hb_font_get_face(font);

  bool is_extended = hb_ot_math_is_glyph_extended_shape(face, glyph);

  hb_font_destroy(font);

  return is_extended;
}

// Get math kerning for script positioning
[[cpp11::register]]
double get_math_kerning(
    cpp11::strings font_path,
    cpp11::integers index,
    cpp11::integers glyph_id,
    cpp11::integers kern_type,
    cpp11::doubles correction_height,
    cpp11::doubles size,
    cpp11::doubles res
) {
  if (font_path.size() == 0 || glyph_id.size() == 0) {
    cpp11::stop("font_path and glyph_id must have at least one element");
  }

  const char* path = CHAR(font_path[0]);
  int idx = index.size() > 0 ? index[0] : 0;
  int glyph = glyph_id[0];
  int kern = kern_type.size() > 0 ? kern_type[0] : 0;
  double corr_height = correction_height.size() > 0 ? correction_height[0] : 0.0;
  double font_size = size.size() > 0 ? size[0] : 12.0;
  double resolution = res.size() > 0 ? res[0] : 72.0;

  if (kern < 0 || kern > 3) {
    cpp11::stop("kern_type must be 0-3 (TOP_RIGHT, TOP_LEFT, BOTTOM_RIGHT, BOTTOM_LEFT)");
  }

  hb_font_t* font = load_math_font(path, idx, font_size, resolution);

  hb_position_t kerning = hb_ot_math_get_glyph_kerning(
    font,
    glyph,
    static_cast<hb_ot_math_kern_t>(kern),
    static_cast<hb_position_t>(corr_height)
  );

  hb_font_destroy(font);

  return static_cast<double>(kerning);
}
