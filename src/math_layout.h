#pragma once

#include "math_parser.h"
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>
#include <string>
#include <unordered_map>

namespace math {

// A positioned glyph in the final output
struct GlyphInfo {
  uint32_t glyph_id;      // Glyph index in font
  double x;               // X position (origin at left)
  double y;               // Y position (origin at baseline)
  double size;            // Font size used for this glyph
  double advance;         // Horizontal advance width
  std::string color;      // Color for this glyph (empty = use default)
};

// A filled rectangle (for colorbox backgrounds)
struct BoxInfo {
  double x;               // X position of left edge
  double y;               // Y position of bottom edge
  double width;           // Width of box
  double height;          // Height of box
  std::string color;      // Fill color
};

// A line segment (for fraction bars, overlines, cancel lines, etc.)
struct LineInfo {
  double x1, y1;          // Start point
  double x2, y2;          // End point
  double thickness;       // Line thickness
  std::string color;      // Color for the line
};

// A laid out box with dimensions and glyphs
struct MathBox {
  double width;           // Total width
  double height;          // Height above baseline
  double depth;           // Depth below baseline (positive value)
  double italic_corr;     // Italic correction for scripts

  std::vector<GlyphInfo> glyphs;  // All glyphs with positions
  std::vector<LineInfo> lines;    // All lines (fraction bars, overlines, cancel, etc.)
  std::vector<BoxInfo> boxes;     // All filled boxes (colorbox backgrounds)

  MathBox() : width(0), height(0), depth(0), italic_corr(0) {}

  // Shift all glyphs and rules by offset
  void shift(double dx, double dy);

  // Merge another box's glyphs and rules (applying offset)
  void merge(const MathBox& other, double dx, double dy);
};

// MATH table constants cached for a font
struct MathConstants {
  // Percent scale downs
  int script_percent_scale_down;
  int script_script_percent_scale_down;

  // Axis height
  int axis_height;

  // Superscript positioning
  int superscript_shift_up;
  int superscript_shift_up_cramped;
  int superscript_bottom_min;
  int superscript_baseline_drop_max;

  // Subscript positioning
  int subscript_shift_down;
  int subscript_top_max;
  int subscript_baseline_drop_min;

  // Sub-superscript positioning
  int sub_superscript_gap_min;
  int superscript_bottom_max_with_subscript;
  int space_after_script;

  // Fraction positioning
  int fraction_numerator_shift_up;
  int fraction_numerator_display_style_shift_up;
  int fraction_denominator_shift_down;
  int fraction_denominator_display_style_shift_down;
  int fraction_numerator_gap_min;
  int fraction_num_display_style_gap_min;
  int fraction_rule_thickness;
  int fraction_denominator_gap_min;
  int fraction_denom_display_style_gap_min;

  // Radical positioning
  int radical_vertical_gap;
  int radical_display_style_vertical_gap;
  int radical_rule_thickness;
  int radical_extra_ascender;
  int radical_kern_before_degree;
  int radical_kern_after_degree;
  int radical_degree_bottom_raise_percent;

  // Limits positioning
  int upper_limit_gap_min;
  int upper_limit_baseline_rise_min;
  int lower_limit_gap_min;
  int lower_limit_baseline_drop_min;

  // Accent positioning
  int accent_base_height;
  int flattened_accent_base_height;

  // Overbar/underbar
  int overbar_vertical_gap;
  int overbar_rule_thickness;
  int overbar_extra_ascender;
  int underbar_vertical_gap;
  int underbar_rule_thickness;
  int underbar_extra_descender;

  // Stack
  int stack_top_shift_up;
  int stack_top_display_style_shift_up;
  int stack_bottom_shift_down;
  int stack_bottom_display_style_shift_down;
  int stack_gap_min;
  int stack_display_style_gap_min;

  // Big operator height
  int display_operator_min_height;
};

// The layout engine
class MathLayout {
public:
  MathLayout(const char* font_path, int font_index, double size, double res);
  ~MathLayout();

  // Layout the math expression and return positioned glyphs
  MathBox layout(const std::string& latex, bool display_mode = false);

  // Layout from an already-parsed AST
  MathBox layout(MathNode* node, bool display_mode = false);

