#' Calculate glyph positions for strings
#'
#' Performs advanced text shaping of strings including font fallbacks,
#' bidirectional script support, word wrapping and various character and
#' paragraph level formatting settings.
#'
#' @param strings A character vector of strings to shape
#' @param id A vector grouping the strings together. If strings share an id the
#' shaping will continue between strings
#' @inheritParams systemfonts::match_fonts
#' @param features A [systemfonts::font_feature()] object or a list of them,
#' giving the OpenType font features to set
#' @param variation A [systemfonts::font_variation()] object or a list of them,
#' giving the coordinates on the variable font axes to shape with. Settings in
#' `variation` take precedence over the corresponding `italic`, `weight`, and
#' `width` arguments during shaping.
#' @param size The size in points to use for the font
#' @param res The resolution to use when doing the shaping. Should optimally
#' match the resolution used when rendering the glyphs.
#' @param lineheight A multiplier for the lineheight
#' @param align Within text box alignment, either `'auto'`, `'left'`, `'center'`,
#' `'right'`, `'justified'`, `'justified-left'`, `'justified-right'`,
#' `'justified-center'`, or `'distributed'`. `'auto'` and `'justified'` will
#' chose the left or right version depending on the direction of the text.
#' @param hjust,vjust The justification of the textbox surrounding the text
#' @param max_width The requested with of the string in inches. Setting this to
#' something other than `NA` will turn on word wrapping.
#' @param tracking Tracking of the glyphs (space adjustment) measured in 1/1000
#' em.
#' @param indent The indent of the first line in a paragraph measured in inches.
#' @param hanging The indent of the remaining lines in a paragraph measured in
#' inches.
#' @param space_before,space_after The spacing above and below a paragraph,
#' measured in points
#' @param direction The overall directional flow of the text. The default
#' (`"auto"`) will guess the direction based on the content of the string,
#' resolving to top-to-bottom if the string contains scripts that can only be
#' typeset vertically (e.g. Mongolian) and to left-to-right or right-to-left
#' otherwise. Use `"ltr"` (left-to-right), `"rtl"` (right-to-left), `"ttb"`
#' (top-to-bottom, lines stacking right-to-left as in Japanese), and
#' `"ttb-ltr"` (top-to-bottom, lines stacking left-to-right as in Mongolian)
#' to turn off detection and set
#' it manually. Scripts that can be typeset both horizontally and vertically
#' (e.g. Japanese) are always set horizontally with `"auto"`.
#' @param mixed How to treat runs of horizontal-only scripts (e.g. Latin) in
#' vertical text: `"rotate"` shapes them horizontally and marks them for a 90
#' degree rotation during rendering (see the `rotation` column in the output);
#' `"upright"` sets them upright in the vertical line using the font's `vert`/
#' `vrt2` substitutions, falling back to `"rotate"` behaviour if the font
#' doesn't provide these; `"tate"` additionally sets ASCII digit runs of up
#' to 4 digits horizontally on a single line (tate-chu-yoko), as is common
#' for e.g. years and day numbers in Japanese typesetting. Tate runs use the
#' font's `hwid`/`twid`/`qwid` narrow digit forms when available (unless the
#' features are set explicitly), falling back to scaling down to fit the em
#' box. `"combine"` generalises tate-chu-yoko to any horizontal-only run of
#' up to 4 characters — Latin abbreviations, era names, etc. — equivalent to
#' CSS `text-combine-upright: all` (narrow digit features are only used for
#' all-digit runs). `"tate2"`/`"tate3"` and `"combine2"`/`"combine3"` are
#' like `"tate"` and `"combine"` but cap the combined run at 2 or 3
#' digits/characters instead of 4 (runs longer than the cap stay rotated).
#' Finally, `"sideways"` shapes the entire string
#' horizontally — CJK scripts included — and marks it for rotation,
#' equivalent to CSS `text-orientation: sideways`. Ignored for horizontal
#' text. Can be given per string.
#' @param kinsoku How to handle CJK line-break prohibitions (kinsoku shori)
#' when wrapping is forced past the last valid break opportunity: `"strict"`
#' (the default) prohibits lines from starting with closing brackets, sentence
#' punctuation, small kana, the prolonged sound mark, and iteration marks, and
#' from ending with opening brackets; `"loose"` (like CSS `line-break: loose`)
#' additionally permits lines to start with small kana, the prolonged sound
#' mark, iteration marks, and centred punctuation \ifelse{latex}{}{(・：；)};
#' `"none"` disables
#' kinsoku processing entirely (the JLREQ spacing collapses of fullwidth
#' punctuation blanks are unaffected). Can be given per string.
#' @param kinsoku_style How line-break prohibition violations are resolved
#' when wrapping is forced: `"hang"` \ifelse{latex}{}{(ぶら下げ, }the
#' default\ifelse{latex}{}{)} lets offending
#' punctuation hang past the line end; `"push-out"` \ifelse{latex}{}{(追い出し) }
#' moves it to the
#' next line and expands the spacing of the vacated line; `"push-in"`
#' \ifelse{latex}{}{(追い込み) }pulls it onto the current line by compressing its spacing
#' (punctuation blanks first, then inter-character gaps), falling back to
#' hanging when the line cannot absorb the overflow. Can be given per string.
#' @param path,index path an index of a font file to circumvent lookup based on
#' family and style
#'
#' @return
#' A list with two element: `shape` contains the position of each glyph,
#' relative to the origin in the enclosing textbox. `metrics` contain metrics
#' about the full strings.
#'
#' `shape` is a data.frame with the following columns:
#' \describe{
#'   \item{glyph}{The placement of the the first character contributing to the glyph within the string}
#'   \item{index}{The index of the glyph in the font file}
#'   \item{metric_id}{The index of the string the glyph is part of (referencing a row in the `metrics` data.frame)}
#'   \item{string_id}{The index of the string the glyph came from (referencing an element in the `strings` input)}
#'   \item{x_offset}{The x offset in pixels from the origin of the textbox}
#'   \item{y_offset}{The y offset in pixels from the origin of the textbox}
#'   \item{font_path}{The path to the font file used during shaping of the glyph}
#'   \item{font_index}{The index of the font used to shape the glyph in the font file}
#'   \item{font_size}{The size of the font used during shaping}
#'   \item{advance}{The advancement amount to the next glyph}
#'   \item{ascender}{The ascend of the font used for the glyph. This does not measure the actual glyph}
#'   \item{descender}{The descend of the font used for the glyph. This does not measure the actual glyph}
#'   \item{rotation}{The rotation in degrees the renderer should apply to the glyph. Non-zero for rotated runs in vertical text: horizontal-only scripts with `mixed = "rotate"` (the default), UAX#50 Tr characters when the font lacks `vert`/`vrt2`, and everything with `mixed = "sideways"`; `0` otherwise}
#'   \item{spacer}{Logical. `TRUE` for null glyphs created from `NA` strings, which reserve advance (given by `tracking`) without shaping any characters. `FALSE` for all real glyphs}
#'   \item{cell_x0, cell_y0, cell_x1, cell_y1}{The bounds of the glyph's cell, i.e. its advance slot along the main axis and its em box along the cross axis, in pixels relative to the origin of the textbox (see below for the precise anchoring convention). Cells tile seamlessly: within a run of text the end of one cell coincides with the start of the next, so they can be used directly for backgrounds and other per-cell decorations}
#' }
#'
#' ## Cell anchoring convention
#'
#' The cell is the glyph's advance slot along the main axis and its em box
#' along the cross axis. Deriving it from `x_offset`/`y_offset` requires
#' knowing the anchoring convention, which differs by direction and rotation,
#' so prefer the `cell_*` columns. For reference: for horizontal text the
#' cell spans `[x_offset, x_offset + advance]` along the line and
#' `[y_offset + descender, y_offset + ascender]` across it. For vertical
#' text, upright glyphs (and spacers) occupy the advance slot along the
#' column and `[x_offset, x_offset + em]` across it — the slot is *not*
#' recoverable from `y_offset`, which is the font's VORG origin and may sit
#' anywhere inside the slot. Rotated runs sit on their baseline: `-90` runs
#' (in `"ttb"`) span `[y_offset - advance, y_offset]` along the column and
#' `[x_offset + descender, x_offset + ascender]` across it, while `+90` runs
#' (only in `"ttb-ltr"`) span `[y_offset, y_offset + advance]` (exact only
#' when `tracking` is 0 for the run) and `[x_offset - ascender, x_offset - descender]`.
#' Note that the cross-axis em width is generally not `ascender - descender`;
#' use `cell_x0`/`cell_x1` rather than deriving it from the font extents.
#'
#' ## Block anchoring
#'
#' The whole shaped block is anchored the same way in every direction: the
#' origin sits at the `hjust`/`vjust` anchor point of the content box, with
#' `hjust` justifying along the main axis and `vjust` across it. Concretely,
#' `left_border` is `-vjust * width` for vertical text (`-hjust * width` for
#' horizontal) and `top_border` marks the block-start edge by `hjust`. With
#' the default `hjust = vjust = 0` the content box thus always starts at the
#' origin — its left edge at `x = 0` and its block-start edge at `y = 0` —
#' whether the text is horizontal, `"ttb"`, or `"ttb-ltr"`; the direction only
#' affects the order in which lines or columns fill the box. Ink may extend
#' past the box edges; the bearings report how far.
#'
#' Null glyphs (`spacer == TRUE`) have no glyph to report so `index` is `NA`,
#' but they carry the font their span resolved to (`font_path`, `font_index`,
#' `font_size`, `ascender`, and `descender`) so consumers can compute
#' font-relative metrics such as underline position for the reserved space. The
#' resolved font may legitimately differ from that of neighbouring glyphs when
#' the `NA` span specifies a different family.
#'
#' `metrics` is a data.frame with the following columns:
#' \describe{
#'   \item{string}{The text the string consist of}
#'   \item{width}{The width of the string}
#'   \item{height}{The height of the string}
#'   \item{left_bearing}{The distance from the left edge of the textbox and the leftmost glyph}
#'   \item{right_bearing}{The distance from the right edge of the textbox and the rightmost glyph}
#'   \item{top_bearing}{The distance from the top edge of the textbox and the topmost glyph}
#'   \item{bottom_bearing}{The distance from the bottom edge of the textbox and the bottommost glyph}
#'   \item{left_border}{The position of the leftmost edge of the textbox related to the origin}
#'   \item{top_border}{The position of the topmost edge of the textbox related to the origin}
#'   \item{pen_x}{The horizontal position of the next glyph after the string}
#'   \item{pen_y}{The vertical position of the next glyph after the string}
#'   \item{ltr}{The global direction of the string. If `TRUE` then it is left-to-right, otherwise it is right-to-left. Vertical text is not right-to-left so `ltr` is always `TRUE` for it; consult `direction` for the resolved direction}
#'   \item{direction}{The resolved global direction of the string (`"ltr"`, `"rtl"`, `"ttb"`, or `"ttb-ltr"`)}
#' }
#'
#' For vertical text (`"ttb"`/`"ttb-ltr"`) glyphs flow along the y axis and lines
#' stack as columns (right-to-left for `"ttb"`, left-to-right for `"ttb-ltr"`).
#' Upright runs automatically use the font's `vert`/`vrt2` and `vkna`
#' substitutions when available unless these features are set explicitly.
#' `max_width` constrains the height of a column, and `hjust`/`vjust` justify
#' along the main axis and across columns respectively. The `advance` column
#' gives the advance along the main axis.
#'
#' @export
#' @importFrom systemfonts font_feature font_variation match_fonts
#'
#' @examples
#' string <- "This is a long string\nLook; It spans multiple lines\nand all"
#'
#' # Shape with default settings
#' shape_text(string)
#'
#' # Mix styles within the same string
#' string <- c(
#'   "This string will have\na ",
#'   "very large",
#'   " text style\nin the middle"
#' )
#'
#' shape_text(string, id = c(1, 1, 1), size = c(12, 24, 12))
#'
#' # Vertical text. CJK scripts stay horizontal with "auto", so request it
#' # explicitly; runs of horizontal-only scripts are rotated by default
#' # (Japanese strings written with Unicode escapes so they render in the
#' # PDF manual)
#' shape_text("\\u65e5\\u672c\\u8a9eABC\\u3067\\u3059", direction = "ttb")
#'
#' # ...or set them upright if the font provides vert/vrt2 substitutions
#' shape_text("\\u65e5\\u672c\\u8a9eABC\\u3067\\u3059", direction = "ttb", mixed = "upright")
#'
#' # Digit runs of up to 4 digits can be set horizontally on a single line
#' # (tate-chu-yoko) with mixed = "tate", using the font's narrow digit
#' # forms (hwid/twid/qwid) when available
#' shape_text("2024\\u5e7412\\u670825\\u65e5\\u767a\\u58f2", direction = "ttb", mixed = "tate")
#'
shape_text <- function(
  strings,
  id = NULL,
  family = '',
  italic = FALSE,
  weight = 'normal',
  width = 'undefined',
  features = font_feature(),
  variation = font_variation(),
  size = 12,
  res = 72,
  lineheight = 1,
  align = 'auto',
  hjust = 0,
  vjust = 0,
  max_width = NA,
  tracking = 0,
  indent = 0,
  hanging = 0,
  space_before = 0,
  space_after = 0,
  direction = "auto",
  mixed = "rotate",
  kinsoku = "strict",
  kinsoku_style = "hang",
  path = NULL,
  index = 0,
  bold = deprecated()
) {
  n_strings <- length(strings)
  if (is.null(id)) {
    id <- seq_len(n_strings)
  }
  id <- rep_len(id, n_strings)
  id <- match(id, unique(id))
  if (anyNA(id)) {
    stop('id must be a vector of valid integers', call. = FALSE)
  }
  ido <- order(id)
  id <- id[ido]
  strings <- as.character(strings)[ido]

  if (lifecycle::is_present(bold)) {
    lifecycle::deprecate_warn(
      "0.4.0",
      "shape_text(bold)",
      "shape_text(weight='bold')"
    )
    weight <- ifelse(bold, "bold", "normal")
  }

  if (inherits(features, 'font_feature')) {
    features <- list(features)
  }
  features <- rep_len(features, n_strings)
  if (inherits(variation, 'font_variation')) {
    variation <- list(variation)
  }
  variation <- rep_len(variation, n_strings)

  if (is.null(path)) {
    family <- rep_len(family, n_strings)
    italic <- rep_len(italic, n_strings)
    weight <- rep_len(weight, n_strings)
    width <- rep_len(width, n_strings)
    loc <- match_fonts(family, italic, weight, width)
    path <- loc$path[ido]
    index <- loc$index[ido]
    features <- Map(c, loc$features, features)[ido]
    # Build up the variable axis coordinates from least to most important:
    # the requested style settings, the coordinates carried by the matched
    # font (e.g. from registered fonts), and the explicit variation settings
    variation <- Map(
      function(v, loc_v, it, wt, wd) {
        merge_variations(add_standard_variations(loc_v, it, wt, wd), v)
      },
      variation,
      loc$variations,
      italic,
      weight,
      width
    )[ido]
  } else {
    path <- rep_len(path, n_strings)[ido]
    index <- rep_len(index, n_strings)[ido]
    features <- features[ido]
    variation <- variation[ido]
  }
  size <- rep_len(size, n_strings)[ido]
  res <- rep_len(res, n_strings)[ido]
  lineheight <- rep_len(lineheight, n_strings)[ido]
  align <- if (length(align) != 0) {
    match.arg(
      align,
      c(
        'left',
        'center',
        'right',
        'justified-left',
        'justified-center',
        'justified-right',
        'distributed',
        'auto',
        'justified'
      ),
      TRUE
    )
  }
  align <- match(
    align,
    c(
      'left',
      'center',
      'right',
      'justified-left',
      'justified-center',
      'justified-right',
      'distributed',
      'auto',
      'justified'
    )
  )
  align <- rep_len(align, n_strings)[ido]
  hjust <- rep_len(hjust, n_strings)[ido]
  vjust <- rep_len(vjust, n_strings)[ido]
  max_width <- rep_len(max_width, n_strings)[ido]
  max_width[is.na(max_width)] <- -1
  tracking <- rep_len(tracking, n_strings)[ido]
  indent <- rep_len(indent, n_strings)[ido]
  hanging <- rep_len(hanging, n_strings)[ido]
  space_before <- rep_len(space_before, n_strings)[ido]
  space_after <- rep_len(space_after, n_strings)[ido]
  direction <- if (length(direction) != 0) {
    match.arg(direction, c('auto', 'ltr', 'rtl', 'ttb', 'ttb-ltr'), TRUE)
  }
  direction <- match(direction, c('auto', 'ltr', 'rtl', 'ttb', 'ttb-ltr')) - 1L
  direction <- rep_len(direction, n_strings)[ido]
  mixed <- match.arg(
    mixed,
    c(
      "rotate",
      "upright",
      "tate",
      "combine",
      "sideways",
      "tate2",
      "tate3",
      "combine2",
      "combine3"
    ),
    several.ok = TRUE
  )
  mixed <- match(
    mixed,
    c(
      'rotate',
      'upright',
      'tate',
      'combine',
      'sideways',
      'tate2',
      'tate3',
      'combine2',
      'combine3'
    )
  ) -
    1L
  mixed <- rep_len(mixed, n_strings)[ido]
  kinsoku <- match.arg(kinsoku, c("strict", "loose", "none"), several.ok = TRUE)
  kinsoku <- rep_len(
    match(kinsoku, c("strict", "loose", "none")) - 1L,
    n_strings
  )[ido]
  kinsoku_style <- match.arg(
    kinsoku_style,
    c("hang", "push-out", "push-in"),
    several.ok = TRUE
  )
  kinsoku_style <- rep_len(
    match(kinsoku_style, c("hang", "push-out", "push-in")) - 1L,
    n_strings
  )[ido]

  max_width <- max_width * res
  tracking <- tracking * res
  indent <- indent * res
  hanging <- hanging * res
  space_before <- space_before * res / 72
  space_after <- space_after * res / 72

  wraps <- lapply(
    stringi::stri_locate_all_boundaries(
      strings,
      omit_no_match = TRUE
    ),
    function(x) x[, 2]
  )

  if (!all(file.exists(path))) {
    stop("path must point to a valid file", call. = FALSE)
  }
  shape <- get_string_shape_c(
    strings,
    id,
    path,
    as.integer(index),
    features,
    variation,
    as.numeric(size),
    as.numeric(res),
    as.numeric(lineheight),
    as.integer(align) - 1L,
    as.numeric(hjust),
    as.numeric(vjust),
    as.numeric(max_width),
    as.numeric(tracking),
    as.numeric(indent),
    as.numeric(hanging),
    as.numeric(space_before),
    as.numeric(space_after),
    as.integer(direction),
    as.integer(mixed),
    as.integer(kinsoku),
    as.integer(kinsoku_style),
    wraps
  )
  if (nrow(shape$shape) == 0) {
    return(shape)
  }

  shape$metrics$string <- vapply(
    split(strings, id),
    paste,
    character(1),
    collapse = ''
  )
  shape$metrics[-c(1, 12, 13)] <- lapply(
    shape$metrics[-c(1, 12, 13)],
    function(x) x * 72 / res[!duplicated(id)]
  )

  shape$shape$string_id <- ido[
    (cumsum(c(0, rle(id)$lengths)) + 1)[shape$shape$metric_id] +
      shape$shape$string_id -
      1
  ]
  shape$shape <- shape$shape[order(shape$shape$string_id), , drop = FALSE]
  res_mod <- (72 / res[shape$shape$string_id])
  shape$shape$x_offset <- shape$shape$x_offset * res_mod
  shape$shape$y_offset <- shape$shape$y_offset * res_mod
  shape$shape$advance <- shape$shape$advance * res_mod
  shape$shape$ascender <- shape$shape$ascender * res_mod
  shape$shape$descender <- shape$shape$descender * res_mod
  shape$shape$cell_x0 <- shape$shape$cell_x0 * res_mod
  shape$shape$cell_y0 <- shape$shape$cell_y0 * res_mod
  shape$shape$cell_x1 <- shape$shape$cell_x1 * res_mod
  shape$shape$cell_y1 <- shape$shape$cell_y1 * res_mod
  shape
}
#' Calculate the width of a string, ignoring new-lines
#'
#' This is a very simple alternative to [systemfonts::shape_string()] that
#' simply calculates the width of strings without taking any newline into
#' account. As such it is suitable to calculate the width of words or lines that
#' have already been split by `\n`. Input is recycled to the length of
#' `strings`.
#'
#' @inheritParams shape_text
#' @param strings A character vector of strings
#' @param include_bearing Logical, should left and right bearing be included in
#' the string width?
#'
#' @return A numeric vector giving the width of the strings in pixels, measured
#' along the main text axis (i.e. the height of the string for vertical text).
#' Use the provided `res` value to convert it into absolute values.
#'
#' @export
#' @importFrom systemfonts match_fonts
#'
#' @examples
#' strings <- c('A short string', 'A very very looong string')
#' text_width(strings)
#'
text_width <- function(
  strings,
  family = '',
  italic = FALSE,
  weight = 'normal',
  width = 'undefined',
  features = font_feature(),
  variation = font_variation(),
  size = 12,
  res = 72,
  include_bearing = TRUE,
  direction = "auto",
  mixed = c("rotate", "upright"),
  path = NULL,
  index = 0,
  bold = deprecated()
) {
  n_strings <- length(strings)

  if (lifecycle::is_present(bold)) {
    lifecycle::deprecate_warn(
      "0.4.1",
      "text_width(bold)",
      "text_width(weight='bold')"
    )
    weight <- ifelse(bold, "bold", "normal")
  }

  if (inherits(features, 'font_feature')) {
    features <- list(features)
  }
  features <- rep_len(features, n_strings)
  if (inherits(variation, 'font_variation')) {
    variation <- list(variation)
  }
  variation <- rep_len(variation, n_strings)

  if (is.null(path)) {
    family <- rep_len(family, n_strings)
    italic <- rep_len(italic, n_strings)
    weight <- rep_len(weight, n_strings)
    width <- rep_len(width, n_strings)
    loc <- match_fonts(family, italic, weight, width)
    path <- loc$path
    index <- loc$index
    features <- Map(c, loc$features, features)
    # Build up the variable axis coordinates from least to most important:
    # the requested style settings, the coordinates carried by the matched
    # font (e.g. from registered fonts), and the explicit variation settings
    variation <- Map(
      function(v, loc_v, it, wt, wd) {
        merge_variations(add_standard_variations(loc_v, it, wt, wd), v)
      },
      variation,
      loc$variations,
      italic,
      weight,
      width
    )
  } else {
    path <- rep_len(path, n_strings)
    index <- rep_len(index, n_strings)
  }
  size <- rep_len(size, n_strings)
  res <- rep_len(res, n_strings)
  include_bearing <- rep_len(include_bearing, n_strings)
  direction <- if (length(direction) != 0) {
    match.arg(direction, c('auto', 'ltr', 'rtl', 'ttb', 'ttb-ltr'), TRUE)
  }
  direction <- match(direction, c('auto', 'ltr', 'rtl', 'ttb', 'ttb-ltr')) - 1L
  direction <- rep_len(direction, n_strings)
  mixed <- if (missing(mixed)) {
    "rotate"
  } else {
    match.arg(
      mixed,
      c(
        "rotate",
        "upright",
        "tate",
        "combine",
        "sideways",
        "tate2",
        "tate3",
        "combine2",
        "combine3"
      ),
      several.ok = TRUE
    )
  }
  mixed <- rep_len(
    match(
      mixed,
      c(
        'rotate',
        'upright',
        'tate',
        'combine',
        'sideways',
        'tate2',
        'tate3',
        'combine2',
        'combine3'
      )
    ) -
      1L,
    n_strings
  )
  if (!all(file.exists(path))) {
    stop("path must point to a valid file", call. = FALSE)
  }
  get_line_width_c(
    as.character(strings),
    path,
    as.integer(index),
    as.numeric(size),
    as.numeric(res),
    as.logical(include_bearing),
    features,
    variation,
    as.integer(direction),
    as.integer(mixed)
  )
}

