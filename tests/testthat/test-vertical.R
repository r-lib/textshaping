# Use a CJK font for vertical shaping tests. Hiragino Sans is available on
# macOS; skip otherwise
cjk_family <- "Hiragino Sans"
cjk_available <- function() {
  loc <- try(systemfonts::match_fonts(cjk_family), silent = TRUE)
  !inherits(loc, "try-error") && nrow(loc) > 0 && file.exists(loc$path[1])
}

test_that("ttb shaping stacks glyphs downward", {
  skip_if_not(cjk_available())
  shape <- shape_text("テスト", family = cjk_family, direction = "ttb")

  expect_equal(shape$metrics$direction, "ttb")
  # Vertical text is not right-to-left
  expect_true(shape$metrics$ltr)
  # Main axis is vertical: height grows with glyphs, width is one column
  expect_equal(shape$metrics$height, 36, tolerance = 1)
  expect_equal(shape$metrics$width, 12, tolerance = 2)
  # Glyphs march down the y axis
  y <- shape$shape$y_offset
  expect_true(all(diff(y) < 0))
  expect_true(all(shape$shape$rotation == 0))
  # Advance is along the main (vertical) axis
  expect_true(all(shape$shape$advance > 0))
})

test_that("btt shaping stacks glyphs upward", {
  skip_if_not(cjk_available())
  shape <- shape_text("日本語", family = cjk_family, direction = "btt")

  expect_equal(shape$metrics$direction, "btt")
  expect_true(shape$metrics$height > 0)
  y <- shape$shape$y_offset
  expect_true(all(diff(y) > 0))
})

test_that("auto resolves required-vertical scripts to ttb but keeps both-ways scripts horizontal", {
  # Mongolian (U+1820) is required-vertical
  mongol <- shape_text("\u1820\u1822")
  expect_equal(mongol$metrics$direction, "ttb")

  skip_if_not(cjk_available())
  japanese <- shape_text("日本語", family = cjk_family)
  expect_equal(japanese$metrics$direction, "ltr")
})

test_that("mixed = rotate marks horizontal-only runs for rotation", {
  skip_if_not(cjk_available())
  shape <- shape_text("日本語ABCです", family = cjk_family, direction = "ttb")

  latin <- shape$shape[shape$shape$glyph %in% 4:6, ]
  cjk <- shape$shape[shape$shape$glyph %in% c(1:3, 7:8), ]
  expect_true(all(latin$rotation == -90))
  expect_true(all(cjk$rotation == 0))
  # Rotated runs still flow along the main axis
  expect_true(all(diff(latin$y_offset) < 0))
})

test_that("mixed = upright keeps horizontal-only runs upright", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "日本語ABCです",
    family = cjk_family,
    direction = "ttb",
    mixed = "upright"
  )
  expect_true(all(shape$shape$rotation == 0))
  expect_true(all(diff(shape$shape$y_offset) < 0))
})

test_that("strictly vertical scripts in explicit horizontal text shape horizontally", {
  mongol <- shape_text("\u1820\u1822", direction = "ltr")
  expect_equal(mongol$metrics$direction, "ltr")
  expect_equal(mongol$metrics$height, 12, tolerance = 4)
})

test_that("vertical text wraps into columns progressing right-to-left", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "あいうえおかきくけこさしすせそ",
    family = cjk_family,
    direction = "ttb",
    max_width = 0.5
  )
  # 15 glyphs at ~12pt each wrapped at 0.5in -> 5 columns of 3 glyphs
  expect_equal(shape$metrics$height, 36, tolerance = 1)
  expect_equal(shape$metrics$width, 60, tolerance = 3)
  x <- shape$shape$x_offset
  # First column is the rightmost one
  expect_true(x[1] > x[4])
  expect_true(x[4] > x[13])
})

test_that("direction is part of the shaping cache key", {
  skip_if_not(cjk_available())
  h1 <- shape_text("テスト", family = cjk_family)
  v <- shape_text("テスト", family = cjk_family, direction = "ttb")
  h2 <- shape_text("テスト", family = cjk_family)
  expect_equal(h1$shape$x_offset, h2$shape$x_offset)
  expect_false(isTRUE(all.equal(h1$shape$y_offset, v$shape$y_offset)))
})

