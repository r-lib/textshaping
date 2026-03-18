#include "math_parser.h"
#include "math_symbols.h"
#include <cctype>

namespace math {

// =============================================================================
// MathLexer implementation
// =============================================================================

MathLexer::MathLexer(const std::string& input)
  : input_(input), current_(TokenType::END, "", 0) {}

char MathLexer::current_char() const {
  if (pos_ >= input_.size()) return '\0';
  return input_[pos_];
}

char MathLexer::advance() {
  if (pos_ < input_.size()) {
    return input_[pos_++];
  }
  return '\0';
}

bool MathLexer::is_letter(char c) const {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool MathLexer::is_digit(char c) const {
  return c >= '0' && c <= '9';
}

Token MathLexer::scan_command() {
  size_t start = pos_ - 1;  // Include the backslash
  std::string cmd;

  // Handle single-character commands
  char c = current_char();
  if (!is_letter(c)) {
    // Single character command like \, \; \{ etc.
    if (c != '\0') {
      cmd += advance();
    }
    return Token(TokenType::COMMAND, cmd, start);
  }

  // Multi-character command
  while (is_letter(current_char())) {
    cmd += advance();
  }

  return Token(TokenType::COMMAND, cmd, start);
}

Token MathLexer::scan_token() {
  // Skip whitespace
  while (std::isspace(current_char())) {
    advance();
  }

  if (at_end()) {
    return Token(TokenType::END, "", pos_);
  }

  size_t start = pos_;
  char c = advance();

  // Single character tokens
  switch (c) {
    case '{': return Token(TokenType::LBRACE, "{", start);
    case '}': return Token(TokenType::RBRACE, "}", start);
    case '(': return Token(TokenType::LPAREN, "(", start);
    case ')': return Token(TokenType::RPAREN, ")", start);
    case '[': return Token(TokenType::LBRACKET, "[", start);
    case ']': return Token(TokenType::RBRACKET, "]", start);
    case '^': return Token(TokenType::SUPER, "^", start);
    case '_': return Token(TokenType::SUB, "_", start);
    case '\\': return scan_command();
    default: break;
  }

  // Letters
  if (is_letter(c)) {
    return Token(TokenType::LETTER, std::string(1, c), start);
  }

  // Digits
  if (is_digit(c)) {
    std::string num(1, c);
    while (is_digit(current_char())) {
      num += advance();
    }
    // Check for decimal point
    if (current_char() == '.') {
      num += advance();
      while (is_digit(current_char())) {
        num += advance();
      }
    }
    return Token(TokenType::DIGIT, num, start);
  }

  // Operators and other characters
  static const std::string operators = "+-=<>!|/.,;:'\"*&@#%~`?";
  if (operators.find(c) != std::string::npos) {
    return Token(TokenType::OPERATOR, std::string(1, c), start);
  }

  return Token(TokenType::OTHER, std::string(1, c), start);
}

Token MathLexer::next() {
  if (has_current_) {
    has_current_ = false;
    return current_;
  }
  return scan_token();
}

Token MathLexer::peek() {
  if (!has_current_) {
    current_ = scan_token();
    has_current_ = true;
  }
  return current_;
}

void MathLexer::consume() {
  if (has_current_) {
    has_current_ = false;
  } else {
    scan_token();
  }
}

bool MathLexer::at_end() const {
  return pos_ >= input_.size();
}

// =============================================================================
// MathParser implementation
// =============================================================================

MathParser::MathParser(const std::string& input) : lexer_(input) {}

void MathParser::error(const std::string& msg) {
  errors_.push_back(msg);
}

bool MathParser::check(TokenType type) {
  return lexer_.peek().type == type;
}

bool MathParser::match(TokenType type) {
  if (check(type)) {
    lexer_.consume();
    return true;
  }
  return false;
}

Token MathParser::consume(TokenType type, const std::string& msg) {
  Token t = lexer_.peek();
  if (t.type != type) {
    error(msg + " at position " + std::to_string(t.pos));
    return t;
  }
  lexer_.consume();
  return t;
}

MathNodePtr MathParser::parse() {
  return parse_math_list();
}

MathNodePtr MathParser::parse_math_list() {
  auto row = std::make_unique<MathNode>(NodeType::ROW);

  while (!check(TokenType::END) && !check(TokenType::RBRACE) &&
         !check(TokenType::RBRACKET)) {
    auto atom = parse_atom();
    if (atom) {
      // Check for scripts
      atom = parse_scripts(std::move(atom));
      row->add_child(std::move(atom));
    } else {
      // Skip unrecognized token
      lexer_.consume();
    }
  }

  // If only one child, return it directly
  if (row->children.size() == 1) {
    return std::move(row->children[0]);
  }

  return row;
}

MathNodePtr MathParser::parse_atom() {
  Token t = lexer_.peek();

  switch (t.type) {
    case TokenType::LETTER: {
      lexer_.consume();
      return std::make_unique<MathNode>(NodeType::SYMBOL, t.value);
    }

    case TokenType::DIGIT: {
      lexer_.consume();
      return std::make_unique<MathNode>(NodeType::NUMBER, t.value);
    }

    case TokenType::OPERATOR: {
      lexer_.consume();
      return std::make_unique<MathNode>(NodeType::SYMBOL, t.value);
    }

    case TokenType::LPAREN:
    case TokenType::RPAREN:
    case TokenType::LBRACKET:
    case TokenType::RBRACKET: {
      lexer_.consume();
      return std::make_unique<MathNode>(NodeType::SYMBOL, t.value);
    }

    case TokenType::LBRACE:
      return parse_group();

    case TokenType::COMMAND:
      return parse_command();

    default:
      return nullptr;
  }
}

MathNodePtr MathParser::parse_scripts(MathNodePtr base) {
  if (!base) return base;

  MathNodePtr sub, sup;

  // Check for ^ or _
  while (check(TokenType::SUPER) || check(TokenType::SUB)) {
    if (match(TokenType::SUPER)) {
      if (sup) {
        error("Double superscript");
      }
      sup = parse_argument();
    } else if (match(TokenType::SUB)) {
      if (sub) {
        error("Double subscript");
      }
      sub = parse_argument();
    }
  }

  if (!sub && !sup) {
    return base;
  }

  if (sub && sup) {
    auto node = std::make_unique<MathNode>(NodeType::SUBSUPERSCRIPT);
    node->add_child(std::move(base));
    node->add_child(std::move(sub));
    node->add_child(std::move(sup));
    return node;
  } else if (sup) {
    auto node = std::make_unique<MathNode>(NodeType::SUPERSCRIPT);
    node->add_child(std::move(base));
    node->add_child(std::move(sup));
    return node;
  } else {
    auto node = std::make_unique<MathNode>(NodeType::SUBSCRIPT);
    node->add_child(std::move(base));
    node->add_child(std::move(sub));
    return node;
  }
}

MathNodePtr MathParser::parse_group() {
  consume(TokenType::LBRACE, "Expected '{'");
  auto content = parse_math_list();
  consume(TokenType::RBRACE, "Expected '}'");

  auto group = std::make_unique<MathNode>(NodeType::GROUP);
  if (content) {
    group->add_child(std::move(content));
  }
  return group;
}

MathNodePtr MathParser::parse_argument() {
  if (check(TokenType::LBRACE)) {
    return parse_group();
  }
  // Single token argument
  return parse_atom();
}

MathNodePtr MathParser::parse_optional_argument() {
  if (!check(TokenType::LBRACKET)) {
    return nullptr;
  }
  consume(TokenType::LBRACKET, "Expected '['");
  auto content = parse_math_list();
  consume(TokenType::RBRACKET, "Expected ']'");
  return content;
}

MathNodePtr MathParser::parse_command() {
  Token cmd = lexer_.next();
  const std::string& name = cmd.value;

  // Environments (\begin{...}...\end{...})
  if (name == "begin") {
    return parse_begin_environment();
  }

  // Fractions
  if (name == "frac" || is_fraction_variant(name)) {
    auto node = std::make_unique<MathNode>(NodeType::FRACTION);
    node->command = name;

    // Set fraction style based on command
    if (name == "dfrac") {
      node->frac_style = "display";
    } else if (name == "tfrac") {
      node->frac_style = "text";
    } else if (name == "cfrac") {
      node->frac_style = "continued";
    } else {
      node->frac_style = "normal";
    }

    auto num = parse_argument();
    auto denom = parse_argument();

    if (num) node->add_child(std::move(num));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    if (denom) node->add_child(std::move(denom));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    return node;
  }

  // Big delimiters (\big, \Big, \bigg, \Bigg and variants)
  if (is_big_delim_command(name)) {
    return parse_big_delim(name);
  }

  // Display style commands (\displaystyle, \textstyle, etc.)
  if (is_display_style_command(name)) {
    return parse_style_change(name);
  }

  // Limits control (\limits, \nolimits)
  if (name == "limits" || name == "nolimits") {
    // These are handled in parse_big_op, if we reach here it's a stray command
    error("\\limits or \\nolimits without preceding operator");
    return nullptr;
  }

  // Smash command
  if (name == "smash") {
    return parse_smash();
  }

  // Mod commands
  if (is_mod_command(name)) {
    return parse_mod_command(name);
  }

  // Rule command
  if (name == "rule") {
    return parse_rule();
  }

  // Square roots
  if (name == "sqrt") {
    return parse_sqrt();
  }

  // Delimiters
  if (name == "left") {
    return parse_left_right();
  }

  // Text (exits math mode)
  if (name == "text") {
    return parse_text();
  }

  // Style commands (\mathbf, \mathrm, \mathbb, etc.)
  if (is_style_command(name)) {
    return parse_style_command(name);
  }

  // Big operators
  if (is_big_operator(name)) {
    return parse_big_op(name);
  }

  // Functions
  if (is_function_command(name)) {
    return parse_function(name);
  }

  // Accents
  if (is_accent_command(name)) {
    return parse_accent(name, false);
  }
  if (is_wide_accent_command(name)) {
    return parse_accent(name, true);
  }

  // Space commands
  if (is_space_command(name)) {
    return parse_space_command(name);
  }

  // Stack commands (\binom, \overset, \underset, \stackrel, \atop)
  if (is_stack_command(name)) {
    return parse_stack_command(name);
  }

  // Operatorname
  if (name == "operatorname") {
    // Check for * after operatorname
    Token next = lexer_.peek();
    bool with_limits = false;
    if (next.type == TokenType::OPERATOR && next.value == "*") {
      lexer_.consume();
      with_limits = true;
    }
    return parse_operatorname(with_limits);
  }

  // Phantom commands
  if (is_phantom_command(name)) {
    return parse_phantom(name);
  }

  // Color commands
  if (is_color_command(name)) {
    return parse_color_command(name);
  }

  // Cancel commands
  if (is_cancel_command(name)) {
    return parse_cancel(name);
  }

  // Boxed
  if (name == "boxed") {
    return parse_boxed();
  }

  // Substack
  if (name == "substack") {
    return parse_substack();
  }

  // Negation (\not)
  if (name == "not") {
    return parse_not();
  }

  // Extensible arrows
  if (is_extensible_arrow(name)) {
    return parse_ext_arrow(name);
  }

  // Symbol commands (Greek letters, operators, relations, etc.)
  return parse_symbol_command(name);
}


MathNodePtr MathParser::parse_sqrt() {
  auto node = std::make_unique<MathNode>(NodeType::SQRT);

  // Check for optional index
  auto index = parse_optional_argument();
  if (index) {
    node->add_child(std::move(index));
  }

  // Radicand
  auto radicand = parse_argument();
  if (radicand) {
    node->add_child(std::move(radicand));
  } else {
    node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  }

  return node;
}

MathNodePtr MathParser::parse_left_right() {
  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);

  // Left delimiter
  Token left = lexer_.next();
  if (left.type == TokenType::OPERATOR || left.type == TokenType::LPAREN ||
      left.type == TokenType::LBRACKET || left.type == TokenType::LBRACE ||
      left.type == TokenType::COMMAND) {
    node->left_delim = left.value;
  } else if (left.value == ".") {
    node->left_delim = "";  // Invisible delimiter
  } else {
    error("Expected delimiter after \\left");
    node->left_delim = "(";
  }

  // Content (until \right)
  auto row = std::make_unique<MathNode>(NodeType::ROW);
  while (!check(TokenType::END)) {
    if (check(TokenType::COMMAND) && lexer_.peek().value == "right") {
      break;
    }
    auto atom = parse_atom();
    if (atom) {
      atom = parse_scripts(std::move(atom));
      row->add_child(std::move(atom));
    } else {
      lexer_.consume();
    }
  }

  if (row->children.size() == 1) {
    node->add_child(std::move(row->children[0]));
  } else if (!row->children.empty()) {
    node->add_child(std::move(row));
  }

  // Consume \right
  if (check(TokenType::COMMAND) && lexer_.peek().value == "right") {
    lexer_.consume();
    Token right = lexer_.next();
    if (right.type == TokenType::OPERATOR || right.type == TokenType::RPAREN ||
        right.type == TokenType::RBRACKET || right.type == TokenType::RBRACE ||
        right.type == TokenType::COMMAND) {
      node->right_delim = right.value;
    } else if (right.value == ".") {
      node->right_delim = "";
    } else {
      node->right_delim = ")";
    }
  } else {
    error("Expected \\right");
    node->right_delim = ")";
  }

  return node;
}

MathNodePtr MathParser::parse_accent(const std::string& cmd, bool wide) {
  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = cmd;
  node->wide = wide;

  auto base = parse_argument();
  if (base) {
    node->add_child(std::move(base));
  }

  // For overbrace/underbrace, check for annotation (^{...} or _{...})
  if (cmd == "overbrace" || cmd == "underbrace") {
    if (cmd == "overbrace" && check(TokenType::SUPER)) {
      lexer_.consume();
      auto annotation = parse_argument();
      if (annotation) {
        node->add_child(std::move(annotation));
      }
    } else if (cmd == "underbrace" && check(TokenType::SUB)) {
      lexer_.consume();
      auto annotation = parse_argument();
      if (annotation) {
        node->add_child(std::move(annotation));
      }
    }
  }

  return node;
}

MathNodePtr MathParser::parse_text() {
  auto node = std::make_unique<MathNode>(NodeType::TEXT);

  if (!check(TokenType::LBRACE)) {
    error("Expected '{' after \\text");
    return node;
  }
  lexer_.consume();

  // Collect raw text until closing brace
  std::string text;
  int brace_depth = 1;

  while (!lexer_.at_end() && brace_depth > 0) {
    Token t = lexer_.next();
    if (t.type == TokenType::LBRACE) {
      brace_depth++;
      text += '{';
    } else if (t.type == TokenType::RBRACE) {
      brace_depth--;
      if (brace_depth > 0) text += '}';
    } else {
      text += t.value;
    }
  }

  node->value = text;
  return node;
}

MathNodePtr MathParser::parse_style_command(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::STYLED);
  node->command = cmd;
  node->style = style_command_to_style(cmd);

