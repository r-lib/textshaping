test_that("shape_text preserves final glyph and handles explicit line breaks", {
  single <- shape_text("hello world")
  expect_equal(single$shape$glyph, seq_len(nchar("hello world")))
  expect_length(unique(single$shape$y_offset), 1L)

  multi <- shape_text("line 1\nline 2")
  # \n at index 7 is consumed as a hard break, leaving 1..6 and 8..13
  expect_equal(multi$shape$glyph, c(1:6, 8:13))
  expect_length(unique(multi$shape$y_offset), 2L)
  expect_gt(multi$metrics$height, single$metrics$height)
  # Line 1 is positioned above line 2
  expect_gt(multi$shape$y_offset[1], multi$shape$y_offset[7])
})

test_that("shape_text handles trailing and standalone hard line breaks", {
  no_nl <- shape_text("line 1")
  trailing_nl <- shape_text("line 1\n")
  expect_equal(trailing_nl$shape$glyph, 1:6)
  expect_gt(trailing_nl$metrics$height, no_nl$metrics$height)
  expect_gt(trailing_nl$metrics$bottom_bearing, no_nl$metrics$bottom_bearing)

  # Empty final line (bare newline, double trailing newline, or mixed-BiDi before \n)
  bare_nl <- shape_text("\n")
  expect_shape(bare_nl$shape, nrow = 0L)
  expect_shape(bare_nl$metrics, nrow = 1L)
  expect_gt(bare_nl$metrics$height, 0)

  double_trailing <- shape_text("line 1\n\n")
  expect_equal(double_trailing$shape$glyph, 1:6)
  expect_gt(double_trailing$metrics$height, trailing_nl$metrics$height)

  bidi_nl <- shape_text("a \u0634\n")
  bidi_no_nl <- shape_text("a \u0634")
  expect_gt(bidi_nl$metrics$height, bidi_no_nl$metrics$height)
})

test_that("shape_text wraps at spaces, hyphens, and soft hyphens with max_width", {
  unwrapped <- shape_text("This is a long sentence that should wrap at spaces")
  wrapped <- shape_text(
    "This is a long sentence that should wrap at spaces",
    max_width = 1
  )
  expect_equal(wrapped$shape$glyph, unwrapped$shape$glyph)
  expect_gt(length(unique(wrapped$shape$y_offset)), 1L)
  expect_gt(wrapped$metrics$height, unwrapped$metrics$height)

  hyphen_wrapped <- shape_text("super-cali-fragilistic", max_width = 0.5)
  expect_equal(
    hyphen_wrapped$shape$glyph,
    seq_len(nchar("super-cali-fragilistic"))
  )
  expect_gt(length(unique(hyphen_wrapped$shape$y_offset)), 1L)

  shy_text <- "super\u00adcali\u00adfragilistic"
  shy_unwrapped <- shape_text(shy_text)
  shy_wrapped <- shape_text(shy_text, max_width = 0.5)
  expect_gt(length(unique(shy_wrapped$shape$y_offset)), 1L)
  # Soft hyphen at cluster 6 is zero-width when unwrapped, but gets positive advance when broken
  expect_equal(shy_unwrapped$shape$advance[shy_unwrapped$shape$glyph == 6L], 0)
  expect_gt(shy_wrapped$shape$advance[shy_wrapped$shape$glyph == 6L], 0)
})

test_that("shape_text handles multi-run strings and edge-case inputs", {
  runs <- shape_text(
    c("hello ", "world\n", "next"),
    id = c(1L, 1L, 1L),
    max_width = 0.6
  )
  expect_equal(runs$shape$glyph, c(1:11, 13:16))
  expect_equal(runs$shape$string_id, rep(1:3, 6:4))
  expect_length(unique(runs$shape$y_offset), 3L)

  empty <- shape_text("")
  expect_shape(empty$shape, nrow = 0L)
  expect_shape(empty$metrics, nrow = 1L)
  # Glyph-less blocks report zero extent, not the constraint or a phantom
  # line height
  expect_equal(empty$metrics$width, 0)
  expect_equal(empty$metrics$height, 0)
  empty_constrained <- shape_text("", max_width = 5)
  expect_equal(empty_constrained$metrics$width, 0)
  expect_equal(empty_constrained$metrics$height, 0)
  empty_vertical <- shape_text("", direction = "ttb", max_width = 5)
  expect_equal(empty_vertical$metrics$width, 0)
  expect_equal(empty_vertical$metrics$height, 0)

  na_res <- shape_text(NA_character_)
  expect_shape(na_res$shape, nrow = 1L)
  expect_identical(na_res$shape$index, NA_integer_)

  zero_res <- shape_text(character(0))
  expect_shape(zero_res$shape, nrow = 0L)
  expect_shape(zero_res$metrics, nrow = 0L)
})

