#include "math_layout.h"
#include "math_symbols.h"
#include <systemfonts.h>
#include <systemfonts-ft.h>
#include <cmath>
#include <algorithm>
#include <functional>

namespace math {

// Check FreeType compatibility with systemfonts
static const bool ft_compat = systemfonts::ver2::check_ft_version();

// =============================================================================
// MathBox implementation
// =============================================================================

void MathBox::shift(double dx, double dy) {
  for (auto& g : glyphs) {
    g.x += dx;
    g.y += dy;
  }
  for (auto& l : lines) {
    l.x1 += dx;
    l.y1 += dy;
    l.x2 += dx;
    l.y2 += dy;
  }
  for (auto& b : boxes) {
    b.x += dx;
    b.y += dy;
  }
}

void MathBox::merge(const MathBox& other, double dx, double dy) {
  for (const auto& g : other.glyphs) {
    GlyphInfo shifted = g;
    shifted.x += dx;
    shifted.y += dy;
    glyphs.push_back(shifted);
  }
  for (const auto& l : other.lines) {
    LineInfo shifted = l;
    shifted.x1 += dx;
    shifted.y1 += dy;
    shifted.x2 += dx;
    shifted.y2 += dy;
    lines.push_back(shifted);
  }
  for (const auto& b : other.boxes) {
    BoxInfo shifted = b;
    shifted.x += dx;
    shifted.y += dy;
    boxes.push_back(shifted);
  }
}

// =============================================================================
// MathLayout implementation
// =============================================================================

MathLayout::MathLayout(const char* font_path, int font_index, double size, double res)
  : hb_font_(nullptr)
  , hb_face_(nullptr)
  , ft_face_(nullptr)
  , font_path_(font_path)
  , ft_library_(nullptr)
  , owns_ft_library_(false)
  , has_math_(false)
  , base_size_(size)
  , display_mode_(false)
  , current_scale_(1.0 / 64.0)  // Convert from 26.6 fixed-point to points
{
  int error = 0;

  if (ft_compat) {
    ft_face_ = get_cached_face(font_path, font_index, size, res, &error);
  } else {
    FT_Init_FreeType(&ft_library_);
    owns_ft_library_ = true;
    FT_Error err = FT_New_Face(ft_library_, font_path, font_index, &ft_face_);
    if (err != 0) {
      errors_.push_back("Failed to load font");
      return;
    }
    if (FT_IS_SCALABLE(ft_face_)) {
      FT_Set_Char_Size(ft_face_, 0, size * 64, res, res);
    }
  }

  if (ft_face_ == nullptr || error != 0) {
    errors_.push_back("Failed to load font");
    return;
  }

  // Create HarfBuzz font
  hb_font_ = hb_ft_font_create_referenced(ft_face_);
  if (hb_font_ == nullptr) {
    errors_.push_back("Failed to create HarfBuzz font");
    return;
  }
  hb_face_ = hb_font_get_face(hb_font_);
  if (hb_face_ == nullptr) {
    errors_.push_back("Failed to get HarfBuzz face");
    return;
  }

  // Check for MATH table
  has_math_ = hb_ot_math_has_data(hb_face_);

  if (has_math_) {
    load_constants();
  } else {
    load_default_constants();
  }

  // Build flattened accent lookup
  load_flattened_accents();
}

MathLayout::~MathLayout() {
  if (hb_font_) {
    hb_font_destroy(hb_font_);
  }
  if (!ft_compat && ft_face_) {
    FT_Done_Face(ft_face_);
  }
  if (owns_ft_library_ && ft_library_) {
    FT_Done_FreeType(ft_library_);
  }
}

void MathLayout::load_constants() {
  auto get = [this](hb_ot_math_constant_t c) {
    return hb_ot_math_get_constant(hb_font_, c);
  };

  constants_.script_percent_scale_down = get(HB_OT_MATH_CONSTANT_SCRIPT_PERCENT_SCALE_DOWN);
  constants_.script_script_percent_scale_down = get(HB_OT_MATH_CONSTANT_SCRIPT_SCRIPT_PERCENT_SCALE_DOWN);
  constants_.axis_height = get(HB_OT_MATH_CONSTANT_AXIS_HEIGHT);
  constants_.superscript_shift_up = get(HB_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP);
  constants_.superscript_shift_up_cramped = get(HB_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP_CRAMPED);
  constants_.superscript_bottom_min = get(HB_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MIN);
  constants_.superscript_baseline_drop_max = get(HB_OT_MATH_CONSTANT_SUPERSCRIPT_BASELINE_DROP_MAX);
  constants_.subscript_shift_down = get(HB_OT_MATH_CONSTANT_SUBSCRIPT_SHIFT_DOWN);
  constants_.subscript_top_max = get(HB_OT_MATH_CONSTANT_SUBSCRIPT_TOP_MAX);
  constants_.subscript_baseline_drop_min = get(HB_OT_MATH_CONSTANT_SUBSCRIPT_BASELINE_DROP_MIN);
  constants_.sub_superscript_gap_min = get(HB_OT_MATH_CONSTANT_SUB_SUPERSCRIPT_GAP_MIN);
  constants_.superscript_bottom_max_with_subscript = get(HB_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT);
  constants_.space_after_script = get(HB_OT_MATH_CONSTANT_SPACE_AFTER_SCRIPT);
  constants_.fraction_numerator_shift_up = get(HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_SHIFT_UP);
  constants_.fraction_numerator_display_style_shift_up = get(HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP);
  constants_.fraction_denominator_shift_down = get(HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_SHIFT_DOWN);
  constants_.fraction_denominator_display_style_shift_down = get(HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN);
  constants_.fraction_numerator_gap_min = get(HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_GAP_MIN);
  constants_.fraction_num_display_style_gap_min = get(HB_OT_MATH_CONSTANT_FRACTION_NUM_DISPLAY_STYLE_GAP_MIN);
  constants_.fraction_rule_thickness = get(HB_OT_MATH_CONSTANT_FRACTION_RULE_THICKNESS);
  constants_.fraction_denominator_gap_min = get(HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_GAP_MIN);
  constants_.fraction_denom_display_style_gap_min = get(HB_OT_MATH_CONSTANT_FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN);
  constants_.radical_vertical_gap = get(HB_OT_MATH_CONSTANT_RADICAL_VERTICAL_GAP);
  constants_.radical_display_style_vertical_gap = get(HB_OT_MATH_CONSTANT_RADICAL_DISPLAY_STYLE_VERTICAL_GAP);
  constants_.radical_rule_thickness = get(HB_OT_MATH_CONSTANT_RADICAL_RULE_THICKNESS);
  constants_.radical_extra_ascender = get(HB_OT_MATH_CONSTANT_RADICAL_EXTRA_ASCENDER);
  constants_.radical_kern_before_degree = get(HB_OT_MATH_CONSTANT_RADICAL_KERN_BEFORE_DEGREE);
  constants_.radical_kern_after_degree = get(HB_OT_MATH_CONSTANT_RADICAL_KERN_AFTER_DEGREE);
  constants_.radical_degree_bottom_raise_percent = get(HB_OT_MATH_CONSTANT_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT);
  constants_.upper_limit_gap_min = get(HB_OT_MATH_CONSTANT_UPPER_LIMIT_GAP_MIN);
  constants_.upper_limit_baseline_rise_min = get(HB_OT_MATH_CONSTANT_UPPER_LIMIT_BASELINE_RISE_MIN);
  constants_.lower_limit_gap_min = get(HB_OT_MATH_CONSTANT_LOWER_LIMIT_GAP_MIN);
  constants_.lower_limit_baseline_drop_min = get(HB_OT_MATH_CONSTANT_LOWER_LIMIT_BASELINE_DROP_MIN);
  constants_.accent_base_height = get(HB_OT_MATH_CONSTANT_ACCENT_BASE_HEIGHT);
  constants_.flattened_accent_base_height = get(HB_OT_MATH_CONSTANT_FLATTENED_ACCENT_BASE_HEIGHT);
  constants_.overbar_vertical_gap = get(HB_OT_MATH_CONSTANT_OVERBAR_VERTICAL_GAP);
  constants_.overbar_rule_thickness = get(HB_OT_MATH_CONSTANT_OVERBAR_RULE_THICKNESS);
  constants_.overbar_extra_ascender = get(HB_OT_MATH_CONSTANT_OVERBAR_EXTRA_ASCENDER);
  constants_.underbar_vertical_gap = get(HB_OT_MATH_CONSTANT_UNDERBAR_VERTICAL_GAP);
  constants_.underbar_rule_thickness = get(HB_OT_MATH_CONSTANT_UNDERBAR_RULE_THICKNESS);
  constants_.underbar_extra_descender = get(HB_OT_MATH_CONSTANT_UNDERBAR_EXTRA_DESCENDER);
  constants_.stack_top_shift_up = get(HB_OT_MATH_CONSTANT_STACK_TOP_SHIFT_UP);
  constants_.stack_top_display_style_shift_up = get(HB_OT_MATH_CONSTANT_STACK_TOP_DISPLAY_STYLE_SHIFT_UP);
  constants_.stack_bottom_shift_down = get(HB_OT_MATH_CONSTANT_STACK_BOTTOM_SHIFT_DOWN);
  constants_.stack_bottom_display_style_shift_down = get(HB_OT_MATH_CONSTANT_STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN);
  constants_.stack_gap_min = get(HB_OT_MATH_CONSTANT_STACK_GAP_MIN);
  constants_.stack_display_style_gap_min = get(HB_OT_MATH_CONSTANT_STACK_DISPLAY_STYLE_GAP_MIN);
  constants_.display_operator_min_height = get(HB_OT_MATH_CONSTANT_DISPLAY_OPERATOR_MIN_HEIGHT);
}

void MathLayout::load_default_constants() {
  // Fallback constants for fonts without MATH tables
  // Values are based on TeX's defaults, expressed relative to font size
  // Since HarfBuzz returns values in 26.6 fixed-point, 1em = base_size_ * 64

  int em = static_cast<int>(base_size_ * 64);

  // Percent scale downs (these are actual percentages, not font units)
  constants_.script_percent_scale_down = 70;
  constants_.script_script_percent_scale_down = 50;

  // Axis height (center of fraction bar, approximately half x-height)
  constants_.axis_height = em * 25 / 100;  // 0.25em

  // Superscript positioning
  constants_.superscript_shift_up = em * 45 / 100;           // 0.45em
  constants_.superscript_shift_up_cramped = em * 35 / 100;   // 0.35em
  constants_.superscript_bottom_min = em * 25 / 100;         // 0.25em
  constants_.superscript_baseline_drop_max = em * 40 / 100;  // 0.40em

  // Subscript positioning
  constants_.subscript_shift_down = em * 20 / 100;           // 0.20em
  constants_.subscript_top_max = em * 40 / 100;              // 0.40em
  constants_.subscript_baseline_drop_min = em * 10 / 100;    // 0.10em

  // Sub-superscript positioning
  constants_.sub_superscript_gap_min = em * 15 / 100;        // 0.15em
  constants_.superscript_bottom_max_with_subscript = em * 40 / 100;  // 0.40em
  constants_.space_after_script = em * 5 / 100;              // 0.05em

  // Fraction positioning
  constants_.fraction_numerator_shift_up = em * 45 / 100;    // 0.45em
  constants_.fraction_numerator_display_style_shift_up = em * 55 / 100;  // 0.55em
  constants_.fraction_denominator_shift_down = em * 45 / 100;  // 0.45em
  constants_.fraction_denominator_display_style_shift_down = em * 55 / 100;  // 0.55em
  constants_.fraction_numerator_gap_min = em * 5 / 100;      // 0.05em
  constants_.fraction_num_display_style_gap_min = em * 15 / 100;  // 0.15em
  constants_.fraction_rule_thickness = em * 4 / 100;         // 0.04em
  constants_.fraction_denominator_gap_min = em * 5 / 100;    // 0.05em
  constants_.fraction_denom_display_style_gap_min = em * 15 / 100;  // 0.15em

  // Radical positioning
  constants_.radical_vertical_gap = em * 8 / 100;            // 0.08em
  constants_.radical_display_style_vertical_gap = em * 12 / 100;  // 0.12em
  constants_.radical_rule_thickness = em * 4 / 100;          // 0.04em
  constants_.radical_extra_ascender = em * 4 / 100;          // 0.04em
  constants_.radical_kern_before_degree = em * 5 / 100;      // 0.05em
  constants_.radical_kern_after_degree = -em * 50 / 100;     // -0.50em
  constants_.radical_degree_bottom_raise_percent = 60;       // percentage

  // Limits positioning (for big operators)
  constants_.upper_limit_gap_min = em * 10 / 100;            // 0.10em
  constants_.upper_limit_baseline_rise_min = em * 20 / 100;  // 0.20em
  constants_.lower_limit_gap_min = em * 10 / 100;            // 0.10em
  constants_.lower_limit_baseline_drop_min = em * 40 / 100;  // 0.40em

  // Accent positioning
  constants_.accent_base_height = em * 50 / 100;             // 0.50em (x-height)
  constants_.flattened_accent_base_height = em * 65 / 100;   // 0.65em

  // Overbar/underbar
  constants_.overbar_vertical_gap = em * 15 / 100;           // 0.15em
  constants_.overbar_rule_thickness = em * 4 / 100;          // 0.04em
  constants_.overbar_extra_ascender = em * 4 / 100;          // 0.04em
  constants_.underbar_vertical_gap = em * 15 / 100;          // 0.15em
  constants_.underbar_rule_thickness = em * 4 / 100;         // 0.04em
  constants_.underbar_extra_descender = em * 4 / 100;        // 0.04em

  // Stack (for atop-style fractions without bar)
  constants_.stack_top_shift_up = em * 45 / 100;             // 0.45em
  constants_.stack_top_display_style_shift_up = em * 55 / 100;  // 0.55em
  constants_.stack_bottom_shift_down = em * 45 / 100;        // 0.45em
  constants_.stack_bottom_display_style_shift_down = em * 55 / 100;  // 0.55em
  constants_.stack_gap_min = em * 20 / 100;                  // 0.20em
  constants_.stack_display_style_gap_min = em * 35 / 100;    // 0.35em

  // Big operator height
  constants_.display_operator_min_height = em * 150 / 100;   // 1.50em
}

void MathLayout::load_flattened_accents() {
  // Build flattened accent lookup using GSUB 'flac' feature
  // Iterate over the shared accents table from math_symbols.h
  hb_buffer_t* buf = hb_buffer_create();
  hb_feature_t flac = { HB_TAG('f','l','a','c'), 1, 0, (unsigned)-1 };

  for (const auto& entry : accents) {
    uint32_t cp = entry.second;
    uint32_t regular_glyph = get_glyph(cp);
    if (regular_glyph == 0) continue;

    hb_buffer_clear_contents(buf);
    hb_buffer_add_codepoints(buf, &cp, 1, 0, 1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
    hb_shape(hb_font_, buf, &flac, 1);

    unsigned int glyph_count;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    if (glyph_count > 0) {
      uint32_t flattened_glyph = info[0].codepoint;
      if (flattened_glyph != regular_glyph) {
        flattened_accents_[regular_glyph] = flattened_glyph;
      }
    }
  }

  hb_buffer_destroy(buf);
}

double MathLayout::scale(int value) const {
  // HarfBuzz returns values in font units already scaled for the font size
  // Convert from font units to points
  return value * current_scale_;
}

double MathLayout::get_script_size(int level) const {
  if (level == 0) return base_size_;
  if (level == 1) return base_size_ * constants_.script_percent_scale_down / 100.0;
  return base_size_ * constants_.script_script_percent_scale_down / 100.0;
}

uint32_t MathLayout::get_glyph(uint32_t codepoint) {
  hb_codepoint_t glyph;
  if (hb_font_get_nominal_glyph(hb_font_, codepoint, &glyph)) {
    return glyph;
  }
  return 0;  // .notdef
}

void MathLayout::get_glyph_metrics(uint32_t glyph_id, double* advance, double* height, double* depth) {
  if (hb_font_ == nullptr) {
    *advance = *height = *depth = 0;
    return;
  }

  hb_glyph_extents_t extents;
  if (!hb_font_get_glyph_extents(hb_font_, glyph_id, &extents)) {
    // Glyph doesn't exist, use zeros
    extents.x_bearing = 0;
    extents.y_bearing = 0;
    extents.width = 0;
    extents.height = 0;
  }

  hb_position_t h_advance = hb_font_get_glyph_h_advance(hb_font_, glyph_id);

  *advance = h_advance * current_scale_;
  *height = (extents.y_bearing) * current_scale_;
  *depth = (-extents.y_bearing - extents.height) * current_scale_;
  if (*depth < 0) *depth = 0;
}

void MathLayout::get_glyph_extents(uint32_t glyph_id, double* x_bearing, double* y_bearing,
                                    double* width, double* height) {
  hb_glyph_extents_t extents;
  hb_font_get_glyph_extents(hb_font_, glyph_id, &extents);

  *x_bearing = extents.x_bearing * current_scale_;
  *y_bearing = extents.y_bearing * current_scale_;
  *width = extents.width * current_scale_;
  *height = extents.height * current_scale_;
}

double MathLayout::get_italic_correction(uint32_t glyph_id) {
  if (!has_math_) return 0;
  return hb_ot_math_get_glyph_italics_correction(hb_font_, glyph_id) * current_scale_;
}

double MathLayout::get_top_accent_attachment(uint32_t glyph_id) {
  if (!has_math_) {
    // Default: center of glyph
    double advance, height, depth;
    get_glyph_metrics(glyph_id, &advance, &height, &depth);
    return advance / 2;
  }
  return hb_ot_math_get_glyph_top_accent_attachment(hb_font_, glyph_id) * current_scale_;
}

uint32_t MathLayout::get_size_variant(uint32_t glyph_id, double target_size, bool vertical) {
  if (!has_math_) return glyph_id;

  hb_direction_t dir = vertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;

  // Get total number of variants (return value gives total count)
  unsigned int count = hb_ot_math_get_glyph_variants(hb_font_, glyph_id, dir, 0, nullptr, nullptr);

  if (count == 0) {
    return glyph_id;
  }

  // Fetch the variants
  std::vector<hb_ot_math_glyph_variant_t> variants(count);
  unsigned int fetched = count;
  hb_ot_math_get_glyph_variants(hb_font_, glyph_id, dir, 0, &fetched, variants.data());

  // Find smallest variant that's large enough
  int target = static_cast<int>(target_size / current_scale_);
  for (const auto& v : variants) {
    if (v.advance >= target) {
      return v.glyph;
    }
  }

  // Return largest available
  return variants.back().glyph;
}

void MathLayout::error(const std::string& msg) {
  errors_.push_back(msg);
}

MathBox MathLayout::layout(const std::string& latex, bool display_mode) {
  MathParser parser(latex);
  auto ast = parser.parse();

  if (parser.has_errors()) {
    for (const auto& e : parser.errors()) {
      errors_.push_back(e);
    }
  }

  return layout(ast.get(), display_mode);
}

MathBox MathLayout::layout(MathNode* node, bool display_mode) {
  // Check if font was initialized properly
  if (hb_font_ == nullptr) {
    error("Font not initialized");
    return MathBox();
  }

  display_mode_ = display_mode;
  current_scale_ = 1.0 / 64.0;  // Reset to base scale (26.6 fixed-point to points)
  forced_style_ = MathDisplayStyle::INHERIT;  // No forced style by default
  return layout_node(node);
}

MathBox MathLayout::layout_node(MathNode* node) {
  if (!node) return MathBox();

  switch (node->type) {
    case NodeType::SYMBOL:
      return layout_symbol(node);
    case NodeType::NUMBER:
      return layout_number(node);
    case NodeType::ROW:
      return layout_row(node);
    case NodeType::GROUP:
      return layout_group(node);
    case NodeType::SUPERSCRIPT:
      return layout_superscript(node);
    case NodeType::SUBSCRIPT:
      return layout_subscript(node);
    case NodeType::SUBSUPERSCRIPT:
      return layout_subsuperscript(node);
    case NodeType::FRACTION:
      return layout_fraction(node);
    case NodeType::STACK:
      return layout_stack(node);
    case NodeType::SQRT:
      return layout_sqrt(node);
    case NodeType::DELIMITED:
      return layout_delimited(node);
    case NodeType::BIG_OP:
      return layout_big_op(node);
    case NodeType::ACCENT:
      return layout_accent(node);
    case NodeType::FUNCTION:
      return layout_function(node);
    case NodeType::OPERATORNAME:
      return layout_operatorname(node);
    case NodeType::TEXT:
      return layout_text(node);
    case NodeType::SPACE_NODE:
      return layout_space(node);
    case NodeType::STYLED:
      return layout_styled(node);
    case NodeType::MATRIX:
      return layout_matrix(node);
    case NodeType::SUBSTACK:
      return layout_substack(node);
    case NodeType::PHANTOM:
      return layout_phantom(node);
    case NodeType::COLORED:
      return layout_colored(node);
    case NodeType::COLORBOX:
      return layout_colorbox(node);
    case NodeType::CANCEL:
      return layout_cancel(node);
    case NodeType::NEGATED:
      return layout_negated(node);
    case NodeType::BOXED:
      return layout_boxed(node);
    case NodeType::EXT_ARROW:
      return layout_ext_arrow(node);
    case NodeType::BIG_DELIM:
      return layout_big_delim(node);
    case NodeType::STYLE_CHANGE:
      return layout_style_change(node);
    case NodeType::ALIGNED:
      return layout_aligned(node);
    case NodeType::SMASH:
      return layout_smash(node);
    case NodeType::MOD:
      return layout_mod(node);
    case NodeType::RULE:
      return layout_rule(node);
    default:
      error("Unknown node type");
      return MathBox();
  }
}

MathBox MathLayout::make_glyph_box(uint32_t glyph_id) {
  MathBox box;
  double advance, height, depth;
  get_glyph_metrics(glyph_id, &advance, &height, &depth);

  box.width = advance;
  box.height = height;
  box.depth = depth;
  box.italic_corr = get_italic_correction(glyph_id);

  GlyphInfo glyph;
  glyph.glyph_id = glyph_id;
  glyph.x = 0;
  glyph.y = 0;
  // current_scale_ includes 1/64 for coordinate conversion, multiply by 64 to get logical size
  glyph.size = base_size_ * current_scale_ * 64.0;
  glyph.advance = advance;
  glyph.color = current_color_;
  box.glyphs.push_back(glyph);

  return box;
}

MathBox MathLayout::layout_symbol(MathNode* node) {
  const std::string& val = node->value;
  if (val.empty()) return MathBox();

  // Decode UTF-8 to get first codepoint
  uint32_t codepoint = 0;
  const unsigned char* s = reinterpret_cast<const unsigned char*>(val.c_str());
  if ((s[0] & 0x80) == 0) {
    codepoint = s[0];
  } else if ((s[0] & 0xE0) == 0xC0) {
    codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
  } else if ((s[0] & 0xF0) == 0xE0) {
    codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
  } else if ((s[0] & 0xF8) == 0xF0) {
    codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
  }

  // Map ASCII characters to proper math Unicode equivalents
  if (codepoint == '-') {
    codepoint = 0x2212;  // Minus sign
  }

  uint32_t glyph_id = 0;

  // Use math italic for ASCII letters (standard LaTeX math behavior)
  // In math mode, single letters represent variables and should be italic
  if (codepoint >= 'A' && codepoint <= 'Z') {
    glyph_id = get_glyph(0x1D434 + (codepoint - 'A'));  // Math italic capitals
  } else if (codepoint >= 'a' && codepoint <= 'z') {
    // h is special case (planck constant ℎ)
    if (codepoint == 'h') {
      glyph_id = get_glyph(0x210E);
    } else {
      glyph_id = get_glyph(0x1D44E + (codepoint - 'a'));  // Math italic lowercase
    }
  }

  // Fallback to regular glyph if math italic not available
  if (glyph_id == 0) {
    glyph_id = get_glyph(codepoint);
  }

  return make_glyph_box(glyph_id);
}

MathBox MathLayout::layout_number(MathNode* node) {
  return layout_string(node->value);
}

MathBox MathLayout::layout_string(const std::string& str) {
  MathBox result;
  double x = 0;

  for (char c : str) {
    uint32_t glyph_id = get_glyph(static_cast<uint32_t>(c));
    MathBox glyph_box = make_glyph_box(glyph_id);
    result.merge(glyph_box, x, 0);
    x += glyph_box.width;
    result.height = std::max(result.height, glyph_box.height);
    result.depth = std::max(result.depth, glyph_box.depth);
  }

  result.width = x;
  return result;
}

MathBox MathLayout::layout_row(MathNode* node) {
  MathBox result;
  double x = 0;

  for (auto& child : node->children) {
    MathBox child_box = layout_node(child.get());
    result.merge(child_box, x, 0);
    x += child_box.width;
    result.height = std::max(result.height, child_box.height);
    result.depth = std::max(result.depth, child_box.depth);
  }

  result.width = x;
  return result;
}

MathBox MathLayout::layout_group(MathNode* node) {
  if (node->children.empty()) return MathBox();
  return layout_node(node->children[0].get());
}

MathBox MathLayout::layout_superscript(MathNode* node) {
  if (node->children.size() < 2) return MathBox();

  MathNode* base_node = node->children[0].get();

  // Check if this is a big operator that should use limits positioning
  bool use_limits = false;
  if (base_node->type == NodeType::BIG_OP) {
    bool eff_display = display_mode_;
    if (forced_style_ == MathDisplayStyle::DISPLAY) {
      eff_display = true;
    } else if (forced_style_ == MathDisplayStyle::TEXT ||
               forced_style_ == MathDisplayStyle::SCRIPT ||
               forced_style_ == MathDisplayStyle::SCRIPTSCRIPT) {
      eff_display = false;
    }

    if (base_node->limits_override == 1) {
      use_limits = true;
    } else if (base_node->limits_override == -1) {
      use_limits = false;
    } else {
      use_limits = eff_display;
    }
  }

  MathBox base = layout_node(base_node);

  // Layout superscript at smaller size
  double old_scale = current_scale_;
  current_scale_ *= constants_.script_percent_scale_down / 100.0;
  MathBox sup = layout_node(node->children[1].get());
  current_scale_ = old_scale;

  if (use_limits) {
    // Position script above the operator (limits style)
    double upper_gap = scale(constants_.upper_limit_gap_min);
    double upper_rise = scale(constants_.upper_limit_baseline_rise_min);
    double sup_shift = std::max(base.height + upper_gap + sup.depth, upper_rise);

    double max_width = std::max(base.width, sup.width);
    double base_x = (max_width - base.width) / 2;
    double sup_x = (max_width - sup.width) / 2;

    MathBox result;
    result.merge(base, base_x, 0);
    result.merge(sup, sup_x, sup_shift);

    result.width = max_width;
    result.height = sup_shift + sup.height;
    result.depth = base.depth;

    return result;
  }

  // Standard side-script positioning
  // Calculate superscript shift
  double shift_up = scale(constants_.superscript_shift_up);
  double sup_bottom_min = scale(constants_.superscript_bottom_min);

  // Ensure superscript bottom is above minimum
  double sup_shift = std::max(shift_up, base.height - scale(constants_.superscript_baseline_drop_max));
  sup_shift = std::max(sup_shift, sup_bottom_min + sup.depth);

  // Position superscript
  double sup_x = base.width + base.italic_corr;
  double sup_y = sup_shift;

  MathBox result;
  result.merge(base, 0, 0);
  result.merge(sup, sup_x, sup_y);

  result.width = sup_x + sup.width + scale(constants_.space_after_script);
  result.height = std::max(base.height, sup_y + sup.height);
  result.depth = base.depth;
  result.italic_corr = sup.italic_corr;

  return result;
}

MathBox MathLayout::layout_subscript(MathNode* node) {
  if (node->children.size() < 2) return MathBox();

  MathNode* base_node = node->children[0].get();

  // Check if this is a big operator that should use limits positioning
  bool use_limits = false;
  if (base_node->type == NodeType::BIG_OP) {
    bool eff_display = display_mode_;
    if (forced_style_ == MathDisplayStyle::DISPLAY) {
      eff_display = true;
    } else if (forced_style_ == MathDisplayStyle::TEXT ||
               forced_style_ == MathDisplayStyle::SCRIPT ||
               forced_style_ == MathDisplayStyle::SCRIPTSCRIPT) {
      eff_display = false;
    }

    if (base_node->limits_override == 1) {
      use_limits = true;
    } else if (base_node->limits_override == -1) {
      use_limits = false;
    } else {
      use_limits = eff_display;
    }
  }

  MathBox base = layout_node(base_node);

  // Layout subscript at smaller size
  double old_scale = current_scale_;
  current_scale_ *= constants_.script_percent_scale_down / 100.0;
  MathBox sub = layout_node(node->children[1].get());
  current_scale_ = old_scale;

  if (use_limits) {
    // Position script below the operator (limits style)
    double lower_gap = scale(constants_.lower_limit_gap_min);
    double lower_drop = scale(constants_.lower_limit_baseline_drop_min);
    double sub_shift = std::max(base.depth + lower_gap + sub.height, lower_drop);

    double max_width = std::max(base.width, sub.width);
    double base_x = (max_width - base.width) / 2;
    double sub_x = (max_width - sub.width) / 2;

    MathBox result;
    result.merge(base, base_x, 0);
    result.merge(sub, sub_x, -sub_shift);

    result.width = max_width;
    result.height = base.height;
    result.depth = sub_shift + sub.depth;

    return result;
  }

  // Standard side-script positioning
  // Calculate subscript shift
  double shift_down = scale(constants_.subscript_shift_down);
  double sub_top_max = scale(constants_.subscript_top_max);

  double sub_shift = std::max(shift_down, base.depth + scale(constants_.subscript_baseline_drop_min));
  sub_shift = std::max(sub_shift, sub.height - sub_top_max);

  // Position subscript
  double sub_x = base.width;
  double sub_y = -sub_shift;

  MathBox result;
  result.merge(base, 0, 0);
  result.merge(sub, sub_x, sub_y);

  result.width = sub_x + sub.width + scale(constants_.space_after_script);
  result.height = base.height;
  result.depth = std::max(base.depth, sub_shift + sub.depth);
  result.italic_corr = 0;

  return result;
}

MathBox MathLayout::layout_subsuperscript(MathNode* node) {
  if (node->children.size() < 3) return MathBox();

  MathNode* base_node = node->children[0].get();

  // Check if this is a big operator that should use limits positioning
  bool use_limits = false;
  if (base_node->type == NodeType::BIG_OP) {
    // Determine effective display mode
    bool eff_display = display_mode_;
    if (forced_style_ == MathDisplayStyle::DISPLAY) {
      eff_display = true;
    } else if (forced_style_ == MathDisplayStyle::TEXT ||
               forced_style_ == MathDisplayStyle::SCRIPT ||
               forced_style_ == MathDisplayStyle::SCRIPTSCRIPT) {
      eff_display = false;
    }

    // Check limits_override: 1=force limits, -1=force no limits, 0=default (display mode)
    if (base_node->limits_override == 1) {
      use_limits = true;
    } else if (base_node->limits_override == -1) {
      use_limits = false;
    } else {
      use_limits = eff_display;
    }
  }

  MathBox base = layout_node(base_node);

  // Layout scripts at smaller size
  double old_scale = current_scale_;
  current_scale_ *= constants_.script_percent_scale_down / 100.0;
  MathBox sub = layout_node(node->children[1].get());
  MathBox sup = layout_node(node->children[2].get());
  current_scale_ = old_scale;

  if (use_limits) {
    // Position scripts above and below the operator (limits style)
    double upper_gap = scale(constants_.upper_limit_gap_min);
    double upper_rise = scale(constants_.upper_limit_baseline_rise_min);
    double lower_gap = scale(constants_.lower_limit_gap_min);
    double lower_drop = scale(constants_.lower_limit_baseline_drop_min);

    // Position superscript above
    double sup_shift = std::max(base.height + upper_gap + sup.depth, upper_rise);

    // Position subscript below
    double sub_shift = std::max(base.depth + lower_gap + sub.height, lower_drop);

    // Center scripts horizontally over the base
    double max_width = std::max({base.width, sup.width, sub.width});
    double base_x = (max_width - base.width) / 2;
    double sup_x = (max_width - sup.width) / 2;
    double sub_x = (max_width - sub.width) / 2;

    MathBox result;
    result.merge(base, base_x, 0);
    result.merge(sup, sup_x, sup_shift);
    result.merge(sub, sub_x, -sub_shift);

    result.width = max_width;
    result.height = sup_shift + sup.height;
    result.depth = sub_shift + sub.depth;

    return result;
  }

  // Standard side-script positioning
  // Initial shifts
  double sup_shift = std::max(scale(constants_.superscript_shift_up),
                               base.height - scale(constants_.superscript_baseline_drop_max));
  double sub_shift = std::max(scale(constants_.subscript_shift_down),
                               base.depth + scale(constants_.subscript_baseline_drop_min));

  // Ensure minimum gap between sub and sup
  double gap = (sup_shift - sup.depth) - (-sub_shift + sub.height);
  double min_gap = scale(constants_.sub_superscript_gap_min);
  if (gap < min_gap) {
    double adjust = (min_gap - gap) / 2;
    sup_shift += adjust;
    sub_shift += adjust;
  }

  // Ensure superscript bottom doesn't go too low
  double sup_bottom_max = scale(constants_.superscript_bottom_max_with_subscript);
  if (sup_shift - sup.depth < sup_bottom_max) {
    double adjust = sup_bottom_max - (sup_shift - sup.depth);
    sup_shift += adjust;
  }

  // Position scripts
  double sup_x = base.width + base.italic_corr;
  double sub_x = base.width;

  MathBox result;
  result.merge(base, 0, 0);
  result.merge(sup, sup_x, sup_shift);
  result.merge(sub, sub_x, -sub_shift);

  result.width = std::max(sup_x + sup.width, sub_x + sub.width) + scale(constants_.space_after_script);
  result.height = std::max(base.height, sup_shift + sup.height);
  result.depth = std::max(base.depth, sub_shift + sub.depth);

  return result;
}

MathBox MathLayout::layout_fraction(MathNode* node) {
  if (node->children.size() < 2) return MathBox();

  // Determine effective display mode based on frac_style and forced_style_
  bool use_display = display_mode_;
  if (node->frac_style == "display") {
    use_display = true;
  } else if (node->frac_style == "text") {
    use_display = false;
  }
  // Override with forced style if set
  if (forced_style_ == MathDisplayStyle::DISPLAY) {
    use_display = true;
  } else if (forced_style_ == MathDisplayStyle::TEXT ||
             forced_style_ == MathDisplayStyle::SCRIPT ||
             forced_style_ == MathDisplayStyle::SCRIPTSCRIPT) {
    use_display = false;
  }

  // Layout numerator and denominator
  MathBox num = layout_node(node->children[0].get());
  MathBox denom = layout_node(node->children[1].get());

  // Get positioning constants
  double axis = scale(constants_.axis_height);
  double rule_thickness = scale(constants_.fraction_rule_thickness);

  double num_gap, denom_gap, num_shift, denom_shift;
  if (use_display) {
    num_shift = scale(constants_.fraction_numerator_display_style_shift_up);
    denom_shift = scale(constants_.fraction_denominator_display_style_shift_down);
    num_gap = scale(constants_.fraction_num_display_style_gap_min);
    denom_gap = scale(constants_.fraction_denom_display_style_gap_min);
  } else {
    num_shift = scale(constants_.fraction_numerator_shift_up);
    denom_shift = scale(constants_.fraction_denominator_shift_down);
    num_gap = scale(constants_.fraction_numerator_gap_min);
    denom_gap = scale(constants_.fraction_denominator_gap_min);
  }

  // Adjust shifts to ensure minimum gaps
  double num_bottom = num_shift - num.depth;
  double bar_top = axis + rule_thickness / 2;
  if (num_bottom - bar_top < num_gap) {
    num_shift += num_gap - (num_bottom - bar_top);
  }

  double denom_top = -denom_shift + denom.height;
  double bar_bottom = axis - rule_thickness / 2;
  if (bar_bottom - denom_top < denom_gap) {
    denom_shift += denom_gap - (bar_bottom - denom_top);
  }

  // Center numerator and denominator
  double max_width = std::max(num.width, denom.width);
  double num_x = (max_width - num.width) / 2;
  double denom_x = (max_width - denom.width) / 2;

  MathBox result;
  result.merge(num, num_x, num_shift);
  result.merge(denom, denom_x, -denom_shift);

  // Add fraction bar
  LineInfo bar;
  bar.x1 = 0;
  bar.y1 = axis;
  bar.x2 = max_width;
  bar.y2 = axis;
  bar.thickness = rule_thickness;
  bar.color = current_color_;
  result.lines.push_back(bar);

  result.width = max_width;
  result.height = num_shift + num.height;
  result.depth = denom_shift + denom.depth;

  return result;
}

MathBox MathLayout::layout_sqrt(MathNode* node) {
  if (node->children.empty()) return MathBox();

  bool has_index = node->children.size() > 1;
  MathNode* radicand_node = has_index ? node->children[1].get() : node->children[0].get();
  MathNode* index_node = has_index ? node->children[0].get() : nullptr;

  // Layout radicand
  MathBox radicand = layout_node(radicand_node);

  // Get radical glyph
  uint32_t radical_glyph = get_glyph(0x221A);  // √

  // Target height for radical: need to cover radicand plus gap above
  double gap = display_mode_ ? scale(constants_.radical_display_style_vertical_gap)
                             : scale(constants_.radical_vertical_gap);
  double rule_thickness = scale(constants_.radical_rule_thickness);
  double target_height = radicand.height + radicand.depth + gap + rule_thickness;

  // Get stretched radical (may be assembled from parts for tall content)
  MathBox radical_box = get_stretched_glyph(radical_glyph, target_height, true);

  // Position radical at baseline. The radical glyph's depth (hook below baseline)
  // should align with radicand depth. If radicand has more depth, shift radical down.
  double radical_y = -std::max(0.0, radicand.depth - radical_box.depth);
  double radical_x = 0;

  // The overline should be at the top of the radical glyph (minus rule thickness,
  // since the radical's top diagonal connects to the bottom of the overline)
  double overline_y = radical_box.height + radical_y - rule_thickness;

  // Position radicand to the right of the radical, on baseline
  double radicand_x = radical_box.width;
  double radicand_y = 0;

  MathBox result;
  result.merge(radical_box, radical_x, radical_y);
  result.merge(radicand, radicand_x, radicand_y);

  // Add radical overline - connects from radical top, extending over radicand
  LineInfo overline;
  overline.x1 = radicand_x;
  overline.y1 = overline_y + rule_thickness / 2;
  overline.x2 = radicand_x + radicand.width + scale(constants_.radical_extra_ascender);
  overline.y2 = overline_y + rule_thickness / 2;
  overline.thickness = rule_thickness;
  overline.color = current_color_;
  result.lines.push_back(overline);

  result.width = radical_box.width + radicand.width + scale(constants_.radical_extra_ascender);
  result.height = overline_y + rule_thickness;
  result.depth = std::max(radical_box.depth - radical_y, radicand.depth);

  // Handle index if present
  if (index_node) {
    double old_scale = current_scale_;
    current_scale_ *= constants_.script_script_percent_scale_down / 100.0;
    MathBox index = layout_node(index_node);
    current_scale_ = old_scale;

    double index_x = scale(constants_.radical_kern_before_degree);
    // Position index above the "hook" of the radical
    // The index should be raised so it clears the diagonal part of the radical
    double raise_percent = constants_.radical_degree_bottom_raise_percent / 100.0;
    double base_y = radical_y + (radical_box.height - radical_box.depth) * raise_percent;
    // Add additional offset to clear the radical stroke
    double index_y = base_y + index.height * 0.3;

    result.merge(index, index_x, index_y);
    // Shift everything right (glyphs and lines)
    double shift = index_x + index.width + scale(constants_.radical_kern_after_degree);
    if (shift > 0) {
      size_t n_index_glyphs = index.glyphs.size();
      for (size_t i = 0; i < result.glyphs.size() - n_index_glyphs; i++) {
        result.glyphs[i].x += shift;
      }
      for (auto& l : result.lines) {
        l.x1 += shift;
        l.x2 += shift;
      }
      result.width += shift;
    }
  }

  return result;
}

MathBox MathLayout::layout_delimited(MathNode* node) {
  // Layout content first
  MathBox content;
  if (!node->children.empty()) {
    content = layout_node(node->children[0].get());
  }

  // Target height for delimiters - add clearance so delimiters extend beyond content
  double axis = scale(constants_.axis_height);
  double clearance = axis * 0.5;  // Extra space above and below content
  double target_height = content.height + content.depth + 2 * clearance;

  // Get delimiter glyphs using shared lookup
  uint32_t left_cp = get_delimiter_codepoint(node->left_delim);
  uint32_t right_cp = get_delimiter_codepoint(node->right_delim);
  uint32_t left_glyph = left_cp ? get_glyph(left_cp) : 0;
  uint32_t right_glyph = right_cp ? get_glyph(right_cp) : 0;

  MathBox result;
  double x = 0;

  // Left delimiter - use get_stretched_glyph for potentially assembled glyphs
  if (left_glyph != 0) {
    MathBox left_box = get_stretched_glyph(left_glyph, target_height, true);
    // Center vertically on axis
    double delim_center = (left_box.height - left_box.depth) / 2;
    double y_offset = axis - delim_center;
    result.merge(left_box, x, y_offset);
    x += left_box.width;
    result.height = std::max(result.height, y_offset + left_box.height);
    result.depth = std::max(result.depth, left_box.depth - y_offset);
  }

  // Content
  result.merge(content, x, 0);
  x += content.width;
  result.height = std::max(result.height, content.height);
  result.depth = std::max(result.depth, content.depth);

  // Right delimiter - use get_stretched_glyph for potentially assembled glyphs
  if (right_glyph != 0) {
    MathBox right_box = get_stretched_glyph(right_glyph, target_height, true);
    double axis = scale(constants_.axis_height);
    double delim_center = (right_box.height - right_box.depth) / 2;
    double y_offset = axis - delim_center;
    result.merge(right_box, x, y_offset);
    x += right_box.width;
    result.height = std::max(result.height, y_offset + right_box.height);
    result.depth = std::max(result.depth, right_box.depth - y_offset);
  }

  result.width = x;
  return result;
}

MathBox MathLayout::layout_big_op(MathNode* node) {
  // Get operator glyph
  uint32_t op_codepoint = command_to_codepoint(node->value);
  if (op_codepoint == 0) {
    return layout_string(node->value);
  }

  uint32_t op_glyph = get_glyph(op_codepoint);

  // Determine effective display mode
  bool use_display = display_mode_;
  if (forced_style_ == MathDisplayStyle::DISPLAY) {
    use_display = true;
  } else if (forced_style_ == MathDisplayStyle::TEXT ||
             forced_style_ == MathDisplayStyle::SCRIPT ||
             forced_style_ == MathDisplayStyle::SCRIPTSCRIPT) {
    use_display = false;
  }

  // In display mode, use larger variant or scale up
  if (use_display) {
    double min_height = scale(constants_.display_operator_min_height);
    uint32_t variant_glyph = get_size_variant(op_glyph, min_height, true);

    // If no variant found (same glyph returned), use font scaling
    if (variant_glyph == op_glyph) {
      // Scale up by ~1.4x for display mode operators
      double display_scale = 1.4;
      MathBox box;

      hb_glyph_extents_t extents;
      hb_font_get_glyph_extents(hb_font_, op_glyph, &extents);
      hb_position_t h_advance = hb_font_get_glyph_h_advance(hb_font_, op_glyph);

      double scale_factor = current_scale_ * display_scale;
      box.width = h_advance * scale_factor;
      box.height = extents.y_bearing * scale_factor;
      box.depth = (-extents.y_bearing - extents.height) * scale_factor;
      if (box.depth < 0) box.depth = 0;
      box.italic_corr = get_italic_correction(op_glyph) * display_scale;

      GlyphInfo glyph;
      glyph.glyph_id = op_glyph;
      glyph.x = 0;
      glyph.y = 0;
      glyph.size = base_size_ * display_scale;
      glyph.advance = box.width;
      glyph.color = current_color_;
      box.glyphs.push_back(glyph);

      return box;
    }

    op_glyph = variant_glyph;
  }

  return make_glyph_box(op_glyph);
}

MathBox MathLayout::layout_accent(MathNode* node) {
  if (node->children.empty()) return MathBox();

  MathBox base = layout_node(node->children[0].get());

  // Get accent codepoint using command_to_codepoint from math_parser
  uint32_t accent_codepoint = command_to_codepoint(node->command);

  if (accent_codepoint == 0) {
    return base;
  }

  // Determine if this is an over or under accent
  bool is_under = (node->command == "underline" || node->command == "underbrace" ||
                   node->command == "underrightarrow" || node->command == "underleftarrow" ||
                   node->command == "underleftrightarrow");

  uint32_t accent_glyph = get_glyph(accent_codepoint);

  // For wide accents, try to stretch to match base width
  MathBox accent_box;
  if (node->wide && base.width > 0) {
    accent_box = get_stretched_glyph(accent_glyph, base.width, false);
  } else {
    accent_box = make_glyph_box(accent_glyph);
  }

  // Position accent above or below base
  // Use top accent attachment if base is a single glyph (accounts for italic slant)
  double base_center = base.width / 2;
  if (base.glyphs.size() == 1) {
    base_center = get_top_accent_attachment(base.glyphs[0].glyph_id);
  }

  // For combining accents (zero width), use actual glyph extents to find center
  double accent_center;
  if (accent_box.width < 0.001) {
    // Zero-width combining accent - get actual extents
    hb_glyph_extents_t extents;
    if (hb_font_get_glyph_extents(hb_font_, accent_glyph, &extents)) {
      double x_bearing = extents.x_bearing * current_scale_;
      double glyph_width = extents.width * current_scale_;
      accent_center = x_bearing + glyph_width / 2;
    } else {
      accent_center = 0;
    }
  } else {
    accent_center = accent_box.width / 2;
  }
  double accent_x = base_center - accent_center;

  // Use flattened accent if base is tall and flattened version exists
  if (base.height > scale(constants_.flattened_accent_base_height)) {
    auto it = flattened_accents_.find(accent_glyph);
    if (it != flattened_accents_.end()) {
      accent_glyph = it->second;
      accent_box = make_glyph_box(accent_glyph);
      // Recalculate center for new glyph
      if (accent_box.width < 0.001) {
        hb_glyph_extents_t extents;
        if (hb_font_get_glyph_extents(hb_font_, accent_glyph, &extents)) {
          double x_bearing = extents.x_bearing * current_scale_;
          double glyph_width = extents.width * current_scale_;
          accent_center = x_bearing + glyph_width / 2;
        }
      } else {
        accent_center = accent_box.width / 2;
      }
      accent_x = base_center - accent_center;
    }
  }

  MathBox result;
  double accent_y;

  if (is_under) {
    // Position accent below base
    accent_y = -base.depth - accent_box.height;
    result.merge(base, 0, 0);
    result.merge(accent_box, accent_x, accent_y);
    result.width = base.width;
    result.height = base.height;
    result.depth = base.depth + accent_box.height + accent_box.depth;
  } else {
    // Position accent above base
    // We need to place the accent so its bottom edge sits just above the base's top
    double gap = scale(constants_.accent_base_height) * 0.1;  // Small gap

    // Get the actual bottom of the accent glyph (where the ink starts)
    hb_glyph_extents_t accent_extents;
    double accent_bottom = 0;  // Bottom of accent ink relative to its origin
    if (hb_font_get_glyph_extents(hb_font_, accent_glyph, &accent_extents)) {
      // y_bearing is top, height is negative extent downward
      // Bottom of ink = y_bearing + height
      accent_bottom = (accent_extents.y_bearing + accent_extents.height) * current_scale_;
    }

    // Position accent so its bottom is at base.height + gap
    accent_y = base.height + gap - accent_bottom;

    result.merge(base, 0, 0);
    result.merge(accent_box, accent_x, accent_y);
    result.width = base.width;
    result.height = std::max(base.height, accent_y + accent_box.height);
    result.depth = base.depth;
  }

  // Handle overbrace/underbrace annotations
  if (node->children.size() > 1) {
    double old_scale = current_scale_;
    current_scale_ *= constants_.script_percent_scale_down / 100.0;
    MathBox annotation = layout_node(node->children[1].get());
    current_scale_ = old_scale;

    double annot_x = (result.width - annotation.width) / 2;

    if (node->command == "overbrace") {
      // Annotation goes above the brace
      double annot_y = result.height + scale(constants_.upper_limit_gap_min);
      result.merge(annotation, annot_x, annot_y);
      result.height = annot_y + annotation.height;
    } else if (node->command == "underbrace") {
      // Annotation goes below the brace
      double annot_y = -result.depth - annotation.height - scale(constants_.lower_limit_gap_min);
      result.merge(annotation, annot_x, annot_y);
      result.depth = -annot_y + annotation.depth;
    }
  }

  return result;
}

MathBox MathLayout::layout_function(MathNode* node) {
  // Layout function name in upright (roman) style
  // For now, just use regular glyphs
  return layout_string(node->value);
}

MathBox MathLayout::layout_text(MathNode* node) {
  return layout_string(node->value);
}

MathBox MathLayout::layout_space(MathNode* node) {
  MathBox result;

  // Get space width in em using shared table
  double em_amount = get_space_amount(node->value);
  if (em_amount != 0.0) {
    // 1em = base_size * script_scale (current_scale_ includes 1/64 factor, so multiply by 64)
    result.width = em_amount * base_size_ * current_scale_ * 64.0;
  }

  return result;
}

// Map a codepoint to its styled variant in Unicode Mathematical Alphanumeric Symbols
static uint32_t map_styled_codepoint(uint32_t cp, MathStyle style) {
  // For ROMAN style, just use the plain character (no transformation)
  if (style == MathStyle::ROMAN || style == MathStyle::NORMAL) {
    return cp;
  }

  bool is_upper = (cp >= 'A' && cp <= 'Z');
  bool is_lower = (cp >= 'a' && cp <= 'z');
  bool is_digit = (cp >= '0' && cp <= '9');

  if (!is_upper && !is_lower && !is_digit) {
    return cp;  // No transformation for non-alphanumeric
  }

  int offset = is_upper ? (cp - 'A') : (is_lower ? (cp - 'a') : (cp - '0'));

  switch (style) {
    case MathStyle::BOLD:
      if (is_upper) return 0x1D400 + offset;      // Bold A-Z
      if (is_lower) return 0x1D41A + offset;      // Bold a-z
      if (is_digit) return 0x1D7CE + offset;      // Bold 0-9
      break;

    case MathStyle::ITALIC:
      if (is_upper) return 0x1D434 + offset;      // Italic A-Z
      if (is_lower) {
        if (cp == 'h') return 0x210E;             // Planck constant
        return 0x1D44E + offset;                  // Italic a-z
      }
      break;

    case MathStyle::BOLD_ITALIC:
      if (is_upper) return 0x1D468 + offset;      // Bold Italic A-Z
      if (is_lower) return 0x1D482 + offset;      // Bold Italic a-z
      break;

    case MathStyle::SCRIPT:
      if (is_upper) {
        // Script has exceptions for B, E, F, H, I, L, M, R
        switch (cp) {
          case 'B': return 0x212C;
          case 'E': return 0x2130;
          case 'F': return 0x2131;
          case 'H': return 0x210B;
          case 'I': return 0x2110;
          case 'L': return 0x2112;
          case 'M': return 0x2133;
          case 'R': return 0x211B;
          default: return 0x1D49C + offset;
        }
      }
      if (is_lower) {
        // Script lowercase has exceptions for e, g, o
        switch (cp) {
          case 'e': return 0x212F;
          case 'g': return 0x210A;
          case 'o': return 0x2134;
          default: return 0x1D4B6 + offset;
        }
      }
      break;

    case MathStyle::BOLD_SCRIPT:
      if (is_upper) return 0x1D4D0 + offset;      // Bold Script A-Z
      if (is_lower) return 0x1D4EA + offset;      // Bold Script a-z
      break;

    case MathStyle::FRAKTUR:
      if (is_upper) {
        // Fraktur has exceptions for C, H, I, R, Z
        switch (cp) {
          case 'C': return 0x212D;
          case 'H': return 0x210C;
          case 'I': return 0x2111;
          case 'R': return 0x211C;
          case 'Z': return 0x2128;
          default: return 0x1D504 + offset;
        }
      }
      if (is_lower) return 0x1D51E + offset;      // Fraktur a-z
      break;

    case MathStyle::BOLD_FRAKTUR:
      if (is_upper) return 0x1D56C + offset;      // Bold Fraktur A-Z
      if (is_lower) return 0x1D586 + offset;      // Bold Fraktur a-z
      break;

    case MathStyle::DOUBLE_STRUCK:
      if (is_upper) {
        // Double-struck has exceptions for C, H, N, P, Q, R, Z
        switch (cp) {
          case 'C': return 0x2102;
          case 'H': return 0x210D;
          case 'N': return 0x2115;
          case 'P': return 0x2119;
          case 'Q': return 0x211A;
          case 'R': return 0x211D;
          case 'Z': return 0x2124;
          default: return 0x1D538 + offset;
        }
      }
      if (is_lower) return 0x1D552 + offset;      // Double-struck a-z
      if (is_digit) return 0x1D7D8 + offset;      // Double-struck 0-9
      break;

    case MathStyle::SANS:
      if (is_upper) return 0x1D5A0 + offset;      // Sans A-Z
      if (is_lower) return 0x1D5BA + offset;      // Sans a-z
      if (is_digit) return 0x1D7E2 + offset;      // Sans 0-9
      break;

    case MathStyle::SANS_BOLD:
      if (is_upper) return 0x1D5D4 + offset;      // Sans Bold A-Z
      if (is_lower) return 0x1D5EE + offset;      // Sans Bold a-z
      if (is_digit) return 0x1D7EC + offset;      // Sans Bold 0-9
      break;

    case MathStyle::SANS_ITALIC:
      if (is_upper) return 0x1D608 + offset;      // Sans Italic A-Z
      if (is_lower) return 0x1D622 + offset;      // Sans Italic a-z
      break;

    case MathStyle::MONO:
      if (is_upper) return 0x1D670 + offset;      // Monospace A-Z
      if (is_lower) return 0x1D68A + offset;      // Monospace a-z
      if (is_digit) return 0x1D7F6 + offset;      // Monospace 0-9
      break;

    default:
      break;
  }

  return cp;  // Fallback
}

MathBox MathLayout::layout_styled(MathNode* node) {
  if (node->children.empty()) return MathBox();

  MathStyle style = node->style;
  MathBox result;
  double x = 0;

  // Helper to layout a styled symbol
  auto layout_styled_symbol = [this, style](const std::string& val) -> MathBox {
    if (val.empty()) return MathBox();

    // Decode UTF-8 to get first codepoint
    uint32_t codepoint = 0;
    const unsigned char* s = reinterpret_cast<const unsigned char*>(val.c_str());
    if ((s[0] & 0x80) == 0) {
      codepoint = s[0];
    } else if ((s[0] & 0xE0) == 0xC0) {
      codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    } else if ((s[0] & 0xF0) == 0xE0) {
      codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    } else if ((s[0] & 0xF8) == 0xF0) {
      codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }

    // Map to styled variant
    uint32_t styled_cp = map_styled_codepoint(codepoint, style);
    uint32_t glyph_id = get_glyph(styled_cp);

    // Fallback to original if styled glyph not found
    if (glyph_id == 0 && styled_cp != codepoint) {
      glyph_id = get_glyph(codepoint);
    }

    return make_glyph_box(glyph_id);
  };

  // Recursive helper to process nodes with styling applied
  std::function<void(MathNode*)> process_styled;
  process_styled = [&](MathNode* n) {
    if (!n) return;

    MathBox child_box;

    switch (n->type) {
      case NodeType::SYMBOL:
        child_box = layout_styled_symbol(n->value);
        result.merge(child_box, x, 0);
        x += child_box.width;
        result.height = std::max(result.height, child_box.height);
        result.depth = std::max(result.depth, child_box.depth);
        break;

      case NodeType::NUMBER:
        // Numbers with style
        for (char c : n->value) {
          std::string s(1, c);
          MathBox glyph_box = layout_styled_symbol(s);
          result.merge(glyph_box, x, 0);
          x += glyph_box.width;
          result.height = std::max(result.height, glyph_box.height);
          result.depth = std::max(result.depth, glyph_box.depth);
        }
        break;

      case NodeType::ROW:
      case NodeType::GROUP:
        // Recursively process children
        for (auto& child : n->children) {
          process_styled(child.get());
        }
        break;

      default:
        // For other node types (scripts, fractions, etc.), layout normally
        child_box = layout_node(n);
        result.merge(child_box, x, 0);
        x += child_box.width;
        result.height = std::max(result.height, child_box.height);
        result.depth = std::max(result.depth, child_box.depth);
        break;
    }
  };

  // Process all children with styling
  for (auto& child : node->children) {
    process_styled(child.get());
  }

  result.width = x;
  return result;
}

MathBox MathLayout::layout_matrix(MathNode* node) {
  int rows = node->rows;
  int cols = node->cols;

  if (rows == 0 || cols == 0) return MathBox();

  // Spacing constants (in em units, scaled to current size)
  double col_gap = base_size_ * 0.8;   // Gap between columns
  double row_gap = base_size_ * 0.4;   // Gap between rows
  double h_padding = base_size_ * 0.3; // Horizontal padding inside delimiters

  // For smallmatrix, use smaller spacing
  if (node->matrix_type == "smallmatrix") {
    col_gap = base_size_ * 0.4;
    row_gap = base_size_ * 0.2;
    h_padding = base_size_ * 0.15;
  }

  // Layout all cells and collect them
  std::vector<MathBox> cells;
  cells.reserve(rows * cols);

  for (int i = 0; i < rows * cols; i++) {
    if (i < static_cast<int>(node->children.size())) {
      cells.push_back(layout_node(node->children[i].get()));
    } else {
      cells.push_back(MathBox());
    }
  }

  // Calculate column widths (max width in each column)
  std::vector<double> col_widths(cols, 0);
  for (int c = 0; c < cols; c++) {
    for (int r = 0; r < rows; r++) {
      int idx = r * cols + c;
      col_widths[c] = std::max(col_widths[c], cells[idx].width);
    }
  }

  // Calculate row heights and depths
  std::vector<double> row_heights(rows, 0);
  std::vector<double> row_depths(rows, 0);
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      int idx = r * cols + c;
      row_heights[r] = std::max(row_heights[r], cells[idx].height);
      row_depths[r] = std::max(row_depths[r], cells[idx].depth);
    }
  }