  auto content = parse_argument();
  if (content) {
    node->add_child(std::move(content));
  }

  return node;
}

MathNodePtr MathParser::parse_big_op(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::BIG_OP, cmd);
  node->limits = true;  // Big operators have limits by default in display mode

  // Check for \limits or \nolimits modifier
  if (check(TokenType::COMMAND)) {
    Token t = lexer_.peek();
    if (t.value == "limits") {
      lexer_.consume();
      node->limits_override = 1;  // Force limits
    } else if (t.value == "nolimits") {
      lexer_.consume();
      node->limits_override = -1;  // Force no limits
    }
  }

  return node;
}

MathNodePtr MathParser::parse_function(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::FUNCTION, cmd);
  return node;
}

MathNodePtr MathParser::parse_symbol_command(const std::string& cmd) {
  uint32_t cp = command_to_codepoint(cmd);
  if (cp != 0) {
    // Convert codepoint to UTF-8 string
    std::string utf8;
    if (cp < 0x80) {
      utf8 += static_cast<char>(cp);
    } else if (cp < 0x800) {
      utf8 += static_cast<char>(0xC0 | (cp >> 6));
      utf8 += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      utf8 += static_cast<char>(0xE0 | (cp >> 12));
      utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      utf8 += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      utf8 += static_cast<char>(0xF0 | (cp >> 18));
      utf8 += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      utf8 += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return std::make_unique<MathNode>(NodeType::SYMBOL, utf8);
  }

  // Unknown command - return as-is
  error("Unknown command: \\" + cmd);
  auto node = std::make_unique<MathNode>(NodeType::SYMBOL, "?" + cmd);
  return node;
}

MathNodePtr MathParser::parse_space_command(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::SPACE_NODE, cmd);
  return node;
}

