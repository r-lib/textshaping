test_that("Textshaping follows UNICODE Bidi conventions", {
  # First test from the unicode test file
  test_string <- gsub(
    " ",
    "\\u",
    "' 05D0 05D1 0028 05D2 05D3 005B 0026 0065 0066 005D 002E 0029 0067 0068'",
    fixed = TRUE
  )
  test_string <- eval(parse(text = test_string))
  shape <- shape_text(test_string, direction = "ltr")
  expect_equal(
    shape$shape$glyph,
    c(2L, 1L, 3L, 5L, 4L, 6L, 7L, 8L, 9L, 10L, 11L, 12L, 13L, 14L)
  )

  skip_on_cran()
  skip_on_ci()

  # Change to FALSE to run ALL tests locally
  skip_if(TRUE, "Skipping time consuming UNICODE Bidi tests")

  test_url <- "https://www.unicode.org/Public/UCD/latest/ucd/BidiCharacterTest.txt"
  tests <- read.csv2(test_url, header = FALSE, comment.char = "#")
  names(tests) <- c(
    "unicode",
    "ltr_set",
    "ltr_detected",
    "bidi_embedding",
    "visual_order"
  )
  tests$visual_order <- lapply(
    strsplit(tests$visual_order, " ", TRUE),
    as.integer
  )
  for (i in seq_len(nrow(tests))[-1]) {
    test_string <- paste0(
      "\"\\u",
      gsub(" ", "\\u", tests$unicode[i], fixed = TRUE),
      "\""
    )
    test_string <- eval(parse(text = test_string))
    dir <- c("ltr", "rtl", "auto")[tests$ltr_set[i] + 1]
    shape <- shape_text(test_string, direction = dir)

    expect_equal(
      shape$metrics$ltr,
      tests$ltr_detected[i] == 0,
      label = paste0("calculated direction [test ", i, "]")
    )

    true_ordering <- tests$visual_order[[i]] + 1L

    # We do the below to make sure that any ligature substitutions doesn't affect the
    # changes
    true_ordering <- intersect(true_ordering, shape$shape$glyph)
    calc_ordering <- intersect(shape$shape$glyph, true_ordering)

    expect_equal(
      calc_ordering,
      true_ordering,
      label = paste0("calculated visual order [test ", i, "]")
    )
  }
})

# Regression tests for bidi bugs found in the 2026-11 C++ review

test_that("deeply nested bidi embeddings don't overflow the embed stack", {
  # This corrupted the heap before the fix
  expect_no_error(shape_text(paste0(strrep("\u202b", 130), "x")))
})

test_that("rtl embeddings ending in a line break don't throw", {
  expect_no_error(shape_text(c("שלום ", "עולם\nאבג"), id = c(1, 1)))
})

test_that("single character strings resolve their direction", {
  expect_true(shape_text("a")$metrics$ltr)
})