  // Calculate total dimensions
  double total_width = 0;
  for (int c = 0; c < cols; c++) {
    total_width += col_widths[c];
    if (c < cols - 1) total_width += col_gap;
  }

  double total_height = 0;
  for (int r = 0; r < rows; r++) {
    total_height += row_heights[r] + row_depths[r];
    if (r < rows - 1) total_height += row_gap;
  }

  // Calculate axis height (where matrix should be vertically centered)
  double axis = scale(constants_.axis_height);

  // Position cells (centered in their column, baseline aligned in row)
  MathBox content;

  // First row baseline: centered matrix top minus first row's height
  // Matrix is centered on axis, so top is at axis + total_height/2
  double baseline = axis + total_height / 2 - row_heights[0];

  for (int r = 0; r < rows; r++) {
    double x = 0;

    for (int c = 0; c < cols; c++) {
      int idx = r * cols + c;
      MathBox& cell = cells[idx];

      // For cases environments, left-align cells; otherwise center
      double cell_x;
      bool is_cases = (node->matrix_type == "cases" || node->matrix_type == "rcases" ||
                       node->matrix_type == "dcases" || node->matrix_type == "drcases");
      if (is_cases) {
        cell_x = x;  // Left-aligned
      } else {
        cell_x = x + (col_widths[c] - cell.width) / 2;  // Centered
      }

      content.merge(cell, cell_x, baseline);

      x += col_widths[c];
      if (c < cols - 1) x += col_gap;
    }

    // Move baseline down to next row
    if (r < rows - 1) {
      baseline -= row_depths[r] + row_gap + row_heights[r + 1];
    }
  }