MathNodePtr MathParser::parse_begin_environment() {
  // Parse environment name: {name}
  if (!check(TokenType::LBRACE)) {
    error("Expected '{' after \\begin");
    return nullptr;
  }
  lexer_.consume();

  // Collect environment name
  std::string env_name;
  while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
    Token t = lexer_.next();
    env_name += t.value;
  }

  if (!match(TokenType::RBRACE)) {
    error("Expected '}' after environment name");
  }

  // Check if it's a recognized environment
  bool is_matrix = is_matrix_environment(env_name);
  bool is_cases = is_cases_environment(env_name);
  bool is_aligned = is_alignment_environment(env_name);

  if (!is_matrix && !is_cases && !is_aligned) {
    error("Unknown environment: " + env_name);
    return nullptr;
  }

  // Create appropriate node type
  MathNodePtr node;
  if (is_aligned) {
    node = std::make_unique<MathNode>(NodeType::ALIGNED);
    node->matrix_type = env_name;  // Store environment name

    // For array environment, parse column specification
    if (env_name == "array" && check(TokenType::LBRACE)) {
      lexer_.consume();
      std::string col_spec;
      while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
        Token t = lexer_.next();
        col_spec += t.value;
      }
      match(TokenType::RBRACE);
      node->col_alignments = col_spec;
    }
  } else {
    node = std::make_unique<MathNode>(NodeType::MATRIX);
    node->matrix_type = env_name;
  }

  // Parse rows and cells
  // Each row is separated by \\, each cell by &
  std::vector<std::vector<MathNodePtr>> rows;
  std::vector<MathNodePtr> current_row;
  auto current_cell = std::make_unique<MathNode>(NodeType::ROW);

  while (!check(TokenType::END)) {
    Token t = lexer_.peek();

    // Check for \end
    if (t.type == TokenType::COMMAND && t.value == "end") {
      break;
    }

    // Check for row separator (double backslash)
    if (t.type == TokenType::COMMAND && t.value == "\\") {
      lexer_.consume();
      // Finish current cell
      if (current_cell->children.size() == 1) {
        current_row.push_back(std::move(current_cell->children[0]));
      } else if (!current_cell->children.empty()) {
        current_row.push_back(std::move(current_cell));
      } else {
        current_row.push_back(std::make_unique<MathNode>(NodeType::GROUP));
      }
      current_cell = std::make_unique<MathNode>(NodeType::ROW);
      // Finish current row
      rows.push_back(std::move(current_row));
      current_row.clear();
      continue;
    }

    // Check for cell separator &
    if (t.type == TokenType::OPERATOR && t.value == "&") {
      lexer_.consume();
      // Finish current cell
      if (current_cell->children.size() == 1) {
        current_row.push_back(std::move(current_cell->children[0]));
      } else if (!current_cell->children.empty()) {
        current_row.push_back(std::move(current_cell));
      } else {
        current_row.push_back(std::make_unique<MathNode>(NodeType::GROUP));
      }
      current_cell = std::make_unique<MathNode>(NodeType::ROW);
      continue;
    }

    // Parse atom for current cell
    auto atom = parse_atom();
    if (atom) {
      atom = parse_scripts(std::move(atom));
      current_cell->add_child(std::move(atom));
    } else {
      lexer_.consume();  // Skip unrecognized token
    }
  }

  // Finish last cell and row
  if (current_cell->children.size() == 1) {
    current_row.push_back(std::move(current_cell->children[0]));
  } else if (!current_cell->children.empty()) {
    current_row.push_back(std::move(current_cell));
  } else if (!current_row.empty()) {
    // Only add empty cell if row has other cells
    current_row.push_back(std::make_unique<MathNode>(NodeType::GROUP));
  }
  if (!current_row.empty()) {
    rows.push_back(std::move(current_row));
  }

  // Consume \end{name}
  if (check(TokenType::COMMAND) && lexer_.peek().value == "end") {
    lexer_.consume();
    if (check(TokenType::LBRACE)) {
      lexer_.consume();
      std::string end_name;
      while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
        Token t = lexer_.next();
        end_name += t.value;
      }
      match(TokenType::RBRACE);
      if (end_name != env_name) {
        error("Mismatched environment: \\begin{" + env_name + "} ... \\end{" + end_name + "}");
      }
    }
  } else {
    error("Expected \\end{" + env_name + "}");
  }

  // Store dimensions
  node->rows = static_cast<int>(rows.size());
  node->cols = 0;
  for (const auto& row : rows) {
    node->cols = std::max(node->cols, static_cast<int>(row.size()));
  }

  // Flatten cells into children (row-major order)
  for (auto& row : rows) {
    for (auto& cell : row) {
      node->add_child(std::move(cell));
    }
    // Pad short rows with empty cells
    for (int i = static_cast<int>(row.size()); i < node->cols; i++) {
      node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
    }
  }

  return node;
}

