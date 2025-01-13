test_that("Textshaping follows UNICODE Bidi conventions", {
  skip_on_cran()
  skip_on_ci()

  # Change to FALSE to run tests locally
  skip_if(TRUE, "Skipping time consuming UNICODE Bidi tests")

  test_url <- "https://www.unicode.org/Public/UCD/latest/ucd/BidiCharacterTest.txt"
  tests <- read.csv2(test_url, header = FALSE, comment.char = "#")
  names(tests) <- c("unicode", "ltr_set", "ltr_detected", "bidi_embedding", "visual_order")
  tests$visual_order <- lapply(strsplit(tests$visual_order, " ", TRUE), as.integer)
  for (i in seq_len(nrow(tests))) {
    test_string <- paste0("\"\\u", gsub(" ", "\\u", tests$unicode[i], fixed = TRUE), "\"")
    test_string <- eval(parse(text = test_string))
    dir <- c("ltr", "rtl", "auto")[tests$ltr_set[i] + 1]
    shape <- shape_text(test_string, direction = dir)

    expect_equal(shape$metrics$ltr, tests$ltr_detected[i] == 0, label = paste0("calculated direction [test ", i,"]"))

    true_ordering <- tests$visual_order[[i]] + 1L

    # We do the below to make sure that any ligature substitutions doesn't affect the
    # changes
    true_ordering <- intersect(true_ordering, shape$shape$glyph)
    calc_ordering <- intersect(shape$shape$glyph, true_ordering)

    expect_equal(calc_ordering, true_ordering, label = paste0("calculated visual order [test ", i,"]"))
  }
})