  // Check if font has MATH table
  bool has_math() const { return has_math_; }

  // Get errors from layout
  const std::vector<std::string>& errors() const { return errors_; }

private:
  hb_font_t* hb_font_;
  hb_face_t* hb_face_;
  FT_Face ft_face_;
  std::string font_path_;
  FT_Library ft_library_;
  bool owns_ft_library_;
  bool has_math_;
  double base_size_;
  bool display_mode_;
  MathConstants constants_;
  std::vector<std::string> errors_;

  // Current font scale (for scripts etc.)
  double current_scale_;

  // Current color for glyphs (empty = use default)
  std::string current_color_;

  // Current forced display style (for \displaystyle, \textstyle, etc.)
  MathDisplayStyle forced_style_;

  // Flattened accent glyph lookup (regular glyph ID -> flattened glyph ID)
  std::unordered_map<uint32_t, uint32_t> flattened_accents_;

  // Load MATH constants from font
  void load_constants();

  // Build flattened accent lookup table using GSUB 'flac' feature
  void load_flattened_accents();

  // Load default constants when font has no MATH table
  void load_default_constants();

  // Get glyph ID for a codepoint
  uint32_t get_glyph(uint32_t codepoint);

  // Get glyph metrics
  void get_glyph_metrics(uint32_t glyph_id, double* advance, double* height, double* depth);

  // Get glyph dimensions
  void get_glyph_extents(uint32_t glyph_id, double* x_bearing, double* y_bearing,
                         double* width, double* height);

  // Get italics correction
  double get_italic_correction(uint32_t glyph_id);

  // Get top accent attachment
  double get_top_accent_attachment(uint32_t glyph_id);

  // Get glyph variant for target size
  uint32_t get_size_variant(uint32_t glyph_id, double target_size, bool vertical);

  // Layout methods for each node type
  MathBox layout_node(MathNode* node);
  MathBox layout_symbol(MathNode* node);
  MathBox layout_number(MathNode* node);
  MathBox layout_row(MathNode* node);
  MathBox layout_group(MathNode* node);
  MathBox layout_superscript(MathNode* node);
  MathBox layout_subscript(MathNode* node);
  MathBox layout_subsuperscript(MathNode* node);
  MathBox layout_fraction(MathNode* node);
  MathBox layout_sqrt(MathNode* node);
  MathBox layout_delimited(MathNode* node);
  MathBox layout_big_op(MathNode* node);
  MathBox layout_accent(MathNode* node);
  MathBox layout_function(MathNode* node);
  MathBox layout_text(MathNode* node);
  MathBox layout_space(MathNode* node);
  MathBox layout_styled(MathNode* node);
  MathBox layout_matrix(MathNode* node);
  MathBox layout_stack(MathNode* node);
  MathBox layout_operatorname(MathNode* node);
  MathBox layout_phantom(MathNode* node);
  MathBox layout_colored(MathNode* node);
  MathBox layout_colorbox(MathNode* node);
  MathBox layout_cancel(MathNode* node);
  MathBox layout_boxed(MathNode* node);
  MathBox layout_substack(MathNode* node);
  MathBox layout_negated(MathNode* node);
  MathBox layout_ext_arrow(MathNode* node);
  MathBox layout_big_delim(MathNode* node);
  MathBox layout_style_change(MathNode* node);
  MathBox layout_aligned(MathNode* node);
  MathBox layout_smash(MathNode* node);
  MathBox layout_mod(MathNode* node);
  MathBox layout_rule(MathNode* node);

  // Helper to layout text string
  MathBox layout_string(const std::string& str);

  // Helper to assemble a glyph from parts for arbitrary sizes
  MathBox assemble_glyph(uint32_t glyph_id, double target_size, bool vertical);

  // Get stretched glyph - first tries size variants, falls back to assembly
  MathBox get_stretched_glyph(uint32_t glyph_id, double target_size, bool vertical);

  // Helper to create a single glyph box
  MathBox make_glyph_box(uint32_t glyph_id);

  // Scale value from design units to current size
  double scale(int value) const;

  // Get effective size for script level
  double get_script_size(int level) const;

  void error(const std::string& msg);
};

} // namespace math
