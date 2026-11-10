#pragma once

#include "math_ast.h"
#include <string>
#include <vector>
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

// Tokenizer/Lexer
class MathLexer {
public:
  explicit MathLexer(const std::string& input);

  Token next();
  Token peek();
  void consume();
  bool at_end() const;
  size_t position() const { return pos_; }

  // Raw access to the input (for collecting literal text)
  std::string input_substr(size_t from, size_t to) const {
    if (from > input_.size()) from = input_.size();
    if (to > input_.size()) to = input_.size();
    if (to < from) to = from;
    return input_.substr(from, to - from);
  }

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

  // Recursion depth guard against stack overflow on deeply nested input
  static const int MAX_PARSE_DEPTH = 256;
  int depth_ = 0;
  bool depth_exceeded_ = false;

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
