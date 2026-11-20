# textshaping 1.1.0

* Vertical text shaping is now supported through the `direction` argument of
  `shape_text()` and `text_width()`, which now also accepts `"ttb"` and
  `"ttb-ltr"` (columns progressing left-to-right, as in Mongolian; `"auto"`
  resolves required-vertical scripts like Mongolian and Phags-pa to it). The new `mixed` argument controls how horizontal-only
  runs (e.g. Latin) are set in vertical text: `"rotate"` shapes them
  horizontally and marks them for a 90 degree rotation (new `rotation` column
  in the `shape` output), `"upright"` uses the font's `vert`/`vrt2`
  substitutions, `"tate"` sets short digit runs horizontally within the
  column (tate-chu-yoko), `"combine"` extends this to arbitrary runs of up to
  4 characters (like CSS `text-combine-upright: all`; `"tate2"`/`"tate3"`
  and `"combine2"`/`"combine3"` cap the run at 2 or 3 digits/characters
  instead), and `"sideways"`
  shapes the whole string horizontally for rotation into the column (like
  CSS `text-orientation: sideways`). Vertical glyph placement follows the
  font's VORG origins and UAX#50 orientation classes, with Tr-class
  characters (brackets, quotes, dashes) using `vert`/`vrt2` glyphs when
  available. The `metrics` output gains a `direction` column
* Null glyphs now report the font their span resolved to in `font_path`,
  `font_index`, `font_size`, `ascender`, and `descender` (instead of an empty
  path and fallback metrics), so consumers can compute font-relative
  decorations for the reserved space. Since this removes the `font_path == ""`
  sentinel, the `shape` output gains an explicit logical `spacer` column
  marking null glyphs (`index` remains `NA` for them)
* The `shape` output gains `cell_x0`, `cell_y0`, `cell_x1`, and `cell_y1`
  columns giving the bounds of each glyph's cell (its advance slot along the
  main axis and its em box along the cross axis) in real coordinates, so
  consumers no longer need to reverse-engineer the direction- and
  rotation-dependent anchoring convention to draw per-cell decorations. Cells
  tile seamlessly within a run of text. The convention itself is now also
  documented in `?shape_text`
* Strings producing no glyphs (e.g. empty strings) now report zero `width`,
  `height`, bearings, and borders in their metrics instead of the full
  `max_width` constraint extent (plus a phantom line height), so layout
  engines deriving box sizes from content need no special-casing for empty
  blocks. Blocks containing bare line breaks keep their line height, as they
  represent deliberately empty lines
* Kinsoku shori (Japanese line-break prohibition) support, driven by UAX#14
  line-break classes and covering inseparable and number-binding sequences
  (¥100, 50%, ‥…). The new `kinsoku` argument of `shape_text()` selects
  `"strict"` (default), `"loose"` (like CSS `line-break: loose`), or `"none"`
* The new `kinsoku_style` argument of `shape_text()` selects how line-break
  prohibition violations are resolved: `"hang"` (ぶら下げ, default),
  `"push-out"` (追い出し), or `"push-in"` (追い込み)
* The trailing blank of fullwidth punctuation (。、！」etc.) now hangs outside
  the line box at line end per JLREQ line-end spacing rules, as does the
  leading blank of opening brackets at line start, so reported line widths
  and alignment use the ink edge
* The bearings reported in the `metrics` output of `shape_text()` now account
  for glyph offsets (e.g. combining accents)
* Variation coordinates attached to registered fonts (via
  `systemfonts::register_font()`) are now honoured during shaping
* Text-presentation-default emoji codepoints without an emoji variation
  selector now use the regular font fallback chain, with the emoji font only
  as a last resort
* Fixed load-time "undefined symbol" failures on systems whose harfbuzz
  headers and library don't match (#79)
* Added LaTeX and Typst math shaping
* `shape_text()` and `text_width()` gain a `variation` argument accepting
  `systemfonts::font_variation()` objects for setting variable font axes

## Crash and memory-safety fixes

* Fixed a crash (and potential infinite loop) when a forced line break was
  needed on a line too narrow for a single glyph, in both ltr and rtl
* Fixed a crash when shaping an empty string with justified alignment
* Fixed a crash when calculating the width of an empty string with
  `include_bearing = FALSE`
* Fixed a crash with distributed alignment on lines containing a single glyph
* Fixed heap corruption with more bidi embedding levels than the stack had
  room for; the stack now grows on demand
* Fixed use-after-free and double-free bugs in font loading on systems where
  the FreeType versions of systemfonts and textshaping differ
* Fixed an out-of-bounds read on truncated or invalid UTF-8 input
* Fixed a FreeType face leak in `get_font_features()`, and made font and
  library handles exception-safe
* Fixed a leak of the global shaper after package unload/reload
* Fixed a leak of the HarfBuzz font when hyphen substitution failed
* Handle-owning C++ classes can no longer be copied (preventing double frees)
* Guarded against undefined behaviour when converting non-finite or
  out-of-range doubles to integers
* Fixed potential out-of-bounds reads on fonts without a family name and on
  bitmap fonts reporting a zero strike height

## Shaping correctness fixes

* Fixed stale paragraph positions leaking from the shape cache into
  subsequent shaping when the same run appears at different positions in a
  paragraph
* Cache keys for shaping and bidi now compare actual string content, so a
  hash collision can no longer return another string's result
* Fixed emoji detection in left-to-right text
* Fixed an error ("Can't combine embeddings past termination point") when an
  rtl embedding ended a paragraph
* Fixed fallback sizing tables getting out of sync with the fallback font
  list, leading to wrong scaling, tracking, and sizes
* Fixed shaping failures leaving half-built embeddings behind
* Single-character strings now resolve their direction through the bidi
  algorithm, so `ltr` is reported correctly
* Fixed hyphen kerning being computed against a Unicode codepoint rather than
  a glyph id
* Fixed the line-break glyph advance being subtracted from the wrong end in
  rtl embeddings
* Fixed the glyph-per-line count in justified alignment being off by one
* Fixed the "largest strike" selection for bitmap fonts never being updated
* Feature tags not exactly 4 characters long are now padded or rejected
  instead of being read out of bounds
* The exported C API `ts_string_shape2()` now fills the `cluster` output and
  clears all outputs on error
* Fixed reading `include_bearing` from the wrong element and treating `NA` as
  `TRUE` in `text_width()`
* Fixed a potential crash when the last line of a paragraph ended in a hard
  line break
* Font and shaping failures during a paragraph are now reported as errors
  instead of silently dropping the failed text
* The bidi algorithm now falls back to neutral embeddings if analysis fails

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
