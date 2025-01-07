#' Calculate glyph positions for strings
#'
#' @description
#' `r lifecycle::badge('experimental')`
#'
#' Do basic text shaping of strings. This function will use freetype to
#' calculate advances, doing kerning if possible. It will not perform any font
#' substitution or ligature resolving and will thus be much in line with how
#' the standard graphic devices does text shaping. Inputs are recycled to the
#' length of `strings`.
#'
#' @param strings A character vector of strings to shape
#' @param id A vector grouping the strings together. If strings share an id the
#' shaping will continue between strings
#' @inheritParams systemfonts::match_fonts
#' @param features A [systemfonts::font_feature()] object or a list of them,
#' giving the OpenType font features to set
#' @param size The size in points to use for the font
#' @param res The resolution to use when doing the shaping. Should optimally
#' match the resolution used when rendering the glyphs.
#' @param lineheight A multiplier for the lineheight
#' @param align Within text box alignment, either `'left'`, `'center'`, `'right'`,
#' `'justified-left'`, `'justified-right'`, `'justified-center'`, or `'distributed'`
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
#'   \item{glyph}{The glyph as a character}
#'   \item{index}{The index of the glyph in the font file}
#'   \item{metric_id}{The index of the string the glyph is part of (referencing a row in the `metrics` data.frame)}
#'   \item{string_id}{The index of the string the glyph came from (referencing an element in the `strings` input)}
#'   \item{x_offset}{The x offset in pixels from the origin of the textbox}
#'   \item{y_offset}{The y offset in pixels from the origin of the textbox}
#'   \item{x_mid}{The x offset in pixels to the middle of the glyph, measured from the origin of the glyph}
#' }
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
#' }
#'
#' @export
#' @importFrom systemfonts font_feature match_fonts
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
shape_text <- function(strings, id = NULL, family = '', italic = FALSE,
                       weight = 'normal', width = 'undefined', features = font_feature(),
                       size = 12, res = 72, lineheight = 1, align = 'left',
                       hjust = 0, vjust = 0, max_width = NA, tracking = 0,
                       indent = 0, hanging = 0, space_before = 0, space_after = 0,
                       path = NULL, index = 0, bold = deprecated()) {
  n_strings = length(strings)
  if (is.null(id)) id <- seq_len(n_strings)
  id <- rep_len(id, n_strings)
  id <- match(id, unique(id))
  if (anyNA(id)) {
    stop('id must be a vector of valid integers', call. = FALSE)
  }
  ido <- order(id)
  id <- id[ido]
  strings <- as.character(strings)[ido]

  if (lifecycle::is_present(bold)) {
    lifecycle::deprecate_soft("0.4.0", "shape_text(bold)", "shape_text(weight='bold')")
    weight <- ifelse(bold, "bold", "normal")
  }

  if (inherits(features, 'font_feature')) features <- list(features)
  features <- rep_len(features, n_strings)

  if (is.null(path)) {
    family <- rep_len(family, n_strings)
    italic <- rep_len(italic, n_strings)
    weight <- rep_len(weight, n_strings)
    width <- rep_len(width, n_strings)
    loc <- match_fonts(family, italic, weight, width)
    path <- loc$path[ido]
    index <- loc$index[ido]
    features <- Map(c, loc$features, features)[ido]
  } else {
    path <- rep_len(path, n_strings)[ido]
    index <- rep_len(index, n_strings)[ido]
    features <- features[ido]
  }
  size <- rep_len(size, n_strings)[ido]
  res <- rep_len(res, n_strings)[ido]
  lineheight <- rep_len(lineheight, n_strings)[ido]
  align <- match.arg(align, c('left', 'center', 'right', 'justified-left', 'justified-center', 'justified-right', 'distributed'), TRUE)
  align <- match(align, c('left', 'center', 'right', 'justified-left', 'justified-center', 'justified-right', 'distributed'))
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

  max_width <- max_width * res
  tracking <- tracking * res
  indent <- indent * res
  hanging <- hanging * res
  space_before <- space_before * res / 72
  space_after <- space_after * res / 72


  if (!all(file.exists(path))) stop("path must point to a valid file", call. = FALSE)
  shape <- get_string_shape_c(
    strings, id, path, as.integer(index), features, as.numeric(size),
    as.numeric(res), as.numeric(lineheight), as.integer(align) - 1L,
    as.numeric(hjust), as.numeric(vjust), as.numeric(max_width), as.numeric(tracking),
    as.numeric(indent), as.numeric(hanging), as.numeric(space_before),
    as.numeric(space_after)
  )
  if (nrow(shape$shape) == 0) return(shape)

  shape$metrics$string <- vapply(split(strings, id), paste, character(1), collapse = '')
  shape$metrics[-1] <- lapply(shape$metrics[-1], function(x) x * 72 / res[!duplicated(id)])

  shape$shape$string_id <- ido[(cumsum(c(0, rle(id)$lengths)) + 1)[shape$shape$metric_id] + shape$shape$string_id - 1]
  shape$shape <- shape$shape[order(shape$shape$string_id), , drop = FALSE]
  shape$shape$x_offset <- shape$shape$x_offset * (72 / res[shape$shape$string_id])
  shape$shape$y_offset <- shape$shape$y_offset * (72 / res[shape$shape$string_id])
  shape$shape$advance <- shape$shape$advance * (72 / res[shape$shape$string_id])
  shape$shape$ascender <- shape$shape$ascender * (72 / res[shape$shape$string_id])
  shape$shape$descender <- shape$shape$descender * (72 / res[shape$shape$string_id])
  shape
}
#' Calculate the width of a string, ignoring new-lines
#'
#' This is a very simple alternative to [systemfonts::shape_string()] that
#' simply calculates the width of strings without taking any newline into
#' account. As such it is suitable to calculate the width of words or lines that
#' has already been splitted by `\n`. Input is recycled to the length of
#' `strings`.
#'
#' @inheritParams shape_text
#' @param strings A character vector of strings
#' @param include_bearing Logical, should left and right bearing be included in
#' the string width?
#'
#' @return A numeric vector giving the width of the strings in pixels. Use the
#' provided `res` value to convert it into absolute values.
#'
#' @export
#' @importFrom systemfonts match_fonts
#'
#' @examples
#' strings <- c('A short string', 'A very very looong string')
#' text_width(strings)
#'
text_width <- function(strings, family = '', italic = FALSE, weight = 'normal',
                       width = 'undefined', features = font_feature(),
                       size = 12, res = 72, include_bearing = TRUE, path = NULL,
                       index = 0, bold = deprecated()) {
  n_strings <- length(strings)

  if (lifecycle::is_present(bold)) {
    lifecycle::deprecate_soft("0.4.1", "text_width(bold)", "text_width(weight='bold')")
    weight <- ifelse(bold, "bold", "normal")
  }

  if (inherits(features, 'font_feature')) features <- list(features)
  features <- rep_len(features, n_strings)

  if (is.null(path)) {
    family <- rep_len(family, n_strings)
    italic <- rep_len(italic, n_strings)
    weight <- rep_len(weight, n_strings)
    width <- rep_len(width, n_strings)
    loc <- match_fonts(family, italic, weight, width)
    path <- loc$path
    index <- loc$index
    features <- Map(c, loc$features, features)
  } else {
    path <- rep_len(path, n_strings)
    index <- rep_len(index, n_strings)
  }
  size <- rep_len(size, n_strings)
  res <- rep_len(res, n_strings)
  include_bearing <- rep_len(include_bearing, n_strings)
  if (!all(file.exists(path))) stop("path must point to a valid file", call. = FALSE)
  get_line_width_c(
    as.character(strings), path, as.integer(index), as.numeric(size),
    as.numeric(res), as.logical(include_bearing), features
  )
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
  if (!requireNamespace("grDevices", quietly = TRUE) || utils::packageVersion("grDevices") < package_version("4.3.0")) {
    stop("This function requires grDevices 4.3.0 or above")
  }
  if (!requireNamespace("grid", quietly = TRUE) || utils::packageVersion("grid") < package_version("4.3.0")) {
    stop("This function requires grid 4.3.0 or above")
  }

  has_glyph_support <- grDevices::dev.capabilities()$glyphs
  if (is.na(has_glyph_support)) {
    warning("The device does not report whether it supports rendering glyphs")
  } else if (!isTRUE(has_glyph_support)) {
    stop("The current device doesn't support rendering glyphs")
  }

  if (!is.numeric(id) || length(id) != 1 || id <= 0 || id %% 1 != 0 || id > nrow(shape$metrics)) {
    stop("`id` must be an integer pointing to a paragraph in `shape`")
  }
  glyphs <- shape$shape[shape$shape$metric_id == id, ]
  box <- shape$metrics[id, ]

  font_id <- paste0(glyphs$font_path, "&", glyphs$font_index)
  font_match <- match(font_id, unique(font_id))
  unique_font <- !duplicated(font_id)
  fonts <- Map(grDevices::glyphFont, glyphs$font_path[unique_font], glyphs$font_index[unique_font], "", 0, "")
  fonts <- do.call(grDevices::glyphFontList, fonts)
  glyphs <- grDevices::glyphInfo(
    id = glyphs$index,
    x = glyphs$x_offset,
    y = glyphs$y_offset,
    font = font_match,
    size = glyphs$font_size,
    fontList = fonts,
    width = box$width,
    height = -box$height,
    hAnchor = grDevices::glyphAnchor(0, "left"),
    vAnchor = grDevices::glyphAnchor(0, "bottom")
  )

  grid::grid.newpage()

  vp <- grid::viewport(
    width = box$width,
    height = box$height,
    default.units = "bigpts"
  )

  grid::pushViewport(vp)
  grid::grid.rect(gp = grid::gpar(fill = "lightgrey", col = NA))
  grid::grid.rect(
    x = box$left_bearing,
    y = box$bottom_bearing,
    width = box$width - box$left_bearing - box$right_bearing,
    height = box$height - box$top_bearing - box$bottom_bearing,
    hjust = 0,
    vjust = 0,
    default.units = "bigpts",
    gp = grid::gpar(fill = NA, col = "darkgrey", lty = 2)
  )
  grid::grid.glyph(
    glyphs,
    x = 0,
    y = 0,
    hjust = 0,
    vjust = 0
  )
  grid::grid.points(
    x = box$pen_x,
    y = box$pen_y,
    default.units = "bigpts",
    pch = 16,
    gp = grid::gpar(col = "red", cex = 0.5)
  )
}
