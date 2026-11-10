# Regression tests for wrapping crashes found in the 2026-11 C++ review.
# Each of these crashed or hung before the fix.

test_that("wrapping a single glyph wider than the line doesn't crash", {
  expect_no_error(shape_text("W", max_width = 0.001))
})

test_that("wrapping a spacer wider than the line doesn't crash", {
  expect_no_error(
    shape_text(
      c("abc", NA),
      id = c(1, 1),
      size = c(12, 100),
      max_width = 0.5
    )
  )
})

test_that("wrapping a single rtl glyph wider than the line doesn't hang", {
  expect_no_error(shape_text("א", max_width = 0.001))
})
