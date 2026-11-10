# Regression tests for shaping bugs found in the 2026-11 C++ review

test_that("shape cache stores glyph clusters relative to the run", {
  # Populate the cache with a run positioned inside a longer paragraph
  shape_text(c("Hello there ", "world foo"), id = c(1, 1))
  # Shaping the same run on its own must give clusters within the run
  s <- shape_text("world foo")
  expect_true(all(s$shape$glyph <= nchar("world foo")))
})
