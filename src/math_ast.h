#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace math {

// Math text styles for \mathbf, \mathrm, etc.
enum class MathStyle {
  NORMAL,        // Default (math italic for letters)
  BOLD,          // \mathbf
  ITALIC,        // \mathit (text italic)
  BOLD_ITALIC,   // \boldsymbol
  ROMAN,         // \mathrm (upright)
  SANS,          // \mathsf
  SANS_BOLD,     // \mathsf + bold
  SANS_ITALIC,   // \mathsf + italic
  MONO,          // \mathtt
  SCRIPT,        // \mathcal
  FRAKTUR,       // \mathfrak
  DOUBLE_STRUCK, // \mathbb
  BOLD_SCRIPT,   // bold script
  BOLD_FRAKTUR   // bold fraktur
};

// Math display style (for \displaystyle, \textstyle, etc.)
enum class MathDisplayStyle {
  INHERIT,       // Use parent's style
  DISPLAY,       // \displaystyle
  TEXT,          // \textstyle
  SCRIPT,        // \scriptstyle
  SCRIPTSCRIPT   // \scriptscriptstyle
};

// AST node types
enum class NodeType {
  // Atoms
  SYMBOL,        // single letter/digit/operator
  NUMBER,        // sequence of digits
  TEXT,          // \text{...}
  STYLED,        // \mathbf{...}, \mathrm{...}, etc.

  // Structures
  GROUP,         // {...}
  ROW,           // horizontal sequence of nodes

  // Scripts
  SUPERSCRIPT,   // base^exp
  SUBSCRIPT,     // base_sub
  SUBSUPERSCRIPT,// base_sub^exp or base^exp_sub
  MULTISCRIPT,   // Four-corner attachments (tensor indices): t, b, tl, tr, bl, br

  // Fractions and stacks
  FRACTION,      // \frac{num}{denom}
  STACK,         // \binom, \atop, \stackrel, \overset, \underset

  // Radicals
  SQRT,          // \sqrt{...} or \sqrt[n]{...}

  // Delimiters
  DELIMITED,     // \left...\right pair
  BIG_DELIM,     // \big(, \Big(, \bigg(, \Bigg( etc.

  // Big operators
  BIG_OP,        // \sum, \int, \prod with limits

  // Accents
  ACCENT,        // \hat, \bar, \vec, etc.

  // Spacing
  SPACE_NODE,    // explicit spacing commands

  // Functions
  FUNCTION,      // \sin, \cos, \log, etc.
  OPERATORNAME,  // \operatorname{...}

  // Arrays and matrices
  MATRIX,        // \begin{matrix}...\end{matrix} and variants
  ALIGNED,       // \begin{aligned}, \begin{gathered}, \begin{array}, \begin{split}
  SUBSTACK,      // \substack{a \\ b \\ c}

  // Phantoms
  PHANTOM,       // \phantom, \hphantom, \vphantom
  SMASH,         // \smash, \smash[t], \smash[b]

  // Colors and formatting
  COLORED,       // \textcolor{color}{content}
  COLORBOX,      // \colorbox{color}{content}
  CANCEL,        // \cancel, \xcancel

  // Negation
  NEGATED,       // \not followed by symbol

  // Boxed
  BOXED,         // \boxed{expression}

  // Extensible arrows
  EXT_ARROW,     // \xrightarrow, \xleftarrow, etc.

  // Style changes
  STYLE_CHANGE,  // \displaystyle, \textstyle, etc.

  // Modular arithmetic
  MOD,           // \pmod, \bmod, \pod

  // Rules
  RULE,          // \rule{width}{height}

  // Typst-specific
  STRETCHED,     // stretch(body, size)
  CLASS_OVERRIDE // class(class, body)
};

// Forward declaration
struct MathNode;

using MathNodePtr = std::unique_ptr<MathNode>;

struct MathNode {
  NodeType type;

  // For SYMBOL, NUMBER, TEXT, FUNCTION, BIG_OP
  std::string value;

  // For commands that have a specific command name (e.g., accent type)
  std::string command;

  // Children
  // For MULTISCRIPT: [base, t, b, tl, tr, bl, br] (nullptr for unused positions)
  std::vector<MathNodePtr> children;

  // For DELIMITED: delimiter characters
  std::string left_delim;
  std::string right_delim;
  double delim_height = 0;  // Explicit delimiter height (0 = auto)
  std::string delim_height_unit;  // "em", "pt", or "relative" (for %)

  // For SPACE_NODE: explicit spacing with units
  double space_amount = 0;  // Numeric amount
  std::string space_unit;   // "em", "pt", or empty for legacy (lookup by value)

  // For ACCENT: whether it's a wide accent
  bool wide = false;

  // For scripts: whether limits should go above/below (vs as scripts)
  bool limits = false;

  // For STYLED: the math style to use
  MathStyle style = MathStyle::NORMAL;

  // For MATRIX: dimensions and type
  int rows = 0;
  int cols = 0;
  std::string matrix_type;  // "matrix", "pmatrix", "bmatrix", etc.

  // For STACK: type of stacking
  std::string stack_type;  // "binom", "atop", "stackrel", "overset", "underset"

  // For PHANTOM: type of phantom
  std::string phantom_type;  // "full", "h", "v"

  // For COLORED, COLORBOX, rules: color specification
  std::string color;

  // For CANCEL: type of cancellation
  std::string cancel_type;  // "cancel", "xcancel"

  // For EXT_ARROW: arrow direction
  std::string arrow_type;  // "right", "left", "leftright", "Right", "Left", etc.

  // For BIG_DELIM: size level (1=\big, 2=\Big, 3=\bigg, 4=\Bigg)
  int delim_size = 0;

  // For FRACTION: style variant
  std::string frac_style;  // "normal", "display", "text", "continued"

  // For BIG_OP: limits override
  int limits_override = 0;  // 0=default, 1=force_limits, -1=force_nolimits

  // For STYLE_CHANGE: the display style
  MathDisplayStyle display_style = MathDisplayStyle::INHERIT;

  // For ALIGNED: column alignments
  std::string col_alignments;  // e.g., "lcr" for array

  // For SMASH: smash type
  std::string smash_type;  // "full", "t", "b"

  // For MOD: mod type
  std::string mod_type;  // "pmod", "bmod", "pod"

  // For RULE: dimensions in em
  double rule_width = 0;
  double rule_height = 0;
  double rule_raise = 0;

  // For MATRIX: augment lines (vertical/horizontal divider positions)
  std::vector<int> augment_vlines;  // Column indices for vertical lines (after column i)
  std::vector<int> augment_hlines;  // Row indices for horizontal lines (after row i)

  // For MATRIX/VEC: custom gap parameters (-1 = use default)
  double row_gap = -1;     // Custom row gap in em
  double column_gap = -1;  // Custom column gap in em

  // For STYLE_CHANGE: cramped parameter
  bool cramped = false;

  // For STRETCHED: target size
  double stretch_size = 0;       // Target size
  std::string stretch_size_unit; // "em", "pt", or "relative"

  // For CLASS_OVERRIDE: math class name
  std::string math_class;  // "normal", "punctuation", "opening", "closing", "fence", "large", "relation", "unary", "binary", "vary"

  // For ACCENT: additional parameters
  double accent_size = 0;      // Accent size scaling (0 = default, e.g. 1.5 = 150%)
  bool dotless = false;        // Use dotless variant for i/j

  MathNode(NodeType t) : type(t) {}
  MathNode(NodeType t, const std::string& v) : type(t), value(v) {}

  // Add a child node
  void add_child(MathNodePtr child) {
    children.push_back(std::move(child));
  }
};

} // namespace math
