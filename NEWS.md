# textshaping (development version)

* Text-presentation-default emoji codepoints (e.g. U+2199) without an emoji
  variation selector that are not covered by the primary font are now shaped
  using the regular font fallback chain, with the emoji font only used as a
  last resort. This prevents text characters such as arrows from being
  replaced by color emoji (requires systemfonts >= 1.3.3, r-lib/ragg#217)

* configure now link-tests `hb_ft_font_create_referenced()` and falls back to
  the older two-argument `hb_ft_font_create()` when the symbol is missing from
  libharfbuzz, fixing load-time "undefined symbol" failures on systems whose
  harfbuzz headers and library don't match (#79)
* Add LaTeX and Typst math shaping (at least a large subset of it)
* Font variations (variable font axis coordinates) now travel through shaping.
  `shape_text()` and `text_width()` gain a `variation` argument accepting
  `systemfonts::font_variation()` objects. The C API has been extended with
  `textshaping::ver2::string_width()` and `textshaping::ver2::string_shape()`
  that take `FontSettings2`; the existing `FontSettings`-based entry points
  remain available unchanged during the transition.

## Crash and memory-safety fixes

* Fixed a crash (and a potential infinite loop) when a forced line break was
  needed on a line too narrow for a single glyph, both left-to-right and
  right-to-left
* Fixed a crash when shaping an empty string with justified alignment
* Fixed a crash when calculating the width of an empty string with
  `include_bearing = FALSE`
* Fixed a crash (integer division by zero) with distributed alignment on lines
  containing a single glyph
* Fixed a heap corruption when text contained more bidi embedding levels than
  the embedding stack had room for; the stack now grows on demand
* Fixed use-after-free and double-free bugs in font loading on systems where
  the FreeType versions of systemfonts and textshaping differ
* Fixed an out-of-bounds read when converting truncated or invalid UTF-8
  input to codepoints
* Fixed a format-string vulnerability where user-provided math input was
  passed as the format string of an error message
* Fixed FreeType face leaks in `get_font_features()` and in math layout, and
  made font and library handles exception-safe so they can't leak on error
  paths
* Fixed a leak of the global shaper after package unload/reload; C-callable
  entry points now lazily recreate it instead of risking a use-after-free
* Fixed a leak of the HarfBuzz font when hyphen substitution failed
* Handle-owning C++ classes can no longer be copied (which would have led to
  double frees)
* Guarded against undefined behaviour when converting non-finite or
  out-of-range doubles (widths, indents, sizes, etc.) to integers
* Fixed potential out-of-bounds reads on fonts without a family name and on
  bitmap fonts reporting a zero strike height

## Shaping correctness fixes

* The shape cache now stores glyph clusters relative to the run start, so
  cached shaping is correct when the same run appears at different positions
  in a paragraph; previously, stale paragraph positions could leak into
  subsequent shaping and lead to out-of-bounds reads
* Cache keys for shaping, bidi, and math layout now compare the actual string
  content rather than only a hash, so a hash collision can no longer silently
  return another string's result
* Fixed emoji handling in left-to-right text: emoji detection no longer
  relies on negating the embedding level (a no-op for level 0) and now uses
  the correct paragraph offset
* Fixed an error ("Can't combine embeddings past termination point") when an
  rtl embedding ended a paragraph; rtl runs are now merged in reverse order
  rather than reversing first
* Fixed fallback sizing tables getting out of sync with the fallback font
  list when an existing font was retried, leading to wrong scaling, tracking,
  and sizes
* Fixed shaping failures leaving half-built embeddings behind that could be
  indexed out of bounds downstream
* Single-character strings now resolve their direction through the bidi
  algorithm, so `ltr` is reported correctly
* Fixed hyphen kerning being computed against a Unicode codepoint rather than
  a glyph id
* Fixed the line-break glyph advance being subtracted from the wrong end in
  right-to-left embeddings
* Fixed the glyph-per-line count in justified alignment being off by one
* Fixed the "largest strike" selection for bitmap fonts never being updated
* Feature tags shorter or longer than 4 characters are now padded or rejected
  instead of being read out of bounds, and mismatched tag/setting lengths are
  caught early
* The exported C API `ts_string_shape2()` now fills the `cluster` output and
  clears all outputs on error
* Fixed reading `include_bearing` from the wrong element and treating `NA` as
  `TRUE` in `text_width()`
* Fixed a potential crash when the last line of a paragraph ended in a hard
  line break
* Font and shaping failures during a paragraph are now reported as errors
  when finalising shaping instead of silently dropping the failed text
* The bidi algorithm now falls back to neutral embeddings if analysis fails,
  rather than returning uninitialised embedding levels

## Math (LaTeX/Typst) fixes

* Fixed two infinite loops in the Typst parser (`{...}` groups with
  unexpected tokens and `mat(augment: #(...))` with non-identifier entries)
* Deeply nested math input now raises a parse error instead of crashing R
  with a stack overflow (both parsers are depth-limited)
* Typst backslash sequences now work: escaped delimiters (`\(`, `\,`, ...),
  and `\` line breaks, which were previously dead code
* The Typst parser now reports unexpected trailing content instead of
  silently dropping it (e.g. after `,`, `:`, `;`, or non-ASCII input)
* Typst primes (`f'(x)`), parenthesised comma lists (`f(x, y)`), subscripts
  on identifiers (`x_1`), and multi-letter identifiers (`xy`) now parse
  correctly
* `root(n, x)` no longer swaps the index and radicand
* Ragged and empty cells in Typst `mat()`/`cases()` no longer shift or lose
  columns, and `cases()` keeps its condition column
* Typst `row-gap:`/`column-gap:` matrix parameters now work ('-' was being
  rewritten to a minus sign before the parameter name was checked)
* `mat(augment:)` now accepts negative values and arrays of positions
* `lr(|x|)` no longer swallows the closing delimiter into the content
* `attach(x, t:, b:)` now renders instead of producing nothing
* Parentheses used for grouping in Typst scripts (`x^(a+b)`) are stripped as
  Typst does
* LaTeX `\displaystyle` (and friends) now stop at `\right`, `&`, `\\`, and
  `\end` instead of swallowing the rest of the group
* LaTeX `\text{}` and `\operatorname{}` now preserve spaces and backslashes
  by copying the raw input
* LaTeX non-ASCII input is now rendered instead of silently dropped
* LaTeX `f'^2` now puts the prime and the `2` in the same superscript in the
  right order
* Fixed `get_math_glyph_variants()` and `get_math_glyph_assembly()` always
  returning empty results
* Glyph assemblies from MATH tables are now capped in size, centred around
  the middle piece, and no longer risk undefined behaviour on infinite or
  huge target sizes
* `\rule` no longer accepts infinite or NaN dimensions
* Text in `\text{}`, strings, and operator names is now decoded as UTF-8
  instead of byte by byte
* Em-based spacing and matrix gaps in math layout now scale with the device
  resolution, matching the glyph metrics
* Typst `phi`/`epsilon` and their `.alt` variants now follow Typst's
  conventions rather than LaTeX's, and a duplicate `diamond.stroked` symbol
  table key was removed
* Font variations and MATH table queries now handle the `res` argument and
  guard against invalid font sizes
* The LaTeX/Typst syntax detector no longer misclassifies LaTeX thin spaces
  (`\,`, `\;`, `\!`, `\:`) as Typst escapes when LaTeX commands are present

## Build fixes

* Fixed the `NO_HARFBUZZ_FRIBIDI` fallback build (stub signature mismatches
  and an undeclared constant)
* Removed the `all: clean` rule from `Makevars.in` that forced full rebuilds
  on every compile and was racy under parallel make
* Windows build now defines `NDEBUG` and `FRIBIDI_LIB_STATIC` consistently
  with the Unix build

# textshaping 1.0.5

* Fixed a bug when reverting back from one fallback to the previous one (#76)

# textshaping 1.0.4

* Guard against issues related to incompatible versions of freetype in
  systemfonts and textshaping (#68).

# textshaping 1.0.3

* Fixed a signed integer overflow in the fix for ragg#193

# textshaping 1.0.2

* Fixed a bug in bidi embedding arrangement when shaping a single line
  (ggplot2#6497)
* Fixed a bug in shape caching due to a weak vector hash implementation
  (ragg#193)
* Fixed a bug in line positioning when line containes mix of different sizes

# textshaping 1.0.1

* Fixed a bug where hard line breaks where ignored if the line consisted of
  multiple embeddings (marquee#58)
* Fixed a bug where newline characters would increase the width of a line and
  lead to an empty line getting inserted in some situations (marquee#55)

# textshaping 1.0.0

* Added `lorem_text()` and `lorem_bidi()` for generating nonsense text in
  various scripts
* Added `plot_shape()` to plot the result of a shaping along with the metrics
* Rewrite of the shaping engine to honor global direction of text. It introduces
  a `direction` argument to `shape_text()` that defaults to `auto`, meaning that
  it is deduced from the content of the shaped text. `align` gets two new
  settings that responds to the global direction of the text. `"auto"` will
  chose between `"left"` and `"right"` and `"justified"` will choose between
  `"justified-left"` and `"justified-right"` depending of the global direction
  is ltr or rtl. Lastly the soft break locations are now based on ICU and thus
  better support ideographic scripts such as Han/Kanji/Hangul.
* Textshaping now properly supports soft hyphens in that a hyphen is rendered if
  a soft-wrap happens at a soft hyphen (#52)

# textshaping 0.4.1

* Make compiled code somewhat less assumptive about the correctness of the input
* Fix a bug from too aggressive early exiting shaping of strings with no max
  width (#45)
* Fixed a mismatch between the default values of the `width` argument in
  `shape_text()` and `systemfonts::match_fonts()` (#44)
* Updated `text_width()` to take the same inputs as `shape_text()`

# textshaping 0.4.0

* Full rewrite of `shape_text()` to allow proper font-fallback, bidi text
  support, support for font-features, spacers, new align settings, etc.

# textshaping 0.3.7

* Prepare for Arm Windows

# textshaping 0.3.6

* Fix a bug in fallback font loading which would crash the process if the font
  failed to load (#23)
* Fixed bug that would reset fallback to the original font for short strings
  (#25)

# textshaping 0.3.5

* Address an UBCSAN issue in packages linking to textshaping
* Remove a few compiler warnings

# textshaping 0.3.4

* Prepare textshaping for UCRT support
* Address upstream changes in cpp11

# textshaping 0.3.3

* Support static linking on macOS (#17, @jeroen)

# textshaping 0.3.2

* Avoid overindexing fallbacks when no fallback is found

# textshaping 0.3.1

* Try to avoid ASAN issue reported by CRAN

# textshaping 0.3.0

* Add support for performing font fallback as part of the single-line shaping
* Provide support for non-scalable fonts

# textshaping 0.2.1

* Fix issues with the Solaris mock solution

# textshaping 0.2.0

* Update C API to prepare for font fallback
* Make sure it compiles on Solaris without system dependencies

# textshaping 0.1.2

* Fix a bug in the interaction with the systemfonts font cache that could cause
  random crashes on some mac installations.

# textshaping 0.1.1

* Small changes to comply with next cpp11 version

# textshaping 0.1.0

* First release. Provide access to HarfBuzz shaping and FriBidi bidirectional
  script support.
