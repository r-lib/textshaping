# Generate src/line_break_classes.inc from the UAX#14 data file LineBreak.txt
# (UCD). Run manually when updating to a new Unicode version:
#   Rscript data-raw/gen-linebreak-tables.R

url <- "https://www.unicode.org/Public/UCD/latest/ucd/LineBreak.txt"
tmp <- tempfile(fileext = ".txt")
download.file(url, tmp, quiet = TRUE)
lines <- readLines(tmp, warn = FALSE)

version <- sub(
  ".*LineBreak-([0-9.]+)\\.txt.*",
  "\\1",
  grep(
    "# LineBreak-[0-9.]+\\.txt",
    lines,
    value = TRUE
  )[1]
)

# Classes used by the kinsoku shori / spacing logic
classes <- c(
  "OP", # opening punctuation
  "CL", # closing punctuation
  "CP", # closing parenthesis
  "NS", # non-starters (iteration marks, ・：；)
  "CJ", # conditional Japanese starters (small kana, ー)
  "EX", # exclamation/interrogation
  "IN", # inseparable (‥…)
  "GL", # glue (nbsp etc.)
  "HY", # hyphen
  "BA", # break-after (dashes)
  "B2", # break before and after (em dash)
  "ID", # ideographic
  "PR", # prefix numeric (¥$)
  "PO", # postfix numeric (%°)
  "NU" # number (digits)
)
class_code <- setNames(seq_along(classes), classes)

parse_entry <- function(line) {
  line <- sub("#.*$", "", line)
  line <- trimws(line)
  if (!nzchar(line)) {
    return(NULL)
  }
  parts <- strsplit(line, ";", fixed = TRUE)[[1]]
  cls <- trimws(parts[2])
  if (!cls %in% classes) {
    return(NULL)
  }
  range <- trimws(parts[1])
  if (grepl("\\.\\.", range)) {
    bounds <- strtoi(strsplit(range, "\\.\\.")[[1]], base = 16)
  } else {
    bounds <- rep(strtoi(range, base = 16), 2)
  }
  c(bounds, unname(class_code[cls]))
}

entries <- do.call(rbind, lapply(lines, parse_entry))

# @missing lines give default classes for unassigned ranges (mainly
# unassigned CJK ideograph planes defaulting to ID)
missing_lines <- grep("^# @missing:", lines, value = TRUE)
for (line in missing_lines) {
  parts <- strsplit(sub("^# @missing:\\s*", "", line), ";\\s*")[[1]]
  cls <- trimws(parts[2])
  if (!cls %in% classes) {
    next
  }
  bounds <- strtoi(strsplit(trimws(parts[1]), "\\.\\.")[[1]], base = 16)
  entries <- rbind(entries, c(bounds, unname(class_code[cls])))
}

entries <- entries[order(entries[, 1]), , drop = FALSE]
# Merge adjacent ranges of the same class
merged <- entries[1, , drop = FALSE]
for (i in seq(2, nrow(entries))) {
  if (
    entries[i, 3] == merged[nrow(merged), 3] &&
      entries[i, 1] <= merged[nrow(merged), 2] + 1
  ) {
    merged[nrow(merged), 2] <- entries[i, 2]
  } else {
    merged <- rbind(merged, entries[i, , drop = FALSE])
  }
}

out <- c(
  paste0(
    "// Generated from the UAX#14 data file LineBreak.txt (Unicode ",
    version,
    ") by data-raw/gen-linebreak-tables.R. Do not edit by hand"
  ),
  "//",
  "// Line-break classes used by the kinsoku shori and spacing logic. Classes",
  "// not in this list (letters, spaces, etc.) are reported as LB_OTHER",
  "enum LBClass : uint8_t {",
  "  LB_OTHER = 0,",
  paste0(
    "  LB_",
    classes,
    c(rep(",", length(classes) - 1), "")
  ),
  "};",
  "",
  "// Sorted, non-overlapping codepoint ranges with their line-break class",
  "struct LBRange {",
  "  uint32_t first;",
  "  uint32_t last;",
  "  uint8_t cls;",
  "};",
  "static const LBRange lb_class_ranges[] = {",
  sprintf(
    "  {0x%04X, 0x%04X, LB_%s},",
    merged[, 1],
    merged[, 2],
    names(class_code)[merged[, 3]]
  ),
  "};",
  ""
)
writeLines(out, "src/line_break_classes.inc")
message(
  "Wrote ",
  nrow(merged),
  " ranges to src/line_break_classes.inc (Unicode ",
  version,
  ")"
)
unlink(tmp)
