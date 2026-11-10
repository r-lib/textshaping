#include "typst_parser.h"
#include "math_symbols.h"
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace math {

// std::stoi/std::stod throw on out-of-range input; the lexer can produce
// arbitrarily long digit sequences, so convert with a fallback instead
static double safe_stod(const std::string& s, double fallback = 0.0) {
  try {
    return std::stod(s);
  } catch (...) {
    return fallback;
  }
}

static int safe_stoi(const std::string& s, int fallback = 0) {
  try {
    return std::stoi(s);
  } catch (...) {
    return fallback;
  }
}

// =============================================================================
// Typst symbol tables
// =============================================================================

// Typst function names that map to special constructs
static const std::unordered_set<std::string> typst_functions = {
  // Generic accent
  "accent",
  // Fractions and stacks
  "frac", "binom",
  // Roots
  "sqrt", "root",
  // Delimiters
  "lr", "abs", "norm", "floor", "ceil", "round",
  // Mid delimiter (for set-builder notation)
  "mid",
  // Matrices
  "mat", "pmat", "bmat", "vmat", "Vmat", "Bmat",
  // Cases
  "cases",
  // Vectors/matrices helper
  "vec",
  // Accents
  "hat", "tilde", "dot", "ddot", "dddot", "ddddot", "bar", "vec",
  "acute", "grave", "breve", "check", "mathring",
  // Wide accents
  "widehat", "widetilde", "overbrace", "underbrace", "overline", "underline",
  "overrightarrow", "overleftarrow", "overleftrightarrow",
  "underrightarrow", "underleftarrow", "underleftrightarrow",
  // Under/over bracket variants
  "overbracket", "underbracket", "overparen", "underparen", "overshell", "undershell",
  // Styling
  "bold", "italic", "upright", "sans", "mono", "cal", "frak", "bb",
  // Additional styles
  "serif", "scr",
  // Display styles
  "display", "inline", "script", "sscript",
  // Cancel/hide
  "cancel", "strike", "hide",
  // Operators
  "op", "limits", "scripts",
  // Attachment
  "attach",
  // Spacing functions
  "h", "v",
  // Explicit primes
  "primes",
  // Stretch and class
  "stretch", "class"
};

// Typst accent functions
static const std::unordered_map<std::string, std::string> accent_functions = {
  {"hat", "hat"}, {"tilde", "tilde"}, {"dot", "dot"}, {"ddot", "ddot"},
  {"dddot", "dddot"}, {"ddddot", "ddddot"}, {"bar", "bar"},
  {"acute", "acute"}, {"grave", "grave"}, {"breve", "breve"},
  {"check", "check"}, {"mathring", "mathring"}
};

// Wide accent functions
static const std::unordered_map<std::string, std::string> wide_accent_functions = {
  {"widehat", "widehat"}, {"widetilde", "widetilde"},
  {"overbrace", "overbrace"}, {"underbrace", "underbrace"},
  {"overline", "overline"}, {"underline", "underline"},
  {"overrightarrow", "overrightarrow"}, {"overleftarrow", "overleftarrow"},
  {"overleftrightarrow", "overleftrightarrow"},
  {"underrightarrow", "underrightarrow"}, {"underleftarrow", "underleftarrow"},
  {"underleftrightarrow", "underleftrightarrow"},
  // Under/over bracket variants
  {"overbracket", "overbracket"}, {"underbracket", "underbracket"},
  {"overparen", "overparen"}, {"underparen", "underparen"},
  {"overshell", "overshell"}, {"undershell", "undershell"}
};

// Typst style functions
static const std::unordered_map<std::string, MathStyle> style_functions = {
  {"bold", MathStyle::BOLD},
  {"italic", MathStyle::ITALIC},
  {"upright", MathStyle::ROMAN},
  {"sans", MathStyle::SANS},
  {"mono", MathStyle::MONO},
  {"cal", MathStyle::SCRIPT},
  {"scr", MathStyle::SCRIPT},  // Typst alias for script/calligraphy
  {"frak", MathStyle::FRAKTUR},
  {"bb", MathStyle::DOUBLE_STRUCK},
  {"serif", MathStyle::ROMAN}  // Typst serif is upright/roman
};

// Typst big operators
static const std::unordered_set<std::string> typst_big_operators = {
  "sum", "prod", "product", "integral", "int", "iint", "iiint", "oint",
  "bigcup", "bigcap", "bigvee", "bigwedge", "bigoplus", "bigotimes",
  "bigodot", "biguplus", "bigsqcup", "coprod"
};

// Typst math functions (sin, cos, etc.)
static const std::unordered_set<std::string> typst_math_functions = {
  "sin", "cos", "tan", "cot", "sec", "csc",
  "arcsin", "arccos", "arctan", "arccot",
  "sinh", "cosh", "tanh", "coth",
  "log", "ln", "lg", "exp",
  "lim", "liminf", "limsup",
  "max", "min", "sup", "inf",
  "det", "dim", "ker", "hom", "arg", "deg", "gcd",
  "Pr", "mod"
};

// Typst spacing values (in em)
static const std::unordered_map<std::string, double> typst_spacing = {
  {"thin", 3.0/18.0},
  {"med", 4.0/18.0},
  {"thick", 5.0/18.0},
  {"quad", 1.0},
  {"wide", 2.0}
};

// =============================================================================
// Typst shorthand sequences: multi-character -> Unicode codepoint
// Stored as vector of pairs, ordered by length (longer first) for matching
// =============================================================================
static const std::vector<std::pair<std::string, uint32_t>> typst_shorthands = {
  // 4 character sequences
  {"<==>", 0x27FA},  // ⟺ long double left-right arrow
  {"<-->", 0x27F7},  // ⟷ long left-right arrow

  // 3 character sequences
  {"-->", 0x27F6},   // ⟶ long right arrow
  {"|->", 0x21A6},   // ↦ maps to
  {"|=>", 0x2907},   // ⤇ maps to with double stroke
  {"==>", 0x27F9},   // ⟹ long double right arrow
  {"~~>", 0x27FF},   // ⟿ long squiggly arrow
  {">->", 0x21A3},   // ↣ right arrow with tail
  {"->>", 0x21A0},   // ↠ two-headed right arrow
  {"<--", 0x27F5},   // ⟵ long left arrow
  {"<==", 0x27F8},   // ⟸ long double left arrow
  {"<~~", 0x2B33},   // ⬳ long squiggly left arrow
  {"<-<", 0x21A2},   // ↢ left arrow with tail
  {"<<-", 0x219E},   // ↞ two-headed left arrow
  {"<=>", 0x21D4},   // ⇔ double left-right arrow
  {"::=", 0x2A74},   // ⩴ double colon equal
  {">>>", 0x22D9},   // ⋙ very much greater than
  {"<<<", 0x22D8},   // ⋘ very much less than
  {"[|", 0x27E6},    // ⟦ left double bracket
  {"|]", 0x27E7},    // ⟧ right double bracket
  {"~>", 0x21DD},    // ⇝ squiggly right arrow
  {"<~", 0x21DC},    // ⇜ squiggly left arrow

  // 2 character sequences
  {"->", 0x2192},    // → right arrow
  {"=>", 0x21D2},    // ⇒ double right arrow (implies)
  {"<-", 0x2190},    // ← left arrow
  {":=", 0x2254},    // ≔ colon equal
  {"=:", 0x2255},    // ≕ equal colon
  {"!=", 0x2260},    // ≠ not equal
  {">>", 0x226B},    // ≫ much greater than
  {">=", 0x2265},    // ≥ greater than or equal
  {"<<", 0x226A},    // ≪ much less than
  {"<=", 0x2264},    // ≤ less than or equal
  {"||", 0x2016},    // ‖ double vertical bar

  // 1 character shorthands (must be last)
  {"*", 0x2217},     // ∗ asterisk operator
  {"-", 0x2212},     // − minus sign
  {"~", 0x223C},     // ∼ tilde operator
};

// =============================================================================
// Typst symbols: both base names and dotted variants -> Unicode codepoint
// =============================================================================
static const std::unordered_map<std::string, uint32_t> typst_dotted_symbols = {
  // === BASE SYMBOLS (without dots) ===
  // These are Typst-specific names that differ from LaTeX or aren't in shared tables

  // Basic operators
  {"plus", 0x002B},       // +
  {"minus", 0x2212},      // − (math minus, not hyphen)
  {"times", 0x00D7},      // ×
  {"div", 0x00F7},        // ÷
  {"ast", 0x2217},        // ∗
  {"star", 0x22C6},       // ⋆
  {"compose", 0x2218},    // ∘

  // Relations
  {"eq", 0x003D},         // =
  {"lt", 0x003C},         // <
  {"gt", 0x003E},         // >
  {"approx", 0x2248},     // ≈
  {"equiv", 0x2261},      // ≡
  {"sim", 0x223C},        // ∼
  {"cong", 0x2245},       // ≅
  {"prec", 0x227A},       // ≺
  {"succ", 0x227B},       // ≻
  {"perp", 0x22A5},       // ⊥
  {"parallel", 0x2225},   // ∥
  {"mid", 0x2223},        // ∣

  // Logic
  {"and", 0x2227},        // ∧
  {"or", 0x2228},         // ∨
  {"not", 0x00AC},        // ¬
  {"forall", 0x2200},     // ∀
  {"exists", 0x2203},     // ∃
  {"top", 0x22A4},        // ⊤
  {"bot", 0x22A5},        // ⊥
  {"tack", 0x22A2},       // ⊢
  {"models", 0x22A8},     // ⊨

  // Sets
  {"in", 0x2208},         // ∈
  {"subset", 0x2282},     // ⊂
  {"supset", 0x2283},     // ⊃
  {"union", 0x222A},      // ∪
  {"inter", 0x2229},      // ∩ (Typst name for intersection)
  {"sect", 0x2229},       // ∩ (alternate name)
  {"emptyset", 0x2205},   // ∅
  {"nothing", 0x2205},    // ∅ (Typst alias)

  // Calculus
  {"partial", 0x2202},    // ∂
  {"nabla", 0x2207},      // ∇
  {"gradient", 0x2207},   // ∇ (Typst alias)
  {"infinity", 0x221E},   // ∞ (Typst name, LaTeX uses infty)

  // Arrows (base)
  {"arrow", 0x2192},      // → (default right arrow)

  // Geometry
  {"angle", 0x2220},      // ∠
  {"triangle", 0x25B3},   // △
  {"square", 0x25A1},     // □
  {"circle", 0x25CB},     // ○
  {"diamond", 0x25C7},    // ◇
  {"bullet", 0x2022},     // •
  {"lozenge", 0x25CA},    // ◊
  {"bowtie", 0x22C8},     // ⋈

  // Miscellaneous
  {"degree", 0x00B0},     // °
  {"prime", 0x2032},      // ′
  {"dprime", 0x2033},     // ″
  {"tprime", 0x2034},     // ‴
  {"qprime", 0x2057},     // ⁗
  {"dagger", 0x2020},     // †
  {"ddagger", 0x2021},    // ‡
  {"section", 0x00A7},    // §
  {"pilcrow", 0x00B6},    // ¶
  {"copyright", 0x00A9},  // ©
  {"trademark", 0x2122},  // ™
  {"registered", 0x00AE}, // ®
  {"servicemark", 0x2120},// ℠
  {"ellipsis", 0x2026},   // …
  {"dots", 0x2026},       // … (base dots)

  // Greek letter synonyms (standard names already in greek_letters table)
  // These handle any Typst-specific naming

  // Harpoons (base)
  {"harpoon", 0x21C0},    // ⇀ (default: right barb up)
  {"harpoons", 0x21CB},   // ⇋ (default: left-right)

  // Turnstiles (base)
  {"forces", 0x22A9},     // ⊩
  {"dashv", 0x22A3},      // ⊣

  // Blackboard bold letters (for direct use like bb(R))
  // These are handled by style functions

  // Planck constant
  {"planck", 0x210E},     // ℎ

  // Aleph and other Hebrew letters
  {"aleph", 0x2135},      // ℵ
  {"beth", 0x2136},       // ℶ
  {"gimel", 0x2137},      // ℷ
  {"daleth", 0x2138},     // ℸ

  // Script letters
  {"ell", 0x2113},        // ℓ
  {"wp", 0x2118},         // ℘

  // Real/imaginary
  {"Re", 0x211C},         // ℜ
  {"Im", 0x2111},         // ℑ

  // More operators
  {"oplus", 0x2295},      // ⊕
  {"ominus", 0x2296},     // ⊖
  {"otimes", 0x2297},     // ⊗
  {"oslash", 0x2298},     // ⊘
  {"odot", 0x2299},       // ⊙

  // Size comparison
  {"ll", 0x226A},         // ≪
  {"gg", 0x226B},         // ≫

  // Other common symbols
  {"colon", 0x003A},      // :
  {"therefore", 0x2234},  // ∴
  {"because", 0x2235},    // ∵
  {"qed", 0x220E},        // ∎
  {"space", 0x0020},      // explicit space

  // === ARROWS ===
  // Basic arrows
  {"arrow.r", 0x2192}, {"arrow.l", 0x2190}, {"arrow.t", 0x2191}, {"arrow.b", 0x2193},
  {"arrow.lr", 0x2194}, {"arrow.tb", 0x2195},
  // Long arrows
  {"arrow.r.long", 0x27F6}, {"arrow.l.long", 0x27F5}, {"arrow.lr.long", 0x27F7},
  {"arrow.long.r", 0x27F6}, {"arrow.long.l", 0x27F5}, {"arrow.long.lr", 0x27F7},
  // Double arrows (implies)
  {"arrow.r.double", 0x21D2}, {"arrow.l.double", 0x21D0}, {"arrow.lr.double", 0x21D4},
  {"arrow.t.double", 0x21D1}, {"arrow.b.double", 0x21D3}, {"arrow.tb.double", 0x21D5},
  {"arrow.double.r", 0x21D2}, {"arrow.double.l", 0x21D0}, {"arrow.double.lr", 0x21D4},
  {"arrow.double.t", 0x21D1}, {"arrow.double.b", 0x21D3}, {"arrow.double.tb", 0x21D5},
  // Long double arrows
  {"arrow.r.long.double", 0x27F9}, {"arrow.l.long.double", 0x27F8}, {"arrow.lr.long.double", 0x27FA},
  {"arrow.double.long.r", 0x27F9}, {"arrow.double.long.l", 0x27F8}, {"arrow.double.long.lr", 0x27FA},
  // Triple arrows
  {"arrow.r.triple", 0x21DB}, {"arrow.l.triple", 0x21DA},
  {"arrow.triple.r", 0x21DB}, {"arrow.triple.l", 0x21DA},
  // Hooked arrows
  {"arrow.r.hook", 0x21AA}, {"arrow.l.hook", 0x21A9},
  {"arrow.hook.r", 0x21AA}, {"arrow.hook.l", 0x21A9},
  // Two-headed arrows
  {"arrow.r.twohead", 0x21A0}, {"arrow.l.twohead", 0x219E},
  {"arrow.twohead.r", 0x21A0}, {"arrow.twohead.l", 0x219E},
  // Tailed arrows
  {"arrow.r.tail", 0x21A3}, {"arrow.l.tail", 0x21A2},
  {"arrow.tail.r", 0x21A3}, {"arrow.tail.l", 0x21A2},
  // Bar arrows (mapsto)
  {"arrow.r.bar", 0x21A6}, {"arrow.l.bar", 0x21A4}, {"arrow.t.bar", 0x21A5}, {"arrow.b.bar", 0x21A7},
  {"arrow.bar.r", 0x21A6}, {"arrow.bar.l", 0x21A4},
  // Curved arrows
  {"arrow.r.curve", 0x21B7}, {"arrow.l.curve", 0x21B6},
  {"arrow.curve.r", 0x21B7}, {"arrow.curve.l", 0x21B6},
  // Loop arrows
  {"arrow.r.loop", 0x21AC}, {"arrow.l.loop", 0x21AB},
  {"arrow.loop.r", 0x21AC}, {"arrow.loop.l", 0x21AB},
  // Squiggly arrows
  {"arrow.r.squiggly", 0x21DD}, {"arrow.l.squiggly", 0x21DC},
  {"arrow.squiggly.r", 0x21DD}, {"arrow.squiggly.l", 0x21DC},
  // Diagonal arrows
  {"arrow.tr", 0x2197}, {"arrow.tl", 0x2196}, {"arrow.br", 0x2198}, {"arrow.bl", 0x2199},
  // Negated arrows
  {"arrow.r.not", 0x219B}, {"arrow.l.not", 0x219A}, {"arrow.lr.not", 0x21AE},
  {"arrow.r.double.not", 0x21CF}, {"arrow.l.double.not", 0x21CD}, {"arrow.lr.double.not", 0x21CE},
  // Stroked arrows
  {"arrow.r.stroke", 0x219B}, {"arrow.l.stroke", 0x219A},
  // Dashed arrows
  {"arrow.r.dashed", 0x21E2}, {"arrow.l.dashed", 0x21E0},
  {"arrow.dashed.r", 0x21E2}, {"arrow.dashed.l", 0x21E0},
  // Wave arrows
  {"arrow.r.wave", 0x219D}, {"arrow.l.wave", 0x219C},
  {"arrow.wave.r", 0x219D}, {"arrow.wave.l", 0x219C},

  // === HARPOONS ===
  {"harpoon.rt", 0x21C0}, {"harpoon.rb", 0x21C1}, {"harpoon.lt", 0x21BC}, {"harpoon.lb", 0x21BD},
  {"harpoon.tl", 0x21BF}, {"harpoon.tr", 0x21BE}, {"harpoon.bl", 0x21C3}, {"harpoon.br", 0x21C2},
  {"harpoon.lr", 0x21CB}, {"harpoon.rl", 0x21CC},
  {"harpoons.ltrb", 0x21CB}, {"harpoons.rtlb", 0x21CC},

  // === GREEK VARIANTS ===
  // Typst conventions (opposite of LaTeX): phi/epsilon are the small letters,
  // the .alt variants are the lunate/symbol forms
  {"epsilon.alt", 0x03F5}, {"varepsilon", 0x03B5},  // ϵ lunate epsilon symbol
  {"phi.alt", 0x03D5}, {"varphi", 0x03C6},  // ϕ phi symbol
  {"kappa.alt", 0x03F0}, {"varkappa", 0x03F0},  // ϰ
  {"theta.alt", 0x03D1}, {"vartheta", 0x03D1},  // ϑ
  {"pi.alt", 0x03D6}, {"varpi", 0x03D6},  // ϖ
  {"rho.alt", 0x03F1}, {"varrho", 0x03F1},  // ϱ
  {"sigma.alt", 0x03C2}, {"varsigma", 0x03C2},  // ς (final sigma)

  // === DELIMITERS/BRACKETS ===
  {"paren.l", 0x0028}, {"paren.r", 0x0029},
  {"bracket.l", 0x005B}, {"bracket.r", 0x005D},
  {"brace.l", 0x007B}, {"brace.r", 0x007D},
  {"angle.l", 0x27E8}, {"angle.r", 0x27E9},
  {"angle.l.double", 0x27EA}, {"angle.r.double", 0x27EB},
  {"angle.right", 0x221F}, {"angle.arc", 0x2221}, {"angle.spheric", 0x2222},
  {"floor.l", 0x230A}, {"floor.r", 0x230B},
  {"ceil.l", 0x2308}, {"ceil.r", 0x2309},
  {"bracket.l.double", 0x27E6}, {"bracket.r.double", 0x27E7},
  {"turtle.l", 0x27EC}, {"turtle.r", 0x27ED},
  {"paren.t", 0x23DC}, {"paren.b", 0x23DD},
  {"brace.t", 0x23DE}, {"brace.b", 0x23DF},
  {"bracket.t", 0x23B4}, {"bracket.b", 0x23B5},
  {"shell.l", 0x27EE}, {"shell.r", 0x27EF},

  // === RELATIONS ===
  // Equality variants
  {"eq.not", 0x2260}, {"eq.def", 0x225D}, {"eq.quest", 0x225F}, {"eq.delta", 0x225C},
  {"eq.triple", 0x2261}, {"eq.triple.not", 0x2262},
  {"eq.colon", 0x2254}, {"eq.colon.rev", 0x2255},  // := and =:
  {"eq.star", 0x225B}, {"eq.circ", 0x2257}, {"eq.m", 0x225E},
  {"eq.prec", 0x227C}, {"eq.succ", 0x227D},

  // Less/greater variants
  {"lt.eq", 0x2264}, {"lt.eq.slant", 0x2A7D}, {"lt.eq.not", 0x2270},
  {"lt.not", 0x226E}, {"lt.lt", 0x226A}, {"lt.triple", 0x22D8},
  {"lt.curly", 0x227A}, {"lt.curly.eq", 0x227C}, {"lt.curly.not", 0x2280},
  {"lt.tilde", 0x2272}, {"lt.tilde.not", 0x2274},
  {"lt.approx", 0x2A85}, {"lt.gt", 0x2276}, {"lt.gt.not", 0x2278},
  {"gt.eq", 0x2265}, {"gt.eq.slant", 0x2A7E}, {"gt.eq.not", 0x2271},
  {"gt.not", 0x226F}, {"gt.gt", 0x226B}, {"gt.triple", 0x22D9},
  {"gt.curly", 0x227B}, {"gt.curly.eq", 0x227D}, {"gt.curly.not", 0x2281},
  {"gt.tilde", 0x2273}, {"gt.tilde.not", 0x2275},
  {"gt.approx", 0x2A86}, {"gt.lt", 0x2277}, {"gt.lt.not", 0x2279},

  // Approximation variants
  {"approx.eq", 0x224A}, {"approx.not", 0x2249},
  {"sim.eq", 0x2243}, {"sim.not", 0x2241}, {"sim.eq.not", 0x2244},
  {"tilde.op", 0x223C}, {"tilde.basic", 0x007E},
  {"tilde.eq", 0x2243}, {"tilde.eq.not", 0x2244},
  {"tilde.eq.rev", 0x22CD}, {"tilde.triple", 0x224B},
  {"tilde.not", 0x2241}, {"tilde.rev", 0x223D},
  {"cong.not", 0x2247},

  // Precedence
  {"prec", 0x227A}, {"prec.eq", 0x2AAF}, {"prec.not", 0x2280}, {"prec.eq.not", 0x22E0},
  {"prec.curly", 0x227C}, {"prec.curly.eq", 0x227E},
  {"prec.tilde", 0x227F}, {"prec.approx", 0x2AB7},
  {"succ", 0x227B}, {"succ.eq", 0x2AB0}, {"succ.not", 0x2281}, {"succ.eq.not", 0x22E1},
  {"succ.curly", 0x227D}, {"succ.curly.eq", 0x227F},
  {"succ.tilde", 0x2273}, {"succ.approx", 0x2AB8},

  // Subset/superset
  {"subset.eq", 0x2286}, {"subset.eq.not", 0x2288}, {"subset.not", 0x2284},
  {"subset.neq", 0x228A}, {"subset.eq.sq", 0x2291},
  {"subset.sq", 0x228F}, {"subset.sq.eq", 0x2291},
  {"supset.eq", 0x2287}, {"supset.eq.not", 0x2289}, {"supset.not", 0x2285},
  {"supset.neq", 0x228B}, {"supset.eq.sq", 0x2292},
  {"supset.sq", 0x2290}, {"supset.sq.eq", 0x2292},

  // Membership
  {"in.not", 0x2209}, {"in.rev", 0x220B}, {"in.rev.not", 0x220C},
  {"in.rev.small", 0x220D},

  // Parallel/perpendicular
  {"parallel.not", 0x2226}, {"perp.not", 0x27C2},

  // Proportional
  {"prop", 0x221D},

  // Turnstiles
  {"tack.r", 0x22A2}, {"tack.l", 0x22A3}, {"tack.t", 0x22A4}, {"tack.b", 0x22A5},
  {"tack.r.double", 0x22A8}, {"tack.l.double", 0x2AE4},
  {"tack.r.not", 0x22AC}, {"tack.r.double.not", 0x22AD},
  {"tack.r.long", 0x27DD}, {"tack.r.short", 0x22A6},
  {"tack.l.long", 0x27DE}, {"tack.l.short", 0x2ADE},
  {"tack.t.big", 0x27D8}, {"tack.t.double", 0x2AEB}, {"tack.t.short", 0x2AE0},
  {"tack.b.big", 0x27D9}, {"tack.b.double", 0x2AEA}, {"tack.b.short", 0x2ADF},
  {"tack.l.r", 0x27DB},
  {"models", 0x22A8}, {"forces", 0x22A9},

  // === OPERATORS ===
  // Plus variants
  {"plus.minus", 0x00B1}, {"plus.circle", 0x2295}, {"plus.o", 0x2295},
  {"plus.square", 0x229E}, {"plus.triangle", 0x2A39},
  {"plus.small", 0x002B}, {"plus.big", 0x2A01},
  {"plus.dot", 0x2214}, {"plus.double", 0x2A7A}, {"plus.triple", 0x2A7B},
  // Minus variants
  {"minus.plus", 0x2213}, {"minus.circle", 0x2296}, {"minus.o", 0x2296},
  {"minus.square", 0x229F}, {"minus.triangle", 0x2A3A},
  {"minus.dot", 0x2238}, {"minus.tilde", 0x2242},
  // Times variants
  {"times.circle", 0x2297}, {"times.o", 0x2297},
  {"times.square", 0x22A0}, {"times.big", 0x2A02},
  {"times.triangle", 0x2A3B}, {"times.l", 0x22C9}, {"times.r", 0x22CA},
  {"times.div", 0x22C7},
  // Division variants
  {"div.circle", 0x2298}, {"div.o", 0x2298},
  // Dot variants
  {"dot.op", 0x22C5}, {"dot.basic", 0x002E}, {"dot.c", 0x00B7},
  {"dot.circle", 0x2299}, {"dot.o", 0x2299}, {"dot.square", 0x22A1},
  {"dot.big", 0x2A00}, {"dot.o.big", 0x2A00}, {"dot.circle.big", 0x2A00},
  {"dot.double", 0x00A8}, {"dot.triple", 0x20DB}, {"dot.quad", 0x20DC},
  // Star variants
  {"star.op", 0x22C6}, {"star.circle", 0x229B}, {"star.filled", 0x2605},
  {"star.stroked", 0x2606}, {"star.big", 0x2605},
  // Asterisk variants
  {"ast.op", 0x2217}, {"ast.circle", 0x229B}, {"ast.low", 0x204E},
  // Circle variants
  {"circle.stroked", 0x25CB}, {"circle.filled", 0x25CF},
  {"circle.stroked.tiny", 0x2218}, {"circle.stroked.small", 0x26AC},
  {"circle.stroked.big", 0x25EF}, {"circle.filled.tiny", 0x2981},
  {"circle.filled.small", 0x2219}, {"circle.filled.big", 0x2B24},
  {"circle.nested", 0x229A}, {"circle.dotted", 0x25CC},
  // Square variants
  {"square.stroked", 0x25A1}, {"square.filled", 0x25A0},
  {"square.stroked.tiny", 0x25AB}, {"square.filled.tiny", 0x25AA},
  {"square.stroked.small", 0x25FD}, {"square.filled.small", 0x25FE},
  {"square.stroked.medium", 0x25FB}, {"square.filled.medium", 0x25FC},
  {"square.stroked.big", 0x2B1C}, {"square.filled.big", 0x2B1B},
  {"square.stroked.dotted", 0x2B1A}, {"square.stroked.rounded", 0x25A2},
  // Diamond variants
  {"diamond.stroked", 0x25C7}, {"diamond.filled", 0x25C6},
  {"diamond.stroked.small", 0x22C4}, {"diamond.stroked.medium", 0x2B26},
  {"diamond.stroked.dot", 0x27D0},
  {"diamond.filled.medium", 0x2B25}, {"diamond.filled.small", 0x2B29},
  // Triangle variants
  {"triangle.stroked.t", 0x25B3}, {"triangle.stroked.b", 0x25BD},
  {"triangle.stroked.l", 0x25C1}, {"triangle.stroked.r", 0x25B7},
  {"triangle.stroked.bl", 0x25FA}, {"triangle.stroked.br", 0x25FF},
  {"triangle.stroked.tl", 0x25F8}, {"triangle.stroked.tr", 0x25F9},
  {"triangle.stroked.small.t", 0x25B5}, {"triangle.stroked.small.b", 0x25BF},
  {"triangle.stroked.small.l", 0x25C3}, {"triangle.stroked.small.r", 0x25B9},
  {"triangle.stroked.nested", 0x27C1}, {"triangle.stroked.dot", 0x25EC},
  {"triangle.filled.t", 0x25B2}, {"triangle.filled.b", 0x25BC},
  {"triangle.filled.l", 0x25C0}, {"triangle.filled.r", 0x25B6},
  {"triangle.filled.small.t", 0x25B4}, {"triangle.filled.small.b", 0x25BE},
  {"triangle.filled.small.l", 0x25C2}, {"triangle.filled.small.r", 0x25B8},
  // Bowtie
  {"bowtie.r", 0x22C9}, {"bowtie.l", 0x22CA}, {"bowtie", 0x22C8},

  // === BAR AND DASH ===
  {"bar.v", 0x007C}, {"bar.v.double", 0x2016}, {"bar.v.triple", 0x2980},
  {"bar.v.broken", 0x00A6}, {"bar.v.circle", 0x29B6}, {"bar.h", 0x2015},
  {"dash.en", 0x2013}, {"dash.em", 0x2014},
  {"dash.em.two", 0x2E3A}, {"dash.em.three", 0x2E3B},
  {"dash.fig", 0x2012}, {"dash.colon", 0x2239},
  {"dash.circle", 0x229D}, {"dash.wave", 0x301C},

  // === CORNERS ===
  {"corner.l.t", 0x231C}, {"corner.l.b", 0x231E},
  {"corner.r.t", 0x231D}, {"corner.r.b", 0x231F},
  {"corner.tl", 0x231C}, {"corner.bl", 0x231E},  // alternate naming
  {"corner.tr", 0x231D}, {"corner.br", 0x231F},

  // === LOGIC ===
  {"and.big", 0x22C0}, {"or.big", 0x22C1},
  {"and.curly", 0x22CF}, {"or.curly", 0x22CE},
  {"and.dot", 0x27D1},
  {"not.not", 0x00AC}, {"not.rev", 0x2310},
  {"exists.not", 0x2204},

  // === SETS ===
  {"union.big", 0x22C3}, {"union.sq", 0x2294}, {"union.sq.big", 0x2A06},
  {"union.plus", 0x228E}, {"union.arrow", 0x228C}, {"union.dot", 0x228D},
  {"union.minus", 0x2A41}, {"union.double", 0x22D3},
  {"sect.big", 0x22C2}, {"sect.sq", 0x2293}, {"sect.sq.big", 0x2A05},
  {"sect.dot", 0x2A40}, {"sect.double", 0x22D2},
  {"empty.rev", 0x29B0},

  // === MISCELLANEOUS ===
  // Infinity variants
  {"infinity.bar", 0x29DC}, {"infinity.incomplete", 0x29DD},
  // Constants
  {"planck.reduce", 0x210F},  // ℏ (h-bar)
  {"angstrom", 0x212B},
  // Calculus
  {"diff.partial", 0x2202},  // ∂
  {"gradient", 0x2207},  // ∇
  // Other
  {"degree.c", 0x2103}, {"degree.f", 0x2109},
  {"prime.rev", 0x2035}, {"prime.double.rev", 0x2036}, {"prime.triple.rev", 0x2037},
  {"bullet.op", 0x2219}, {"bullet.circle", 0x29BF},

  // === DOTS ===
  {"dots.h", 0x2026}, {"dots.h.c", 0x22EF}, {"dots.v", 0x22EE},
  {"dots.c", 0x22EF}, {"dots.down", 0x22F1}, {"dots.up", 0x22F0},

  // === INTEGRALS ===
  {"integral.double", 0x222C}, {"integral.triple", 0x222D}, {"integral.quad", 0x2A0C},
  {"integral.cont", 0x222E}, {"integral.cont.double", 0x222F}, {"integral.cont.triple", 0x2230},
  {"integral.ccw", 0x2233}, {"integral.cw", 0x2232},
  {"integral.sq", 0x2A16}, {"integral.sect", 0x2A1B}, {"integral.union", 0x2A1C},
  {"integral.dash", 0x2A0D},

  // === SUMS/PRODUCTS ===
  {"sum.integral", 0x2A0B},
  {"product", 0x220F}, {"coproduct", 0x2210},

  // === CHECKMARKS ===
  {"checkmark.light", 0x2713}, {"checkmark", 0x2713}, {"checkmark.heavy", 0x2714},
  {"crossmark", 0x2717}, {"crossmark.heavy", 0x2718},

  // === FACES ===
  {"face.smile", 0x263A}, {"face.frown", 0x2639}, {"face.neutral", 0x263B},

  // === PLAYING CARDS ===
  {"suit.club", 0x2663}, {"suit.diamond", 0x2666}, {"suit.heart", 0x2665}, {"suit.spade", 0x2660},
  {"club.stroked", 0x2667}, {"suit.diamond.stroked", 0x2662},
  {"heart.stroked", 0x2661}, {"spade.stroked", 0x2664},

  // === MUSIC ===
  {"note.flat", 0x266D}, {"note.natural", 0x266E}, {"note.sharp", 0x266F},
  {"note.quarter", 0x2669}, {"note.eighth", 0x266A}, {"note.eighth.beamed", 0x266B},

  // === CURRENCY ===
  {"dollar", 0x0024}, {"cent", 0x00A2}, {"pound", 0x00A3}, {"yen", 0x00A5},
  {"euro", 0x20AC}, {"bitcoin", 0x20BF}, {"franc", 0x20A3},
  {"rupee", 0x20B9}, {"ruble", 0x20BD}, {"won", 0x20A9}, {"peso", 0x20B1},
  {"baht", 0x0E3F}, {"lira", 0x20BA}, {"shekel", 0x20AA},
  // Typst dotted currency aliases
  {"currency.dollar", 0x0024}, {"currency.cent", 0x00A2},
  {"currency.pound", 0x00A3}, {"currency.yen", 0x00A5},
  {"currency.euro", 0x20AC}, {"currency.bitcoin", 0x20BF},

  // === BULLET VARIANTS ===
  {"bullet.op", 0x2219}, {"bullet.circle", 0x29BF},
  {"bullet.stroked", 0x25E6}, {"bullet.tri", 0x2023},
  {"bullet.hyph", 0x2043},

  // === COLON VARIANTS ===
  {"colon", 0x003A}, {"colon.double", 0x2237}, {"colon.eq", 0x2254},
  {"colon.double.eq", 0x2A74},

  // === QUOTE VARIANTS ===
  {"quote.double", 0x0022}, {"quote.single", 0x0027},
  {"quote.l.double", 0x201C}, {"quote.r.double", 0x201D},
  {"quote.l.single", 0x2018}, {"quote.r.single", 0x2019},
  {"quote.low.double", 0x201E}, {"quote.low.single", 0x201A},
  {"quote.high.double", 0x201F}, {"quote.high.single", 0x201B},
  {"quote.angle.l.double", 0x00AB}, {"quote.angle.r.double", 0x00BB},
  {"quote.angle.l.single", 0x2039}, {"quote.angle.r.single", 0x203A},

  // === ASTERISK VARIANTS ===
  {"ast.basic", 0x002A}, {"ast.double", 0x2051}, {"ast.triple", 0x2042},
  {"ast.small", 0xFE61}, {"ast.square", 0x29C6},

  // === HYPHEN VARIANTS ===
  {"hyph", 0x2010}, {"hyph.minus", 0x002D}, {"hyph.nobreak", 0x2011},
  {"hyph.soft", 0x00AD}, {"hyph.point", 0x2027},

  // === SPECIAL PUNCTUATION ===
  {"hash", 0x0023}, {"percent", 0x0025}, {"at", 0x0040},
  {"ampersand", 0x0026}, {"asterisk", 0x002A},
  {"numero", 0x2116}, {"diameter", 0x2300},
  {"pilcrow.rev", 0x204B}, {"ref.mark", 0x203B},

  // === INTEGRAL VARIANTS (additional) ===
  {"integral.surf", 0x222F}, {"integral.vol", 0x2230},
  {"integral.cont.ccw", 0x2233}, {"integral.cont.cw", 0x2232},
  {"integral.times", 0x2A18}, {"integral.slash", 0x2A0F},
  {"integral.arrow.hook", 0x2A17},

  // === PRODUCT VARIANTS ===
  {"product.co", 0x2210}
};

// =============================================================================
// TypstLexer implementation
// =============================================================================

TypstLexer::TypstLexer(const std::string& input)
  : input_(input), current_(TypstTokenType::END, "", 0) {}

char TypstLexer::current_char() const {
  if (pos_ >= input_.size()) return '\0';
  return input_[pos_];
}

char TypstLexer::advance() {
  if (pos_ < input_.size()) pos_++;
  return current_char();
}

bool TypstLexer::is_letter(char c) const {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool TypstLexer::is_digit(char c) const {
  return c >= '0' && c <= '9';
}

bool TypstLexer::is_identifier_char(char c) const {
  // '_' is the subscript operator in Typst, not an identifier character
  return is_letter(c) || is_digit(c);
}

TypstToken TypstLexer::scan_identifier() {
  size_t start = pos_;
  while (is_identifier_char(current_char())) {
    advance();
  }
  // Check for dotted identifier (e.g., arrow.r, eq.not)
  // Continue scanning if dot is followed by a letter (not a digit - that would be a decimal)
  while (current_char() == '.' && pos_ + 1 < input_.size() && is_letter(input_[pos_ + 1])) {
    advance();  // consume dot
    while (is_identifier_char(current_char())) {
      advance();
    }
  }
  return TypstToken(TypstTokenType::IDENTIFIER,
                    input_.substr(start, pos_ - start), start);
}

TypstToken TypstLexer::scan_number() {
  size_t start = pos_;
  while (is_digit(current_char())) {
    advance();
  }
  // Check for decimal point
  if (current_char() == '.' && is_digit(input_[pos_ + 1])) {
    advance();  // consume '.'
    while (is_digit(current_char())) {
      advance();
    }
  }
  return TypstToken(TypstTokenType::NUMBER,
                    input_.substr(start, pos_ - start), start);
}

TypstToken TypstLexer::scan_string() {
  size_t start = pos_;
  advance();  // consume opening quote
  std::string content;
  while (current_char() != '\0' && current_char() != '"') {
    if (current_char() == '\\' && pos_ + 1 < input_.size()) {
      advance();  // skip backslash
      char escaped = current_char();
      if (escaped == 'n') content += '\n';
      else if (escaped == 't') content += '\t';
      else if (escaped == '"') content += '"';
      else if (escaped == '\\') content += '\\';
      else content += escaped;
      advance();
    } else {
      content += current_char();
      advance();
    }
  }
  if (current_char() == '"') {
    advance();  // consume closing quote
  }
  return TypstToken(TypstTokenType::STRING, content, start);
}

TypstToken TypstLexer::scan_token() {
  // Skip whitespace but track it
  while (std::isspace(static_cast<unsigned char>(current_char()))) {
    advance();
  }

  if (pos_ >= input_.size()) {
    return TypstToken(TypstTokenType::END, "", pos_);
  }

  size_t start = pos_;
  char c = current_char();

  // Identifiers
  if (is_letter(c)) {
    return scan_identifier();
  }

  // Numbers
  if (is_digit(c)) {
    return scan_number();
  }

  // Strings
  if (c == '"') {
    return scan_string();
  }

  // Backslash sequences (Typst)
  if (c == '\\') {
    if (pos_ + 1 >= input_.size()) {
      // Backslash at end of input counts as a linebreak
      advance();
      return TypstToken(TypstTokenType::LINEBREAK, "\\", start);
    }
    char next = input_[pos_ + 1];
    // Escaped delimiters become literal symbols (non-scaling)
    if (next == '(' || next == ')' || next == '[' || next == ']' ||
        next == '{' || next == '}' || next == '|') {
      advance();  // consume the backslash
      advance();  // consume the escaped character
      return TypstToken(TypstTokenType::OPERATOR, std::string(1, next), start);
    }
    // Escaped comma and semicolon (literal, not argument separator)
    if (next == ',' || next == ';') {
      advance();  // consume the backslash
      advance();  // consume the escaped character
      return TypstToken(TypstTokenType::OPERATOR, std::string(1, next), start);
    }
    // Standalone backslash followed by whitespace/newline = linebreak
    if (std::isspace(static_cast<unsigned char>(next))) {
      advance();  // consume the backslash
      // Skip any whitespace after the backslash
      while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
        advance();
      }
      return TypstToken(TypstTokenType::LINEBREAK, "\\", start);
    }
    // Unknown escape: emit the backslash and lex the next char separately
    advance();
    return TypstToken(TypstTokenType::OPERATOR, "\\", start);
  }

  // Check for shorthand sequences (longer matches first)
  // The table is ordered by length, so we try each in order
  for (const auto& [shorthand, codepoint] : typst_shorthands) {
    if (pos_ + shorthand.size() <= input_.size() &&
        input_.substr(pos_, shorthand.size()) == shorthand) {
      // Consume all characters
      for (size_t j = 0; j < shorthand.size(); j++) {
        advance();
      }
      // Convert codepoint to UTF-8
      std::string utf8;
      if (codepoint < 0x80) {
        utf8 += static_cast<char>(codepoint);
      } else if (codepoint < 0x800) {
        utf8 += static_cast<char>(0xC0 | (codepoint >> 6));
        utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
      } else if (codepoint < 0x10000) {
        utf8 += static_cast<char>(0xE0 | (codepoint >> 12));
        utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
      } else {
        utf8 += static_cast<char>(0xF0 | (codepoint >> 18));
        utf8 += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
      }
      return TypstToken(TypstTokenType::OPERATOR, utf8, start);
    }
  }

  // Single character tokens
  advance();
  switch (c) {
    case '(': return TypstToken(TypstTokenType::LPAREN, "(", start);
    case ')': return TypstToken(TypstTokenType::RPAREN, ")", start);
    case '[': return TypstToken(TypstTokenType::LBRACKET, "[", start);
    case ']': return TypstToken(TypstTokenType::RBRACKET, "]", start);
    case '{': return TypstToken(TypstTokenType::LBRACE, "{", start);
    case '}': return TypstToken(TypstTokenType::RBRACE, "}", start);
    case ',': return TypstToken(TypstTokenType::COMMA, ",", start);
    case ';': return TypstToken(TypstTokenType::SEMICOLON, ";", start);
    case ':': return TypstToken(TypstTokenType::COLON, ":", start);
    case '&': return TypstToken(TypstTokenType::AMPERSAND, "&", start);
    case '^': return TypstToken(TypstTokenType::SUPER, "^", start);
    case '_': return TypstToken(TypstTokenType::SUB, "_", start);
    case '+': return TypstToken(TypstTokenType::OPERATOR, "+", start);
    case '-': return TypstToken(TypstTokenType::OPERATOR, "-", start);
    case '*': return TypstToken(TypstTokenType::OPERATOR, "*", start);
    case '/': return TypstToken(TypstTokenType::OPERATOR, "/", start);
    case '=': return TypstToken(TypstTokenType::OPERATOR, "=", start);
    case '<': return TypstToken(TypstTokenType::OPERATOR, "<", start);
    case '>': return TypstToken(TypstTokenType::OPERATOR, ">", start);
    case '|': return TypstToken(TypstTokenType::OPERATOR, "|", start);
    case '!': return TypstToken(TypstTokenType::OPERATOR, "!", start);
    case '.': return TypstToken(TypstTokenType::OPERATOR, ".", start);
    case '%': return TypstToken(TypstTokenType::OPERATOR, "%", start);
    case '\'': return TypstToken(TypstTokenType::OPERATOR, "'", start);
    case '#': return TypstToken(TypstTokenType::OPERATOR, "#", start);
    default:
      return TypstToken(TypstTokenType::OTHER, std::string(1, c), start);
  }
}

TypstToken TypstLexer::next() {
  if (has_current_) {
    has_current_ = false;
    return current_;
  }
  return scan_token();
}

TypstToken TypstLexer::peek() {
  if (!has_current_) {
    current_ = scan_token();
    has_current_ = true;
  }
  return current_;
}

void TypstLexer::consume() {
  if (has_current_) {
    has_current_ = false;
  } else {
    scan_token();
  }
}

bool TypstLexer::at_end() const {
  if (has_current_) {
    return current_.type == TypstTokenType::END;
  }
  return pos_ >= input_.size();
}

// =============================================================================
// TypstParser implementation
// =============================================================================

TypstParser::TypstParser(const std::string& input) : lexer_(input) {}

void TypstParser::error(const std::string& msg) {
  errors_.push_back(msg);
}

bool TypstParser::check(TypstTokenType type) {
  return lexer_.peek().type == type;
}

bool TypstParser::match(TypstTokenType type) {
  if (check(type)) {
    lexer_.consume();
    return true;
  }
  return false;
}

TypstToken TypstParser::consume(TypstTokenType type, const std::string& msg) {
  if (check(type)) {
    return lexer_.next();
  }
  error(msg);
  return TypstToken(type, "", lexer_.position());
}

MathNodePtr TypstParser::parse() {
  auto node = parse_math_list();
  if (!lexer_.at_end()) {
    error("Unexpected '" + lexer_.peek().value + "' at end of input");
  }
  return node;
}

// Parse a list of expressions (stops at function argument delimiters)
// Handles Typst multiline equations with \ (linebreak) and & (alignment)
MathNodePtr TypstParser::parse_math_list(bool stop_at_ampersand) {
  // First, check if we have multiline content (LINEBREAK tokens at top level)
  // by peeking ahead - if we find any LINEBREAK, we parse as aligned
  std::vector<std::vector<MathNodePtr>> rows;
  std::vector<MathNodePtr> current_row;
  auto current_cell = std::make_unique<MathNode>(NodeType::ROW);
  bool has_multiline = false;

  while (!lexer_.at_end() && !check(TypstTokenType::RPAREN) &&
         !check(TypstTokenType::COMMA) && !check(TypstTokenType::SEMICOLON) &&
         !check(TypstTokenType::RBRACKET) && !check(TypstTokenType::RBRACE) &&
         !check(TypstTokenType::COLON)) {

    // Check for linebreak (Typst \)
    if (check(TypstTokenType::LINEBREAK)) {
      has_multiline = true;
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

    // Check for alignment point (Typst &)
    if (check(TypstTokenType::AMPERSAND)) {
      if (stop_at_ampersand) {
        break;
      }
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

    auto expr = parse_expression();
    if (expr) {
      current_cell->add_child(std::move(expr));
    } else {
      break;
    }
  }

  // Finish last cell and row
  if (current_cell->children.size() == 1) {
    current_row.push_back(std::move(current_cell->children[0]));
  } else if (!current_cell->children.empty()) {
    current_row.push_back(std::move(current_cell));
  } else if (!current_row.empty() || has_multiline) {
    current_row.push_back(std::make_unique<MathNode>(NodeType::GROUP));
  }
  if (!current_row.empty()) {
    rows.push_back(std::move(current_row));
  }

  // If we have multiline content, create an ALIGNED node
  if (has_multiline || rows.size() > 1) {
    auto node = std::make_unique<MathNode>(NodeType::ALIGNED);
    node->matrix_type = "aligned";  // Similar to LaTeX aligned
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

  // Single line - return as simple ROW or unwrapped single child
  if (rows.empty()) {
    return nullptr;
  }
  if (rows.size() == 1 && rows[0].size() == 1) {
    return std::move(rows[0][0]);
  }

  auto row = std::make_unique<MathNode>(NodeType::ROW);
  for (auto& cell : rows[0]) {
    row->add_child(std::move(cell));
  }
  if (row->children.size() == 1) {
    return std::move(row->children[0]);
  }
  return row;
}

// Parse content inside braces (allows commas as content, not delimiters)
MathNodePtr TypstParser::parse_brace_content() {
  auto row = std::make_unique<MathNode>(NodeType::ROW);

  while (!lexer_.at_end() && !check(TypstTokenType::RBRACE)) {
    auto expr = parse_expression();
    if (expr) {
      row->add_child(std::move(expr));
    } else if (!check(TypstTokenType::COMMA) && !check(TypstTokenType::RBRACE) &&
               !lexer_.at_end()) {
      // Guarantee progress on tokens parse_expression can't handle
      error("Unexpected token '" + lexer_.peek().value + "' in brace group");
      lexer_.consume();
    }
    // Commas inside braces are just regular content
    if (check(TypstTokenType::COMMA)) {
      auto comma = std::make_unique<MathNode>(NodeType::SYMBOL, ",");
      row->add_child(std::move(comma));
      lexer_.consume();
    } else if (check(TypstTokenType::RBRACE)) {
      break;
    } else if (lexer_.at_end()) {
      break;
    }
  }

  if (row->children.size() == 1) {
    return std::move(row->children[0]);
  }
  if (row->children.empty()) {
    return nullptr;
  }
  return row;
}

MathNodePtr TypstParser::parse_expression() {
  auto base = parse_atom();
  if (!base) return nullptr;
  base = parse_scripts(std::move(base));

  // Check for fraction shorthand: a/b (can chain: a/b/c = (a/b)/c)
  while (check(TypstTokenType::OPERATOR) && lexer_.peek().value == "/") {
    lexer_.consume();  // consume '/'
    auto denom = parse_atom();
    if (denom) {
      denom = parse_scripts(std::move(denom));
    }

    auto frac = std::make_unique<MathNode>(NodeType::FRACTION);
    frac->add_child(std::move(base));  // numerator
    frac->add_child(denom ? std::move(denom) : std::make_unique<MathNode>(NodeType::ROW));  // denominator
    base = std::move(frac);
  }

  return base;
}

MathNodePtr TypstParser::parse_atom() {
  // All recursion in the parser funnels through here, so guard the depth
  struct DepthGuard {
    int& depth;
    explicit DepthGuard(int& d) : depth(d) { ++depth; }
    ~DepthGuard() { --depth; }
  } guard(depth_);

  if (depth_ > MAX_PARSE_DEPTH) {
    if (!depth_exceeded_) {
      depth_exceeded_ = true;
      error("Expression too deeply nested");
    }
    return nullptr;
  }

  TypstToken token = lexer_.peek();

  switch (token.type) {
    case TypstTokenType::IDENTIFIER: {
      lexer_.consume();
      // Check if it's a function call (followed by '(')
      if (check(TypstTokenType::LPAREN) && is_typst_function(token.value)) {
        return parse_function_call(token.value);
      }
      return parse_identifier(token.value);
    }

    case TypstTokenType::NUMBER: {
      lexer_.consume();
      auto node = std::make_unique<MathNode>(NodeType::NUMBER, token.value);
      return node;
    }

    case TypstTokenType::STRING: {
      lexer_.consume();
      auto node = std::make_unique<MathNode>(NodeType::TEXT, token.value);
      return node;
    }

    case TypstTokenType::LPAREN: {
      return parse_group();
    }

    case TypstTokenType::OPERATOR: {
      lexer_.consume();
      auto node = std::make_unique<MathNode>(NodeType::SYMBOL, token.value);
      return node;
    }

    case TypstTokenType::LBRACE:
    case TypstTokenType::LBRACKET: {
      return parse_group();
    }

    default:
      return nullptr;
  }
}

MathNodePtr TypstParser::parse_scripts(MathNodePtr base) {
  if (!base) return nullptr;

  MathNodePtr sub = nullptr;
  MathNodePtr sup = nullptr;

  // Typst strips parentheses used for grouping in scripts (x^(a+b) renders
  // the superscript without parens)
  auto strip_parens = [](MathNodePtr& node) {
    if (node && node->type == NodeType::DELIMITED && node->left_delim == "(" &&
        node->right_delim == ")" && node->children.size() == 1) {
      node = std::move(node->children[0]);
    }
  };

  while (check(TypstTokenType::SUB) || check(TypstTokenType::SUPER)) {
    if (match(TypstTokenType::SUB)) {
      sub = parse_atom();
      strip_parens(sub);
    } else if (match(TypstTokenType::SUPER)) {
      sup = parse_atom();
      strip_parens(sup);
    }
  }

  // Handle prime notation: ' is shorthand for superscript prime
  // Use dedicated Unicode glyphs for multiple primes:
  // U+2032 ′ PRIME, U+2033 ″ DOUBLE PRIME, U+2034 ‴ TRIPLE PRIME, U+2057 ⁗ QUADRUPLE PRIME
  int prime_count = 0;
  while (lexer_.peek().type == TypstTokenType::OPERATOR && lexer_.peek().value == "'") {
    lexer_.consume();
    prime_count++;
  }

  if (prime_count > 0) {
    MathNodePtr primes;
    if (prime_count == 1) {
      primes = std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x80\xB2");  // ′ U+2032
    } else if (prime_count == 2) {
      primes = std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x80\xB3");  // ″ U+2033
    } else if (prime_count == 3) {
      primes = std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x80\xB4");  // ‴ U+2034
    } else if (prime_count == 4) {
      primes = std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x81\x97");  // ⁗ U+2057
    } else {
      // More than 4 primes: use quadruple + remainder
      auto row = std::make_unique<MathNode>(NodeType::ROW);
      while (prime_count >= 4) {
        row->add_child(std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x81\x97"));
        prime_count -= 4;
      }
      if (prime_count == 3) {
        row->add_child(std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x80\xB4"));
      } else if (prime_count == 2) {
        row->add_child(std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x80\xB3"));
      } else if (prime_count == 1) {
        row->add_child(std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x80\xB2"));
      }
      primes = std::move(row);
    }

    // Merge primes with existing superscript or create new one
    if (sup) {
      auto combined = std::make_unique<MathNode>(NodeType::ROW);
      combined->add_child(std::move(sup));
      combined->add_child(std::move(primes));
      sup = std::move(combined);
    } else {
      sup = std::move(primes);
    }
  }

  if (sub && sup) {
    auto node = std::make_unique<MathNode>(NodeType::SUBSUPERSCRIPT);
    node->add_child(std::move(base));
    node->add_child(std::move(sub));
    node->add_child(std::move(sup));
    return node;
  } else if (sub) {
    auto node = std::make_unique<MathNode>(NodeType::SUBSCRIPT);
    node->add_child(std::move(base));
    node->add_child(std::move(sub));
    return node;
  } else if (sup) {
    auto node = std::make_unique<MathNode>(NodeType::SUPERSCRIPT);
    node->add_child(std::move(base));
    node->add_child(std::move(sup));
    return node;
  }

  return base;
}

MathNodePtr TypstParser::parse_group() {
  char open = lexer_.peek().value[0];
  char close = (open == '(') ? ')' : (open == '[') ? ']' : '}';
  TypstTokenType close_type = (open == '(') ? TypstTokenType::RPAREN :
                              (open == '[') ? TypstTokenType::RBRACKET :
                                              TypstTokenType::RBRACE;

  lexer_.consume();  // consume opening bracket

  // For brace groups, allow commas as content (not delimiters)
  MathNodePtr content;
  if (open == '{') {
    content = parse_brace_content();
  } else {
    content = parse_math_list();
  }

  if (check(close_type)) {
    lexer_.consume();  // consume closing bracket
  } else {
    error(std::string("Expected '") + close + "'");
  }

  if (!content) {
    return std::make_unique<MathNode>(NodeType::GROUP);
  }

  // Wrap in GROUP if it's a plain parenthesis group
  if (open == '(') {
    auto group = std::make_unique<MathNode>(NodeType::DELIMITED);
    group->left_delim = "(";
    group->right_delim = ")";
    group->add_child(std::move(content));
    return group;
  } else if (open == '[') {
    auto group = std::make_unique<MathNode>(NodeType::DELIMITED);
    group->left_delim = "[";
    group->right_delim = "]";
    group->add_child(std::move(content));
    return group;
  } else {
    auto group = std::make_unique<MathNode>(NodeType::GROUP);
    group->add_child(std::move(content));
    return group;
  }
}

MathNodePtr TypstParser::parse_function_call(const std::string& name) {
  // Over/underbrace with labels (check BEFORE general accents)
  if (name == "overbrace") return parse_overbrace();
  if (name == "underbrace") return parse_underbrace();
  if (name == "overline") return parse_overline();
  if (name == "underline") return parse_underline();

  // Under/over bracket variants
  if (name == "overbracket") return parse_overbracket();
  if (name == "underbracket") return parse_underbracket();
  if (name == "overparen") return parse_overparen();
  if (name == "underparen") return parse_underparen();
  if (name == "overshell") return parse_overshell();
  if (name == "undershell") return parse_undershell();

  // Generic accent function: accent(base, char)
  if (name == "accent") return parse_generic_accent();

  // Named accents (but not the special ones handled above)
  if (is_accent_function(name)) {
    return parse_accent(name);
  }

  // Style functions
  if (is_style_function(name)) {
    return parse_style(name);
  }

  // Delimiter functions
  if (name == "lr") return parse_lr();
  if (name == "abs") return parse_abs();
  if (name == "norm") return parse_norm();
  if (name == "floor") return parse_floor();
  if (name == "ceil") return parse_ceil();
  if (name == "round") return parse_round();
  if (name == "mid") return parse_mid();

  // Fractions and binomials
  if (name == "frac") return parse_frac();
  if (name == "binom") return parse_binom();

  // Roots
  if (name == "sqrt") return parse_sqrt();
  if (name == "root") return parse_root();

  // Matrices - pass the name to determine delimiter type
  if (name == "mat" || name == "pmat" || name == "bmat" ||
      name == "vmat" || name == "Vmat" || name == "Bmat") {
    return parse_mat(name);
  }

  // Cases
  if (name == "cases") return parse_cases();

  // Vec
  if (name == "vec") return parse_vec();

  // Cancel/hide
  if (name == "cancel" || name == "strike") return parse_cancel();
  if (name == "hide") return parse_hide();

  // Explicit primes function
  if (name == "primes") return parse_primes();

  // Stretch and class functions
  if (name == "stretch") return parse_stretch();
  if (name == "class") return parse_class();

  // Limits control
  if (name == "limits") return parse_limits();
  if (name == "scripts") return parse_scripts_func();

  // Display styles
  if (name == "display") return parse_display();
  if (name == "inline") return parse_inline();
  if (name == "script") return parse_script_style();
  if (name == "sscript") return parse_sscript_style();

  // Custom operator
  if (name == "op") return parse_op();

  // Attachment
  if (name == "attach") return parse_attach();

  // Wide accents
  if (name == "widehat" || name == "widetilde" ||
      name == "overrightarrow" || name == "overleftarrow" ||
      name == "overleftrightarrow" || name == "underrightarrow" ||
      name == "underleftarrow" || name == "underleftrightarrow") {
    return parse_accent(name);
  }

  // Spacing functions h(...) and v(...)
  if (name == "h" || name == "v") {
    consume(TypstTokenType::LPAREN, "Expected '(' after spacing function");

    double amount = 0;
    std::string unit = "em";  // default to em

    // Parse the spacing value - expect a number followed by optional unit
    if (check(TypstTokenType::NUMBER)) {
      std::string num_str = lexer_.next().value;
      amount = safe_stod(num_str);

      // Check for unit suffix
      if (check(TypstTokenType::IDENTIFIER)) {
        std::string unit_name = lexer_.peek().value;
        if (unit_name == "em") {
          lexer_.consume();
          unit = "em";
        } else if (unit_name == "pt") {
          lexer_.consume();
          unit = "pt";
        } else if (unit_name == "mm") {
          lexer_.consume();
          amount *= 2.834645669;  // convert mm to pt
          unit = "pt";
        } else if (unit_name == "cm") {
          lexer_.consume();
          amount *= 28.34645669;  // convert cm to pt
          unit = "pt";
        } else if (unit_name == "in") {
          lexer_.consume();
          amount *= 72.0;  // convert in to pt
          unit = "pt";
        }
      }
    }

    consume(TypstTokenType::RPAREN, "Expected ')' after spacing argument");

    auto node = std::make_unique<MathNode>(NodeType::SPACE_NODE);
    node->space_amount = amount;
    node->space_unit = unit;
    // For v(), we could add vertical spacing support later; for now treat as horizontal
    return node;
  }

  // Unknown function - treat as error
  error("Unknown function: " + name);
  consume(TypstTokenType::LPAREN, "Expected '('");
  parse_math_list();  // consume arguments
  match(TypstTokenType::RPAREN);
  return std::make_unique<MathNode>(NodeType::SYMBOL, name);
}

MathNodePtr TypstParser::parse_identifier(const std::string& name) {
  // Check spacing keywords
  auto spacing_it = typst_spacing.find(name);
  if (spacing_it != typst_spacing.end()) {
    auto node = std::make_unique<MathNode>(NodeType::SPACE_NODE);
    node->value = std::to_string(spacing_it->second);
    return node;
  }

  // Check for dotted/namespaced symbols (e.g., arrow.r, eq.not)
  auto dotted_it = typst_dotted_symbols.find(name);
  if (dotted_it != typst_dotted_symbols.end()) {
    auto node = std::make_unique<MathNode>(NodeType::SYMBOL);
    uint32_t cp = dotted_it->second;
    // Convert codepoint to UTF-8
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
    node->value = utf8;
    return node;
  }

  // Check for differential operators (dif, Dif, dd -> upright d, D)
  if (is_differential_operator(name)) {
    auto node = std::make_unique<MathNode>(NodeType::STYLED);
    node->style = MathStyle::ROMAN;
    auto letter = std::make_unique<MathNode>(NodeType::SYMBOL);
    // dif and dd -> "d", Dif -> "D"
    letter->value = (name == "Dif") ? "D" : "d";
    node->add_child(std::move(letter));
    return node;
  }

  // Check for big operators
  if (is_big_operator(name)) {
    auto node = std::make_unique<MathNode>(NodeType::BIG_OP, name);
    // Map Typst names to LaTeX equivalents
    if (name == "product") node->value = "prod";
    else if (name == "integral") node->value = "int";
    return node;
  }

  // Check for math functions (sin, cos, etc.)
  if (is_function_name(name)) {
    auto node = std::make_unique<MathNode>(NodeType::FUNCTION, name);
    return node;
  }

  // Typst uses the opposite phi/epsilon convention to LaTeX (the shared
  // symbol table below follows LaTeX)
  if (name == "phi") {
    return std::make_unique<MathNode>(NodeType::SYMBOL, "\xCF\x86");  // φ U+03C6
  }
  if (name == "epsilon") {
    return std::make_unique<MathNode>(NodeType::SYMBOL, "\xCE\xB5");  // ε U+03B5
  }

  // Check for Greek letters using existing symbol table
  if (is_greek_letter(name)) {
    auto node = std::make_unique<MathNode>(NodeType::SYMBOL);
    uint32_t cp = command_to_codepoint(name);
    if (cp != 0) {
      // Store as UTF-8
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
      node->value = utf8;
    } else {
      node->value = name;
    }
    return node;
  }

  // Check for other symbols
  if (is_symbol(name)) {
    auto node = std::make_unique<MathNode>(NodeType::SYMBOL);
    uint32_t cp = command_to_codepoint(name);
    if (cp != 0) {
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
      node->value = utf8;
    } else {
      node->value = name;
    }
    return node;
  }

  // Single letter - treat as variable
  if (name.length() == 1) {
    auto node = std::make_unique<MathNode>(NodeType::SYMBOL, name);
    return node;
  }

  // Unknown multi-letter identifier: Typst treats juxtaposed letters as
  // individual variables, so split it up (the layout only renders the first
  // codepoint of a SYMBOL)
  auto row = std::make_unique<MathNode>(NodeType::ROW);
  for (char ch : name) {
    row->add_child(std::make_unique<MathNode>(NodeType::SYMBOL, std::string(1, ch)));
  }
  return row;
}

// =============================================================================
// Function-specific parsers
// =============================================================================

MathNodePtr TypstParser::parse_frac() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'frac'");
  auto num = parse_math_list();
  consume(TypstTokenType::COMMA, "Expected ',' in frac");
  auto denom = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after frac arguments");

  auto node = std::make_unique<MathNode>(NodeType::FRACTION);
  node->add_child(std::move(num));
  node->add_child(std::move(denom));
  return node;
}

MathNodePtr TypstParser::parse_binom() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'binom'");
  auto n = parse_math_list();
  consume(TypstTokenType::COMMA, "Expected ',' in binom");
  auto k = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after binom arguments");

  auto node = std::make_unique<MathNode>(NodeType::STACK);
  node->stack_type = "binom";
  node->add_child(std::move(n));
  node->add_child(std::move(k));
  return node;
}

MathNodePtr TypstParser::parse_sqrt() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'sqrt'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after sqrt argument");

  auto node = std::make_unique<MathNode>(NodeType::SQRT);
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_root() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'root'");
  auto index = parse_math_list();
  consume(TypstTokenType::COMMA, "Expected ',' in root");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after root arguments");

  auto node = std::make_unique<MathNode>(NodeType::SQRT);
  // The layout expects [index, radicand] for indexed roots
  node->add_child(std::move(index));
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_accent(const std::string& name) {
  consume(TypstTokenType::LPAREN, "Expected '(' after accent function");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after accent argument");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = name;
  node->wide = (wide_accent_functions.count(name) > 0);
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_generic_accent() {
  // accent(base, char, size: N%, dotless: true)
  consume(TypstTokenType::LPAREN, "Expected '(' after 'accent'");

  auto content = parse_math_list();

  consume(TypstTokenType::COMMA, "Expected ',' after base in accent()");

  // Parse the accent character - can be an identifier (named accent) or string
  std::string accent_name;
  if (check(TypstTokenType::IDENTIFIER)) {
    accent_name = lexer_.next().value;
  } else if (check(TypstTokenType::STRING)) {
    // String contains the accent character directly
    std::string str = lexer_.next().value;
    if (!str.empty()) {
      // Store the character - it will be decoded as UTF-8 in the layout
      accent_name = str;
    }
  }

  // Parse optional named parameters: size:, dotless:
  double accent_size = 0;  // 0 = default
  bool dotless = false;

  while (match(TypstTokenType::COMMA)) {
    if (check(TypstTokenType::IDENTIFIER)) {
      std::string param = lexer_.next().value;
      if (match(TypstTokenType::COLON)) {
        if (param == "size") {
          if (check(TypstTokenType::NUMBER)) {
            accent_size = safe_stod(lexer_.next().value);
            // Check for % suffix
            if (check(TypstTokenType::OPERATOR) && lexer_.peek().value == "%") {
              lexer_.consume();
              accent_size /= 100.0;  // convert to fraction
            } else {
              // Assume it's already a multiplier if no %
            }
          }
        } else if (param == "dotless") {
          if (check(TypstTokenType::IDENTIFIER)) {
            std::string val = lexer_.next().value;
            dotless = (val == "true");
          }
        }
      }
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after accent arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->accent_size = accent_size;
  node->dotless = dotless;

  // Check if it's a known named accent
  if (accent_functions.count(accent_name) > 0) {
    node->command = accent_name;
  } else if (wide_accent_functions.count(accent_name) > 0) {
    node->command = accent_name;
    node->wide = true;
  } else {
    // Direct character - store in value field for layout to handle
    node->value = accent_name;
  }

  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_style(const std::string& name) {
  consume(TypstTokenType::LPAREN, "Expected '(' after style function");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after style argument");

  auto node = std::make_unique<MathNode>(NodeType::STYLED);
  auto it = style_functions.find(name);
  if (it != style_functions.end()) {
    node->style = it->second;
  }
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_lr() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'lr'");

  double explicit_size = 0;  // 0 means auto
  std::string size_unit;     // "em", "pt", or "relative" (for %)

  // Check for size: parameter first
  if (check(TypstTokenType::IDENTIFIER) && lexer_.peek().value == "size") {
    lexer_.consume();  // consume "size"
    if (match(TypstTokenType::COLON)) {
      // Parse size value - number followed by optional unit
      if (check(TypstTokenType::NUMBER)) {
        std::string num_str = lexer_.next().value;
        explicit_size = safe_stod(num_str);
        size_unit = "em";  // default to em

        // Check for unit suffix
        if (check(TypstTokenType::IDENTIFIER)) {
          std::string unit = lexer_.peek().value;
          if (unit == "em") {
            lexer_.consume();
            size_unit = "em";
          } else if (unit == "pt") {
            lexer_.consume();
            size_unit = "pt";
          } else if (unit == "mm") {
            lexer_.consume();
            explicit_size *= 2.834645669;  // convert mm to pt
            size_unit = "pt";
          } else if (unit == "cm") {
            lexer_.consume();
            explicit_size *= 28.34645669;  // convert cm to pt
            size_unit = "pt";
          } else if (unit == "in") {
            lexer_.consume();
            explicit_size *= 72.0;  // convert in to pt
            size_unit = "pt";
          }
        } else if (check(TypstTokenType::OPERATOR) && lexer_.peek().value == "%") {
          lexer_.consume();
          explicit_size /= 100.0;  // convert to fraction
          size_unit = "relative";
        }
      }
      // Skip comma after size parameter
      match(TypstTokenType::COMMA);
    }
  }

  // Parse the opening delimiter
  std::string left_delim;
  if (check(TypstTokenType::LPAREN)) {
    left_delim = "(";
    lexer_.consume();
  } else if (check(TypstTokenType::LBRACKET)) {
    left_delim = "[";
    lexer_.consume();
  } else if (check(TypstTokenType::LBRACE)) {
    left_delim = "{";
    lexer_.consume();
  } else if (check(TypstTokenType::OPERATOR)) {
    left_delim = lexer_.next().value;
  } else {
    left_delim = "(";
  }

  auto content = parse_math_list();

  // parse_math_list greedily consumes operator characters, so a closing
  // delimiter that lexes as an operator (e.g. | in lr(|x|)) ends up as the
  // last content node. Reclaim it when the closing paren follows directly.
  std::string right_delim;
  if (content && check(TypstTokenType::RPAREN)) {
    MathNode* tail = content.get();
    if (tail->type == NodeType::ROW && !tail->children.empty()) {
      tail = tail->children.back().get();
    }
    if (tail->type == NodeType::SYMBOL &&
        (tail->value == "|" || tail->value == "<" || tail->value == ">" ||
         tail->value == "/")) {
      right_delim = tail->value;
      if (content->type == NodeType::ROW) {
        content->children.pop_back();
      } else {
        content = std::make_unique<MathNode>(NodeType::GROUP);
      }
    }
  }

  // Parse the closing delimiter
  if (!right_delim.empty()) {
    // Already reclaimed from the content above
  } else if (check(TypstTokenType::RPAREN)) {
    right_delim = ")";
    lexer_.consume();
  } else if (check(TypstTokenType::RBRACKET)) {
    right_delim = "]";
    lexer_.consume();
  } else if (check(TypstTokenType::RBRACE)) {
    right_delim = "}";
    lexer_.consume();
  } else if (check(TypstTokenType::OPERATOR)) {
    right_delim = lexer_.next().value;
  } else {
    right_delim = ")";
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after lr");

  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);
  node->left_delim = left_delim;
  node->right_delim = right_delim;
  node->delim_height = explicit_size;
  node->delim_height_unit = size_unit;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_abs() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'abs'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after abs argument");

  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);
  node->left_delim = "|";
  node->right_delim = "|";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_norm() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'norm'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after norm argument");

  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);
  node->left_delim = "Vert";
  node->right_delim = "Vert";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_floor() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'floor'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after floor argument");

  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);
  node->left_delim = "lfloor";
  node->right_delim = "rfloor";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_ceil() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'ceil'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after ceil argument");

  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);
  node->left_delim = "lceil";
  node->right_delim = "rceil";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_round() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'round'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after round argument");

  auto node = std::make_unique<MathNode>(NodeType::DELIMITED);
  node->left_delim = "(";
  node->right_delim = ")";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_mid() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'mid'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after mid argument");

  // Create a symbol node for the mid delimiter
  // It will be scaled to match the enclosing lr() delimiter height
  auto node = std::make_unique<MathNode>(NodeType::SYMBOL, "\xE2\x88\xA3");  // ∣ U+2223
  return node;
}

MathNodePtr TypstParser::parse_primes() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'primes'");

  // Parse the count argument
  int count = 1;
  if (check(TypstTokenType::NUMBER)) {
    std::string num_str = lexer_.next().value;
    count = safe_stoi(num_str);
    if (count < 1) count = 1;
    if (count > 4) count = 4;  // Limit to quadruple prime
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after primes argument");

  // Use dedicated Unicode glyphs for prime combinations
  std::string prime_char;
  if (count == 1) {
    prime_char = "\xE2\x80\xB2";  // ′ U+2032
  } else if (count == 2) {
    prime_char = "\xE2\x80\xB3";  // ″ U+2033
  } else if (count == 3) {
    prime_char = "\xE2\x80\xB4";  // ‴ U+2034
  } else {
    prime_char = "\xE2\x81\x97";  // ⁗ U+2057
  }

  return std::make_unique<MathNode>(NodeType::SYMBOL, prime_char);
}

// Under/over bracket variant functions
// These all follow the same pattern as overbrace/underbrace but with different glyphs

MathNodePtr TypstParser::parse_overbracket() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'overbracket'");
  auto content = parse_math_list();

  MathNodePtr annotation;
  if (match(TypstTokenType::COMMA)) {
    annotation = parse_math_list();
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after overbracket arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "overbracket";
  node->wide = true;
  node->add_child(std::move(content));
  if (annotation) {
    node->add_child(std::move(annotation));
  }
  return node;
}

MathNodePtr TypstParser::parse_underbracket() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'underbracket'");
  auto content = parse_math_list();

  MathNodePtr annotation;
  if (match(TypstTokenType::COMMA)) {
    annotation = parse_math_list();
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after underbracket arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "underbracket";
  node->wide = true;
  node->add_child(std::move(content));
  if (annotation) {
    node->add_child(std::move(annotation));
  }
  return node;
}

MathNodePtr TypstParser::parse_overparen() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'overparen'");
  auto content = parse_math_list();

  MathNodePtr annotation;
  if (match(TypstTokenType::COMMA)) {
    annotation = parse_math_list();
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after overparen arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "overparen";
  node->wide = true;
  node->add_child(std::move(content));
  if (annotation) {
    node->add_child(std::move(annotation));
  }
  return node;
}

MathNodePtr TypstParser::parse_underparen() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'underparen'");
  auto content = parse_math_list();

  MathNodePtr annotation;
  if (match(TypstTokenType::COMMA)) {
    annotation = parse_math_list();
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after underparen arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "underparen";
  node->wide = true;
  node->add_child(std::move(content));
  if (annotation) {
    node->add_child(std::move(annotation));
  }
  return node;
}

MathNodePtr TypstParser::parse_overshell() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'overshell'");
  auto content = parse_math_list();

  MathNodePtr annotation;
  if (match(TypstTokenType::COMMA)) {
    annotation = parse_math_list();
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after overshell arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "overshell";
  node->wide = true;
  node->add_child(std::move(content));
  if (annotation) {
    node->add_child(std::move(annotation));
  }
  return node;
}

MathNodePtr TypstParser::parse_undershell() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'undershell'");
  auto content = parse_math_list();

  MathNodePtr annotation;
  if (match(TypstTokenType::COMMA)) {
    annotation = parse_math_list();
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after undershell arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "undershell";
  node->wide = true;
  node->add_child(std::move(content));
  if (annotation) {
    node->add_child(std::move(annotation));
  }
  return node;
}

MathNodePtr TypstParser::parse_mat(const std::string& name) {
  consume(TypstTokenType::LPAREN, "Expected '(' after matrix function");

  auto node = std::make_unique<MathNode>(NodeType::MATRIX);

  // Map Typst matrix function names to LaTeX matrix environment names
  // mat  -> matrix   (no delimiters)
  // pmat -> pmatrix  (parentheses)
  // bmat -> bmatrix  (square brackets)
  // vmat -> vmatrix  (vertical bars)
  // Vmat -> Vmatrix  (double vertical bars)
  // Bmat -> Bmatrix  (curly braces)
  if (name == "mat") {
    node->matrix_type = "matrix";
  } else if (name == "pmat") {
    node->matrix_type = "pmatrix";
  } else if (name == "bmat") {
    node->matrix_type = "bmatrix";
  } else if (name == "vmat") {
    node->matrix_type = "vmatrix";
  } else if (name == "Vmat") {
    node->matrix_type = "Vmatrix";
  } else if (name == "Bmat") {
    node->matrix_type = "Bmatrix";
  } else {
    node->matrix_type = "pmatrix";  // default
  }

  // Parse optional named parameters before matrix elements
  // augment: #(vline: N, hline: M) or augment: N (shorthand for vline)
  // row-gap: Nem, column-gap: Nem
  while (check(TypstTokenType::IDENTIFIER)) {
    std::string param_name = lexer_.peek().value;

    // Check if this is a known parameter name that could be followed by ':'
    // Note: row-gap and column-gap are tokenized as "row" "-" "gap" so check first part
    bool is_param = (param_name == "augment" || param_name == "rowgap" ||
                     param_name == "columngap" || param_name == "gap" ||
                     param_name == "row" || param_name == "column");
    if (!is_param) {
      // Not a parameter name - stop looking for parameters
      break;
    }

    // Consume the identifier
    lexer_.consume();

    // Check for hyphenated names: "row-gap" or "column-gap"
    // ('-' is converted to U+2212 by the shorthand table, so check both)
    if ((param_name == "row" || param_name == "column") &&
        check(TypstTokenType::OPERATOR) &&
        (lexer_.peek().value == "-" || lexer_.peek().value == "\xE2\x88\x92")) {
      lexer_.consume();  // consume '-'
      if (check(TypstTokenType::IDENTIFIER) && lexer_.peek().value == "gap") {
        param_name += "-gap";
        lexer_.consume();  // consume 'gap'
      } else {
        // Just "row" or "column" without "-gap" - not a known parameter, break
        break;
      }
    } else if (param_name == "row" || param_name == "column") {
      // Just "row" or "column" without hyphen - not a known parameter, break
      break;
    }

    if (!match(TypstTokenType::COLON)) {
      // Not a named parameter - this shouldn't happen for known params
      // but break anyway (token is lost, which is a bug but rare)
      break;
    }

    if (param_name == "augment") {
      // Parse augment value - can be a number or a dictionary #(vline: N, hline: M)
      if (check(TypstTokenType::NUMBER)) {
        // Simple number = vline position
        int vline = safe_stoi(lexer_.next().value);
        node->augment_vlines.push_back(vline);
      } else if (check(TypstTokenType::OPERATOR) && lexer_.peek().value == "#") {
        // Dictionary syntax: #(vline: N, hline: M)
        lexer_.consume();  // consume '#'
        if (match(TypstTokenType::LPAREN)) {
          while (!check(TypstTokenType::RPAREN) && !lexer_.at_end()) {
            if (check(TypstTokenType::IDENTIFIER)) {
              std::string key = lexer_.next().value;
              if (match(TypstTokenType::COLON)) {
                auto read_int = [&]() -> int {
                  // Allow negative values (the '-' is converted to U+2212 by
                  // the shorthand table, so check both)
                  bool negative = false;
                  if (check(TypstTokenType::OPERATOR) &&
                      (lexer_.peek().value == "-" || lexer_.peek().value == "\xE2\x88\x92")) {
                    lexer_.consume();
                    negative = true;
                  }
                  if (!check(TypstTokenType::NUMBER)) {
                    return 0;
                  }
                  int val = safe_stoi(lexer_.next().value);
                  return negative ? -val : val;
                };
                auto record = [&](int val) {
                  if (key == "vline") {
                    node->augment_vlines.push_back(val);
                  } else if (key == "hline") {
                    node->augment_hlines.push_back(val);
                  }
                };
                if (check(TypstTokenType::NUMBER) || check(TypstTokenType::OPERATOR)) {
                  record(read_int());
                } else if (check(TypstTokenType::LPAREN)) {
                  // Array of positions: (1, 2, -3)
                  lexer_.consume();
                  while (!check(TypstTokenType::RPAREN) && !lexer_.at_end()) {
                    if (check(TypstTokenType::NUMBER) ||
                        (check(TypstTokenType::OPERATOR) &&
                         (lexer_.peek().value == "-" || lexer_.peek().value == "\xE2\x88\x92"))) {
                      record(read_int());
                    } else if (!match(TypstTokenType::COMMA)) {
                      lexer_.consume();  // guarantee progress
                    }
                  }
                  match(TypstTokenType::RPAREN);
                }
              }
              match(TypstTokenType::COMMA);  // skip commas
            } else if (!match(TypstTokenType::COMMA)) {
              // Guarantee progress on unexpected tokens
              error("Unexpected token in augment dictionary");
              lexer_.consume();
            }
          }
          match(TypstTokenType::RPAREN);
        }
      }
      match(TypstTokenType::COMMA);  // consume trailing comma
    } else if (param_name == "row-gap" || param_name == "rowgap") {
      if (check(TypstTokenType::NUMBER)) {
        double gap = safe_stod(lexer_.next().value);
        // Check for unit
        if (check(TypstTokenType::IDENTIFIER)) {
          std::string unit = lexer_.peek().value;
          if (unit == "em") {
            lexer_.consume();
          } else if (unit == "pt") {
            lexer_.consume();
            gap /= 12.0;  // convert pt to em (approximate)
          }
        }
        node->row_gap = gap;
      }
      match(TypstTokenType::COMMA);
    } else if (param_name == "column-gap" || param_name == "columngap" || param_name == "gap") {
      if (check(TypstTokenType::NUMBER)) {
        double gap = safe_stod(lexer_.next().value);
        // Check for unit
        if (check(TypstTokenType::IDENTIFIER)) {
          std::string unit = lexer_.peek().value;
          if (unit == "em") {
            lexer_.consume();
          } else if (unit == "pt") {
            lexer_.consume();
            gap /= 12.0;  // convert pt to em (approximate)
          }
        }
        node->column_gap = gap;
      }
      match(TypstTokenType::COMMA);
    } else {
      // Unknown parameter - this might be a matrix element
      // Break and handle as content
      break;
    }
  }

  std::vector<std::vector<MathNodePtr>> rows;
  std::vector<MathNodePtr> current_row;

  while (!check(TypstTokenType::RPAREN) && !lexer_.at_end()) {
    auto cell = parse_math_list();
    // Keep empty cells so column alignment isn't lost
    if (cell) {
      current_row.push_back(std::move(cell));
    } else {
      current_row.push_back(std::make_unique<MathNode>(NodeType::GROUP));
    }

    if (match(TypstTokenType::COMMA)) {
      // Next cell in row
      continue;
    } else if (match(TypstTokenType::SEMICOLON)) {
      // End of row
      rows.push_back(std::move(current_row));
      current_row.clear();
    } else {
      break;
    }
  }

  // Add final row if not empty
  if (!current_row.empty()) {
    rows.push_back(std::move(current_row));
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after matrix");

  // Set dimensions from the widest row and pad ragged rows with empty cells
  node->rows = static_cast<int>(rows.size());
  node->cols = 0;
  for (const auto& row : rows) {
    node->cols = std::max(node->cols, static_cast<int>(row.size()));
  }

  // Add all cells as children (row by row)
  for (auto& row : rows) {
    for (auto& cell : row) {
      node->add_child(std::move(cell));
    }
    for (int i = static_cast<int>(row.size()); i < node->cols; i++) {
      node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
    }
  }

  return node;
}

MathNodePtr TypstParser::parse_cases() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'cases'");

  auto node = std::make_unique<MathNode>(NodeType::MATRIX);
  node->matrix_type = "cases";

  std::vector<std::vector<MathNodePtr>> rows;
  std::vector<MathNodePtr> current_row;

  while (!check(TypstTokenType::RPAREN) && !lexer_.at_end()) {
    // Stop cell parsing at & so it can act as the condition separator
    auto cell = parse_math_list(true);
    if (cell) {
      current_row.push_back(std::move(cell));
    } else {
      current_row.push_back(std::make_unique<MathNode>(NodeType::GROUP));
    }

    if (match(TypstTokenType::AMPERSAND)) {
      // Next cell in row (condition)
      continue;
    } else if (match(TypstTokenType::COMMA)) {
      // End of row
      rows.push_back(std::move(current_row));
      current_row.clear();
    } else {
      break;
    }
  }

  // Add final row
  if (!current_row.empty()) {
    rows.push_back(std::move(current_row));
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after cases");

  node->rows = static_cast<int>(rows.size());
  node->cols = 0;
  for (const auto& row : rows) {
    node->cols = std::max(node->cols, static_cast<int>(row.size()));
  }

  for (auto& row : rows) {
    for (auto& cell : row) {
      node->add_child(std::move(cell));
    }
    for (int i = static_cast<int>(row.size()); i < node->cols; i++) {
      node->add_child(std::make_unique<MathNode>(NodeType::GROUP));
    }
  }

  return node;
}

MathNodePtr TypstParser::parse_vec() {
  // vec(x) is just an accent
  consume(TypstTokenType::LPAREN, "Expected '(' after 'vec'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after vec argument");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "vec";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_cancel() {
  consume(TypstTokenType::LPAREN, "Expected '(' after cancel function");

  // Parse first argument (the content to cancel)
  auto content = parse_math_list();

  // Default cancel type
  std::string cancel_type = "cancel";

  // Check for named arguments: inverted: true, cross: true
  while (match(TypstTokenType::COMMA)) {
    // Skip whitespace and look for identifier
    if (check(TypstTokenType::IDENTIFIER)) {
      std::string arg_name = lexer_.next().value;
      if (match(TypstTokenType::COLON)) {
        // Parse argument value
        if (check(TypstTokenType::IDENTIFIER)) {
          std::string arg_value = lexer_.next().value;
          if (arg_name == "inverted" && arg_value == "true") {
            cancel_type = "bcancel";  // backward cancel
          } else if (arg_name == "cross" && arg_value == "true") {
            cancel_type = "xcancel";  // X-cancel
          }
        }
      }
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after cancel argument");

  auto node = std::make_unique<MathNode>(NodeType::CANCEL);
  node->cancel_type = cancel_type;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_hide() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'hide'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after hide argument");

  auto node = std::make_unique<MathNode>(NodeType::PHANTOM);
  node->phantom_type = "full";
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_limits() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'limits'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after limits argument");

  // Mark the content as having forced limits
  if (content && content->type == NodeType::BIG_OP) {
    content->limits_override = 1;
  }
  return content;
}

MathNodePtr TypstParser::parse_scripts_func() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'scripts'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after scripts argument");

  // Mark the content as having forced nolimits
  if (content && content->type == NodeType::BIG_OP) {
    content->limits_override = -1;
  }
  return content;
}

// Helper to parse cramped parameter: cramped: true
static bool parse_cramped_param(TypstLexer& lexer) {
  bool cramped = false;
  while (lexer.peek().type == TypstTokenType::COMMA) {
    lexer.consume();  // consume comma
    if (lexer.peek().type == TypstTokenType::IDENTIFIER &&
        lexer.peek().value == "cramped") {
      lexer.consume();  // consume "cramped"
      if (lexer.peek().type == TypstTokenType::COLON) {
        lexer.consume();  // consume ":"
        if (lexer.peek().type == TypstTokenType::IDENTIFIER &&
            lexer.peek().value == "true") {
          lexer.consume();
          cramped = true;
        } else if (lexer.peek().type == TypstTokenType::IDENTIFIER &&
                   lexer.peek().value == "false") {
          lexer.consume();
          cramped = false;
        }
      }
    }
  }
  return cramped;
}

MathNodePtr TypstParser::parse_display() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'display'");
  auto content = parse_math_list();
  bool cramped = parse_cramped_param(lexer_);
  consume(TypstTokenType::RPAREN, "Expected ')' after display argument");

  auto node = std::make_unique<MathNode>(NodeType::STYLE_CHANGE);
  node->display_style = MathDisplayStyle::DISPLAY;
  node->cramped = cramped;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_inline() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'inline'");
  auto content = parse_math_list();
  bool cramped = parse_cramped_param(lexer_);
  consume(TypstTokenType::RPAREN, "Expected ')' after inline argument");

  auto node = std::make_unique<MathNode>(NodeType::STYLE_CHANGE);
  node->display_style = MathDisplayStyle::TEXT;
  node->cramped = cramped;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_script_style() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'script'");
  auto content = parse_math_list();
  bool cramped = parse_cramped_param(lexer_);
  consume(TypstTokenType::RPAREN, "Expected ')' after script argument");

  auto node = std::make_unique<MathNode>(NodeType::STYLE_CHANGE);
  node->display_style = MathDisplayStyle::SCRIPT;
  node->cramped = cramped;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_sscript_style() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'sscript'");
  auto content = parse_math_list();
  bool cramped = parse_cramped_param(lexer_);
  consume(TypstTokenType::RPAREN, "Expected ')' after sscript argument");

  auto node = std::make_unique<MathNode>(NodeType::STYLE_CHANGE);
  node->display_style = MathDisplayStyle::SCRIPTSCRIPT;
  node->cramped = cramped;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_op() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'op'");

  std::string name;
  if (check(TypstTokenType::STRING)) {
    name = lexer_.next().value;
  } else {
    auto content = parse_math_list();
    if (content && content->type == NodeType::SYMBOL) {
      name = content->value;
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after op argument");

  auto node = std::make_unique<MathNode>(NodeType::OPERATORNAME);
  node->value = name;
  return node;
}

MathNodePtr TypstParser::parse_attach() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'attach'");

  auto base = parse_math_list();

  // Six attachment positions
  MathNodePtr t = nullptr;   // top (above)
  MathNodePtr b = nullptr;   // bottom (below)
  MathNodePtr tl = nullptr;  // top-left
  MathNodePtr tr = nullptr;  // top-right
  MathNodePtr bl = nullptr;  // bottom-left
  MathNodePtr br = nullptr;  // bottom-right

  // Look for named arguments
  while (match(TypstTokenType::COMMA)) {
    if (check(TypstTokenType::IDENTIFIER)) {
      std::string arg_name = lexer_.next().value;
      if (match(TypstTokenType::COLON)) {
        if (arg_name == "t" || arg_name == "top") {
          t = parse_math_list();
        } else if (arg_name == "b" || arg_name == "bottom" || arg_name == "bot") {
          b = parse_math_list();
        } else if (arg_name == "tl") {
          tl = parse_math_list();
        } else if (arg_name == "tr") {
          tr = parse_math_list();
        } else if (arg_name == "bl") {
          bl = parse_math_list();
        } else if (arg_name == "br") {
          br = parse_math_list();
        }
      }
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after attach arguments");

  // If any corner position is used, create a MULTISCRIPT node
  if (tl || tr || bl || br) {
    auto node = std::make_unique<MathNode>(NodeType::MULTISCRIPT);
    // Children convention: [base, t, b, tl, tr, bl, br]
    node->add_child(std::move(base));
    node->add_child(t ? std::move(t) : nullptr);
    node->add_child(b ? std::move(b) : nullptr);
    node->add_child(tl ? std::move(tl) : nullptr);
    node->add_child(tr ? std::move(tr) : nullptr);
    node->add_child(bl ? std::move(bl) : nullptr);
    node->add_child(br ? std::move(br) : nullptr);
    return node;
  }

  // Fall back to STACK for simple t/b cases (backwards compatible)
  if (t && b) {
    // Nest overset inside underset; layout_stack has no three-way "attach"
    auto over = std::make_unique<MathNode>(NodeType::STACK);
    over->stack_type = "overset";
    over->add_child(std::move(t));
    over->add_child(std::move(base));
    auto node = std::make_unique<MathNode>(NodeType::STACK);
    node->stack_type = "underset";
    node->add_child(std::move(b));
    node->add_child(std::move(over));
    return node;
  } else if (t) {
    auto node = std::make_unique<MathNode>(NodeType::STACK);
    node->stack_type = "overset";
    node->add_child(std::move(t));
    node->add_child(std::move(base));
    return node;
  } else if (b) {
    auto node = std::make_unique<MathNode>(NodeType::STACK);
    node->stack_type = "underset";
    node->add_child(std::move(b));
    node->add_child(std::move(base));
    return node;
  }

  return base;
}

MathNodePtr TypstParser::parse_overbrace() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'overbrace'");
  auto content = parse_math_list();

  MathNodePtr label = nullptr;
  if (match(TypstTokenType::COMMA)) {
    // Parse label - could be a string or expression
    if (check(TypstTokenType::STRING)) {
      label = std::make_unique<MathNode>(NodeType::TEXT, lexer_.next().value);
    } else {
      label = parse_math_list();
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after overbrace arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "overbrace";
  node->wide = true;
  node->add_child(std::move(content));
  if (label) {
    // Create a superscript with the overbrace as base
    auto result = std::make_unique<MathNode>(NodeType::SUPERSCRIPT);
    result->add_child(std::move(node));
    result->add_child(std::move(label));
    return result;
  }
  return node;
}

MathNodePtr TypstParser::parse_underbrace() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'underbrace'");
  auto content = parse_math_list();

  MathNodePtr label = nullptr;
  if (match(TypstTokenType::COMMA)) {
    // Parse label - could be a string or expression
    if (check(TypstTokenType::STRING)) {
      label = std::make_unique<MathNode>(NodeType::TEXT, lexer_.next().value);
    } else {
      label = parse_math_list();
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after underbrace arguments");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "underbrace";
  node->wide = true;
  node->add_child(std::move(content));
  if (label) {
    // Create a subscript with the underbrace as base
    auto result = std::make_unique<MathNode>(NodeType::SUBSCRIPT);
    result->add_child(std::move(node));
    result->add_child(std::move(label));
    return result;
  }
  return node;
}

MathNodePtr TypstParser::parse_overline() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'overline'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after overline argument");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "overline";
  node->wide = true;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_underline() {
  consume(TypstTokenType::LPAREN, "Expected '(' after 'underline'");
  auto content = parse_math_list();
  consume(TypstTokenType::RPAREN, "Expected ')' after underline argument");

  auto node = std::make_unique<MathNode>(NodeType::ACCENT);
  node->command = "underline";
  node->wide = true;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_stretch() {
  // stretch(body, size: Nem)
  consume(TypstTokenType::LPAREN, "Expected '(' after 'stretch'");

  auto content = parse_math_list();

  double size_value = 0;
  std::string size_unit = "em";

  // Look for size: parameter
  while (match(TypstTokenType::COMMA)) {
    if (check(TypstTokenType::IDENTIFIER) && lexer_.peek().value == "size") {
      lexer_.consume();  // consume "size"
      if (match(TypstTokenType::COLON)) {
        // Parse size value - number followed by optional unit
        if (check(TypstTokenType::NUMBER)) {
          std::string num_str = lexer_.next().value;
          size_value = safe_stod(num_str);

          // Check for unit suffix
          if (check(TypstTokenType::IDENTIFIER)) {
            std::string unit = lexer_.peek().value;
            if (unit == "em") {
              lexer_.consume();
              size_unit = "em";
            } else if (unit == "pt") {
              lexer_.consume();
              size_unit = "pt";
            } else if (unit == "mm") {
              lexer_.consume();
              size_value *= 2.834645669;  // convert mm to pt
              size_unit = "pt";
            } else if (unit == "cm") {
              lexer_.consume();
              size_value *= 28.34645669;  // convert cm to pt
              size_unit = "pt";
            } else if (unit == "in") {
              lexer_.consume();
              size_value *= 72.0;  // convert in to pt
              size_unit = "pt";
            }
          } else if (check(TypstTokenType::OPERATOR) && lexer_.peek().value == "%") {
            lexer_.consume();
            size_value /= 100.0;  // convert to fraction
            size_unit = "relative";
          }
        }
      }
    }
  }

  consume(TypstTokenType::RPAREN, "Expected ')' after stretch arguments");

  auto node = std::make_unique<MathNode>(NodeType::STRETCHED);
  node->stretch_size = size_value;
  node->stretch_size_unit = size_unit;
  node->add_child(std::move(content));
  return node;
}

MathNodePtr TypstParser::parse_class() {
  // class("relation", body)
  consume(TypstTokenType::LPAREN, "Expected '(' after 'class'");

  // First argument: class name as a string
  std::string class_name;
  if (check(TypstTokenType::STRING)) {
    class_name = lexer_.next().value;
  } else if (check(TypstTokenType::IDENTIFIER)) {
    class_name = lexer_.next().value;
  }

  consume(TypstTokenType::COMMA, "Expected ',' after class name");

  auto content = parse_math_list();

  consume(TypstTokenType::RPAREN, "Expected ')' after class arguments");

  auto node = std::make_unique<MathNode>(NodeType::CLASS_OVERRIDE);
  node->math_class = class_name;
  node->add_child(std::move(content));
  return node;
}

// =============================================================================
// Classification helpers
// =============================================================================

bool TypstParser::is_typst_function(const std::string& name) const {
  return typst_functions.count(name) > 0 ||
         accent_functions.count(name) > 0 ||
         wide_accent_functions.count(name) > 0 ||
         style_functions.count(name) > 0;
}

bool TypstParser::is_accent_function(const std::string& name) const {
  return accent_functions.count(name) > 0 ||
         wide_accent_functions.count(name) > 0;
}

bool TypstParser::is_style_function(const std::string& name) const {
  return style_functions.count(name) > 0;
}

bool TypstParser::is_delimiter_function(const std::string& name) const {
  return name == "lr" || name == "abs" || name == "norm" ||
         name == "floor" || name == "ceil";
}

bool TypstParser::is_function_name(const std::string& name) const {
  return typst_math_functions.count(name) > 0;
}

bool TypstParser::is_big_operator(const std::string& name) const {
  return typst_big_operators.count(name) > 0;
}

bool TypstParser::is_greek_letter(const std::string& name) const {
  return math::is_greek_letter(name);  // Use existing function
}

bool TypstParser::is_symbol(const std::string& name) const {
  return command_to_codepoint(name) != 0;
}

// =============================================================================
// Syntax detection
// =============================================================================

// Known Typst function names that indicate Typst syntax (require parentheses)
static const std::unordered_set<std::string> typst_indicator_functions = {
  // These are distinctly Typst (not valid LaTeX without backslash)
  "frac", "binom", "sqrt", "root", "mat", "pmat", "bmat", "vmat",
  "lr", "abs", "norm", "floor", "ceil", "round", "mid", "cases", "vec",
  "bold", "italic", "upright", "sans", "mono", "cal", "frak", "bb", "serif", "scr",
  "display", "inline", "script", "sscript",
  "cancel", "strike", "hide", "op", "limits", "scripts", "attach", "primes",
  "overbrace", "underbrace", "overline", "underline",
  "overbracket", "underbracket", "overparen", "underparen", "overshell", "undershell",
  "widehat", "widetilde", "hat", "tilde", "dot", "ddot", "bar",
  "overrightarrow", "overleftarrow",
  "stretch", "class"
};

// Known Typst identifiers that indicate Typst syntax (no parentheses needed)
static const std::unordered_set<std::string> typst_indicator_symbols = {
  // Differential operators (Typst-specific names)
  "dif", "Dif", "dd",
  // Big operators with Typst-specific names
  "integral", "product",
  // Spacing keywords
  "thin", "med", "thick", "wide"
};

bool is_typst_syntax(const std::string& input) {
  // Strategy: Default to LaTeX (more common), only use Typst if we see clear
  // Typst indicators.
  //
  // LaTeX indicators:
  //   - \begin{ or \end{ (LaTeX environments)
  //   - \\ (LaTeX newline - double backslash)
  //   - _{ or ^{ (LaTeX script syntax with braces)
  //
  // Typst indicators:
  //   - function(  where function is a known Typst function
  //   - "..." quoted strings (Typst text syntax)
  //   - _( or ^( - Typst uses parens for scripts
  //   - dotted symbols (arrow.r, eq.not)
  //   - \( \) \[ \] \{ \} \| (Typst escaped delimiters)
  //   - \, \; (Typst escaped comma/semicolon)
  //   - \ followed by whitespace (Typst linebreak)

  size_t i = 0;
  // Tracks whether we've seen a LaTeX-style command (\ followed by a letter)
  // or a LaTeX-only space command (\! \:). \, \; and \<space> are valid in
  // both syntaxes, so they only indicate Typst when no LaTeX command was seen
  bool saw_latex_command = false;
  while (i < input.size()) {
    char c = input[i];

    // Check for backslash sequences
    if (c == '\\' && i + 1 < input.size()) {
      char next = input[i + 1];

      // \\ is LaTeX newline (double backslash)
      if (next == '\\') {
        return false;  // Definitely LaTeX
      }

      // \begin{ or \end{ are LaTeX environment markers
      if (next == 'b' || next == 'e') {
        std::string check;
        size_t j = i + 1;
        while (j < input.size() && ((input[j] >= 'a' && input[j] <= 'z') ||
                                     (input[j] >= 'A' && input[j] <= 'Z'))) {
          check += input[j];
          j++;
        }
        if ((check == "begin" || check == "end") && j < input.size() && input[j] == '{') {
          return false;  // Definitely LaTeX
        }
      }

      // \( \) \[ \] \{ \} \| are Typst escaped delimiters (non-scaling)
      if (next == '(' || next == ')' || next == '[' || next == ']' ||
          next == '{' || next == '}' || next == '|') {
        return true;  // Typst escaped delimiter syntax
      }

      // \, and \; are Typst escaped comma/semicolon (but thin spaces in
      // LaTeX, so only count them when no LaTeX command was seen)
      if (next == ',' || next == ';') {
        if (!saw_latex_command) {
          return true;  // Typst escape syntax
        }
        i += 2;
        continue;
      }

      // \ followed by whitespace/newline is Typst linebreak (but a space
      // command in LaTeX)
      if (next == ' ' || next == '\n' || next == '\r' || next == '\t') {
        if (!saw_latex_command) {
          return true;  // Typst linebreak syntax
        }
        i += 2;
        continue;
      }

      // \! and \: are LaTeX-only space commands
      if (next == '!' || next == ':') {
        saw_latex_command = true;
        i += 2;
        continue;
      }

      // Otherwise, backslash followed by letter could be either:
      // - LaTeX command (\frac, \alpha, etc.)
      // - Typst doesn't use \commands, so this is LaTeX indicator
      // But don't return false yet - look for stronger indicators first
      if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
        saw_latex_command = true;
      }
      i += 2;
      continue;
    }

    // Check for quoted string (Typst indicator)
    if (c == '"') {
      return true;  // Typst uses "..." for text
    }

    // Check for _( or ^( - Typst script syntax with parentheses
    if ((c == '_' || c == '^') && i + 1 < input.size() && input[i + 1] == '(') {
      return true;  // Typst uses _(...) and ^(...), LaTeX uses _{...} and ^{...}
    }

    // Check for _{ or ^{ - LaTeX script syntax with braces
    if ((c == '_' || c == '^') && i + 1 < input.size() && input[i + 1] == '{') {
      return false;  // LaTeX uses _{...} and ^{...}
    }

    // Check for identifier followed by '(' - could be Typst function
    // Also check for dotted identifiers (arrow.r, eq.not, etc.)
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      size_t start = i;
      while (i < input.size() &&
             ((input[i] >= 'a' && input[i] <= 'z') ||
              (input[i] >= 'A' && input[i] <= 'Z') ||
              (input[i] >= '0' && input[i] <= '9') ||
              input[i] == '_')) {
        i++;
      }

      // Check for dotted identifier (e.g., arrow.r, eq.not)
      while (i < input.size() && input[i] == '.' && i + 1 < input.size() &&
             ((input[i + 1] >= 'a' && input[i + 1] <= 'z') ||
              (input[i + 1] >= 'A' && input[i + 1] <= 'Z'))) {
        i++;  // consume dot
        while (i < input.size() &&
               ((input[i] >= 'a' && input[i] <= 'z') ||
                (input[i] >= 'A' && input[i] <= 'Z') ||
                (input[i] >= '0' && input[i] <= '9') ||
                input[i] == '_')) {
          i++;
        }
      }

      std::string ident = input.substr(start, i - start);

      // Check if it's a dotted symbol (definitely Typst)
      if (ident.find('.') != std::string::npos &&
          typst_dotted_symbols.count(ident) > 0) {
        return true;  // Typst dotted symbol
      }

      // Skip whitespace
      while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) {
        i++;
      }

      // Check if it's a known Typst-only symbol (no parentheses needed)
      if (typst_indicator_symbols.count(ident) > 0) {
        return true;  // Typst-specific symbol
      }

      // Check if followed by '(' and is a known Typst function
      if (i < input.size() && input[i] == '(' &&
          typst_indicator_functions.count(ident) > 0) {
        return true;  // Typst function call syntax
      }
      continue;
    }

    i++;
  }

  // Second pass: check if input contains \command patterns
  // If we found backslash followed by letters, it's likely LaTeX
  i = 0;
  while (i < input.size()) {
    if (input[i] == '\\' && i + 1 < input.size()) {
      char next = input[i + 1];
      if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
        return false;  // Has \command, likely LaTeX
      }
    }
    i++;
  }

  // No clear indicators found - default to LaTeX
  return false;
}

} // namespace math