  content.width = total_width;
  content.height = total_height / 2 + axis;
  content.depth = total_height / 2 - axis;

  // Add delimiters based on matrix type
  std::string left_delim, right_delim;
  if (node->matrix_type == "pmatrix") {
    left_delim = "(";
    right_delim = ")";
  } else if (node->matrix_type == "bmatrix") {
    left_delim = "[";
    right_delim = "]";
  } else if (node->matrix_type == "Bmatrix") {
    left_delim = "{";
    right_delim = "}";
  } else if (node->matrix_type == "vmatrix") {
    left_delim = "|";
    right_delim = "|";
  } else if (node->matrix_type == "Vmatrix") {
    left_delim = "‖";  // Double vertical bar
    right_delim = "‖";
  } else if (node->matrix_type == "cases" || node->matrix_type == "dcases") {
    left_delim = "{";
    right_delim = "";  // No right delimiter for cases
  } else if (node->matrix_type == "rcases" || node->matrix_type == "drcases") {
    left_delim = "";
    right_delim = "}";  // Only right delimiter for rcases
  }

  if (left_delim.empty()) {
    // No delimiters (plain matrix or smallmatrix)
    return content;
  }

  // Get delimiter glyphs with size variants - add clearance so delimiters extend beyond content
  double clearance = axis * 0.5;
  double target_height = content.height + content.depth + 2 * clearance;

