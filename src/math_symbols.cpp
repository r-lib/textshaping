#include "math_symbols.h"
#include "math_parser.h"  // For MathStyle enum

namespace math {

// =============================================================================
// Symbol Table Definitions
// =============================================================================

// Greek letters: command -> Unicode codepoint
const std::unordered_map<std::string, uint32_t> greek_letters = {
  // Lowercase
  {"alpha", 0x03B1}, {"beta", 0x03B2}, {"gamma", 0x03B3}, {"delta", 0x03B4},
  {"epsilon", 0x03F5}, {"varepsilon", 0x03B5}, {"zeta", 0x03B6}, {"eta", 0x03B7},
  {"theta", 0x03B8}, {"vartheta", 0x03D1}, {"iota", 0x03B9}, {"kappa", 0x03BA},
  {"lambda", 0x03BB}, {"mu", 0x03BC}, {"nu", 0x03BD}, {"xi", 0x03BE},
  {"pi", 0x03C0}, {"varpi", 0x03D6}, {"rho", 0x03C1}, {"varrho", 0x03F1},
  {"sigma", 0x03C3}, {"varsigma", 0x03C2}, {"tau", 0x03C4}, {"upsilon", 0x03C5},
  {"phi", 0x03D5}, {"varphi", 0x03C6}, {"chi", 0x03C7}, {"psi", 0x03C8},
  {"omega", 0x03C9},
  // Uppercase
  {"Gamma", 0x0393}, {"Delta", 0x0394}, {"Theta", 0x0398}, {"Lambda", 0x039B},
  {"Xi", 0x039E}, {"Pi", 0x03A0}, {"Sigma", 0x03A3}, {"Upsilon", 0x03A5},
  {"Phi", 0x03A6}, {"Psi", 0x03A8}, {"Omega", 0x03A9}
};

// Binary operators
const std::unordered_map<std::string, uint32_t> binary_operators = {
  {"pm", 0x00B1}, {"mp", 0x2213}, {"times", 0x00D7}, {"div", 0x00F7},
  {"cdot", 0x22C5}, {"ast", 0x2217}, {"star", 0x22C6}, {"circ", 0x2218},
  {"bullet", 0x2219}, {"cap", 0x2229}, {"cup", 0x222A}, {"vee", 0x2228},
  {"lor", 0x2228},  // Alias for vee
  {"wedge", 0x2227}, {"land", 0x2227},  // land is alias for wedge
  {"setminus", 0x2216}, {"oplus", 0x2295}, {"ominus", 0x2296},
  {"otimes", 0x2297}, {"oslash", 0x2298}, {"odot", 0x2299},
  // Additional binary operators
  {"ltimes", 0x22C9}, {"rtimes", 0x22CA}, {"boxplus", 0x229E}, {"boxminus", 0x229F},
  {"boxtimes", 0x22A0}
};

// Relations
const std::unordered_map<std::string, uint32_t> relations = {
  {"leq", 0x2264}, {"le", 0x2264}, {"geq", 0x2265}, {"ge", 0x2265},
  {"neq", 0x2260}, {"ne", 0x2260}, {"equiv", 0x2261}, {"approx", 0x2248},
  {"sim", 0x223C}, {"simeq", 0x2243}, {"cong", 0x2245}, {"propto", 0x221D},
  {"ll", 0x226A}, {"gg", 0x226B}, {"subset", 0x2282}, {"supset", 0x2283},
  {"subseteq", 0x2286}, {"supseteq", 0x2287}, {"in", 0x2208}, {"ni", 0x220B},
  {"notin", 0x2209}, {"perp", 0x22A5}, {"parallel", 0x2225}, {"mid", 0x2223},
  {"prec", 0x227A}, {"succ", 0x227B}, {"preceq", 0x2AAF}, {"succeq", 0x2AB0},
  // Additional relations
  {"leqslant", 0x2A7D}, {"geqslant", 0x2A7E}, {"lesssim", 0x2272}, {"gtrsim", 0x2273},
  {"vDash", 0x22A8}, {"Vdash", 0x22A9}, {"triangleq", 0x225C}
};

// Arrows
const std::unordered_map<std::string, uint32_t> arrows = {
  {"to", 0x2192}, {"rightarrow", 0x2192}, {"leftarrow", 0x2190},
  {"Rightarrow", 0x21D2}, {"Leftarrow", 0x21D0}, {"leftrightarrow", 0x2194},
  {"Leftrightarrow", 0x21D4}, {"mapsto", 0x21A6}, {"hookleftarrow", 0x21A9},
  {"hookrightarrow", 0x21AA}, {"uparrow", 0x2191}, {"downarrow", 0x2193},
  {"Uparrow", 0x21D1}, {"Downarrow", 0x21D3}, {"updownarrow", 0x2195},
  {"nearrow", 0x2197}, {"searrow", 0x2198}, {"swarrow", 0x2199}, {"nwarrow", 0x2196},
  // Additional arrows
  {"leftarrowtail", 0x21A2}, {"rightarrowtail", 0x21A3},
  {"twoheadleftarrow", 0x219E}, {"twoheadrightarrow", 0x21A0},
  {"curvearrowleft", 0x21B6}, {"curvearrowright", 0x21B7},
  // Negated arrows
  {"nleftarrow", 0x219A}, {"nrightarrow", 0x219B},
  {"nLeftarrow", 0x21CD}, {"nRightarrow", 0x21CF},
  {"nleftrightarrow", 0x21AE}, {"nLeftrightarrow", 0x21CE},
  // Long arrows
  {"longrightarrow", 0x27F6}, {"longleftarrow", 0x27F5},
  {"longleftrightarrow", 0x27F7}, {"Longrightarrow", 0x27F9},
  {"Longleftarrow", 0x27F8}, {"Longleftrightarrow", 0x27FA},
  {"longmapsto", 0x27FC}
};

// Big operators
const std::unordered_map<std::string, uint32_t> big_operators = {
  {"sum", 0x2211}, {"prod", 0x220F}, {"coprod", 0x2210}, {"int", 0x222B},
  {"iint", 0x222C}, {"iiint", 0x222D}, {"oint", 0x222E}, {"bigcup", 0x22C3},
  {"bigcap", 0x22C2}, {"bigvee", 0x22C1}, {"bigwedge", 0x22C0},
  {"bigoplus", 0x2A01}, {"bigotimes", 0x2A02}, {"bigodot", 0x2A00},
  {"biguplus", 0x2A04}, {"bigsqcup", 0x2A06}
};

// Accents (single character)
const std::unordered_map<std::string, uint32_t> accents = {
  {"hat", 0x0302}, {"check", 0x030C}, {"tilde", 0x0303}, {"acute", 0x0301},
  {"grave", 0x0300}, {"dot", 0x0307}, {"ddot", 0x0308}, {"breve", 0x0306},
  {"bar", 0x0304}, {"vec", 0x20D7}, {"mathring", 0x030A},
  // Additional dot accents
  {"dddot", 0x20DB}, {"ddddot", 0x20DC}
};

// Wide accents
const std::unordered_map<std::string, uint32_t> wide_accents = {
  {"widehat", 0x0302}, {"widetilde", 0x0303}, {"overline", 0x0305},
  {"underline", 0x0332}, {"overbrace", 0x23DE}, {"underbrace", 0x23DF},
  // Over/under arrows
  {"overrightarrow", 0x20D7}, {"overleftarrow", 0x20D6},
  {"overleftrightarrow", 0x20E1},
  {"underrightarrow", 0x20D7}, {"underleftarrow", 0x20D6},
  {"underleftrightarrow", 0x20E1}
};

// Function names
const std::unordered_set<std::string> functions = {
  "sin", "cos", "tan", "cot", "sec", "csc",
  "arcsin", "arccos", "arctan", "arccot",
  "sinh", "cosh", "tanh", "coth",
  "log", "ln", "lg", "exp",
  "lim", "liminf", "limsup",
  "max", "min", "sup", "inf",
  "det", "dim", "ker", "hom", "arg", "deg", "gcd",
  "Pr", "mod"
};

// Delimiters
const std::unordered_map<std::string, uint32_t> delimiters = {
  {"lbrace", '{'}, {"rbrace", '}'}, {"lbrack", '['}, {"rbrack", ']'},
  {"langle", 0x27E8}, {"rangle", 0x27E9}, {"lfloor", 0x230A}, {"rfloor", 0x230B},
  {"lceil", 0x2308}, {"rceil", 0x2309}, {"vert", '|'}, {"Vert", 0x2016},
  {"|", 0x2016}, {"backslash", '\\'}, {"/", '/'}
};

// Miscellaneous symbols
const std::unordered_map<std::string, uint32_t> misc_symbols = {
  {"infty", 0x221E}, {"partial", 0x2202}, {"nabla", 0x2207}, {"forall", 0x2200},
  {"exists", 0x2203}, {"nexists", 0x2204}, {"emptyset", 0x2205}, {"varnothing", 0x2205},
  {"aleph", 0x2135}, {"hbar", 0x210F}, {"ell", 0x2113}, {"wp", 0x2118},
  {"Re", 0x211C}, {"Im", 0x2111}, {"angle", 0x2220}, {"triangle", 0x25B3},
  {"prime", 0x2032}, {"ldots", 0x2026}, {"cdots", 0x22EF}, {"vdots", 0x22EE},
  {"ddots", 0x22F1}, {"neg", 0x00AC}, {"lnot", 0x00AC}, {"bot", 0x22A5},
  {"top", 0x22A4}, {"dagger", 0x2020}, {"ddagger", 0x2021}, {"diamond", 0x22C4},
  {"square", 0x25A1}, {"clubsuit", 0x2663}, {"diamondsuit", 0x2662},
  {"heartsuit", 0x2661}, {"spadesuit", 0x2660}, {"flat", 0x266D},
  {"natural", 0x266E}, {"sharp", 0x266F}, {"checkmark", 0x2713},
  {"maltese", 0x2720}, {"circledR", 0x00AE}, {"copyright", 0x00A9},
  {"pounds", 0x00A3}, {"yen", 0x00A5}, {"euro", 0x20AC},
  // Corner brackets
  {"ulcorner", 0x231C}, {"urcorner", 0x231D}, {"llcorner", 0x231E}, {"lrcorner", 0x231F},
  // Special symbols
  {"imath", 0x0131}, {"jmath", 0x0237}, {"complement", 0x2201}, {"eth", 0x00F0},
  {"hslash", 0x210F}, {"mho", 0x2127},
  // Semantic dots (map to appropriate ellipsis)
  {"dotsb", 0x22EF}, {"dotsc", 0x2026}, {"dotsi", 0x22EF}, {"dotsm", 0x22EF}, {"dotso", 0x2026}
};

// Space commands: command -> em units (1em = 18mu in TeX)
const std::unordered_map<std::string, double> space_commands = {
  {"!", -3.0/18.0},   // negative thin space
  {",", 3.0/18.0},    // thin space
  {":", 4.0/18.0},    // medium space
  {";", 5.0/18.0},    // thick space
  {" ", 1.0},         // normal space (escaped space)
  {"quad", 1.0},      // 1em
  {"qquad", 2.0}      // 2em
};

// Style commands: command -> MathStyle
const std::unordered_map<std::string, MathStyle> style_commands = {
  {"mathbf", MathStyle::BOLD},
  {"mathit", MathStyle::ITALIC},
  {"mathrm", MathStyle::ROMAN},
  {"mathsf", MathStyle::SANS},
  {"mathtt", MathStyle::MONO},
  {"mathcal", MathStyle::SCRIPT},
  {"mathscr", MathStyle::SCRIPT},
  {"mathfrak", MathStyle::FRAKTUR},
  {"mathbb", MathStyle::DOUBLE_STRUCK},
  {"boldsymbol", MathStyle::BOLD_ITALIC},
  {"bm", MathStyle::BOLD_ITALIC},
  {"textbf", MathStyle::BOLD},
  {"textit", MathStyle::ITALIC},
  {"textrm", MathStyle::ROMAN},
  {"textsf", MathStyle::SANS},
  {"texttt", MathStyle::MONO}
};

// Matrix environment names
const std::unordered_set<std::string> matrix_environments = {
  "matrix", "pmatrix", "bmatrix", "vmatrix", "Vmatrix", "Bmatrix", "smallmatrix"
};

// Cases environment names
const std::unordered_set<std::string> cases_environments = {
  "cases", "rcases", "dcases", "drcases"
};

// Alignment environment names
const std::unordered_set<std::string> alignment_environments = {
  "aligned", "gathered", "array", "split"
};

// Big delimiter commands - map to size level (1=\big, 2=\Big, 3=\bigg, 4=\Bigg)
const std::unordered_map<std::string, int> big_delim_commands = {
  {"big", 1}, {"Big", 2}, {"bigg", 3}, {"Bigg", 4},
  {"bigl", 1}, {"Bigl", 2}, {"biggl", 3}, {"Biggl", 4},
  {"bigr", 1}, {"Bigr", 2}, {"biggr", 3}, {"Biggr", 4},
  {"bigm", 1}, {"Bigm", 2}, {"biggm", 3}, {"Biggm", 4}
};

// Display style commands
const std::unordered_set<std::string> display_style_commands = {
  "displaystyle", "textstyle", "scriptstyle", "scriptscriptstyle"
};

// Fraction variants
const std::unordered_set<std::string> fraction_variants = {
  "dfrac", "tfrac", "cfrac"
};

// Over/under arrow commands
const std::unordered_set<std::string> over_under_arrows = {
  "overrightarrow", "overleftarrow", "overleftrightarrow",
  "underrightarrow", "underleftarrow", "underleftrightarrow"
};

// Mod commands
const std::unordered_set<std::string> mod_commands = {
  "pmod", "bmod", "pod"
};

// Stack commands
const std::unordered_set<std::string> stack_commands = {
  "binom", "tbinom", "dbinom", "atop", "stackrel", "overset", "underset"
};

// Phantom commands
const std::unordered_set<std::string> phantom_commands = {
  "phantom", "hphantom", "vphantom"
};

// Color commands
const std::unordered_set<std::string> color_commands = {
  "textcolor", "color", "colorbox"
};

// Cancel commands
const std::unordered_set<std::string> cancel_commands = {
  "cancel", "bcancel", "xcancel", "cancelto"
};

// Extensible arrow commands
const std::unordered_set<std::string> extensible_arrows = {
  "xrightarrow", "xleftarrow", "xRightarrow", "xLeftarrow",
  "xleftrightarrow", "xLeftrightarrow", "xhookleftarrow", "xhookrightarrow",
  "xmapsto", "xlongequal"
};

// =============================================================================
// Classification functions
// =============================================================================

bool is_greek_letter(const std::string& cmd) {
  return greek_letters.count(cmd) > 0;
}

bool is_binary_operator(const std::string& cmd) {
  return binary_operators.count(cmd) > 0;
}

bool is_relation(const std::string& cmd) {
  return relations.count(cmd) > 0;
}

bool is_arrow(const std::string& cmd) {
  return arrows.count(cmd) > 0;
}

bool is_big_operator(const std::string& cmd) {
  return big_operators.count(cmd) > 0;
}

bool is_accent_command(const std::string& cmd) {
  return accents.count(cmd) > 0;
}

bool is_wide_accent_command(const std::string& cmd) {
  return wide_accents.count(cmd) > 0;
}

bool is_function_command(const std::string& cmd) {
  return functions.count(cmd) > 0;
}

bool is_delimiter(const std::string& cmd) {
  return delimiters.count(cmd) > 0;
}

bool is_space_command(const std::string& cmd) {
  return space_commands.count(cmd) > 0;
}

bool is_style_command(const std::string& cmd) {
  return style_commands.count(cmd) > 0;
}

MathStyle style_command_to_style(const std::string& cmd) {
  auto it = style_commands.find(cmd);
  if (it != style_commands.end()) return it->second;
  return MathStyle::NORMAL;
}

bool is_matrix_environment(const std::string& name) {
  return matrix_environments.count(name) > 0;
}

bool is_cases_environment(const std::string& name) {
  return cases_environments.count(name) > 0;
}

bool is_alignment_environment(const std::string& name) {
  return alignment_environments.count(name) > 0;
}

bool is_big_delim_command(const std::string& cmd) {
  return big_delim_commands.count(cmd) > 0;
}

int get_big_delim_size(const std::string& cmd) {
  auto it = big_delim_commands.find(cmd);
  if (it != big_delim_commands.end()) return it->second;
  return 1;
}

bool is_display_style_command(const std::string& cmd) {
  return display_style_commands.count(cmd) > 0;
}

bool is_fraction_variant(const std::string& cmd) {
  return fraction_variants.count(cmd) > 0;
}

bool is_over_under_arrow(const std::string& cmd) {
  return over_under_arrows.count(cmd) > 0;
}

bool is_mod_command(const std::string& cmd) {
  return mod_commands.count(cmd) > 0;
}

bool is_stack_command(const std::string& cmd) {
  return stack_commands.count(cmd) > 0;
}

bool is_phantom_command(const std::string& cmd) {
  return phantom_commands.count(cmd) > 0;
}

bool is_color_command(const std::string& cmd) {
  return color_commands.count(cmd) > 0;
}

bool is_cancel_command(const std::string& cmd) {
  return cancel_commands.count(cmd) > 0;
}

bool is_extensible_arrow(const std::string& cmd) {
  return extensible_arrows.count(cmd) > 0;
}

uint32_t command_to_codepoint(const std::string& cmd) {
  // Check each table
  auto it = greek_letters.find(cmd);
  if (it != greek_letters.end()) return it->second;

  it = binary_operators.find(cmd);
  if (it != binary_operators.end()) return it->second;

  it = relations.find(cmd);
  if (it != relations.end()) return it->second;

  it = arrows.find(cmd);
  if (it != arrows.end()) return it->second;

  it = big_operators.find(cmd);
  if (it != big_operators.end()) return it->second;

  it = accents.find(cmd);
  if (it != accents.end()) return it->second;

  it = wide_accents.find(cmd);
  if (it != wide_accents.end()) return it->second;

  it = delimiters.find(cmd);
  if (it != delimiters.end()) return it->second;

  it = misc_symbols.find(cmd);
  if (it != misc_symbols.end()) return it->second;

  return 0;
}

double get_space_amount(const std::string& cmd) {
  auto it = space_commands.find(cmd);
  if (it != space_commands.end()) return it->second;
  return 0.0;
}

uint32_t get_delimiter_codepoint(const std::string& delim) {
  // Null delimiter
  if (delim.empty() || delim == ".") return 0;

  // Single character delimiters
  if (delim == "(") return '(';
  if (delim == ")") return ')';
  if (delim == "[") return '[';
  if (delim == "]") return ']';
  if (delim == "{") return '{';
  if (delim == "}") return '}';
  if (delim == "|") return '|';
  if (delim == "/") return '/';

  // Command names - check the delimiters table
  auto it = delimiters.find(delim);
  if (it != delimiters.end()) return it->second;

  // Fallback: if single character, return its codepoint
  if (delim.size() == 1) {
    return static_cast<uint32_t>(delim[0]);
  }

  return 0;
}

} // namespace math