test_that("text_width measures along the main axis", {
  skip_if_not(cjk_available())
  w_h <- text_width("テスト", family = cjk_family)
  w_v <- text_width("テスト", family = cjk_family, direction = "ttb")
  expect_equal(w_v, 36, tolerance = 1)
  expect_false(isTRUE(all.equal(w_h, w_v)))
})

test_that("rotated RTL runs keep reading order in vertical text", {
  skip_if_not(cjk_available())
  # Hebrew is a horizontal-only RTL script; inside ttb text the run is
  # rotated and its inline direction becomes bottom-to-top, i.e. the first
  # logical letter sits at the bottom of the run
  shape <- shape_text(
    "日本\u05D0\u05D1\u05D2語",
    family = cjk_family,
    direction = "ttb"
  )
  # glyph holds cluster + 1, i.e. logical order
  hebrew <- shape$shape[shape$shape$glyph %in% 3:5, ]
  hebrew <- hebrew[order(hebrew$glyph), ]
  expect_true(all(hebrew$rotation == -90))
  # Logical order is alef, bet, gimel: y_offset should increase (go up)
  expect_true(all(diff(hebrew$y_offset) > 0))

  # btt mirrors this: first logical letter sits at the top
  shape_btt <- shape_text(
    "日本\u05D0\u05D1\u05D2語",
    family = cjk_family,
    direction = "btt"
  )
  hebrew_btt <- shape_btt$shape[shape_btt$shape$glyph %in% 3:5, ]
  hebrew_btt <- hebrew_btt[order(hebrew_btt$glyph), ]
  expect_true(all(hebrew_btt$rotation == 90))
  expect_true(all(diff(hebrew_btt$y_offset) < 0))
})

test_that("btt pen position is consistent for strings ending in rotated runs", {
  skip_if_not(cjk_available())
  up <- shape_text("日本語", family = cjk_family, direction = "btt")
  rot <- shape_text("日本語ABC", family = cjk_family, direction = "btt")
  last_rot <- rot$shape[nrow(rot$shape), ]
  # When the string ends in a rotated run the pen's cross-axis position (x)
  # should stay on the run's reference edge (regression: it used to be
  # shifted by the last glyph's em width, landing off the column)
  expect_equal(rot$metrics$pen_x, last_rot$x_offset, tolerance = 1e-6)
  # For an upright-ending string the pen sits one correction step left of
  # the last glyph's origin, consistent with a hypothetical next glyph
  expect_true(abs(up$metrics$pen_x - up$shape$x_offset[nrow(up$shape)]) < 2)
})

test_that("mixed can be given per string", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    c("日本ABC", "日本ABC"),
    family = cjk_family,
    direction = "ttb",
    mixed = c("rotate", "upright")
  )
  latin1 <- shape$shape[shape$shape$metric_id == 1 & shape$shape$glyph > 2, ]
  latin2 <- shape$shape[shape$shape$metric_id == 2 & shape$shape$glyph > 2, ]
  expect_true(all(latin1$rotation == -90))
  expect_true(all(latin2$rotation == 0))
})

test_that("mixed = tate sets short digit runs horizontally (tate-chu-yoko)", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "平成24年3月15日",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate"
  )

  expect_true(all(shape$shape$rotation == 0))
  # glyph holds cluster + 1, i.e. logical order
  year <- shape$shape[shape$shape$glyph %in% 3:4, ]
  year <- year[order(year$glyph), ]
  # Digits share one line and march left-to-right
  expect_equal(year$y_offset[1], year$y_offset[2], tolerance = 0.1)
  expect_true(year$x_offset[1] < year$x_offset[2])
  # The run consumes one em of main-axis extent. The font provides hwid
  # digits, so two digits fill the em at full size without scaling
  expect_equal(year$advance[1], 12, tolerance = 1e-6)
  expect_equal(year$advance[2], 0)
  expect_equal(year$font_size, c(12, 12))

  # Single digits combine too, occupying a full em slot like their CJK
  # neighbours
  day <- shape$shape[shape$shape$glyph == 6, ]
  expect_equal(day$rotation, 0)
  expect_equal(day$advance, 12, tolerance = 1e-6)
  expect_equal(day$font_size, 12)

  # 3- and 4-digit runs use twid/qwid at full size when the font provides
  # them (Hiragino does), each filling exactly one em
  s34 <- shape_text(
    "価980円1980円",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate"
  )
  three <- s34$shape[s34$shape$glyph %in% 2:4, ]
  four <- s34$shape[s34$shape$glyph %in% 6:9, ]
  expect_equal(three$advance[1], 12, tolerance = 1e-6)
  expect_equal(four$advance[1], 12, tolerance = 1e-6)
  expect_true(all(s34$shape$font_size == 12))

  # Explicit feature settings take precedence over the automatic injection:
  # disabling qwid drops back to scale-to-fit
  ov <- shape_text(
    "1980",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate",
    features = systemfonts::font_feature(qwid = 0)
  )
  expect_true(all(ov$shape$font_size < 12))

  # Runs longer than 4 digits fall back to rotation
  long <- shape_text(
    "令和12345年",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate"
  )
  digits <- long$shape[long$shape$glyph %in% 3:7, ]
  expect_true(all(digits$rotation == -90))
})