MathNodePtr MathParser::parse_stack_command(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::STACK);
  node->stack_type = cmd;

  if (cmd == "binom" || cmd == "tbinom" || cmd == "dbinom") {
    // \binom{n}{k} - two arguments
    auto top = parse_argument();
    auto bottom = parse_argument();

    if (top) node->add_child(std::move(top));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    if (bottom) node->add_child(std::move(bottom));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  } else if (cmd == "stackrel") {
    // \stackrel{above}{below} - small above, normal below
    auto above = parse_argument();
    auto below = parse_argument();

    if (above) node->add_child(std::move(above));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    if (below) node->add_child(std::move(below));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  } else if (cmd == "overset") {
    // \overset{above}{base} - small above, normal base
    auto above = parse_argument();
    auto base = parse_argument();

    if (above) node->add_child(std::move(above));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    if (base) node->add_child(std::move(base));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  } else if (cmd == "underset") {
    // \underset{below}{base} - small below, normal base
    auto below = parse_argument();
    auto base = parse_argument();

    if (below) node->add_child(std::move(below));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    if (base) node->add_child(std::move(base));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  } else if (cmd == "atop") {
    // \atop is an infix operator, but we handle it differently
    // Usually used as {a \atop b}
    // For now, treat as requiring two arguments
    auto top = parse_argument();
    auto bottom = parse_argument();

    if (top) node->add_child(std::move(top));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));

    if (bottom) node->add_child(std::move(bottom));
    else node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  }

  return node;
}

