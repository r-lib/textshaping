# Use a CJK font for vertical shaping tests. Hiragino Sans is available on
# macOS; skip otherwise
cjk_family <- "Hiragino Sans"
cjk_available <- function() {
  # match_fonts() always returns a fallback, so check installed fonts directly
  fonts <- try(systemfonts::system_fonts(), silent = TRUE)
  !inherits(fonts, "try-error") && any(fonts$family == cjk_family)
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

test_that("ttb-ltr shaping stacks glyphs downward, columns left-to-right", {
  skip_if_not(cjk_available())
  shape <- shape_text("日本語", family = cjk_family, direction = "ttb-ltr")

  expect_equal(shape$metrics$direction, "ttb-ltr")
  expect_true(shape$metrics$height > 0)
  # Glyphs march down the y axis as in ttb
  y <- shape$shape$y_offset
  expect_true(all(diff(y) < 0))

  # Columns progress left-to-right
  wrapped <- shape_text(
    "あいうえおかきくけこさしすせそ",
    family = cjk_family,
    direction = "ttb-ltr",
    max_width = 0.5
  )
  x <- wrapped$shape$x_offset
  # First column is the leftmost one
  expect_true(x[1] < x[4])
  expect_true(x[4] < x[13])
})

test_that("auto resolves required-vertical scripts to ttb-ltr but keeps both-ways scripts horizontal", {
  # Mongolian (U+1820) is required-vertical
  mongol <- shape_text("\u1820\u1822")
  expect_equal(mongol$metrics$direction, "ttb-ltr")

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

  # ttb-ltr rotates the other way: the first logical letter sits at the top
  shape_btt <- shape_text(
    "日本\u05D0\u05D1\u05D2語",
    family = cjk_family,
    direction = "ttb-ltr"
  )
  hebrew_btt <- shape_btt$shape[shape_btt$shape$glyph %in% 3:5, ]
  hebrew_btt <- hebrew_btt[order(hebrew_btt$glyph), ]
  expect_true(all(hebrew_btt$rotation == 90))
  expect_true(all(diff(hebrew_btt$y_offset) < 0))
})

test_that("ttb-ltr pen position is consistent for strings ending in rotated runs", {
  skip_if_not(cjk_available())
  up <- shape_text("日本語", family = cjk_family, direction = "ttb-ltr")
  rot <- shape_text("日本語ABC", family = cjk_family, direction = "ttb-ltr")
  last_rot <- rot$shape[nrow(rot$shape), ]
  # When the string ends in a rotated run the pen's cross-axis position (x)
  # should stay on the run's reference edge (regression: it used to be
  # shifted by the last glyph's em width, landing off the column)
  expect_equal(rot$metrics$pen_x, last_rot$x_offset, tolerance = 1e-6)
  # For an upright-ending string the pen sits one correction step left of
  # the last glyph's origin, consistent with a hypothetical next glyph
  expect_true(abs(up$metrics$pen_x - up$shape$x_offset[nrow(up$shape)]) < 2)
})

test_that("required-vertical script reads top-to-bottom in ttb-ltr", {
  # Mongolian is UAX#50 R but is rotated counter-clockwise (unlike CJK's
  # sideways runs) and keeps reading top-to-bottom — it emulates the
  # script's own vertical presentation, so the sideways-run flip must not
  # apply to it
  mongol <- shape_text("ᠮᠣᠩᠭᠣᠯ")
  expect_equal(mongol$metrics$direction, "ttb-ltr")
  expect_true(all(mongol$shape$rotation == 90))
  m <- mongol$shape[order(mongol$shape$glyph), ]
  expect_true(all(diff(m$y_offset) < 0))
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

test_that("tate digit order survives the ttb-ltr mirror", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "平成24年",
    family = cjk_family,
    direction = "ttb-ltr",
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
  # ttb-ltr mirrors the rotation, so sideways runs read bottom-to-top
  # (glyph holds cluster + 1, i.e. logical order)
  btt <- shape_text(
    "日本語",
    family = cjk_family,
    direction = "ttb-ltr",
    mixed = "sideways"
  )
  expect_true(all(btt$shape$rotation == 90))
  btt <- btt$shape[order(btt$shape$glyph), ]
  expect_true(all(diff(btt$y_offset) > 0))
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
  expect_equal(
    shape$shape$advance[shape$shape$glyph == 2],
    12,
    tolerance = 1e-6
  )

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

test_that("mixed = combine ignores spaces when capping cluster length", {
  skip_if_not(cjk_available())
  shape <- shape_text(
    "で組むと ABC のように文字を重ねて一文字分に収めます",
    family = cjk_family,
    direction = "ttb",
    mixed = "combine"
  )

  # " ABC " counts 3 non-space characters, so it combines upright in one
  # cluster (flanking spaces included) rather than exceeding the
  # 4-character cap and rotating
  abc <- shape$shape[shape$shape$glyph %in% 5:9, ]
  abc <- abc[order(abc$glyph), ]
  expect_true(all(abc$rotation == 0))
  expect_equal(abc$y_offset[1], abc$y_offset[2], tolerance = 0.1)
  expect_equal(abc$y_offset[1], abc$y_offset[5], tolerance = 0.1)
  expect_true(all(diff(abc$x_offset) > 0))
  expect_equal(sum(abc$advance), 12, tolerance = 1e-6)

  # A run with more than 4 non-space characters does not combine at all:
  # it rotates as a whole instead of splitting word by word
  fallback <- shape_text(
    "we need 10 cats",
    family = cjk_family,
    direction = "ttb",
    mixed = "combine"
  )
  expect_true(all(fallback$shape$rotation == -90))
})

test_that("tate2/tate3/combine2/combine3 cap the combined run length", {
  skip_if_not(cjk_available())

  # tate2: only 1-2 digit runs combine; 3 digits rotate
  s2 <- shape_text(
    "第45巻123",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate2"
  )
  expect_true(all(s2$shape$rotation[s2$shape$glyph %in% 2:3] == 0))
  expect_true(all(s2$shape$rotation[s2$shape$glyph %in% 5:7] == -90))

  # tate3: 3-digit runs combine, 4-digit runs rotate
  s3 <- shape_text(
    "第123巻2024",
    family = cjk_family,
    direction = "ttb",
    mixed = "tate3"
  )
  expect_true(all(s3$shape$rotation[s3$shape$glyph %in% 2:4] == 0))
  expect_true(all(s3$shape$rotation[s3$shape$glyph %in% 6:9] == -90))

  # combine2/combine3: the cap applies to non-space characters of the run
  c3 <- shape_text(
    "第ABC巻ABCD",
    family = cjk_family,
    direction = "ttb",
    mixed = "combine3"
  )
  expect_true(all(c3$shape$rotation[c3$shape$glyph %in% 2:4] == 0))
  expect_true(all(c3$shape$rotation[c3$shape$glyph %in% 6:9] == -90))

  c2 <- shape_text(
    "第AB巻ABC",
    family = cjk_family,
    direction = "ttb",
    mixed = "combine2"
  )
  expect_true(all(c2$shape$rotation[c2$shape$glyph %in% 2:3] == 0))
  expect_true(all(c2$shape$rotation[c2$shape$glyph %in% 5:7] == -90))
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

test_that("kinsoku = none allows prohibited characters at column starts", {
  skip_if_not(cjk_available())
  txt <- "彼は言った。「それは面白い、とても面白い。」しかし、疑問は残る（本当に？）"
  chars <- strsplit(txt, "")[[1]]
  col_starts <- function(shape) {
    x <- round(shape$shape$x_offset, 1)
    chars[shape$shape$glyph[c(TRUE, x[-1] != x[-length(x)])]]
  }
  none <- shape_text(
    txt,
    family = cjk_family,
    direction = "ttb",
    max_width = 0.4,
    kinsoku = "none"
  )
  # With kinsoku disabled the violations the strict mode suppresses reappear
  expect_true(any(col_starts(none) %in% c("。", "」", "）")))
  strict <- shape_text(
    txt,
    family = cjk_family,
    direction = "ttb",
    max_width = 0.4
  )
  expect_false(any(col_starts(strict) %in% c("。", "、", "」", "）")))
})

test_that("kinsoku = loose permits small kana but not closing punctuation at line starts", {
  skip_if_not(cjk_available())
  # At one glyph per column the unbreakable segment うっ straddles the column
  # end: strict hangs っ, loose lets it start a column
  txt <- "あいうっえ"
  chars <- strsplit(txt, "")[[1]]
  col_starts <- function(kinsoku) {
    shape <- shape_text(
      txt,
      family = cjk_family,
      direction = "ttb",
      max_width = 0.2,
      kinsoku = kinsoku
    )
    x <- round(shape$shape$x_offset, 1)
    chars[shape$shape$glyph[c(TRUE, x[-1] != x[-length(x)])]]
  }
  expect_false("っ" %in% col_starts("strict"))
  expect_true("っ" %in% col_starts("loose"))

  # Closing punctuation still hangs in loose mode
  txt2 <- "彼は言った。「それは面白い、とても面白い。」しかし、疑問は残る（本当に？）"
  chars2 <- strsplit(txt2, "")[[1]]
  loose <- shape_text(
    txt2,
    family = cjk_family,
    direction = "ttb",
    max_width = 0.4,
    kinsoku = "loose"
  )
  x <- round(loose$shape$x_offset, 1)
  starts <- chars2[loose$shape$glyph[c(TRUE, x[-1] != x[-length(x)])]]
  expect_false(any(starts %in% c("。", "、", "」", "）")))
})

test_that("inseparable and number-binding characters stay together (UAX#14 tables)", {
  skip_if_not(cjk_available())
  # …… may not split across columns
  txt <- "あいうえお……かきくけこさしすせそ"
  for (w in seq(0.3, 0.8, by = 0.1)) {
    shape <- shape_text(
      txt,
      family = cjk_family,
      direction = "ttb",
      max_width = w
    )
    x <- round(shape$shape$x_offset, 1)
    col <- cumsum(c(TRUE, x[-1] != x[-length(x)]))
    ellipsis <- col[shape$shape$glyph %in% 6:7]
    expect_equal(length(unique(ellipsis)), 1)
  }
  # ¥ binds to the following digits, % to the preceding ones: neither may be
  # split from its number even when wrapping is forced. (Widths below the
  # width of the ¥100 unit itself are excluded — there an emergency break is
  # unavoidable)
  txt2 <- "代金は¥100で50%引き"
  for (w in seq(0.45, 0.85, by = 0.1)) {
    shape <- shape_text(
      txt2,
      family = cjk_family,
      direction = "ttb",
      max_width = w
    )
    x <- round(shape$shape$x_offset, 1)
    col <- cumsum(c(TRUE, x[-1] != x[-length(x)]))
    g <- shape$shape$glyph
    expect_equal(length(unique(col[g %in% 4:7])), 1) # ¥100
    expect_equal(length(unique(col[g %in% 9:11])), 1) # 50%
  }
})

test_that("kinsoku_style = push-out moves punctuation down and expands the line", {
  skip_if_not(cjk_available())
  txt <- paste0(paste(rep("あ", 9), collapse = ""), "」い")
  w <- 9.5 * 12 / 72
  out <- shape_text(
    txt,
    family = cjk_family,
    max_width = w,
    kinsoku_style = "push-out"
  )
  y <- round(out$shape$y_offset, 1)
  # 」 moves to the second line instead of hanging
  expect_equal(out$shape$glyph[y == y[1]], 1:8)
  expect_equal(out$shape$glyph[y != y[1]], 9:11)
  # The slack is spread over the inter-character gaps: the line is flush
  expect_equal(max(out$shape$x_offset[y == y[1]]), 102, tolerance = 0.1)
  expect_equal(
    diff(out$shape$x_offset[y == y[1]]),
    rep(102 / 7, 7),
    tolerance = 0.1
  )
  expect_equal(out$metrics$width, w * 72, tolerance = 0.1)

  # Default (hang) keeps 」 on the first line
  hang <- shape_text(txt, family = cjk_family, max_width = w)
  y_h <- round(hang$shape$y_offset, 1)
  expect_equal(hang$shape$glyph[y_h == y_h[1]], 1:10)
})

test_that("kinsoku_style = push-in compresses gaps to fit the line", {
  skip_if_not(cjk_available())
  txt <- paste0(paste(rep("あ", 10), collapse = ""), "」い")
  # 」 overflows by 700/64 pt — beyond its hanging blank, but within the
  # cumulative 1/4 em tolerance of the ten inter-character gaps
  w <- (11 * 768 - 700) / 64 / 72
  out <- shape_text(
    txt,
    family = cjk_family,
    max_width = w,
    kinsoku_style = "push-in"
  )
  y <- round(out$shape$y_offset, 1)
  # 」 is pulled onto the first line
  expect_equal(out$shape$glyph[y == y[1]], 1:11)
  # The gaps are compressed by up to 1/4 em (3 pt) each
  steps <- diff(out$shape$x_offset[y == y[1]])
  expect_true(all(steps <= 12))
  expect_true(any(steps < 12))
  expect_true(all(steps >= 12 - 3))
  expect_equal(sum(steps) + 12, w * 72, tolerance = 0.1)

  # Default (hang) breaks earlier, leaving 」 for the second line
  hang <- shape_text(txt, family = cjk_family, max_width = w)
  y_h <- round(hang$shape$y_offset, 1)
  expect_equal(hang$shape$glyph[y_h == y_h[1]], 1:9)
})

test_that("push-in compresses punctuation blanks before inter-character gaps", {
  skip_if_not(cjk_available())
  txt <- paste0("あ。", paste(rep("あ", 7), collapse = ""), "」い")
  w <- 7100 / 64 / 72
  out <- shape_text(
    txt,
    family = cjk_family,
    max_width = w,
    kinsoku_style = "push-in"
  )
  y <- round(out$shape$y_offset, 1)
  x <- out$shape$x_offset[y == y[1]]
  # The blank inside 。 is consumed first (7.73 pt), the inter-character
  # gaps only take the remainder
  expect_equal(x[3] - x[2], 12 - 7.73, tolerance = 0.1)
  expect_true(all(diff(x[3:length(x)]) >= 12 - 3))
})

test_that("kinsoku styles leave Latin justification untouched", {
  skip_if_not(cjk_available())
  txt <- "word word word word word word"
  base <- shape_text(txt, max_width = 1.2, align = "justified")
  po <- shape_text(
    txt,
    max_width = 1.2,
    align = "justified",
    kinsoku_style = "push-out"
  )
  pi_ <- shape_text(
    txt,
    max_width = 1.2,
    align = "justified",
    kinsoku_style = "push-in"
  )
  expect_equal(base$shape, po$shape)
  expect_equal(base$shape, pi_$shape)
})

test_that("kinsoku styles work in rtl embeddings", {
  skip_if_not(cjk_available())
  txt <- "בדיקהבדיקה。」בדיקהבדיקה」בדיקה"
  for (style in c("hang", "push-out", "push-in")) {
    shape <- shape_text(
      txt,
      family = cjk_family,
      direction = "rtl",
      max_width = 0.5,
      kinsoku_style = style
    )
    expect_equal(shape$metrics$direction, "rtl")
    expect_equal(nrow(shape$shape), nchar(txt))
  }
  # Push-out never lets the prohibited glyphs start a line
  po <- shape_text(
    txt,
    family = cjk_family,
    direction = "rtl",
    max_width = 0.5,
    kinsoku_style = "push-out"
  )
  y <- round(po$shape$y_offset, 1)
  chars <- strsplit(txt, "")[[1]]
  line_end <- c(y[-1] != y[-length(y)], TRUE)
  start_chars <- chars[pmin(po$shape$glyph[line_end], nchar(txt))]
  expect_false(any(start_chars %in% c("。", "」")))
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

test_that("null (spacer) glyphs follow the column conventions in vertical text", {
  skip_if_not(cjk_available())
  for (dir in c("ttb", "ttb-ltr")) {
    shape <- shape_text(
      c("縦書き", NA, "のテスト"),
      id = 1,
      family = cjk_family,
      tracking = c(0, 24, 0),
      direction = dir,
      size = 12,
      res = 72
    )
    spacer <- shape$shape[is.na(shape$shape$index), ]
    glyphs <- shape$shape[!is.na(shape$shape$index), ]
    expect_equal(nrow(spacer), 1)
    # Cross-axis origin and font metrics match the surrounding run
    expect_equal(spacer$x_offset, glyphs$x_offset[1])
    expect_equal(spacer$ascender, glyphs$ascender[1])
    expect_equal(spacer$descender, glyphs$descender[1])
    # The reserved advance is honoured along the main axis: the glyphs
    # bracketing the spacer are pushed apart by exactly the tracking amount
    expect_equal(spacer$advance, 24)
    expect_equal(
      glyphs$y_offset[4] - glyphs$y_offset[3],
      -(glyphs$advance[3] + 24)
    )
    # The spacer is an up-cell covering exactly the reserved slot
    # ([y_offset, y_offset + advance] = slot): it abuts the slot edges of
    # its neighbours (upright glyph slots are anchored at y_offset +
    # descender / y_offset + ascender)
    expect_equal(
      spacer$y_offset + spacer$advance,
      glyphs$y_offset[3] + glyphs$descender[3]
    )
    expect_equal(spacer$y_offset, glyphs$y_offset[4] + glyphs$ascender[4])
  }
})

test_that("null (spacer) glyphs keep the column origin next to rotated runs", {
  skip_if_not(cjk_available())
  for (dir in c("ttb", "ttb-ltr")) {
    shape <- shape_text(
      c("縦書き", NA, "ABC", NA, "テスト"),
      id = 1,
      family = cjk_family,
      tracking = c(0, 24, 0, 18, 0),
      direction = dir,
      size = 12,
      res = 72
    )
    spacer <- shape$shape[is.na(shape$shape$index), ]
    upright <- shape$shape[
      shape$shape$rotation == 0 & !is.na(shape$shape$index),
    ]
    rotated <- shape$shape[shape$shape$rotation != 0, ]
    expect_equal(nrow(spacer), 2)
    expect_equal(spacer$x_offset, rep(upright$x_offset[1], 2))
    # Slot-exact up-cells abut the advance cells of the rotated run on both
    # sides. Rotated runs are down-cells in ttb (-90) and up-cells in
    # ttb-ltr (+90), so the run's cell edges are taken from the appropriate
    # side of its first/last glyph
    first <- rotated[1, ]
    last <- rotated[nrow(rotated), ]
    run_top <- if (first$rotation > 0) {
      first$y_offset + first$advance
    } else {
      first$y_offset
    }
    run_bottom <- if (last$rotation > 0) {
      last$y_offset
    } else {
      last$y_offset - last$advance
    }
    expect_equal(spacer$y_offset[1], run_top, tolerance = 0.1)
    expect_equal(
      spacer$y_offset[2] + spacer$advance[2],
      run_bottom,
      tolerance = 0.1
    )
  }
})

test_that("cell bounds follow the anchoring convention in vertical text", {
  skip_if_not(cjk_available())
  for (dir in c("ttb", "ttb-ltr")) {
    shape <- shape_text(
      c("テスト", "ABC", NA),
      id = c(1L, 1L, 1L),
      family = cjk_family,
      direction = dir,
      tracking = c(0, 0, 24)
    )
    sh <- shape$shape
    upright <- sh[!sh$spacer & sh$rotation == 0, ]
    rotated <- sh[sh$rotation != 0, ]
    spacer <- sh[sh$spacer, ]

    # Upright cells are the advance slot along the column and
    # [x_offset, x_offset + em] across it (em is 12 at size 12). The slot is
    # not derivable from y_offset, which is the font's VORG origin
    expect_equal(upright$cell_y1 - upright$cell_y0, upright$advance)
    expect_equal(upright$cell_x0, upright$x_offset)
    expect_equal(upright$cell_x1 - upright$cell_x0, rep(12, nrow(upright)))

    # Rotated runs sit on their baseline: -90 (ttb) cells extend down from
    # y_offset, +90 (ttb-ltr) cells extend up; cross-axis they span the run's
    # extents around the baseline (mirrored for +90)
    down <- rotated$rotation < 0
    expect_equal(rotated$cell_y1[down], rotated$y_offset[down])
    expect_equal(
      rotated$cell_y0[down],
      rotated$y_offset[down] - rotated$advance[down]
    )
    expect_equal(rotated$cell_y0[!down], rotated$y_offset[!down])
    expect_equal(
      rotated$cell_y1[!down],
      rotated$y_offset[!down] + rotated$advance[!down]
    )
    expect_equal(
      rotated$cell_x0[down],
      rotated$x_offset[down] + rotated$descender[down]
    )
    expect_equal(
      rotated$cell_x1[down],
      rotated$x_offset[down] + rotated$ascender[down]
    )
    expect_equal(
      rotated$cell_x0[!down],
      rotated$x_offset[!down] - rotated$ascender[!down]
    )
    expect_equal(
      rotated$cell_x1[!down],
      rotated$x_offset[!down] - rotated$descender[!down]
    )

    # Spacers are slot-exact up-cells with the column em across
    expect_equal(spacer$cell_y0, spacer$y_offset)
    expect_equal(spacer$cell_y1, spacer$y_offset + spacer$advance)
    expect_equal(spacer$cell_x1 - spacer$cell_x0, 12)

    # All cells in the column tile seamlessly along the main axis
    ord <- order(sh$cell_y1, decreasing = TRUE)
    expect_equal(sh$cell_y0[ord][-nrow(sh)], sh$cell_y1[ord][-1])
  }
})

test_that("vertical block anchoring is direction-independent", {
  skip_if_not(cjk_available())
  for (vj in c(0, 0.5, 1)) {
    for (dir in c("ttb", "ttb-ltr")) {
      shape <- shape_text(
        "テストの文字列です",
        family = cjk_family,
        direction = dir,
        max_width = 0.6,
        vjust = vj
      )
      # The origin sits at the vjust fraction of the content box, as for the
      # cross axis of horizontal text, regardless of column flow direction
      expect_equal(
        shape$metrics$left_border,
        -vj * shape$metrics$width,
        tolerance = 1e-6
      )
      # Columns flow in opposite directions inside the same box (glyphs are
      # reported in column order, so first appearances give the column flow)
      cols <- shape$shape$cell_x0[!duplicated(round(shape$shape$cell_x0, 3))]
      if (dir == "ttb") {
        expect_true(all(diff(cols) < 0))
      } else {
        expect_true(all(diff(cols) > 0))
      }
    }
  }

  # With default justification the box starts at the origin in both
  # directions (vjust = 0 covered above; check glyph content sits at or
  # inside the box edges modulo the bearings reported in metrics)
  for (dir in c("ttb", "ttb-ltr")) {
    shape <- shape_text(
      "テストの文字列です",
      family = cjk_family,
      direction = dir,
      max_width = 0.6
    )
    expect_equal(shape$metrics$left_border, 0)
    expect_equal(shape$metrics$top_border, 0)
    expect_gte(min(shape$shape$cell_x0), -shape$metrics$width)
    expect_lte(max(shape$shape$cell_x1), 2 * shape$metrics$width)
  }
})