  // Get delimiter glyphs using shared lookup
  uint32_t left_cp = get_delimiter_codepoint(left_delim);
  uint32_t right_cp = get_delimiter_codepoint(right_delim);
  uint32_t left_glyph = left_cp ? get_glyph(left_cp) : 0;
  uint32_t right_glyph = right_cp ? get_glyph(right_cp) : 0;

  // Get stretched delimiters (may use glyph assembly for tall matrices)
  MathBox left_box = left_glyph != 0 ? get_stretched_glyph(left_glyph, target_height, true) : MathBox();
  MathBox right_box = right_glyph != 0 ? get_stretched_glyph(right_glyph, target_height, true) : MathBox();

  // Center delimiters vertically on the axis
  double left_y = axis - (left_box.height - left_box.depth) / 2;
  double right_y = axis - (right_box.height - right_box.depth) / 2;

  MathBox result;

  // Left delimiter
  result.merge(left_box, 0, left_y);

  // Content (shifted right by left delimiter + padding)
  double content_x = left_box.width + h_padding;
  result.merge(content, content_x, 0);

  // Right delimiter
  double right_x = content_x + content.width + h_padding;
  result.merge(right_box, right_x, right_y);

  result.width = right_x + right_box.width;
  result.height = std::max({left_box.height + left_y, content.height, right_box.height + right_y});
  result.depth = std::max({left_box.depth - left_y, content.depth, right_box.depth - right_y});