MathNodePtr MathParser::parse_operatorname(bool limits) {
  auto node = std::make_unique<MathNode>(NodeType::OPERATORNAME);
  node->limits = limits;

  if (!check(TokenType::LBRACE)) {
    error("Expected '{' after \\operatorname");
    return node;
  }
  lexer_.consume();

  // Collect the name
  std::string name;
  int brace_depth = 1;

  while (!lexer_.at_end() && brace_depth > 0) {
    Token t = lexer_.next();
    if (t.type == TokenType::LBRACE) {
      brace_depth++;
      name += '{';
    } else if (t.type == TokenType::RBRACE) {
      brace_depth--;
      if (brace_depth > 0) name += '}';
    } else {
      name += t.value;
    }
  }

  node->value = name;
  return node;
}

MathNodePtr MathParser::parse_phantom(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::PHANTOM);

  if (cmd == "phantom") {
    node->phantom_type = "full";
  } else if (cmd == "hphantom") {
    node->phantom_type = "h";
  } else if (cmd == "vphantom") {
    node->phantom_type = "v";
  }

  auto content = parse_argument();
  if (content) {
    node->add_child(std::move(content));
  }

  return node;
}

MathNodePtr MathParser::parse_color_command(const std::string& cmd) {
  if (cmd == "colorbox") {
    auto node = std::make_unique<MathNode>(NodeType::COLORBOX);

    // Parse color argument
    if (!check(TokenType::LBRACE)) {
      error("Expected '{' for color argument");
      return node;
    }
    lexer_.consume();

    std::string color;
    while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
      Token t = lexer_.next();
      color += t.value;
    }
    match(TokenType::RBRACE);
    node->color = color;

    // Parse content
    auto content = parse_argument();
    if (content) {
      node->add_child(std::move(content));
    }

    return node;
  }

  // \textcolor{color}{content} or \color{color}
  auto node = std::make_unique<MathNode>(NodeType::COLORED);

  // Parse color argument
  if (!check(TokenType::LBRACE)) {
    error("Expected '{' for color argument");
    return node;
  }
  lexer_.consume();

  std::string color;
  while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
    Token t = lexer_.next();
    color += t.value;
  }
  match(TokenType::RBRACE);
  node->color = color;

  if (cmd == "textcolor") {
    // \textcolor{color}{content} - second argument is content
    auto content = parse_argument();
    if (content) {
      node->add_child(std::move(content));
    }
  } else {
    // \color{color} - affects everything after it in the group
    // For simplicity, treat as affecting just the next argument
    auto content = parse_argument();
    if (content) {
      node->add_child(std::move(content));
    }
  }

  return node;
}