test_that("null glyphs are marked and carry their resolved font", {
  res <- shape_text(c("hello", NA, "world"), tracking = 24)
  expect_identical(res$shape$spacer, rep(c(FALSE, TRUE, FALSE), c(5L, 1L, 5L)))

  sp <- res$shape[res$shape$spacer, ]
  expect_identical(sp$index, NA_integer_)
  # The null glyph reports the font its span resolved to
  expect_identical(sp$font_path, res$shape$font_path[1])
  expect_identical(sp$font_index, res$shape$font_index[1])
  expect_identical(sp$font_size, res$shape$font_size[1])
  expect_equal(sp$ascender, res$shape$ascender[1])
  expect_equal(sp$descender, res$shape$descender[1])

  # ... which may differ from neighbouring glyphs when the span specifies
  # another family
  res2 <- shape_text(c("hello", NA), family = c("sans", "mono"))
  expect_identical(
    res2$shape$font_path[res2$shape$spacer],
    systemfonts::match_fonts("mono")$path
  )
})

test_that("cell bounds follow the anchoring convention for horizontal text", {
  res <- shape_text("hello world")
  sh <- res$shape
  # The cell is the advance slot along the line and the font extents across it
  expect_equal(sh$cell_x0, sh$x_offset)
  expect_equal(sh$cell_x1 - sh$cell_x0, sh$advance)
  expect_equal(sh$cell_y0, sh$y_offset + sh$descender)
  expect_equal(sh$cell_y1, sh$y_offset + sh$ascender)
  # Cells tile seamlessly
  expect_equal(sh$cell_x0[-1], sh$cell_x1[-nrow(sh)])

  # RTL cells are reported low-to-high and tile as well
  rtl <- shape_text("\u05e9\u05dc\u05d5\u05dd", direction = "rtl")$shape
  expect_true(all(rtl$cell_x1 > rtl$cell_x0))
  expect_equal(rtl$cell_x0[-1], rtl$cell_x1[-nrow(rtl)])
})

test_that("shape_text handles RTL text with line breaks and wrapping", {
  rtl_single <- shape_text(
    "\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd",
    direction = "rtl"
  )
  rtl_nl <- shape_text(
    "\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd\n\u05e9\u05d5\u05e8\u05d4 \u05e9\u05e0\u05d9\u05d4",
    direction = "rtl"
  )
  expect_false(rtl_nl$metrics$ltr)
  expect_length(unique(rtl_nl$shape$y_offset), 2L)
  expect_gt(rtl_nl$metrics$height, rtl_single$metrics$height)

  rtl_trail_nl <- shape_text(
    "\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd\n",
    direction = "rtl"
  )
  expect_shape(rtl_trail_nl$shape, nrow = 9L)
  expect_gt(rtl_trail_nl$metrics$height, rtl_single$metrics$height)

  rtl_wrap <- shape_text(
    "\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd \u05e9\u05d5\u05e8\u05d4 \u05e9\u05e0\u05d9\u05d4",
    direction = "rtl",
    max_width = 0.8
  )
  expect_gt(length(unique(rtl_wrap$shape$y_offset)), 1L)
})

test_that("text-presentation-default emoji prefer text fallback fonts", {
  arrow <- "↙"
  # Match on family name, not file name: Windows' emoji font file
  # (seguiemj.ttf) doesn't contain "emoji" but its family does
  font_family <- function(path, index) {
    systemfonts::font_info(path = path, index = index)$family
  }
  # Only meaningful on systems whose regular fallback chain covers the
  # arrow with a non-emoji font
  fb <- systemfonts::font_fallback(arrow, family = "sans")
  skip_if(grepl("emoji", font_family(fb$path, fb$index), ignore.case = TRUE))

  s <- shape_text(paste0(arrow, " text"), family = "sans")
  arrow_font <- font_family(s$shape$font_path[1], s$shape$font_index[1])
  expect_false(grepl("emoji", arrow_font, ignore.case = TRUE))

  # The VS16 part of the test requires an emoji font on the system; some
  # minimal CI containers (e.g. the r-hub sanitizer images) have none
  fonts <- systemfonts::system_fonts()
  skip_if_not(
    any(grepl("emoji", fonts$family, ignore.case = TRUE)),
    "No emoji font installed"
  )

  # An explicit emoji variation selector must still pick the emoji font
  s_vs16 <- shape_text(paste0(arrow, "️ text"), family = "sans")
  arrow_font_vs16 <- font_family(
    s_vs16$shape$font_path[1],
    s_vs16$shape$font_index[1]
  )
  expect_true(grepl("emoji", arrow_font_vs16, ignore.case = TRUE))
})