  return result;
}

MathBox MathLayout::layout_stack(MathNode* node) {
  if (node->children.size() < 2) return MathBox();

  const std::string& stack_type = node->stack_type;

  // Determine which child is top and which is bottom
  MathNode* top_node = nullptr;
  MathNode* bottom_node = nullptr;
  bool top_is_small = false;
  bool bottom_is_small = false;

  if (stack_type == "binom" || stack_type == "tbinom" || stack_type == "dbinom" || stack_type == "atop") {
    // Both parts are normal size
    top_node = node->children[0].get();
    bottom_node = node->children[1].get();
  } else if (stack_type == "stackrel" || stack_type == "overset") {
    // Top is small, bottom is normal
    top_node = node->children[0].get();
    bottom_node = node->children[1].get();
    top_is_small = true;
  } else if (stack_type == "underset") {
    // Top is normal, bottom is small
    // For underset, child[0] is what goes below, child[1] is the base
    bottom_node = node->children[0].get();
    top_node = node->children[1].get();
    bottom_is_small = true;
  }

  // Layout top and bottom parts
  MathBox top_box, bottom_box;
  double old_scale = current_scale_;

  if (top_is_small) {
    current_scale_ *= constants_.script_percent_scale_down / 100.0;
  }
  top_box = layout_node(top_node);
  current_scale_ = old_scale;

  if (bottom_is_small) {
    current_scale_ *= constants_.script_percent_scale_down / 100.0;
  }
  bottom_box = layout_node(bottom_node);
  current_scale_ = old_scale;

  // Use stack constants for positioning (like fraction but without rule)
  double top_shift, bottom_shift, gap_min;
  if (display_mode_ || stack_type == "dbinom") {
    top_shift = scale(constants_.stack_top_display_style_shift_up);
    bottom_shift = scale(constants_.stack_bottom_display_style_shift_down);
    gap_min = scale(constants_.stack_display_style_gap_min);
  } else {
    top_shift = scale(constants_.stack_top_shift_up);
    bottom_shift = scale(constants_.stack_bottom_shift_down);
    gap_min = scale(constants_.stack_gap_min);
  }

  // For tbinom, use text style (smaller)
  if (stack_type == "tbinom") {
    top_shift = scale(constants_.stack_top_shift_up);
    bottom_shift = scale(constants_.stack_bottom_shift_down);
    gap_min = scale(constants_.stack_gap_min);
  }

  // Adjust shifts to ensure minimum gap
  double current_gap = (top_shift - top_box.depth) - (-bottom_shift + bottom_box.height);
  if (current_gap < gap_min) {
    double adjust = (gap_min - current_gap) / 2;
    top_shift += adjust;
    bottom_shift += adjust;
  }

  // Center horizontally
  double max_width = std::max(top_box.width, bottom_box.width);
  double top_x = (max_width - top_box.width) / 2;
  double bottom_x = (max_width - bottom_box.width) / 2;

  MathBox content;
  content.merge(top_box, top_x, top_shift);
  content.merge(bottom_box, bottom_x, -bottom_shift);
  content.width = max_width;
  content.height = top_shift + top_box.height;
  content.depth = bottom_shift + bottom_box.depth;

  // For binom variants, wrap in parentheses
  if (stack_type == "binom" || stack_type == "tbinom" || stack_type == "dbinom") {
    double target_height = content.height + content.depth;
    uint32_t left_glyph = get_glyph('(');
    uint32_t right_glyph = get_glyph(')');

    left_glyph = get_size_variant(left_glyph, target_height, true);
    right_glyph = get_size_variant(right_glyph, target_height, true);

    MathBox left_box = make_glyph_box(left_glyph);
    MathBox right_box = make_glyph_box(right_glyph);

    // Center delimiters vertically on axis
    double axis = scale(constants_.axis_height);
    double left_y = axis - (left_box.height - left_box.depth) / 2;
    double right_y = axis - (right_box.height - right_box.depth) / 2;

    MathBox result;
    result.merge(left_box, 0, left_y);
    result.merge(content, left_box.width, 0);
    result.merge(right_box, left_box.width + content.width, right_y);

    result.width = left_box.width + content.width + right_box.width;
    result.height = std::max({left_box.height + left_y, content.height, right_box.height + right_y});
    result.depth = std::max({left_box.depth - left_y, content.depth, right_box.depth - right_y});

    return result;
  }

  return content;
}