MathNodePtr MathParser::parse_cancel(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::CANCEL);
  node->cancel_type = cmd;

  if (cmd == "cancelto") {
    // \cancelto{value}{content} - two arguments
    auto value = parse_argument();
    if (value) {
      node->add_child(std::move(value));  // children[0] = value
    }
    auto content = parse_argument();
    if (content) {
      node->add_child(std::move(content));  // children[1] = content
    }
  } else {
    // \cancel{content}, \bcancel{content}, \xcancel{content} - one argument
    auto content = parse_argument();
    if (content) {
      node->add_child(std::move(content));
    }
  }

  return node;
}

MathNodePtr MathParser::parse_boxed() {
  auto node = std::make_unique<MathNode>(NodeType::BOXED);

  auto content = parse_argument();
  if (content) {
    node->add_child(std::move(content));
  }

  return node;
}

MathNodePtr MathParser::parse_substack() {
  auto node = std::make_unique<MathNode>(NodeType::SUBSTACK);

  if (!check(TokenType::LBRACE)) {
    error("Expected '{' after \\substack");
    return node;
  }
  lexer_.consume();

  // Parse rows separated by double-backslash
  auto current_row = std::make_unique<MathNode>(NodeType::ROW);

  while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
    Token t = lexer_.peek();

    // Check for row separator (double backslash)
    if (t.type == TokenType::COMMAND && t.value == "\\") {
      lexer_.consume();
      // Finish current row
      if (current_row->children.size() == 1) {
        node->add_child(std::move(current_row->children[0]));
      } else if (!current_row->children.empty()) {
        node->add_child(std::move(current_row));
      } else {
        node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
      }
      current_row = std::make_unique<MathNode>(NodeType::ROW);
      continue;
    }

    // Parse atom for current row
    auto atom = parse_atom();
    if (atom) {
      atom = parse_scripts(std::move(atom));
      current_row->add_child(std::move(atom));
    } else {
      lexer_.consume();
    }
  }

  // Add final row
  if (current_row->children.size() == 1) {
    node->add_child(std::move(current_row->children[0]));
  } else if (!current_row->children.empty()) {
    node->add_child(std::move(current_row));
  }

  match(TokenType::RBRACE);
  return node;
}