test_that("tate digit order survives the btt mirror", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "平成24年",
    family = cjk_family,
    direction = "btt",
    mixed = "tate"
  )
  digits <- shape$shape[shape$shape$glyph %in% 3:4, ]
  digits <- digits[order(digits$glyph), ]
  expect_equal(digits$y_offset[1], digits$y_offset[2], tolerance = 0.1)
  expect_true(digits$x_offset[1] < digits$x_offset[2])
})

test_that("text_width measures tate runs along the main axis", {
  skip_if_not(cjk_available())
  # A combined run is capped at one em by the scale-to-fit
  w_tate <- text_width(
    "24",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate"
  )
  expect_equal(w_tate, 12, tolerance = 1e-6)
  # ...and single digits occupy a full em slot
  w_1 <- text_width("3", family = cjk_family, direction = "ttb", mixed = "tate")
  expect_equal(w_1, 12, tolerance = 1e-6)
})

test_that("tate narrow-feature injection degrades gracefully (no twid/qwid)", {
  # Hiragino Sans GB has hwid but not twid/qwid: 3-digit runs use hwid and
  # scale to fit, 4-digit runs fall back to hwid as well
  loc <- try(systemfonts::match_fonts("Hiragino Sans GB"), silent = TRUE)
  skip_if(
    inherits(loc, "try-error") || nrow(loc) == 0 || !file.exists(loc$path[1])
  )

  shape <- shape_text(
    "価980円1980円",
    path = loc$path[1],
    index = loc$index[1],
    direction = "ttb",
    mixed = "tate"
  )
  three <- shape$shape[shape$shape$glyph %in% 2:4, ]
  four <- shape$shape[shape$shape$glyph %in% 6:9, ]
  # Both runs occupy one em, drawn below full size via scale-to-fit
  expect_equal(three$advance[1], 12, tolerance = 1e-6)
  expect_equal(four$advance[1], 12, tolerance = 1e-6)
  expect_true(all(three$font_size < 12))
  expect_true(all(four$font_size < 12))
  # hwid scaling beats raw-digit scaling for 4 digits
  expect_true(four$font_size[1] > 12 / 3)
})

test_that("vertical kana use vkna forms when the font provides them", {
  skip_if_not(cjk_available())
  auto <- shape_text("きゃしゅっー", family = cjk_family, direction = "ttb")
  off <- shape_text(
    "きゃしゅっー",
    family = cjk_family,
    direction = "ttb",
    features = systemfonts::font_feature(vkna = 0)
  )
  # Hiragino has vkna alternates for kana
  expect_true(any(auto$shape$index != off$shape$index))
  # Explicit user setting takes precedence: enabling it explicitly matches
  # the automatic injection
  on <- shape_text(
    "きゃしゅっー",
    family = cjk_family,
    direction = "ttb",
    features = systemfonts::font_feature(vkna = 1)
  )
  expect_equal(auto$shape$index, on$shape$index)
})

test_that("kinsoku shori keeps closing punctuation off column starts", {
  skip_if_not(cjk_available())
  txt <- "彼は言った。「それは面白い、とても面白い。」しかし、疑問は残る（本当に？）"
  shape <- shape_text(
    txt,
    family = cjk_family,
    direction = "ttb",
    max_width = 0.4
  )
  chars <- strsplit(txt, "")[[1]]
  x <- round(shape$shape$x_offset, 1)
  col_start <- c(TRUE, x[-1] != x[-length(x)])
  start_chars <- chars[shape$shape$glyph[col_start]]
  # No column may start with line-start-prohibited punctuation
  expect_false(any(start_chars %in% c("。", "、", "」", "）", "？", "ー")))
  # Closing punctuation hangs past the column end instead
  expect_true(
    max(shape$shape$y_offset) < 0 ||
      any(abs(shape$shape$y_offset) > 0.4 * 72 - 12)
  )
})

