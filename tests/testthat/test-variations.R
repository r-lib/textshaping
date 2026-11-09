test_that("font variations travel through shaping", {
  variable <- systemfonts::system_fonts()
  variable <- variable[variable$variable, ]
  skip_if(nrow(variable) == 0, "No variable fonts available on system")

  # Find a variable font with a weight axis that affects shaping. Some fonts
  # keep advances constant across the weight axis so we need to probe
  light <- heavy <- NA_real_
  light_shape <- heavy_shape <- NA_real_
  for (i in seq_len(nrow(variable))) {
    info <- suppressWarnings(try(
      systemfonts::font_info(path = variable$path[i], index = variable$index[i]),
      silent = TRUE
    ))
    if (inherits(info, "try-error") || nrow(info) == 0) next
    axes <- info$variation_axes[[1]]
    if (!"wght" %in% names(axes)) next
    wght <- axes$wght
    light <- text_width(
      "Hello world",
      path = info$path,
      index = info$index,
      variation = systemfonts::font_variation(wght = wght["min"])
    )
    heavy <- text_width(
      "Hello world",
      path = info$path,
      index = info$index,
      variation = systemfonts::font_variation(wght = wght["max"])
    )
    light_shape <- shape_text(
      "Hello world",
      path = info$path,
      index = info$index,
      variation = systemfonts::font_variation(wght = wght["min"])
    )$metrics$width
    heavy_shape <- shape_text(
      "Hello world",
      path = info$path,
      index = info$index,
      variation = systemfonts::font_variation(wght = wght["max"])
    )$metrics$width
    if (heavy != light && heavy_shape != light_shape) break
  }
  skip_if(
    is.na(light) || (heavy == light && heavy_shape == light_shape),
    "No variable font with a shaping-relevant weight axis on system"
  )

  expect_gt(heavy, light)
  expect_gt(heavy_shape, light_shape)
})
