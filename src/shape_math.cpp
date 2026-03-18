#include <cpp11.hpp>
#include "math_layout.h"
#include "cache_lru.h"
#include <systemfonts.h>
#include <functional>

using namespace cpp11;

// Cache key for math shaping results
struct MathShapeID {
  size_t string_hash;
  std::string font_path;
  int font_index;
  double size;
  double resolution;
  bool display_mode;
  std::string color;

  inline bool operator==(const MathShapeID& other) const {
    return string_hash == other.string_hash &&
           font_path == other.font_path &&
           font_index == other.font_index &&
           size == other.size &&
           resolution == other.resolution &&
           display_mode == other.display_mode &&
           color == other.color;
  }
};

namespace std {
template <>
struct hash<MathShapeID> {
  size_t operator()(const MathShapeID& x) const {
    return x.string_hash ^
           std::hash<std::string>()(x.font_path) ^
           std::hash<int>()(x.font_index) ^
           std::hash<double>()(x.size) ^
           std::hash<double>()(x.resolution) ^
           std::hash<bool>()(x.display_mode) ^
           std::hash<std::string>()(x.color);
  }
};
}

// Global LRU cache for math shaping results
static LRU_Cache<MathShapeID, math::MathBox> math_shape_cache(500);

[[cpp11::register]]
cpp11::writable::list shape_math_c(
    cpp11::strings math,
    cpp11::strings font_path,
    cpp11::integers font_index,
    cpp11::doubles size,
    cpp11::doubles res,
    cpp11::logicals display,
    cpp11::strings color
) {
  // Vectors for shape (glyph positions)
  cpp11::writable::integers shape_glyph;      // row number
  cpp11::writable::integers shape_index;      // glyph index in font
  cpp11::writable::integers shape_metric_id;  // same as string_id for math
  cpp11::writable::integers shape_string_id;
  cpp11::writable::doubles shape_x;
  cpp11::writable::doubles shape_y;
  cpp11::writable::strings shape_font_paths;
  cpp11::writable::integers shape_font_indices;
  cpp11::writable::doubles shape_sizes;
  cpp11::writable::doubles shape_advances;
  cpp11::writable::strings shape_colors;

  // Vectors for lines (fraction bars, radical overlines, cancel lines, etc.)
  cpp11::writable::doubles line_x1;
  cpp11::writable::doubles line_y1;
  cpp11::writable::doubles line_x2;
  cpp11::writable::doubles line_y2;
  cpp11::writable::doubles line_thickness;
  cpp11::writable::integers line_string_ids;
  cpp11::writable::strings line_colors;

  // Vectors for boxes (colorbox backgrounds)
  cpp11::writable::doubles box_x;
  cpp11::writable::doubles box_y;
  cpp11::writable::doubles box_width;
  cpp11::writable::doubles box_height;
  cpp11::writable::integers box_string_ids;
  cpp11::writable::strings box_colors;

  // Vectors for metrics
  cpp11::writable::doubles widths;
  cpp11::writable::doubles heights;
  cpp11::writable::doubles depths;

  int n_strings = math.size();

  for (int i = 0; i < n_strings; i++) {
    const char* latex = CHAR(math[i]);

    const char* path = CHAR(font_path[i]);
    int index = font_index[i];
    double font_size = size[i];
    double resolution = res[i];
    bool display_mode = display[i];
    std::string base_color = std::string(CHAR(color[i]));

    // Create layout engine
    math::MathLayout layout(path, index, font_size, resolution);

    // Create cache key
    MathShapeID cache_key;
    cache_key.string_hash = std::hash<std::string>()(latex);
    cache_key.font_path = path;
    cache_key.font_index = index;
    cache_key.size = font_size;
    cache_key.resolution = resolution;
    cache_key.display_mode = display_mode;
    cache_key.color = base_color;

    // Check cache first
    math::MathBox box;
    if (!math_shape_cache.get(cache_key, box)) {
      // Not in cache, compute and store
      box = layout.layout(latex, display_mode);

     // Throw error if any parsing/layout errors occurred
     if (!layout.errors().empty()) {
       std::string msg = "Math layout error: ";
       for (size_t i = 0; i < layout.errors().size(); i++) {
         if (i > 0) msg += "; ";
         msg += layout.errors()[i];
       }
       cpp11::stop(msg.c_str());
     }

      math_shape_cache.add(cache_key, box);
    }

    // Collect shape (glyph positions)
    int glyph_num = 0;
    for (const auto& glyph : box.glyphs) {
      shape_glyph.push_back(++glyph_num);  // 1-indexed position within this string
      shape_index.push_back(static_cast<int>(glyph.glyph_id));
      shape_metric_id.push_back(i + 1);  // same as string_id for math
      shape_string_id.push_back(i + 1);  // 1-indexed for R
      shape_x.push_back(glyph.x);
      shape_y.push_back(glyph.y);
      shape_font_paths.push_back(path);
      shape_font_indices.push_back(index);
      shape_sizes.push_back(glyph.size);
      shape_advances.push_back(glyph.advance);
      // Use base_color if glyph color is empty
      shape_colors.push_back(glyph.color.empty() ? base_color : glyph.color);
    }

    // Collect lines
    for (const auto& line : box.lines) {
      line_x1.push_back(line.x1);
      line_y1.push_back(line.y1);
      line_x2.push_back(line.x2);
      line_y2.push_back(line.y2);
      line_thickness.push_back(line.thickness);
      line_string_ids.push_back(i + 1);
      // Use base_color if line color is empty
      line_colors.push_back(line.color.empty() ? base_color : line.color);
    }

    // Collect boxes (colorbox backgrounds, rules)
    for (const auto& b : box.boxes) {
      box_x.push_back(b.x);
      box_y.push_back(b.y);
      box_width.push_back(b.width);
      box_height.push_back(b.height);
      box_string_ids.push_back(i + 1);
      // Use base_color if box color is empty
      box_colors.push_back(b.color.empty() ? base_color : b.color);
    }

    // Collect metrics
    widths.push_back(box.width);
    heights.push_back(box.height);
    depths.push_back(box.depth);
  }

  // Create shape data frame (matching shape_text() structure)
  cpp11::writable::list shape;
  shape.push_back(cpp11::named_arg("glyph") = shape_glyph);
  shape.push_back(cpp11::named_arg("index") = shape_index);
  shape.push_back(cpp11::named_arg("metric_id") = shape_metric_id);
  shape.push_back(cpp11::named_arg("string_id") = shape_string_id);
  shape.push_back(cpp11::named_arg("x_offset") = shape_x);
  shape.push_back(cpp11::named_arg("y_offset") = shape_y);
  shape.push_back(cpp11::named_arg("font_path") = shape_font_paths);
  shape.push_back(cpp11::named_arg("font_index") = shape_font_indices);
  shape.push_back(cpp11::named_arg("font_size") = shape_sizes);
  shape.push_back(cpp11::named_arg("advance") = shape_advances);
  shape.push_back(cpp11::named_arg("color") = shape_colors);

  int n_shape = shape_glyph.size();
  shape.attr("class") = "data.frame";
  if (n_shape > 0) {
    shape.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -n_shape});
  } else {
    shape.attr("row.names") = cpp11::writable::integers();
  }

  // Create lines data frame
  cpp11::writable::list lines;
  lines.push_back(cpp11::named_arg("x1") = line_x1);
  lines.push_back(cpp11::named_arg("y1") = line_y1);
  lines.push_back(cpp11::named_arg("x2") = line_x2);
  lines.push_back(cpp11::named_arg("y2") = line_y2);
  lines.push_back(cpp11::named_arg("thickness") = line_thickness);
  lines.push_back(cpp11::named_arg("string_id") = line_string_ids);
  lines.push_back(cpp11::named_arg("color") = line_colors);

  int n_lines = line_x1.size();
  lines.attr("class") = "data.frame";
  if (n_lines > 0) {
    lines.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -n_lines});
  } else {
    lines.attr("row.names") = cpp11::writable::integers();
  }

  // Create boxes data frame
  cpp11::writable::list boxes;
  boxes.push_back(cpp11::named_arg("x") = box_x);
  boxes.push_back(cpp11::named_arg("y") = box_y);
  boxes.push_back(cpp11::named_arg("width") = box_width);
  boxes.push_back(cpp11::named_arg("height") = box_height);
  boxes.push_back(cpp11::named_arg("string_id") = box_string_ids);
  boxes.push_back(cpp11::named_arg("color") = box_colors);

  int n_boxes = box_x.size();
  boxes.attr("class") = "data.frame";
  if (n_boxes > 0) {
    boxes.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -n_boxes});
  } else {
    boxes.attr("row.names") = cpp11::writable::integers();
  }

  // Create metrics data frame
  cpp11::writable::list metrics;
  metrics.push_back(cpp11::named_arg("width") = widths);
  metrics.push_back(cpp11::named_arg("height") = heights);
  metrics.push_back(cpp11::named_arg("depth") = depths);

  metrics.attr("class") = "data.frame";
  if (n_strings > 0) {
    metrics.attr("row.names") = cpp11::writable::integers({NA_INTEGER, -n_strings});
  } else {
    metrics.attr("row.names") = cpp11::writable::integers();
  }

  // Combine into result
  cpp11::writable::list result;
  result.push_back(cpp11::named_arg("shape") = shape);
  result.push_back(cpp11::named_arg("lines") = lines);
  result.push_back(cpp11::named_arg("boxes") = boxes);
  result.push_back(cpp11::named_arg("metrics") = metrics);

  return result;
}