test_that("kinsoku shori also applies to forced breaks in rtl embeddings", {
  skip_if_not(cjk_available())
  txt <- "בדיקהבדיקה。」בדיקהבדיקה」בדיקה"
  shape <- shape_text(
    txt,
    family = cjk_family,
    direction = "rtl",
    max_width = 0.9
  )
  chars <- strsplit(txt, "")[[1]]
  y <- round(shape$shape$y_offset, 1)
  # For rtl embeddings the array-last glyph of each line is the visual line
  # start; it may never be line-start-prohibited punctuation
  line_end <- c(y[-1] != y[-length(y)], TRUE)
  start_chars <- chars[pmin(shape$shape$glyph[line_end], nchar(txt))]
  expect_false(any(start_chars %in% c("。", "、", "」", "）", "？", "ー")))
})

test_that("mixed = sideways rotates everything, CJK included", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "日本語のテキスト",
    family = cjk_family,
    direction = "ttb",
    mixed = "sideways"
  )
  expect_true(all(shape$shape$rotation == -90))
  expect_true(all(diff(shape$shape$y_offset) < 0))
  # btt mirrors the rotation
  btt <- shape_text(
    "日本語",
    family = cjk_family,
    direction = "btt",
    mixed = "sideways"
  )
  expect_true(all(btt$shape$rotation == 90))
  expect_true(all(diff(btt$shape$y_offset) > 0))
  # The string shapes as one horizontal run: width along the column equals
  # the horizontal string width
  expect_equal(
    text_width(
      "日本語のテキスト",
      family = cjk_family,
      direction = "ttb",
      mixed = "sideways"
    ),
    text_width("日本語のテキスト", family = cjk_family),
    tolerance = 1e-6
  )
})

test_that("fullwidth punctuation trailing blank collapses at line end", {
  skip_if_not(cjk_available())
  # Line ending in 。: the blank half of its em advance hangs off the line
  collapsed <- shape_text("あいう。", family = cjk_family)$metrics
  plain <- shape_text("あいう", family = cjk_family)$metrics
  expect_equal(collapsed$width - plain$width, 12 - 7.734375, tolerance = 1e-6)
  expect_equal(collapsed$right_bearing, 0)

  # Same along the vertical main axis
  v_collapsed <- shape_text(
    "あいう。",
    family = cjk_family,
    direction = "ttb"
  )$metrics
  v_plain <- shape_text(
    "あいう",
    family = cjk_family,
    direction = "ttb"
  )$metrics
  expect_equal(
    v_collapsed$height - v_plain$height,
    12 - 7.734375,
    tolerance = 1e-6
  )

  # Mid-line punctuation keeps its full advance
  expect_equal(
    shape_text("あ。い", family = cjk_family)$metrics$width,
    36,
    tolerance = 1e-6
  )
})

test_that("opening bracket leading blank collapses at line start", {
  skip_if_not(cjk_available())
  bracketed <- shape_text("「あいう」", family = cjk_family)$metrics
  plain <- shape_text("あいう", family = cjk_family)$metrics
  # Both the leading blank of 「 and the trailing blank of 」 hang outside:
  # the width is the text plus the two bracket inks
  expect_equal(bracketed$width - plain$width, 7.5, tolerance = 0.1)
  expect_equal(bracketed$left_bearing, 0)

  # Same along the vertical main axis
  v_bracketed <- shape_text(
    "「あいう」",
    family = cjk_family,
    direction = "ttb"
  )$metrics
  expect_equal(v_bracketed$height, bracketed$width, tolerance = 0.1)

  # Mid-line brackets keep their full advance
  expect_equal(
    shape_text("あ「い", family = cjk_family)$metrics$width,
    36,
    tolerance = 1e-6
  )
})

