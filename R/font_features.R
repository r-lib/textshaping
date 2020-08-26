#' Get available OpenType features in a font
#'
#' This is a simply functions that returns the available OpenType feature tags
#' for one or more fonts. See [`font_feature()`][systemfonts::font_feature] for
#' more information on how to use the different feature with a font.
#'
#' @inheritParams systemfonts::font_info
#'
#' @return A list with an element for each of the input fonts containing the
#' supported feature tags for that font.
#'
#' @importFrom systemfonts match_font
#' @export
#'
#' @examples
#' # Select a random font on the system
#' sys_fonts <- systemfonts::system_fonts()
#' random_font <- sys_fonts$family[sample(nrow(sys_fonts), 1)]
#'
#' # Get the features
#' get_font_features(random_font)
#'
get_font_features <- function(family = '', italic = FALSE, bold = FALSE,
                              path = NULL, index = 0) {
  if (is.null(path)) {
    full_length <- max(length(family), length(italic), length(bold))
    if (full_length == 1) {
      loc <- match_font(family, italic, bold)
      path <- loc$path
      index <- loc$index
    } else {
      family <- rep_len(family, full_length)
      italic <- rep_len(italic, full_length)
      bold <- rep_len(bold, full_length)
      loc <- Map(match_font, family = family, italic = italic, bold = bold)
      path <- vapply(loc, `[[`, character(1L), 1, USE.NAMES = FALSE)
      index <- vapply(loc, `[[`, integer(1L), 2, USE.NAMES = FALSE)
    }
  } else {
    full_length <- max(length(path), length(index))
    path <- rep_len(path, full_length)
    index <- rep_len(index, full_length)
  }
  lapply(get_face_features_c(as.character(path), as.integer(index)), unique)
}
