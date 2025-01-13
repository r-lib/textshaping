run_bidi_test <- function(fail = TRUE) {
  test_url <- "https://www.unicode.org/Public/UCD/latest/ucd/BidiCharacterTest.txt"
  tests <- read.csv2(test_url, header = FALSE, comment.char = "#")
  names(tests) <- c("unicode", "ltr_set", "ltr_detected", "bidi_embedding", "visual_order")
  tests$bidi_embedding <- lapply(strsplit(tests$bidi_embedding, " ", TRUE), as.integer)
  tests$visual_order <- lapply(strsplit(tests$visual_order, " ", TRUE), as.integer)
  message("Running ", nrow(tests), " tests")
  pb <- utils::txtProgressBar(max = nrow(tests), style = 3)
  on.exit(close(pb))
  fails <- integer()
  for (i in seq_len(nrow(tests))) {
    utils::setTxtProgressBar(pb, i)
    test_string <- paste0("\"\\u", gsub(" ", "\\u", tests$unicode[i], fixed = TRUE), "\"")
    test_string <- eval(parse(text = test_string))
    dir <- c("ltr", "rtl", "auto")[tests$ltr_set[i] + 1]
    shape <- shape_text(test_string, direction = dir)
    if (shape$metrics$ltr != (tests$ltr_detected[i] == 0)) {
      if (fail) {
        stop("Test ", i, " failed! Wrong paragraph level embedding detected")
      }
      fails <- c(fails, i)
      next
    }
    true_ordering <- tests$visual_order[[i]] + 1L

    # We do the below to make sure that any ligature substitutions doesn't affect the
    # changes
    true_ordering <- intersect(true_ordering, shape$shape$glyph)
    calc_ordering <- intersect(shape$shape$glyph, true_ordering)

    if (!identical(true_ordering, calc_ordering)) {
      if (fail) {
        stop("Test ", i, " failed! Wrong visual order")
      }
      fails <- c(fails, i)
      next
    }
  }
  invisible(fails)
}
