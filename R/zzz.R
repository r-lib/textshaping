.onAttach <- function(...) {
  if (!get_systemfont_cache_compat()) {
    packageStartupMessage(
      "systemfonts and textshaping have been compiled with different versions of FreeType.\n",
      "textshaping will use its own FreeType instance instead of the font cache provided by systemfonts; text rendering is unaffected.\n",
      "To remove this message, reinstall both packages from the same source so they are built against the same FreeType. To suppress it, use suppressPackageStartupMessages(library(textshaping))."
    )
  }
}
