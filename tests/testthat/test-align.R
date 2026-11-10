# Regression tests for alignment crashes found in the 2026-11 C++ review.
# Each of these crashed before the fix.

test_that("empty string with justified alignment doesn't crash", {
  expect_no_error(shape_text("", align = "justified-left"))
})

test_that("distributed alignment with a single glyph doesn't crash", {
  expect_no_error(shape_text("a", align = "distributed"))
})