MathBox MathLayout::layout_operatorname(MathNode* node) {
  // Layout operator name in upright (roman) style, same as functions
  return layout_string(node->value);
}

MathBox MathLayout::layout_phantom(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Layout content to get dimensions
  MathBox content = layout_node(node->children[0].get());

  // Create result box with correct dimensions but NO glyphs
  MathBox result;

  if (node->phantom_type == "full") {
    // Full phantom: preserve all dimensions
    result.width = content.width;
    result.height = content.height;
    result.depth = content.depth;
  } else if (node->phantom_type == "h") {
    // Horizontal phantom: only width, zero height/depth
    result.width = content.width;
    result.height = 0;
    result.depth = 0;
  } else if (node->phantom_type == "v") {
    // Vertical phantom: only height/depth, zero width
    result.width = 0;
    result.height = content.height;
    result.depth = content.depth;
  }

  // Explicitly do NOT copy glyphs, rules, or boxes
  return result;
}

MathBox MathLayout::layout_colored(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Save current color and set new color
  std::string saved_color = current_color_;
  current_color_ = node->color;

  // Layout content with new color
  MathBox result = layout_node(node->children[0].get());

  // Restore color
  current_color_ = saved_color;

  return result;
}

MathBox MathLayout::layout_colorbox(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Layout content
  MathBox content = layout_node(node->children[0].get());

  // Add padding
  double padding = base_size_ * 0.1;

  // Create background box
  BoxInfo bg;
  bg.x = -padding;
  bg.y = -content.depth - padding;
  bg.width = content.width + 2 * padding;
  bg.height = content.height + content.depth + 2 * padding;
  bg.color = node->color;

  MathBox result;
  result.boxes.push_back(bg);
  result.merge(content, 0, 0);

  result.width = content.width + 2 * padding;
  result.height = content.height + padding;
  result.depth = content.depth + padding;

  // Shift content right to account for left padding
  for (auto& g : result.glyphs) {
    g.x += padding;
  }
  for (auto& l : result.lines) {
    l.x1 += padding;
    l.x2 += padding;
  }
  result.boxes[0].x = 0;

  return result;
}

MathBox MathLayout::layout_cancel(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // For cancelto: children[0] = value, children[1] = content
  // For others: children[0] = content
  MathBox content;
  MathBox value;
  bool is_cancelto = (node->cancel_type == "cancelto");

  if (is_cancelto && node->children.size() >= 2) {
    // Layout value at script size
    double old_scale = current_scale_;
    current_scale_ *= constants_.script_percent_scale_down / 100.0;
    value = layout_node(node->children[0].get());
    current_scale_ = old_scale;

    content = layout_node(node->children[1].get());
  } else {
    content = layout_node(node->children[0].get());
  }

  // Add diagonal line(s)
  // For \cancel: line from bottom-left to top-right
  // For \xcancel: X through the content
  // For \bcancel: line from top-left to bottom-right
  // For \cancelto: line from bottom-left to top-right with arrow and value

  double line_thickness = scale(constants_.fraction_rule_thickness);

  if (node->cancel_type == "cancel") {
    // Single diagonal from bottom-left to top-right
    LineInfo line;
    line.x1 = 0;
    line.y1 = -content.depth;
    line.x2 = content.width;
    line.y2 = content.height;
    line.thickness = line_thickness;
    line.color = current_color_;
    content.lines.push_back(line);
  } else if (node->cancel_type == "cancelto") {
    // Arrowhead size
    double arrow_len = base_size_ * 0.2;
    double arrow_width = base_size_ * 0.1;
    double tip_gap = base_size_ * 0.05;  // Gap between arrow tip and value

    // Position value: after the arrow tip with a gap
    double value_x = content.width + arrow_len + tip_gap;
    double value_y = content.height;  // Value baseline at top of content

    // Arrow tip position: just before the value, pointing toward lower-middle of value
    // Using 1/3 of height puts the tip near the x-height level of most characters
    double arrow_tip_x = value_x - tip_gap;
    double arrow_tip_y = value_y + value.height / 3;

    // Main diagonal line from bottom-left of content to arrow tip
    LineInfo line;
    line.x1 = 0;
    line.y1 = -content.depth;
    line.x2 = arrow_tip_x;
    line.y2 = arrow_tip_y;
    line.thickness = line_thickness;
    line.color = current_color_;
    content.lines.push_back(line);

    // Calculate arrow direction for arrowhead
    double dx = arrow_tip_x - 0;
    double dy = arrow_tip_y - (-content.depth);
    double len = std::sqrt(dx * dx + dy * dy);
    double ux = dx / len;  // unit vector along arrow direction
    double uy = dy / len;

    // Perpendicular vector for arrowhead width
    double px = -uy;
    double py = ux;

    // Arrowhead lines (two lines forming a V pointing forward)
    LineInfo arrow1;
    arrow1.x1 = arrow_tip_x;
    arrow1.y1 = arrow_tip_y;
    arrow1.x2 = arrow_tip_x - arrow_len * ux + arrow_width * px;
    arrow1.y2 = arrow_tip_y - arrow_len * uy + arrow_width * py;
    arrow1.thickness = line_thickness;
    arrow1.color = current_color_;
    content.lines.push_back(arrow1);

    LineInfo arrow2;
    arrow2.x1 = arrow_tip_x;
    arrow2.y1 = arrow_tip_y;
    arrow2.x2 = arrow_tip_x - arrow_len * ux - arrow_width * px;
    arrow2.y2 = arrow_tip_y - arrow_len * uy - arrow_width * py;
    arrow2.thickness = line_thickness;
    arrow2.color = current_color_;
    content.lines.push_back(arrow2);

    // Position value after the arrow
    if (value.width > 0) {
      content.merge(value, value_x, value_y);
      content.width = value_x + value.width;
      content.height = std::max(content.height, value_y + value.height);
    }
  } else if (node->cancel_type == "xcancel") {
    // X pattern: two diagonals
    // Bottom-left to top-right
    LineInfo line1;
    line1.x1 = 0;
    line1.y1 = -content.depth;
    line1.x2 = content.width;
    line1.y2 = content.height;
    line1.thickness = line_thickness;
    line1.color = current_color_;
    content.lines.push_back(line1);

    // Top-left to bottom-right
    LineInfo line2;
    line2.x1 = 0;
    line2.y1 = content.height;
    line2.x2 = content.width;
    line2.y2 = -content.depth;
    line2.thickness = line_thickness;
    line2.color = current_color_;
    content.lines.push_back(line2);
  } else if (node->cancel_type == "bcancel") {
    // Single diagonal from top-left to bottom-right
    LineInfo line;
    line.x1 = 0;
    line.y1 = content.height;
    line.x2 = content.width;
    line.y2 = -content.depth;
    line.thickness = line_thickness;
    line.color = current_color_;
    content.lines.push_back(line);
  }

  return content;
}

MathBox MathLayout::layout_boxed(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Layout content
  MathBox content = layout_node(node->children[0].get());

  // Add padding
  double padding = base_size_ * 0.15;
  double line_thickness = scale(constants_.fraction_rule_thickness);

  // Create border as four lines
  double box_width = content.width + 2 * padding;
  double box_top = content.height + padding + line_thickness / 2;
  double box_bottom = -content.depth - padding - line_thickness / 2;

  // Top
  LineInfo top;
  top.x1 = 0;
  top.y1 = box_top;
  top.x2 = box_width;
  top.y2 = box_top;
  top.thickness = line_thickness;
  top.color = current_color_;

  // Bottom
  LineInfo bottom;
  bottom.x1 = 0;
  bottom.y1 = box_bottom;
  bottom.x2 = box_width;
  bottom.y2 = box_bottom;
  bottom.thickness = line_thickness;
  bottom.color = current_color_;

  // Left
  LineInfo left;
  left.x1 = line_thickness / 2;
  left.y1 = box_bottom;
  left.x2 = line_thickness / 2;
  left.y2 = box_top;
  left.thickness = line_thickness;
  left.color = current_color_;

  // Right
  LineInfo right;
  right.x1 = box_width - line_thickness / 2;
  right.y1 = box_bottom;
  right.x2 = box_width - line_thickness / 2;
  right.y2 = box_top;
  right.thickness = line_thickness;
  right.color = current_color_;

  MathBox result;
  result.lines.push_back(top);
  result.lines.push_back(bottom);
  result.lines.push_back(left);
  result.lines.push_back(right);

  // Shift content by padding
  result.merge(content, padding, 0);

  result.width = content.width + 2 * padding;
  result.height = content.height + padding + line_thickness;
  result.depth = content.depth + padding + line_thickness;

  return result;
}

