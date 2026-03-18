#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace math {

// Token types for LaTeX math lexer
enum class TokenType {
  END,           // End of input
  LETTER,        // a-z, A-Z
  DIGIT,         // 0-9
  OPERATOR,      // + - = < > etc.
  LBRACE,        // {
  RBRACE,        // }
  LPAREN,        // (
  RPAREN,        // )
  LBRACKET,      // [
  RBRACKET,      // ]
  SUPER,         // ^
  SUB,           // _
  COMMAND,       // \name
  SPACE,         // whitespace (usually ignored)
  OTHER          // anything else
};

struct Token {
  TokenType type;
  std::string value;
  size_t pos;  // position in input string

  Token(TokenType t, const std::string& v, size_t p)
    : type(t), value(v), pos(p) {}
};

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
  RULE           // \rule{width}{height}
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
  std::vector<MathNodePtr> children;

  // For DELIMITED: delimiter characters
  std::string left_delim;
  std::string right_delim;

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

  MathNode(NodeType t) : type(t) {}
  MathNode(NodeType t, const std::string& v) : type(t), value(v) {}

  // Add a child node
  void add_child(MathNodePtr child) {
    children.push_back(std::move(child));
  }
};

// Tokenizer/Lexer
class MathLexer {
public:
  explicit MathLexer(const std::string& input);

  Token next();
  Token peek();
  void consume();
  bool at_end() const;
  size_t position() const { return pos_; }

private:
  std::string input_;
  size_t pos_ = 0;
  Token current_;
  bool has_current_ = false;

  Token scan_token();
  Token scan_command();
  char current_char() const;
  char advance();
  bool is_letter(char c) const;
  bool is_digit(char c) const;
};

// Parser
class MathParser {
public:
  explicit MathParser(const std::string& input);

  // Parse the entire input and return the AST
  MathNodePtr parse();

  // Get parse errors
  const std::vector<std::string>& errors() const { return errors_; }
  bool has_errors() const { return !errors_.empty(); }

private:
  MathLexer lexer_;
  std::vector<std::string> errors_;

  // Parsing methods (recursive descent)
  MathNodePtr parse_math_list();
  MathNodePtr parse_atom();
  MathNodePtr parse_scripts(MathNodePtr base);
  MathNodePtr parse_group();
  MathNodePtr parse_command();
  MathNodePtr parse_argument();  // parse single token or {...}
  MathNodePtr parse_optional_argument();  // parse [...] if present

  // Command-specific parsers
  MathNodePtr parse_frac();
  MathNodePtr parse_sqrt();
  MathNodePtr parse_left_right();
  MathNodePtr parse_accent(const std::string& cmd, bool wide);
  MathNodePtr parse_text();
  MathNodePtr parse_big_op(const std::string& cmd);
  MathNodePtr parse_function(const std::string& cmd);
  MathNodePtr parse_symbol_command(const std::string& cmd);
  MathNodePtr parse_space_command(const std::string& cmd);
  MathNodePtr parse_style_command(const std::string& cmd);
  MathNodePtr parse_begin_environment();
  MathNodePtr parse_stack_command(const std::string& cmd);
  MathNodePtr parse_operatorname(bool limits);
  MathNodePtr parse_phantom(const std::string& cmd);
  MathNodePtr parse_color_command(const std::string& cmd);
  MathNodePtr parse_cancel(const std::string& cmd);
  MathNodePtr parse_boxed();
  MathNodePtr parse_substack();
  MathNodePtr parse_not();
  MathNodePtr parse_ext_arrow(const std::string& cmd);
  MathNodePtr parse_big_delim(const std::string& cmd);
  MathNodePtr parse_style_change(const std::string& cmd);
  MathNodePtr parse_smash();
  MathNodePtr parse_mod_command(const std::string& cmd);
  MathNodePtr parse_rule();
  MathNodePtr parse_over_under_arrow(const std::string& cmd);

  // Utilities
  void error(const std::string& msg);
  bool check(TokenType type);
  bool match(TokenType type);
  Token consume(TokenType type, const std::string& msg);
};

// Command classification helpers
bool is_greek_letter(const std::string& cmd);
bool is_binary_operator(const std::string& cmd);
bool is_relation(const std::string& cmd);
bool is_arrow(const std::string& cmd);
bool is_big_operator(const std::string& cmd);
bool is_accent_command(const std::string& cmd);
bool is_wide_accent_command(const std::string& cmd);
bool is_function_command(const std::string& cmd);
bool is_delimiter(const std::string& cmd);
bool is_space_command(const std::string& cmd);
bool is_style_command(const std::string& cmd);
bool is_stack_command(const std::string& cmd);
bool is_phantom_command(const std::string& cmd);
bool is_color_command(const std::string& cmd);
bool is_cancel_command(const std::string& cmd);
bool is_extensible_arrow(const std::string& cmd);

// Get the MathStyle for a style command
MathStyle style_command_to_style(const std::string& cmd);

// Get Unicode codepoint for a command (returns 0 if not found)
uint32_t command_to_codepoint(const std::string& cmd);

// Check if environment name is a matrix type
bool is_matrix_environment(const std::string& name);

// Check if environment name is cases
bool is_cases_environment(const std::string& name);

// Check if environment name is an alignment environment
bool is_alignment_environment(const std::string& name);

// Check if command is a big delimiter command
bool is_big_delim_command(const std::string& cmd);

// Get size level for big delimiter command (1-4)
int get_big_delim_size(const std::string& cmd);

// Check if command is a display style command
bool is_display_style_command(const std::string& cmd);

// Check if command is a fraction variant
bool is_fraction_variant(const std::string& cmd);

// Check if command is an over/under arrow
bool is_over_under_arrow(const std::string& cmd);

// Check if command is a mod command
bool is_mod_command(const std::string& cmd);

} // namespace math
