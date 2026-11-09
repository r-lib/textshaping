#pragma once

#include "math_ast.h"
#include <string>
#include <vector>

namespace math {

// Token types for Typst math lexer
enum class TypstTokenType {
  END,           // End of input
  IDENTIFIER,    // alpha, beta, frac, sin, etc.
  NUMBER,        // 123, 3.14
  STRING,        // "text"
  LPAREN,        // (
  RPAREN,        // )
  LBRACKET,      // [
  RBRACKET,      // ]
  LBRACE,        // {
  RBRACE,        // }
  COMMA,         // ,
  SEMICOLON,     // ;
  COLON,         // :
  AMPERSAND,     // &
  SUPER,         // ^
  SUB,           // _
  OPERATOR,      // + - = * / etc.
  SPACE,         // whitespace
  LINEBREAK,     // \ at end of line (Typst multiline)
  OTHER          // anything else
};

struct TypstToken {
  TypstTokenType type;
  std::string value;
  size_t pos;

  TypstToken(TypstTokenType t, const std::string& v, size_t p)
    : type(t), value(v), pos(p) {}
};

// Typst Math Lexer
class TypstLexer {
public:
  explicit TypstLexer(const std::string& input);

  TypstToken next();
  TypstToken peek();
  void consume();
  bool at_end() const;
  size_t position() const { return pos_; }

private:
  std::string input_;
  size_t pos_ = 0;
  TypstToken current_;
  bool has_current_ = false;

  TypstToken scan_token();
  TypstToken scan_identifier();
  TypstToken scan_number();
  TypstToken scan_string();
  char current_char() const;
  char advance();
  bool is_letter(char c) const;
  bool is_digit(char c) const;
  bool is_identifier_char(char c) const;
};

// Typst Math Parser
class TypstParser {
public:
  explicit TypstParser(const std::string& input);

  // Parse the entire input and return the AST
  MathNodePtr parse();

  // Get parse errors
  const std::vector<std::string>& errors() const { return errors_; }
  bool has_errors() const { return !errors_.empty(); }

private:
  TypstLexer lexer_;
  std::vector<std::string> errors_;

  // Parsing methods
  MathNodePtr parse_math_list();
  MathNodePtr parse_brace_content();  // For content inside {...}
  MathNodePtr parse_expression();
  MathNodePtr parse_atom();
  MathNodePtr parse_scripts(MathNodePtr base);
  MathNodePtr parse_group();
  MathNodePtr parse_function_call(const std::string& name);
  MathNodePtr parse_argument_list();

  // Function-specific parsers
  MathNodePtr parse_frac();
  MathNodePtr parse_binom();
  MathNodePtr parse_sqrt();
  MathNodePtr parse_root();
  MathNodePtr parse_accent(const std::string& name);
  MathNodePtr parse_generic_accent();
  MathNodePtr parse_style(const std::string& name);
  MathNodePtr parse_lr();
  MathNodePtr parse_abs();
  MathNodePtr parse_norm();
  MathNodePtr parse_floor();
  MathNodePtr parse_ceil();
  MathNodePtr parse_mat(const std::string& name);
  MathNodePtr parse_cases();
  MathNodePtr parse_vec();
  MathNodePtr parse_cancel();
  MathNodePtr parse_hide();
  MathNodePtr parse_limits();
  MathNodePtr parse_scripts_func();
  MathNodePtr parse_display();
  MathNodePtr parse_inline();
  MathNodePtr parse_script_style();
  MathNodePtr parse_sscript_style();
  MathNodePtr parse_op();
  MathNodePtr parse_attach();
  MathNodePtr parse_overbrace();
  MathNodePtr parse_underbrace();
  MathNodePtr parse_overline();
  MathNodePtr parse_underline();

  // Additional functions
  MathNodePtr parse_round();
  MathNodePtr parse_mid();
  MathNodePtr parse_primes();
  MathNodePtr parse_overbracket();
  MathNodePtr parse_underbracket();
  MathNodePtr parse_overparen();
  MathNodePtr parse_underparen();
  MathNodePtr parse_overshell();
  MathNodePtr parse_undershell();
  MathNodePtr parse_stretch();
  MathNodePtr parse_class();

  // Symbol/identifier handling
  MathNodePtr parse_identifier(const std::string& name);
  bool is_function_name(const std::string& name) const;
  bool is_big_operator(const std::string& name) const;
  bool is_greek_letter(const std::string& name) const;
  bool is_symbol(const std::string& name) const;
  bool is_typst_function(const std::string& name) const;
  bool is_accent_function(const std::string& name) const;
  bool is_style_function(const std::string& name) const;
  bool is_delimiter_function(const std::string& name) const;

  // Utilities
  void error(const std::string& msg);
  bool check(TypstTokenType type);
  bool match(TypstTokenType type);
  TypstToken consume(TypstTokenType type, const std::string& msg);
};

// Detect if input is Typst syntax (vs LaTeX)
bool is_typst_syntax(const std::string& input);

} // namespace math
