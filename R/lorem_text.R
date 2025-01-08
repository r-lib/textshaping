#' Get gibberish text in various scripts
#'
#' Textshaping exists partly to allow all the various scripts that exists in the
#' world to be used in R graphics. This function returns gibberish filler text
#' (lorem ipsum text) in various scripts for testing purpose. Some of these are
#' transliterations of the original lorem ipsum text while others are based an
#' a distribution model.
#'
#' @param script A string giving the script to fetch gibberish for
#' @param n The number of paragraphs to fetch. Each paragraph will be its own
#' element in the returned character vector.
#'
#' @return a charactor vector of length `n`
#'
#' @export
#'
#' @references https://generator.lorem-ipsum.info
#'
#' @examples
#' # Defaults to standard lorem ipsum
#' lorem_text()
#'
#' # Get two paragraphs of hangul (Korean)
#' lorem_text("hangul", 2)
#'
#' # Get gibberish bi-directional text
#' lorem_bidi()
#'
lorem_text <- function(script = c("latin", "chinese", "arabic", "devanagari", "cyrillic", "kana", "hangul", "greek", "hebrew", "armenian", "georgian"), n = 1) {
  #script <- match.arg(script)
  #file <- paste0(script, ".txt")
  #file <- system.file("lorem", file, package = "textshaping")
  #rep_len(readLines(file), n)
}

#' @rdname lorem_text
#'
#' @param ltr,rtl scripts to use for left-to-right and right-to-left text
#' @param ltr_prop The approximate proportion of left-to-right text in the final
#' string
#' @export
#'
lorem_bidi <- function(ltr = c("latin", "chinese", "devanagari", "cyrillic", "kana", "hangul", "greek", "armenian", "georgian"), rtl = c("arabic", "hebrew"), ltr_prop = 0.9, n = 1) {
  #ltr <- match.arg(ltr)
  #rtl = match.arg(rtl)
  #ltr_split <- if (ltr %in% c("chinese", "kana", "hangul")) "" else " "
  #ltr <- lorem_text(ltr, n)
  #rtl <- lorem_text(rtl, n)
  #mapply(function(ltr, rtl) {
  #  if (ltr_prop >= 0.5) {
  #    prop <- ltr_prop
  #    main <- ltr
  #    sub <- rtl
  #    sub_merge <- " "
  #    main_merge <- ltr_split
  #  } else {
  #    prop <- 1 - ltr_prop
  #    main <- rtl
  #    sub <- ltr
  #    sub_merge <- ltr_split
  #    main_merge <- " "
  #  }
  #  n_insert <- min(ceiling(length(main) * (1 - prop)), length(sub))
  #  n_chunks <- ceiling(stats::runif(1, n_insert * (prop - 0.5), n_insert))
  #  sub <- lapply(split(sub[seq_len(n_insert)], sort(c(seq_len(n_chunks), sample(n_chunks, n_insert - n_chunks, TRUE)))), paste, collapse = sub_merge)
  #  insertions <- sort(sample(length(main), n_chunks))
  #  for (i in rev(seq_along(insertions))) {
  #    main <- append(main, sub[[i]], insertions[i])
  #  }
  #  paste(main, collapse = main_merge)
  #}, ltr = strsplit(ltr, ltr_split), rtl = strsplit(rtl, " "))
}