MathBox MathLayout::layout_substack(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Layout each row at script size
  double old_scale = current_scale_;
  current_scale_ *= constants_.script_percent_scale_down / 100.0;

  std::vector<MathBox> rows;
  double max_width = 0;

  for (auto& child : node->children) {
    MathBox row = layout_node(child.get());
    max_width = std::max(max_width, row.width);
    rows.push_back(std::move(row));
  }

  current_scale_ = old_scale;

  // Stack rows vertically, centered
  double row_gap = base_size_ * 0.1;
  double total_height = 0;

  for (size_t i = 0; i < rows.size(); i++) {
    total_height += rows[i].height + rows[i].depth;
    if (i < rows.size() - 1) total_height += row_gap;
  }

  MathBox result;
  double y = total_height / 2;  // Start from top

  for (auto& row : rows) {
    y -= row.height;
    double x = (max_width - row.width) / 2;  // Center horizontally
    result.merge(row, x, y);
    y -= row.depth + row_gap;
  }

  result.width = max_width;
  result.height = total_height / 2;
  result.depth = total_height / 2;

  return result;
}

MathBox MathLayout::layout_negated(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Layout the symbol to be negated
  MathBox content = layout_node(node->children[0].get());

  // Add a negation slash overlay
  uint32_t slash_glyph = get_glyph('/');
  MathBox slash_box = make_glyph_box(slash_glyph);

  // Center the slash over the content
  double slash_x = (content.width - slash_box.width) / 2;
  double slash_y = (content.height - content.depth - slash_box.height + slash_box.depth) / 2;

  content.merge(slash_box, slash_x, slash_y);

  return content;
}

MathBox MathLayout::assemble_glyph(uint32_t glyph_id, double target_size, bool vertical) {
  if (!has_math_) return make_glyph_box(glyph_id);

  hb_direction_t dir = vertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;

  // Get assembly parts count (return value gives total count)
  hb_position_t italics_correction = 0;
  unsigned int count = hb_ot_math_get_glyph_assembly(hb_font_, glyph_id, dir, 0, nullptr, nullptr, &italics_correction);

  if (count == 0) {
    // No assembly available, return the original glyph
    return make_glyph_box(glyph_id);
  }

  std::vector<hb_ot_math_glyph_part_t> parts(count);
  unsigned int fetched = count;
  hb_ot_math_get_glyph_assembly(hb_font_, glyph_id, dir, 0, &fetched, parts.data(), &italics_correction);

  // Get minimum connector overlap
  hb_position_t min_overlap = hb_ot_math_get_min_connector_overlap(hb_font_, dir);
  double min_overlap_scaled = min_overlap * current_scale_;

  // Separate parts into categories
  std::vector<size_t> start_parts;    // Non-extender parts at start
  std::vector<size_t> extender_parts; // Extender parts (can repeat)
  std::vector<size_t> end_parts;      // Non-extender parts at end
  size_t middle_part = SIZE_MAX;      // Middle non-extender part (if any)

  bool found_extender = false;
  for (size_t i = 0; i < parts.size(); i++) {
    bool is_extender = (parts[i].flags & HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER) != 0;
    if (is_extender) {
      extender_parts.push_back(i);
      found_extender = true;
    } else if (!found_extender) {
      start_parts.push_back(i);
    } else {
      // Check if this is the last non-extender
      bool has_more_extenders = false;
      for (size_t j = i + 1; j < parts.size(); j++) {
        if (parts[j].flags & HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER) {
          has_more_extenders = true;
          break;
        }
      }
      if (has_more_extenders) {
        middle_part = i;
      } else {
        end_parts.push_back(i);
      }
    }
  }

  // Calculate base size (without repeating extenders)
  double base_size = 0;
  for (size_t i : start_parts) {
    base_size += parts[i].full_advance * current_scale_;
  }
  if (middle_part != SIZE_MAX) {
    base_size += parts[middle_part].full_advance * current_scale_;
  }
  for (size_t i : end_parts) {
    base_size += parts[i].full_advance * current_scale_;
  }
  for (size_t i : extender_parts) {
    base_size += parts[i].full_advance * current_scale_;
  }

  // Account for overlaps
  if (parts.size() > 1) {
    base_size -= (parts.size() - 1) * min_overlap_scaled;
  }

  // Calculate how many times to repeat extenders
  int repeat_count = 1;
  if (!extender_parts.empty() && target_size > base_size) {
    double extender_contribution = 0;
    for (size_t i : extender_parts) {
      extender_contribution += (parts[i].full_advance * current_scale_ - min_overlap_scaled);
    }
    if (extender_contribution > 0) {
      double needed = target_size - base_size;
      repeat_count = 1 + static_cast<int>(std::ceil(needed / extender_contribution));
    }
  }

  // Assemble the glyph
  MathBox result;
  double pos = 0;  // Current position along the assembly direction

  auto add_part = [&](size_t idx, bool is_first) {
    const auto& part = parts[idx];
    MathBox part_box = make_glyph_box(part.glyph);

    // Apply overlap (except for first part)
    if (!is_first) {
      pos -= min_overlap_scaled;
    }

    if (vertical) {
      // For vertical assembly, pos represents y position from bottom
      result.merge(part_box, 0, pos);
      pos += part.full_advance * current_scale_;
    } else {
      // For horizontal assembly, pos represents x position from left
      result.merge(part_box, pos, 0);
      pos += part.full_advance * current_scale_;
    }

    result.height = std::max(result.height, part_box.height);
    result.depth = std::max(result.depth, part_box.depth);
  };

  bool is_first = true;

  // Add start parts
  for (size_t i : start_parts) {
    add_part(i, is_first);
    is_first = false;
  }

  // Add extenders (repeated)
  for (int r = 0; r < repeat_count; r++) {
    for (size_t i : extender_parts) {
      add_part(i, is_first);
      is_first = false;
    }

    // Add middle part after first set of extenders
    if (r == 0 && middle_part != SIZE_MAX) {
      add_part(middle_part, is_first);
      is_first = false;
    }
  }

  // Add end parts
  for (size_t i : end_parts) {
    add_part(i, is_first);
    is_first = false;
  }

  if (vertical) {
    result.width = result.glyphs.empty() ? 0 : result.glyphs[0].advance;
    // Recalculate height/depth for vertical assembly
    double total_height = pos;
    result.height = total_height;
    result.depth = 0;
  } else {
    result.width = pos;
  }

  result.italic_corr = italics_correction * current_scale_;

  return result;
}

MathBox MathLayout::get_stretched_glyph(uint32_t glyph_id, double target_size, bool vertical) {
  if (!has_math_) return make_glyph_box(glyph_id);

  // For vertical stretching, use BTT; for horizontal, use LTR
  hb_direction_t dir = vertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;

  // Target is in scaled coordinates (points)
  double target = target_size;

  // Helper to get the relevant dimension of a glyph in scaled coordinates
  auto get_glyph_size = [&](uint32_t gid) -> double {
    hb_glyph_extents_t extents;
    if (!hb_font_get_glyph_extents(hb_font_, gid, &extents)) {
      return 0;
    }
    if (vertical) {
      // Total vertical extent: -extents.height (height is negative in HarfBuzz)
      // This matches height + depth from get_glyph_metrics
      return -extents.height * current_scale_;
    } else {
      return extents.width * current_scale_;
    }
  };

  // First check if the base glyph is large enough
  double base_size = get_glyph_size(glyph_id);
  if (base_size >= target) {
    return make_glyph_box(glyph_id);
  }

  // Try size variants (return value gives total count)
  unsigned int count = hb_ot_math_get_glyph_variants(hb_font_, glyph_id, dir, 0, nullptr, nullptr);

  if (count > 0) {
    std::vector<hb_ot_math_glyph_variant_t> variants(count);
    unsigned int fetched = count;
    hb_ot_math_get_glyph_variants(hb_font_, glyph_id, dir, 0, &fetched, variants.data());

    // Find smallest variant that's large enough (using actual glyph metrics)
    for (size_t i = 0; i < variants.size(); i++) {
      double variant_size = get_glyph_size(variants[i].glyph);
      if (variant_size >= target) {
        return make_glyph_box(variants[i].glyph);
      }
    }

    // Use largest variant if close enough (within 20%)
    if (!variants.empty()) {
      double largest_size = get_glyph_size(variants.back().glyph);
      if (largest_size >= target * 0.8) {
        return make_glyph_box(variants.back().glyph);
      }
    }
  }

  // Fall back to glyph assembly
  return assemble_glyph(glyph_id, target_size, vertical);
}

MathBox MathLayout::layout_ext_arrow(MathNode* node) {
  // children[0] = below content (may be empty)
  // children[1] = above content
  MathBox below_box, above_box;

  double old_scale = current_scale_;
  current_scale_ *= constants_.script_percent_scale_down / 100.0;

  if (node->children.size() > 0) {
    below_box = layout_node(node->children[0].get());
  }
  if (node->children.size() > 1) {
    above_box = layout_node(node->children[1].get());
  }

  current_scale_ = old_scale;

  // Calculate required arrow width
  double min_width = std::max(above_box.width, below_box.width);
  double padding = base_size_ * 0.5;  // Padding on each side
  double arrow_width = min_width + 2 * padding;

  // Get base arrow glyph based on arrow type
  uint32_t arrow_glyph = 0;
  if (node->arrow_type == "right") {
    arrow_glyph = get_glyph(0x2192);  // →
  } else if (node->arrow_type == "left") {
    arrow_glyph = get_glyph(0x2190);  // ←
  } else if (node->arrow_type == "Right") {
    arrow_glyph = get_glyph(0x21D2);  // ⇒
  } else if (node->arrow_type == "Left") {
    arrow_glyph = get_glyph(0x21D0);  // ⇐
  } else if (node->arrow_type == "leftright") {
    arrow_glyph = get_glyph(0x2194);  // ↔
  } else if (node->arrow_type == "Leftright") {
    arrow_glyph = get_glyph(0x21D4);  // ⇔
  } else if (node->arrow_type == "hookleft") {
    arrow_glyph = get_glyph(0x21A9);  // ↩
  } else if (node->arrow_type == "hookright") {
    arrow_glyph = get_glyph(0x21AA);  // ↪
  } else if (node->arrow_type == "mapsto") {
    arrow_glyph = get_glyph(0x21A6);  // ↦
  } else if (node->arrow_type == "equal") {
    arrow_glyph = get_glyph('=');
  }

  if (arrow_glyph == 0) {
    arrow_glyph = get_glyph(0x2192);  // Default to right arrow
  }

  // Get stretched arrow (horizontal assembly)
  MathBox arrow_box = get_stretched_glyph(arrow_glyph, arrow_width, false);

  // If assembly didn't work well, create fallback
  if (arrow_box.width < min_width) {
    double line_width = min_width + padding;
    double line_thickness = scale(constants_.fraction_rule_thickness);
    double axis = scale(constants_.axis_height);

    arrow_box = MathBox();

    if (node->arrow_type == "equal") {
      // For xlongequal: draw two parallel horizontal lines (like an equals sign)
      double gap = line_thickness * 2;  // Gap between the two lines

      LineInfo top_line;
      top_line.x1 = 0;
      top_line.y1 = axis + gap / 2 + line_thickness / 2;
      top_line.x2 = line_width;
      top_line.y2 = axis + gap / 2 + line_thickness / 2;
      top_line.thickness = line_thickness;
      top_line.color = current_color_;
      arrow_box.lines.push_back(top_line);

      LineInfo bottom_line;
      bottom_line.x1 = 0;
      bottom_line.y1 = axis - gap / 2 - line_thickness / 2;
      bottom_line.x2 = line_width;
      bottom_line.y2 = axis - gap / 2 - line_thickness / 2;
      bottom_line.thickness = line_thickness;
      bottom_line.color = current_color_;
      arrow_box.lines.push_back(bottom_line);

      arrow_box.height = axis + gap / 2 + line_thickness;
      arrow_box.depth = -(axis - gap / 2 - line_thickness);
      if (arrow_box.depth < 0) arrow_box.depth = 0;
    } else {
      // Fallback for arrows: create a line with arrowhead
      MathBox simple_arrow = make_glyph_box(arrow_glyph);

      LineInfo line;
      line.x1 = 0;
      line.y1 = axis;
      line.x2 = line_width;
      line.y2 = axis;
      line.thickness = line_thickness;
      line.color = current_color_;
      arrow_box.lines.push_back(line);

      // Add arrowhead at appropriate end
      if (node->arrow_type == "left" || node->arrow_type == "Left" ||
          node->arrow_type == "hookleft") {
        // Arrowhead on left
        double arrow_y = axis - (simple_arrow.height - simple_arrow.depth) / 2;
        arrow_box.merge(simple_arrow, 0, arrow_y);
      } else {
        // Arrowhead on right
        double arrow_x = line_width - simple_arrow.width;
        double arrow_y = axis - (simple_arrow.height - simple_arrow.depth) / 2;
        arrow_box.merge(simple_arrow, arrow_x, arrow_y);
      }
      arrow_box.height = std::max(axis + line_thickness / 2, simple_arrow.height);
      arrow_box.depth = std::max(-axis + line_thickness / 2, simple_arrow.depth);
    }

    arrow_box.width = line_width;
  }

  // Position text above and below the arrow
  double axis = scale(constants_.axis_height);
  double gap = base_size_ * 0.1;

  MathBox result;

  // Center everything horizontally
  double total_width = std::max({arrow_box.width, above_box.width, below_box.width});

  // Position arrow centered on axis
  double arrow_x = (total_width - arrow_box.width) / 2;
  double arrow_y = axis - (arrow_box.height - arrow_box.depth) / 2;
  result.merge(arrow_box, arrow_x, arrow_y);

  // Position above content
  if (above_box.width > 0 || above_box.height > 0) {
    double above_x = (total_width - above_box.width) / 2;
    double above_y = arrow_y + arrow_box.height + gap;
    result.merge(above_box, above_x, above_y);
    result.height = above_y + above_box.height;
  } else {
    result.height = arrow_y + arrow_box.height;
  }

  // Position below content
  if (below_box.width > 0 || below_box.height > 0) {
    double below_x = (total_width - below_box.width) / 2;
    double below_y = arrow_y - arrow_box.depth - gap - below_box.height;
    result.merge(below_box, below_x, below_y);
    result.depth = -(below_y - below_box.depth);
  } else {
    result.depth = -(arrow_y - arrow_box.depth);
  }

  result.width = total_width;

  return result;
}