MathNodePtr MathParser::parse_not() {
  auto node = std::make_unique<MathNode>(NodeType::NEGATED);

  // Get the next token/atom to negate
  auto following = parse_atom();
  if (following) {
    node->add_child(std::move(following));
  }

  return node;
}

MathNodePtr MathParser::parse_ext_arrow(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::EXT_ARROW);
  node->command = cmd;

  // Extract arrow type from command name
  if (cmd == "xrightarrow") {
    node->arrow_type = "right";
  } else if (cmd == "xleftarrow") {
    node->arrow_type = "left";
  } else if (cmd == "xRightarrow") {
    node->arrow_type = "Right";
  } else if (cmd == "xLeftarrow") {
    node->arrow_type = "Left";
  } else if (cmd == "xleftrightarrow") {
    node->arrow_type = "leftright";
  } else if (cmd == "xLeftrightarrow") {
    node->arrow_type = "Leftright";
  } else if (cmd == "xhookleftarrow") {
    node->arrow_type = "hookleft";
  } else if (cmd == "xhookrightarrow") {
    node->arrow_type = "hookright";
  } else if (cmd == "xmapsto") {
    node->arrow_type = "mapsto";
  } else if (cmd == "xlongequal") {
    node->arrow_type = "equal";
  }

  // Optional subscript in brackets [below]
  auto below = parse_optional_argument();
  if (below) {
    node->add_child(std::move(below));
  } else {
    // Placeholder for empty below content
    node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  }

  // Required superscript in braces {above}
  auto above = parse_argument();
  if (above) {
    node->add_child(std::move(above));
  } else {
    node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
  }

  return node;
}