# Merge the top-level style settings into the variable axis coordinates. User
# provided variation settings take precedence.
# Merge two font_variation objects. Coordinates in `b` take precedence over
# those in `a`.
merge_variations <- function(a, b) {
  a <- unclass(a)
  b <- unclass(b)
  new_axes <- setdiff(b$axis, a$axis)
  a$axis <- c(a$axis, new_axes)
  a$value[match(b$axis, a$axis)] <- b$value
  structure(a, class = 'font_variation')
}
add_standard_variations <- function(variation, italic, weight, width) {
  standard <- unclass(font_variation(
    italic = if (isTRUE(italic)) 1 else NULL,
    weight = weight,
    width = width
  ))
  variation <- unclass(variation)
  new_axes <- setdiff(variation$axis, standard$axis)
  standard$axis <- c(standard$axis, new_axes)
  standard$value[match(variation$axis, standard$axis)] <- variation$value
  structure(standard, class = 'font_variation')
}

#' Preview shaped text and the metrics for the text box
#'
#' This function allows you to preview the layout that [shape_text()]
#' calculates. It is purely meant as a sanity check to make sure that the values
#' calculated are sensible and shouldn't be used as a plotting function for
#' rendering text on its own.
#'
#' @param shape The output of a call to [shape_text()]
#' @param id The index of the text run to show in case `shape` contains
#' multiples
#'
#' @return This function is called for its side effects
#'
#' @export
#'
#' @examples
#' arab_text <- lorem_text("arabic", 2)
#' shape <- shape_text(
#'   arab_text,
#'   max_width = 5,
#'   indent = 0.2
#' )
#'
#' try(
#'  plot_shape(shape)
#' )
#'
plot_shape <- function(shape, id = 1) {
  if (
    !requireNamespace("grDevices", quietly = TRUE) ||
      utils::packageVersion("grDevices") < package_version("4.3.0")
  ) {
    stop("This function requires grDevices 4.3.0 or above")
  }
  if (
    !requireNamespace("grid", quietly = TRUE) ||
      utils::packageVersion("grid") < package_version("4.3.0")
  ) {
    stop("This function requires grid 4.3.0 or above")
  }

  has_glyph_support <- grDevices::dev.capabilities()$glyphs
  if (is.na(has_glyph_support)) {
    warning("The device does not report whether it supports rendering glyphs")
  } else if (!isTRUE(has_glyph_support)) {
    stop("The current device doesn't support rendering glyphs")
  }

  glyphFont <- utils::getFromNamespace("glyphFont", "grDevices")
  glyphFontList <- utils::getFromNamespace("glyphFontList", "grDevices")
  glyphInfo <- utils::getFromNamespace("glyphInfo", "grDevices")
  glyphAnchor <- utils::getFromNamespace("glyphAnchor", "grDevices")

  grid.glyph <- utils::getFromNamespace("grid.glyph", "grid")

  if (
    !is.numeric(id) ||
      length(id) != 1 ||
      id <= 0 ||
      id %% 1 != 0 ||
      id > nrow(shape$metrics)
  ) {
    stop("`id` must be an integer pointing to a paragraph in `shape`")
  }
  glyphs <- shape$shape[shape$shape$metric_id == id, ]
  box <- shape$metrics[id, ]

  # Glyph positions are relative to the text origin; the textbox edges are at
  # (left_border, top_border - height) to (left_border + width, top_border) in
  # the same coordinates
  box_left <- box$left_border
  box_bottom <- box$top_border - box$height
  x_range <- range(c(glyphs$x_offset, box_left, box_left + box$width))
  y_range <- range(c(glyphs$y_offset, box_bottom, box$top_border))
  x_pad <- max(diff(x_range) * 0.1, 1)
  y_pad <- max(diff(y_range) * 0.1, 1)

  font_id <- paste0(glyphs$font_path, "&", glyphs$font_index)
  font_match <- match(font_id, unique(font_id))
  unique_font <- !duplicated(font_id)
  fonts <- Map(
    glyphFont,
    glyphs$font_path[unique_font],
    glyphs$font_index[unique_font],
    "",
    0,
    ""
  )
  fonts <- do.call(glyphFontList, fonts)

  grid::grid.newpage()

  # Everything is drawn in absolute bigpts coordinates, shifted so the
  # content (glyphs + textbox) sits in the positive quadrant with a small pad
  shift_x <- -x_range[1] + x_pad
  shift_y <- -y_range[1] + y_pad

  vp <- grid::viewport(
    width = grid::unit(diff(x_range) + 2 * x_pad, "bigpts"),
    height = grid::unit(diff(y_range) + 2 * y_pad, "bigpts")
  )

  grid::pushViewport(vp)

  glyphs <- glyphInfo(
    id = glyphs$index,
    x = glyphs$x_offset + shift_x,
    y = glyphs$y_offset + shift_y,
    font = font_match,
    size = glyphs$font_size,
    fontList = fonts,
    width = box$width,
    height = -box$height,
    hAnchor = glyphAnchor(0, "left"),
    vAnchor = glyphAnchor(0, "bottom"),
    rot = glyphs$rotation
  )

  grid::grid.rect(
    x = box_left + shift_x,
    y = box_bottom + shift_y,
    width = box$width,
    height = box$height,
    hjust = 0,
    vjust = 0,
    default.units = "bigpts",
    gp = grid::gpar(fill = "lightgrey", col = NA)
  )
  grid::grid.rect(
    x = box_left + box$left_bearing + shift_x,
    y = box_bottom + box$bottom_bearing + shift_y,
    width = box$width - box$left_bearing - box$right_bearing,
    height = box$height - box$top_bearing - box$bottom_bearing,
    hjust = 0,
    vjust = 0,
    default.units = "bigpts",
    gp = grid::gpar(fill = NA, col = "darkgrey", lty = 2)
  )
  grid.glyph(
    glyphs,
    x = 0,
    y = 0,
    hjust = 0,
    vjust = 0
  )
  grid::grid.points(
    x = box$pen_x + shift_x,
    y = box$pen_y + shift_y,
    default.units = "bigpts",
    pch = 16,
    gp = grid::gpar(col = "red", cex = 0.5)
  )
}