MathBox MathLayout::layout_big_delim(MathNode* node) {
  // Get the delimiter character
  std::string delim = node->value;

  // Get base delimiter glyph using shared lookup
  uint32_t delim_cp = get_delimiter_codepoint(delim);
  uint32_t glyph_id = delim_cp ? get_glyph(delim_cp) : 0;
  if (glyph_id == 0) {
    error("Unknown delimiter: " + delim);
    return MathBox();
  }

  // Scale factors for big delimiter sizes correspond to em units:
  // \big = 1.2x, \Big = 1.8x, \bigg = 2.4x, \Bigg = 3.0x
  static const double scale_factors[] = {1.0, 1.2, 1.8, 2.4, 3.0};
  int size_level = std::min(std::max(node->delim_size, 1), 4);
  double target_scale = scale_factors[size_level];

  // Calculate target height for the delimiter
  // Base glyph height in current scale
  hb_glyph_extents_t base_extents;
  hb_font_get_glyph_extents(hb_font_, glyph_id, &base_extents);
  double base_height = (base_extents.y_bearing - (base_extents.y_bearing + base_extents.height)) * current_scale_;
  double target_height = base_height * target_scale;

  // Try to get a MATH variant glyph of appropriate size
  uint32_t variant_glyph = get_size_variant(glyph_id, target_height, true);

  MathBox delim_box;
  if (variant_glyph != glyph_id) {
    // Use the actual variant glyph
    delim_box = make_glyph_box(variant_glyph);
  } else {
    // Fall back to font size scaling
    double scaled_size = base_size_ * target_scale;

    hb_position_t h_advance = hb_font_get_glyph_h_advance(hb_font_, glyph_id);

    double scale_factor = current_scale_ * target_scale;
    delim_box.width = h_advance * scale_factor;
    delim_box.height = base_extents.y_bearing * scale_factor;
    delim_box.depth = (-base_extents.y_bearing - base_extents.height) * scale_factor;
    if (delim_box.depth < 0) delim_box.depth = 0;

    GlyphInfo glyph;
    glyph.glyph_id = glyph_id;
    glyph.x = 0;
    glyph.y = 0;
    glyph.size = scaled_size;
    glyph.advance = delim_box.width;
    glyph.color = current_color_;
    delim_box.glyphs.push_back(glyph);
  }

  // Center on math axis
  double axis = scale(constants_.axis_height);
  double delim_center = (delim_box.height - delim_box.depth) / 2;
  double y_offset = axis - delim_center;

  MathBox result;
  result.merge(delim_box, 0, y_offset);
  result.width = delim_box.width;
  result.height = y_offset + delim_box.height;
  result.depth = delim_box.depth - y_offset;
  if (result.depth < 0) result.depth = 0;

  return result;
}

MathBox MathLayout::layout_style_change(MathNode* node) {
  // Save current forced style and set new one
  MathDisplayStyle saved_style = forced_style_;
  forced_style_ = node->display_style;

  // Adjust scale for script styles
  double old_scale = current_scale_;
  if (node->display_style == MathDisplayStyle::SCRIPT) {
    current_scale_ *= constants_.script_percent_scale_down / 100.0;
  } else if (node->display_style == MathDisplayStyle::SCRIPTSCRIPT) {
    current_scale_ *= constants_.script_script_percent_scale_down / 100.0;
  }

  // Layout content with new style
  MathBox result;
  if (!node->children.empty()) {
    result = layout_node(node->children[0].get());
  }

  // Restore
  forced_style_ = saved_style;
  current_scale_ = old_scale;

  return result;
}

MathBox MathLayout::layout_aligned(MathNode* node) {
  int rows = node->rows;
  int cols = node->cols;

  if (rows == 0 || cols == 0) return MathBox();

  // Get alignment type
  std::string env_type = node->matrix_type;
  std::string col_spec = node->col_alignments;

  // Spacing constants
  double col_gap = base_size_ * 0.8;
  double row_gap = base_size_ * 0.4;

  // For gathered, everything is centered in a single column
  if (env_type == "gathered") {
    col_gap = 0;
  }

  // Layout all cells
  std::vector<MathBox> cells;
  cells.reserve(rows * cols);

  for (int i = 0; i < rows * cols; i++) {
    if (i < static_cast<int>(node->children.size())) {
      cells.push_back(layout_node(node->children[i].get()));
    } else {
      cells.push_back(MathBox());
    }
  }

  // Calculate column widths
  std::vector<double> col_widths(cols, 0);
  for (int c = 0; c < cols; c++) {
    for (int r = 0; r < rows; r++) {
      int idx = r * cols + c;
      col_widths[c] = std::max(col_widths[c], cells[idx].width);
    }
  }

  // Calculate row heights and depths
  std::vector<double> row_heights(rows, 0);
  std::vector<double> row_depths(rows, 0);
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      int idx = r * cols + c;
      row_heights[r] = std::max(row_heights[r], cells[idx].height);
      row_depths[r] = std::max(row_depths[r], cells[idx].depth);
    }
  }

  // Calculate total dimensions
  double total_width = 0;
  for (int c = 0; c < cols; c++) {
    total_width += col_widths[c];
    if (c < cols - 1) total_width += col_gap;
  }

  double total_height = 0;
  for (int r = 0; r < rows; r++) {
    total_height += row_heights[r] + row_depths[r];
    if (r < rows - 1) total_height += row_gap;
  }

  // Determine alignment for each column
  std::vector<char> alignments(cols, 'c');  // Default center
  if (env_type == "aligned" || env_type == "split") {
    // In aligned/split, odd columns (0, 2, 4...) are right-aligned, even (1, 3, 5...) are left-aligned
    for (int c = 0; c < cols; c++) {
      alignments[c] = (c % 2 == 0) ? 'r' : 'l';
    }
  } else if (env_type == "array" && !col_spec.empty()) {
    // Parse column spec for array
    int spec_idx = 0;
    for (int c = 0; c < cols && spec_idx < static_cast<int>(col_spec.size()); c++) {
      // Skip |, @{}, and other non-alignment specifiers
      while (spec_idx < static_cast<int>(col_spec.size()) &&
             col_spec[spec_idx] != 'l' && col_spec[spec_idx] != 'c' && col_spec[spec_idx] != 'r') {
        spec_idx++;
      }
      if (spec_idx < static_cast<int>(col_spec.size())) {
        alignments[c] = col_spec[spec_idx];
        spec_idx++;
      }
    }
  }
  // gathered: all columns centered (default)

  // Position cells
  MathBox content;
  double axis = scale(constants_.axis_height);
  double baseline = axis + total_height / 2 - row_heights[0];

  for (int r = 0; r < rows; r++) {
    double x = 0;

    for (int c = 0; c < cols; c++) {
      int idx = r * cols + c;
      MathBox& cell = cells[idx];

      // Determine cell position based on alignment
      double cell_x;
      if (alignments[c] == 'l') {
        cell_x = x;  // Left-aligned
      } else if (alignments[c] == 'r') {
        cell_x = x + col_widths[c] - cell.width;  // Right-aligned
      } else {
        cell_x = x + (col_widths[c] - cell.width) / 2;  // Centered
      }

      content.merge(cell, cell_x, baseline);

      x += col_widths[c];
      if (c < cols - 1) x += col_gap;
    }

    // Move baseline to next row
    if (r < rows - 1) {
      baseline -= row_depths[r] + row_gap + row_heights[r + 1];
    }
  }

  content.width = total_width;
  content.height = total_height / 2 + axis;
  content.depth = total_height / 2 - axis;

  return content;
}

MathBox MathLayout::layout_smash(MathNode* node) {
  if (node->children.empty()) return MathBox();

  // Layout content normally
  MathBox content = layout_node(node->children[0].get());

  // Modify dimensions based on smash type
  MathBox result = content;

  if (node->smash_type == "full") {
    // Zero out both height and depth
    result.height = 0;
    result.depth = 0;
  } else if (node->smash_type == "t") {
    // Zero out height only
    result.height = 0;
  } else if (node->smash_type == "b") {
    // Zero out depth only
    result.depth = 0;
  }

  // Keep width and glyphs unchanged
  return result;
}

MathBox MathLayout::layout_mod(MathNode* node) {
  MathBox result;

  if (node->mod_type == "bmod") {
    // \bmod: "mod" as binary operator with surrounding space
    double space = base_size_ * 0.25;  // Medium space

    // Add space before
    result.width = space;

    // Layout "mod" in roman
    MathBox mod_text = layout_string("mod");
    result.merge(mod_text, result.width, 0);
    result.width += mod_text.width;
    result.height = mod_text.height;
    result.depth = mod_text.depth;

    // Add space after
    result.width += space;
  } else if (node->mod_type == "pmod") {
    // \pmod{n}: "(mod n)" with space before
    double space = base_size_ * 0.5;

    result.width = space;

    // Layout "(mod "
    MathBox open = layout_string("(mod");
    result.merge(open, result.width, 0);
    result.width += open.width;
    result.height = open.height;
    result.depth = open.depth;

    // Add small space
    result.width += base_size_ * 0.15;

    // Layout content
    if (!node->children.empty()) {
      MathBox content = layout_node(node->children[0].get());
      result.merge(content, result.width, 0);
      result.width += content.width;
      result.height = std::max(result.height, content.height);
      result.depth = std::max(result.depth, content.depth);
    }

    // Layout ")"
    MathBox close = layout_string(")");
    result.merge(close, result.width, 0);
    result.width += close.width;
  } else if (node->mod_type == "pod") {
    // \pod{n}: "(n)" with space before
    double space = base_size_ * 0.5;

    result.width = space;

    // Layout "("
    MathBox open = layout_string("(");
    result.merge(open, result.width, 0);
    result.width += open.width;
    result.height = open.height;
    result.depth = open.depth;

    // Layout content
    if (!node->children.empty()) {
      MathBox content = layout_node(node->children[0].get());
      result.merge(content, result.width, 0);
      result.width += content.width;
      result.height = std::max(result.height, content.height);
      result.depth = std::max(result.depth, content.depth);
    }

    // Layout ")"
    MathBox close = layout_string(")");
    result.merge(close, result.width, 0);
    result.width += close.width;
  }

  return result;
}

MathBox MathLayout::layout_rule(MathNode* node) {
  MathBox result;

  // Convert dimensions from em to points
  // Assume dimensions are in em units
  double em = base_size_ * current_scale_ * 64.0;
  double width = node->rule_width * em;
  double height = node->rule_height * em;
  double raise = node->rule_raise * em;

  // Create a filled rectangle (rule)
  BoxInfo rule;
  rule.x = 0;
  rule.y = raise;
  rule.width = width;
  rule.height = height;
  rule.color = current_color_;

  result.boxes.push_back(rule);
  result.width = width;
  result.height = raise + height;
  result.depth = -raise;
  if (result.depth < 0) result.depth = 0;

  return result;
}

} // namespace math