MathNodePtr MathParser::parse_big_delim(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::BIG_DELIM);
  node->command = cmd;
  node->delim_size = get_big_delim_size(cmd);

  // Get the delimiter character/command that follows
  Token t = lexer_.next();
  if (t.type == TokenType::OPERATOR || t.type == TokenType::LPAREN ||
      t.type == TokenType::RPAREN || t.type == TokenType::LBRACKET ||
      t.type == TokenType::RBRACKET || t.type == TokenType::COMMAND) {
    node->value = t.value;
  } else if (t.type == TokenType::LBRACE) {
    node->value = "{";
  } else if (t.type == TokenType::RBRACE) {
    node->value = "}";
  } else {
    error("Expected delimiter after \\" + cmd);
    node->value = "(";
  }

  return node;
}

MathNodePtr MathParser::parse_style_change(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::STYLE_CHANGE);
  node->command = cmd;

  if (cmd == "displaystyle") {
    node->display_style = MathDisplayStyle::DISPLAY;
  } else if (cmd == "textstyle") {
    node->display_style = MathDisplayStyle::TEXT;
  } else if (cmd == "scriptstyle") {
    node->display_style = MathDisplayStyle::SCRIPT;
  } else if (cmd == "scriptscriptstyle") {
    node->display_style = MathDisplayStyle::SCRIPTSCRIPT;
  }

  // The style change affects everything that follows in the current group
  // Parse the rest of the group/list as children
  auto content = parse_math_list();
  if (content) {
    node->add_child(std::move(content));
  }

  return node;
}

MathNodePtr MathParser::parse_smash() {
  auto node = std::make_unique<MathNode>(NodeType::SMASH);
  node->smash_type = "full";  // Default

  // Check for optional [t] or [b] argument
  if (check(TokenType::LBRACKET)) {
    lexer_.consume();
    Token t = lexer_.next();
    if (t.value == "t") {
      node->smash_type = "t";
    } else if (t.value == "b") {
      node->smash_type = "b";
    }
    if (!match(TokenType::RBRACKET)) {
      error("Expected ']' after smash option");
    }
  }

  // Parse content
  auto content = parse_argument();
  if (content) {
    node->add_child(std::move(content));
  }

  return node;
}

MathNodePtr MathParser::parse_mod_command(const std::string& cmd) {
  auto node = std::make_unique<MathNode>(NodeType::MOD);
  node->mod_type = cmd;

  if (cmd == "bmod") {
    // \bmod has no argument, it's just "mod" as a binary operator
    return node;
  }

  // \pmod{n} and \pod{n} take an argument
  auto content = parse_argument();
  if (content) {
    node->add_child(std::move(content));
  }

  return node;
}

MathNodePtr MathParser::parse_rule() {
  auto node = std::make_unique<MathNode>(NodeType::RULE);

  // Check for optional raise argument [raise]
  if (check(TokenType::LBRACKET)) {
    lexer_.consume();
    std::string raise_str;
    while (!check(TokenType::RBRACKET) && !check(TokenType::END)) {
      Token t = lexer_.next();
      raise_str += t.value;
    }
    match(TokenType::RBRACKET);
    // Parse dimension (simplified: assume "Xem" or "Xpt" format)
    try {
      node->rule_raise = std::stod(raise_str);
    } catch (...) {
      node->rule_raise = 0;
    }
  }

  // Parse width argument {width}
  if (check(TokenType::LBRACE)) {
    lexer_.consume();
    std::string width_str;
    while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
      Token t = lexer_.next();
      width_str += t.value;
    }
    match(TokenType::RBRACE);
    try {
      node->rule_width = std::stod(width_str);
    } catch (...) {
      node->rule_width = 0;
    }
  }

  // Parse height argument {height}
  if (check(TokenType::LBRACE)) {
    lexer_.consume();
    std::string height_str;
    while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
      Token t = lexer_.next();
      height_str += t.value;
    }
    match(TokenType::RBRACE);
    try {
      node->rule_height = std::stod(height_str);
    } catch (...) {
      node->rule_height = 0;
    }
  }

  return node;
}

MathNodePtr MathParser::parse_over_under_arrow(const std::string& cmd) {
  // Over/under arrows are handled as wide accents
  return parse_accent(cmd, true);
}

} // namespace math