test_that("opening bracket after closing punctuation collapses facing blanks", {
  skip_if_not(cjk_available())
  # 。「: the leading blank of 「 collapses into the trailing blank of 。
  expect_equal(
    shape_text("あ。「い", family = cjk_family)$metrics$width,
    39.75,
    tolerance = 0.1
  )
  expect_equal(
    shape_text(
      "あ。「い",
      family = cjk_family,
      direction = "ttb"
    )$metrics$height,
    39.75,
    tolerance = 0.1
  )
  # No collapse without the punctuation pair
  expect_equal(
    shape_text("あ。い", family = cjk_family)$metrics$width,
    36,
    tolerance = 1e-6
  )
  expect_equal(
    shape_text("あ「「い", family = cjk_family)$metrics$width,
    48,
    tolerance = 1e-6
  )
})

test_that("mixed = combine combines arbitrary short runs", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "第1巻ABC出版ABCDE",
    family = cjk_family,
    direction = "ttb",
    mixed = "combine"
  )

  # The digit combines with narrow forms as in tate mode, in a full em slot
  expect_equal(shape$shape$advance[shape$shape$glyph == 2], 12, tolerance = 1e-6)

  # The 3-letter run combines upright on one line, scaled to the em
  abc <- shape$shape[shape$shape$glyph %in% 4:6, ]
  abc <- abc[order(abc$glyph), ]
  expect_true(all(abc$rotation == 0))
  expect_equal(abc$y_offset[1], abc$y_offset[2], tolerance = 0.1)
  expect_true(all(diff(abc$x_offset) > 0))
  expect_equal(abc$advance[1], 12, tolerance = 1e-6)
  # ...but without digit narrow forms, so it needed scaling
  expect_true(all(abc$font_size < 12))

  # Runs longer than 4 characters rotate as usual
  abcde <- shape$shape[shape$shape$glyph %in% 9:13, ]
  expect_true(all(abcde$rotation == -90))
})

test_that("UAX#50 Tr characters use vert glyphs or rotate as fallback", {
  skip_if_not(cjk_available())
  # Hiragino provides vert alternates for brackets and dashes: upright
  s <- shape_text("「あいう。」〜ー", family = cjk_family, direction = "ttb")
  expect_true(all(s$shape$rotation == 0))

  # In a font without vert substitutions Tr characters rotate like R-class
  # (here via the macOS fallback chain, which resolves Tr characters to a
  # face without vert)
  skip_if(
    inherits(try(systemfonts::match_fonts("Arial"), silent = TRUE), "try-error")
  )
  s2 <- shape_text("〜", family = "Arial", direction = "ttb")
  expect_equal(s2$shape$rotation, -90)
})

test_that("VORG vertical origins are honoured on the main axis", {
  skip_if_not(cjk_available())
  # Hiragino's VORG gives fullwidth lowercase letters a designed origin
  # (originY 773/770) differing from the default (880): ａ and ｚ sit
  # optically centered in their slot rather than aligned with Ａ
  s <- shape_text("あａｚＡ", family = cjk_family, direction = "ttb")
  slot_pos <- s$shape$y_offset - (seq_len(nrow(s$shape)) - 1) * -12
  expect_equal(slot_pos[1], -880 * 12 / 1000, tolerance = 0.1) # あ default
  expect_equal(slot_pos[2], -773 * 12 / 1000, tolerance = 0.1) # ａ VORG record
  expect_equal(slot_pos[3], -770 * 12 / 1000, tolerance = 0.1) # ｚ VORG record
  expect_equal(slot_pos[4], -880 * 12 / 1000, tolerance = 0.1) # Ａ default
})

test_that("line-end punctuation fits by ink (compression)", {
  skip_if_not(cjk_available())
  # 5 full em + 。 in 5.5 em: 。 overflows by advance but its ink fits, so
  # it compresses onto the line with the trailing blank hanging off
  w_in <- 5.5 * 12 / 72
  h <- shape_text("あああああ。", family = cjk_family, max_width = w_in)
  expect_equal(length(unique(round(h$shape$y_offset, 1))), 1)

  v <- shape_text(
    "あああああ。",
    family = cjk_family,
    direction = "ttb",
    max_width = w_in
  )
  expect_equal(length(unique(round(v$shape$x_offset, 1))), 1)
})

test_that("vertical shaping with multiple lines and hard breaks", {
  skip_if_not(cjk_available())
  shape <- shape_text("あいう\nえおか", family = cjk_family, direction = "ttb")
  expect_equal(nrow(shape$metrics), 1)
  # Two columns
  x <- unique(round(shape$shape$x_offset, 1))
  expect_equal(length(x), 2)
  expect_true(x[1] > x[2])
})
