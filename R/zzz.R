.onLoad <- function(...) {
  if (!get_systemfont_cache_compat()) {
    packageStartupMessage(
      "systemfonts and textshaping have been compiled with different versions of Freetype.\nBecause of this, textshaping will not use the global font cache provided by systemfonts"
    )
  }
}
