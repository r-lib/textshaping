# Regression tests for width calculation crashes found in the 2026-11
# C++ review. This crashed before the fix.

test_that("width of empty string without bearings doesn't crash", {
  expect_no_error(text_width("", include_bearing = FALSE))
  expect_equal(unname(text_width("", include_bearing = FALSE)), 0)
})
