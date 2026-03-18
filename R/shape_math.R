#' Shape LaTeX math expression
#'
#' Parses a LaTeX math expression and returns positioned glyphs for rendering.
#' The function uses HarfBuzz's OpenType MATH table support to properly position
#' mathematical symbols, scripts, fractions, and other structures according to
#' the font's mathematical typesetting rules.
#'
#' @param math A character vector of LaTeX math expressions. Use raw strings
#' for cleaner syntax, e.g., `r"(\alpha)"` for α, `r"(\frac{a}{b})"` for
#' fractions. Alternatively escape backslashes: `"\\alpha"`.
#' @param family Font family name. For best results, use a font with an OpenType
#' MATH table such as "STIX Two Math", "Latin Modern Math", "Cambria Math", or
#' "Libertinus Math". Fonts without a MATH table will use fallback positioning
#' constants based on TeX defaults, which produces reasonable but less precise
#' results. Default uses the system's default math font.
#' @param inline Logical. If `TRUE` (default), uses inline style. If `FALSE`,
#' uses display style (larger operators with limits above/below).
#' @param color Base color for the equation (default "black"). Individual
#' elements can override this using `\textcolor{color}{content}`.
#' @inheritParams shape_text
#'
#' @return
#' A list with four element: `shape` contains the position of each glyph,
#' relative to the origin in the enclosing textbox. `metrics` contain metrics
#' about the full math expression. `lines` contain line segments (fraction bars,
#' overlines, cancel lines, etc.). `boxes` contain position and dimensions of boxes.
#'
#' `shape` is a data.frame with the following columns:
#' \describe{
#'   \item{glyph}{Row number within the string}
#'   \item{index}{Glyph index in the font}
#'   \item{metric_id}{Index of the metrics row (same as string_id)}
#'   \item{string_id}{Index of the input string this glyph belongs to}
#'   \item{x_offset}{X position relative to the origin}
#'   \item{y_offset}{Y position relative to the baseline (positive is up)}
#'   \item{font_path}{Path to the font file}
#'   \item{font_index}{Index of the font face in the font file}
#'   \item{font_size}{Font size used for this glyph (may differ for scripts)}
#'   \item{advance}{Horizontal advance width of the glyph}
#'   \item{color}{Color for this glyph}
#' }
#'
#' `metrics` is a data.frame with the following columns:
#' \describe{
#'   \item{width}{Total width of the expression}
#'   \item{height}{Height above the baseline}
#'   \item{depth}{Depth below the baseline}
#' }
#'
#' `lines` is a data.frame with the following columns:
#' \describe{
#'   \item{x1}{X position of start point}
#'   \item{y1}{Y position of start point}
#'   \item{x2}{X position of end point}
#'   \item{y2}{Y position of end point}
#'   \item{thickness}{Line thickness}
#'   \item{string_id}{Index of the input string this line belongs to}
#'   \item{color}{Color for this line}
#' }
#'
#' `boxes` is a data.frame with the following columns:
#' \describe{
#'   \item{x}{X position of the left edge}
#'   \item{y}{Y position of the bottom edge}
#'   \item{width}{Width of the box}
#'   \item{height}{Height of the box}
#'   \item{string_id}{Index of the input string this box belongs to}
#'   \item{color}{Fill color for the box}
#' }
#'
#'
#' @details
#' ## Font Requirements
#'
#' Mathematical typesetting works best with fonts that include an OpenType MATH
#' table, which provides precise positioning parameters for scripts, fractions,
#' radicals, and other structures. When using a font without a MATH table (such
#' as Arial or Times New Roman), the function falls back to default positioning
#' constants based on TeX conventions. The results will be reasonable but may
#' not match the typographic quality of a dedicated math font.
#'
#' ## Supported LaTeX Commands
#' LaTeX math expressions is a huge specification and also includes numerous
#' extensions with additional functionality. Supporting the full set would be
#' unfeasible but textshaping tries hard to cover all but the most esoteric
#' needs. The list of supported functionality is:
#'
#' \describe{
#'   \item{Greek letters}{`\alpha`, `\beta`, `\gamma`, `\delta`, `\epsilon`,
#'     `\varepsilon`, `\zeta`, `\eta`, `\theta`, `\vartheta`, `\iota`, `\kappa`,
#'     `\lambda`, `\mu`, `\nu`, `\xi`, `\pi`, `\varpi`, `\rho`, `\varrho`,
#'     `\sigma`, `\varsigma`, `\tau`, `\upsilon`, `\phi`, `\varphi`, `\chi`,
#'     `\psi`, `\omega`, `\Gamma`, `\Delta`, `\Theta`, `\Lambda`, `\Xi`, `\Pi`,
#'     `\Sigma`, `\Upsilon`, `\Phi`, `\Psi`, `\Omega`}
#'   \item{Binary operators}{`+`, `-`, `=`, `\pm`, `\mp`, `\times`, `\div`,
#'     `\cdot`, `\ast`, `\star`, `\circ`, `\bullet`, `\cap`, `\cup`, `\vee`,
#'     `\wedge`, `\setminus`, `\oplus`, `\ominus`, `\otimes`, `\oslash`, `\odot`,
#'     `\ltimes`, `\rtimes`, `\boxplus`, `\boxminus`, `\boxtimes`}
#'   \item{Relations}{`\leq`, `\le`, `\geq`, `\ge`, `\neq`, `\ne`, `\equiv`,
#'     `\approx`, `\sim`, `\simeq`, `\cong`, `\propto`, `\ll`, `\gg`,
#'     `\subset`, `\supset`, `\subseteq`, `\supseteq`, `\in`, `\ni`, `\notin`,
#'     `\perp`, `\parallel`, `\mid`, `\prec`, `\succ`, `\preceq`, `\succeq`,
#'     `\leqslant`, `\geqslant`, `\lesssim`, `\gtrsim`, `\vDash`, `\Vdash`,
#'     `\triangleq`}
#'   \item{Arrows}{`\to`, `\rightarrow`, `\leftarrow`, `\Rightarrow`, `\Leftarrow`,
#'     `\leftrightarrow`, `\Leftrightarrow`, `\mapsto`, `\hookleftarrow`,
#'     `\hookrightarrow`, `\uparrow`, `\downarrow`, `\Uparrow`, `\Downarrow`,
#'     `\updownarrow`, `\nearrow`, `\searrow`, `\swarrow`, `\nwarrow`,
#'     `\nleftarrow`, `\nrightarrow`, `\nLeftarrow`, `\nRightarrow`,
#'     `\nleftrightarrow`, `\nLeftrightarrow`, `\leftarrowtail`, `\rightarrowtail`,
#'     `\twoheadleftarrow`, `\twoheadrightarrow`, `\curvearrowleft`,
#'     `\curvearrowright`}
#'   \item{Scripts}{`x^2`, `x_i`, `x_i^2`}
#'   \item{Fractions}{`\frac{num}{denom}`, `\dfrac{}{}` (display style),
#'     `\tfrac{}{}` (text style), `\cfrac{}{}` (continued fraction)}
#'   \item{Roots}{`\sqrt{x}`, `\sqrt[n]{x}`}
#'   \item{Big operators}{`\sum`, `\prod`, `\coprod`, `\int`, `\iint`, `\iiint`,
#'     `\oint`, `\bigcup`, `\bigcap`, `\bigvee`, `\bigwedge`, `\bigoplus`,
#'     `\bigotimes`, `\bigodot`, `\biguplus`, `\bigsqcup`
#'     (with `_{lower}^{upper}` limits). Use `\limits` to force limits above/below
#'     or `\nolimits` to force side scripts}
#'   \item{Delimiters}{`\left(`, `\right)`, `\left[`, `\right]`, `\left\{`,
#'     `\right\}`, `\langle`, `\rangle`, `\lfloor`, `\rfloor`, `\lceil`,
#'     `\rceil`, `\vert`, `\Vert`, `\ulcorner`, `\urcorner`, `\llcorner`,
#'     `\lrcorner`}
#'   \item{Manual delimiter sizing}{`\big`, `\Big`, `\bigg`, `\Bigg` and variants
#'     `\bigl`, `\bigr`, `\bigm`, `\Bigl`, `\Bigr`, `\Bigm`, etc. for manual
#'     control of delimiter size}
#'   \item{Accents}{`\hat{x}`, `\check{x}`, `\tilde{x}`, `\acute{x}`, `\grave{x}`,
#'     `\dot{x}`, `\ddot{x}`, `\dddot{x}`, `\ddddot{x}`, `\breve{x}`, `\bar{x}`,
#'     `\vec{x}`, `\mathring{x}`}
#'   \item{Wide accents}{`\widehat{abc}`, `\widetilde{abc}`, `\overline{abc}`,
#'     `\underline{abc}`, `\overbrace{abc}`, `\underbrace{abc}`,
#'     `\overrightarrow{AB}`, `\overleftarrow{AB}`, `\overleftrightarrow{AB}`,
#'     `\underrightarrow{}`, `\underleftarrow{}`, `\underleftrightarrow{}`.
#'     Overbrace/underbrace support annotations: `\overbrace{x+y}^{n}`,
#'     `\underbrace{x+y}_{n}`}
#'   \item{Functions}{`\sin`, `\cos`, `\tan`, `\cot`, `\sec`, `\csc`,
#'     `\arcsin`, `\arccos`, `\arctan`, `\arccot`, `\sinh`, `\cosh`, `\tanh`,
#'     `\coth`, `\log`, `\ln`, `\lg`, `\exp`, `\lim`, `\liminf`, `\limsup`,
#'     `\max`, `\min`, `\sup`, `\inf`, `\det`, `\dim`, `\ker`, `\hom`,
#'     `\arg`, `\deg`, `\gcd`, `\Pr`, `\mod`}
#'   \item{Text}{`\text{...}`}
#'   \item{Font styles}{`\mathbf{x}`, `\mathit{x}`, `\mathrm{x}`, `\mathsf{x}`,
#'     `\mathtt{x}`, `\mathcal{X}`, `\mathscr{X}`, `\mathfrak{X}`, `\mathbb{R}`,
#'     `\boldsymbol{x}`, `\bm{x}`}
#'   \item{Math styles}{`\displaystyle`, `\textstyle`, `\scriptstyle`,
#'     `\scriptscriptstyle` to control operator size and limit placement}
#'   \item{Matrices}{`\begin{matrix}`, `\begin{pmatrix}`, `\begin{bmatrix}`,
#'     `\begin{vmatrix}`, `\begin{Vmatrix}`, `\begin{Bmatrix}`, `\begin{smallmatrix}`
#'     (with `&` for column separators and `\\` for row separators)}
#'   \item{Alignment environments}{`\begin{aligned}`, `\begin{gathered}`,
#'     `\begin{split}`, `\begin{array}{lcr}` for multi-line equations with
#'     alignment points}
#'   \item{Cases}{`\begin{cases}`, `\begin{rcases}`, `\begin{dcases}`,
#'     `\begin{drcases}`}
#'   \item{Stacked expressions}{`\binom{n}{k}`, `\tbinom{n}{k}`, `\dbinom{n}{k}`,
#'     `\overset{a}{b}`, `\underset{a}{b}`, `\stackrel{a}{b}`, `\atop`}
#'   \item{Operator names}{`\operatorname{name}`, `\operatorname*{name}` (with limits)}
#'   \item{Phantoms}{`\phantom{x}` (invisible with full dimensions),
#'     `\hphantom{x}` (width only), `\vphantom{x}` (height/depth only)}
#'   \item{Colors}{`\textcolor{color}{text}`, `\color{color}{text}`,
#'     `\colorbox{color}{text}`}
#'   \item{Cancellation}{`\cancel{x}`, `\bcancel{x}`, `\xcancel{x}`, `\cancelto{v}{x}`}
#'   \item{Boxing}{`\boxed{expression}`}
#'   \item{Substack}{`\substack{a \\ b}` for multi-line subscripts}
#'   \item{Negation}{`\not` followed by a symbol (e.g., `\not=`)}
#'   \item{Extensible arrows}{`\xrightarrow{above}`, `\xleftarrow[below]{above}`,
#'     `\xRightarrow{}`, `\xLeftarrow{}`, `\xleftrightarrow{}`, `\xLeftrightarrow{}`,
#'     `\xhookleftarrow{}`, `\xhookrightarrow{}`, `\xmapsto{}`, `\xlongequal{}`}
#'   \item{Spacing}{`\!` (negative thin), `\,` (thin), `\:` (medium), `\;` (thick),
#'     `\ ` (space), `\quad`, `\qquad`}
#'   \item{Smash}{`\smash{x}` (zero height and depth), `\smash[t]{x}` (zero height),
#'     `\smash[b]{x}` (zero depth) for fine vertical spacing control}
#'   \item{Modular arithmetic}{`\pmod{n}` for "(mod n)", `\bmod` as binary operator,
#'     `\pod{n}` for "(n)"}
#'   \item{Rule}{`\rule{width}{height}` for custom-sized horizontal rules}
#'   \item{Miscellaneous symbols}{`\infty`, `\partial`, `\nabla`, `\forall`,
#'     `\exists`, `\nexists`, `\emptyset`, `\varnothing`, `\aleph`, `\hbar`,
#'     `\ell`, `\wp`, `\Re`, `\Im`, `\angle`, `\triangle`, `\prime`, `\ldots`,
#'     `\cdots`, `\vdots`, `\ddots`, `\neg`, `\lnot`, `\bot`, `\top`,
#'     `\dagger`, `\ddagger`, `\diamond`, `\square`, `\checkmark`, `\imath`,
#'     `\jmath`, `\complement`, `\eth`, `\hslash`, `\mho`, `\dotsb`, `\dotsc`,
#'     `\dotsi`, `\dotsm`, `\dotso`}
#' }
#'
#' @export
#' @importFrom systemfonts match_fonts
#'
#' @examples
#' # Simple superscript
#' shape_math("x^2")
#'
#' # Greek letters
#' shape_math(r"(\alpha + \beta)")
#'
#' # Fraction
#' shape_math(r"(\frac{a}{b})")
#'
#' # Display mode sum (inline = FALSE)
#' shape_math(r"(\sum_{i=1}^n i)", inline = FALSE)
#'
#' # Quadratic formula
#' shape_math(r"(x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a})")
#'
#' # Matrix
#' shape_math(r"(\begin{pmatrix} a & b \\ c & d \end{pmatrix})")
#'
#' # Binomial coefficient
#' shape_math(r"(\binom{n}{k})")
#'
#' # Cases environment
#' shape_math(r"(f(x) = \begin{cases} 1 & x > 0 \\ 0 & x \leq 0 \end{cases})")
#'
#' # Colored text
#' shape_math(r"(x = \textcolor{red}{-b})")
#'
#' # Cancellation
#' shape_math(r"(\frac{\cancel{2}x}{\cancel{2}} = x)")
#'
#' # Extensible arrow with text
#' shape_math(r"(A \xrightarrow{f} B)")
shape_math <- function(
  math,
  family = "math",
  size = 12,
  res = 72,
  inline = TRUE,
  color = "black",
  path = NULL,
  index = 0
) {
  n_math <- length(math)
  math <- as.character(math)

  if (is.null(path)) {
    family <- rep_len(family, n_math)
    loc <- match_fonts(family)
    path <- loc$path
    index <- loc$index
  } else {
    path <- rep_len(path, n_math)
    index <- rep_len(index, n_math)
  }
  size <- rep_len(size, n_math)
  res <- rep_len(res, n_math)
  inline <- rep_len(inline, n_math)
  color <- rep_len(color, n_math)

  if (!all(file.exists(path))) {
    stop("path must point to a valid file", call. = FALSE)
  }

  # Call C++ implementation
  shape_math_c(
    as.character(math),
    path,
    as.integer(index),
    as.numeric(size),
    as.numeric(res),
    as.logical(!inline), # C++ uses display mode flag
    as.character(color)
  )
}

#' Preview shaped math expression
#'
#' This function allows you to preview the layout that [shape_math()]
#' calculates. It renders the positioned glyphs and lines (fraction bars,
#' radical overlines, cancel lines, etc.) using grid graphics, and shows the
#' position of the baseline relative to the equation. This is primarily
#' a debugging and visualization tool.
#'
#' @param shaped The output of a call to [shape_math()]
#' @param id The index of the expression to show if `shaped` contains multiples
#'
#' @return This function is called for its side effects
#'
#' @export
#'
#' @examples
#' \dontrun{
#' shaped <- shape_math(r"(\frac{a}{b})", family = "STIX Two Math")
#' plot_math(shaped)
#'
#' shaped <- shape_math("x^2 + y^2 = z^2", family = "STIX Two Math")
#' plot_math(shaped)
#' }
plot_math <- function(shaped, id = 1) {
  if (
    !requireNamespace("grDevices", quietly = TRUE) ||
      utils::packageVersion("grDevices") < package_version("4.3.0")
  ) {
    stop("This function requires grDevices 4.3.0 or above")
  }
  if (
    !requireNamespace("grid", quietly = TRUE) ||
      utils::packageVersion("grid") < package_version("4.3.0")
  ) {
    stop("This function requires grid 4.3.0 or above")
  }

  has_glyph_support <- grDevices::dev.capabilities()$glyphs
  if (is.na(has_glyph_support)) {
    warning("The device does not report whether it supports rendering glyphs")
  } else if (!isTRUE(has_glyph_support)) {
    stop("The current device doesn't support rendering glyphs")
  }

  glyphFont <- utils::getFromNamespace("glyphFont", "grDevices")
  glyphFontList <- utils::getFromNamespace("glyphFontList", "grDevices")
  glyphInfo <- utils::getFromNamespace("glyphInfo", "grDevices")
  glyphAnchor <- utils::getFromNamespace("glyphAnchor", "grDevices")
  grid.glyph <- utils::getFromNamespace("grid.glyph", "grid")

  if (
    !is.numeric(id) ||
      length(id) != 1 ||
      id <= 0 ||
      id %% 1 != 0 ||
      id > nrow(shaped$metrics)
  ) {
    stop("`id` must be an integer pointing to an expression in `shaped`")
  }

  # Get shape, lines, and boxes for this expression
  shape_df <- shaped$shape[shaped$shape$string_id == id, ]
  lines_df <- shaped$lines[shaped$lines$string_id == id, ]
  boxes_df <- shaped$boxes[shaped$boxes$string_id == id, ]
  box <- shaped$metrics[id, ]

  # Set up font(s) - get from first glyph
  fonts <- glyphFont(
    shape_df$font_path[1],
    shape_df$font_index[1],
    "",
    0,
    ""
  )
  fonts <- glyphFontList(fonts)

  # Create glyphInfo object
  # y-coordinates from shape_math are baseline-relative (y=0 at baseline, positive up)
  # glyphInfo expects coordinates relative to the bottom of the bounding box
  # The baseline is at box$depth from the bottom, so we shift y by box$depth
  glyph_info <- glyphInfo(
    id = shape_df$index,
    x = shape_df$x_offset,
    y = shape_df$y_offset + box$depth,
    font = rep(1L, nrow(shape_df)),
    size = shape_df$font_size,
    fontList = fonts,
    width = box$width,
    height = box$height + box$depth,
    hAnchor = glyphAnchor(0, "left"),
    vAnchor = glyphAnchor(0, "bottom"),
    col = shape_df$color
  )

  # Calculate viewport dimensions with some padding
  padding <- 20
  vp_width <- box$width + 2 * padding
  vp_height <- box$height + box$depth + 2 * padding

  grid::grid.newpage()

  vp <- grid::viewport(
    width = grid::unit(vp_width, "bigpts"),
    height = grid::unit(vp_height, "bigpts")
  )

  grid::pushViewport(vp)

  # Background
  grid::grid.rect(gp = grid::gpar(fill = "white", col = NA))

  # Bounding box (dashed)
  grid::grid.rect(
    x = grid::unit(padding, "bigpts"),
    y = grid::unit(padding, "bigpts"),
    width = grid::unit(box$width, "bigpts"),
    height = grid::unit(box$height + box$depth, "bigpts"),
    hjust = 0,
    vjust = 0,
    gp = grid::gpar(fill = "grey95", col = "grey70", lty = 2)
  )

  # Baseline
  grid::grid.segments(
    x0 = grid::unit(padding - 5, "bigpts"),
    y0 = grid::unit(padding + box$depth, "bigpts"),
    x1 = grid::unit(padding + box$width + 5, "bigpts"),
    y1 = grid::unit(padding + box$depth, "bigpts"),
    gp = grid::gpar(col = "red", lwd = 0.5)
  )

  # Draw boxes (colorbox backgrounds) first, behind everything
  if (nrow(boxes_df) > 0) {
    for (i in seq_len(nrow(boxes_df))) {
      b <- boxes_df[i, ]
      grid::grid.rect(
        x = grid::unit(padding + b$x, "bigpts"),
        y = grid::unit(padding + box$depth + b$y, "bigpts"),
        width = grid::unit(b$width, "bigpts"),
        height = grid::unit(b$height, "bigpts"),
        hjust = 0,
        vjust = 0,
        gp = grid::gpar(fill = b$color, col = NA)
      )
    }
  }

  # Draw lines (fraction bars, radical overlines, cancel lines, etc.)
  if (nrow(lines_df) > 0) {
    for (i in seq_len(nrow(lines_df))) {
      ln <- lines_df[i, ]
      line_color <- if (!is.null(ln$color) && nzchar(ln$color)) {
        ln$color
      } else {
        "black"
      }
      grid::grid.segments(
        x0 = grid::unit(padding + ln$x1, "bigpts"),
        y0 = grid::unit(padding + box$depth + ln$y1, "bigpts"),
        x1 = grid::unit(padding + ln$x2, "bigpts"),
        y1 = grid::unit(padding + box$depth + ln$y2, "bigpts"),
        gp = grid::gpar(
          col = line_color,
          lwd = ln$thickness * 96 / 72,
          lineend = "butt"
        )
      )
    }
  }

  # Draw glyphs
  grid.glyph(
    glyph_info,
    x = grid::unit(padding, "bigpts"),
    y = grid::unit(padding, "bigpts"),
    hjust = 0,
    vjust = 0
  )

  grid::popViewport()

  invisible(shaped)
}

#' Create a grid grob for a math expression
#'
#' This function creates a grid grob that renders a math expression.
#' The grob can be used with standard grid functions like [grid::grid.draw()],
#' combined with other grobs using [grid::grobTree()], or used in ggplot2
#' annotations. Like the standard grid grobs, `math_grob()` creates a grob
#' object to pass around while `grid.math()` both creates the grob and render it
#' in one pass.
#'
#' @param math A single math expression string. Use raw strings for
#' cleaner syntax, e.g., `r"(\frac{a}{b})"`.
#' @param x,y The position of the grob. Can be a unit object or a numeric
#' value (interpreted as npc units).
#' @param hjust Horizontal justification. Either a numeric value (0 = left,
#' 0.5 = center, 1 = right) or a character string (`"left"`, `"center"`,
#' `"right"`).
#' @param vjust Vertical justification. Either a numeric value (0 = bottom,
#' 0.5 = center, 1 = top) or a character string (`"bottom"`, `"center"`,
#' `"top"`, `"baseline"`). The default `"baseline"` places the math baseline
#' at the y position, which is most useful for aligning with text.
#' @param default.units A string indicating the default units to use if
#' `x` or `y` are only given as numeric values.
#' @param name A character identifier for the grob.
#' @param gp A [grid::gpar()] object containing graphical parameters, or `NULL`
#' for defaults. Supported parameters are `fontsize`, `col`, and `fontfamily`.
#' @param vp A [grid::viewport()] object (or `NULL`).
#' @inheritParams shape_math
#'
#' @return A grob object of class `"math_grob"`.
#'
#' @export
#'
#' @examplesIf requireNamespace("grid", quietly = TRUE)
#' # Create a math grob (baseline at y=0.5)
#' g <- math_grob(r"(\frac{a}{b})", x = 0.5, y = 0.5)
#'
#' # Draw it (requires a device that supports glyph rendering)
#' try(grid::grid.draw(g))
#'
#' # Center the expression in the viewport
#' g <- math_grob(r"(E = mc^2)", x = 0.5, y = 0.5,
#'                hjust = "center", vjust = "center")
#' try(grid::grid.draw(g))
#'
#' # Combine with other grobs
#' tree <- grid::grobTree(
#'   grid::rectGrob(gp = grid::gpar(fill = "lightblue")),
#'   math_grob(r"(E = mc^2)", x = 0.5, y = 0.5,
#'             hjust = "center", vjust = "center")
#' )
#' try(grid::grid.draw(tree))
#'
math_grob <- function(
  math,
  x = 0.5,
  y = 0.5,
  hjust = "left",
  vjust = "baseline",
  default.units = "npc",
  name = NULL,
  gp = NULL,
  vp = NULL,
  family = "math",
  size = 12,
  res = 72,
  inline = TRUE,
  path = NULL,
  index = 0,
  color = "black"
) {
  if (!requireNamespace("grid", quietly = TRUE)) {
    stop("The grid package is required for math_grob()")
  }

  if (is.null(gp)) {
    gp <- grid::gpar()
  }

  if (length(math) != 1) {
    stop("`math` must be a single string", call. = FALSE)
  }

  # Convert x and y to units if necessary
  if (!grid::is.unit(x)) {
    x <- grid::unit(x, default.units)
  }
  if (!grid::is.unit(y)) {
    y <- grid::unit(y, default.units)
  }

  # Extract gpar overrides
  if (!is.null(gp$fontsize)) {
    size <- gp$fontsize
  }
  if (!is.null(gp$col)) {
    color <- gp$col
  }
  if (!is.null(gp$fontfamily)) {
    family <- gp$fontfamily
  }

  grid::grob(
    math = math,
    x = x,
    y = y,
    hjust = hjust,
    vjust = vjust,
    family = family,
    size = size,
    res = res,
    inline = inline,
    path = path,
    index = index,
    color = color,
    name = name,
    gp = gp,
    vp = vp,
    cl = "math_grob"
  )
}

#' @rdname math_grob
#' @export
#' @param draw Should the created grob be rendered
grid.math <- function(
  math,
  x = 0.5,
  y = 0.5,
  hjust = "left",
  vjust = "baseline",
  default.units = "npc",
  name = NULL,
  gp = NULL,
  vp = NULL,
  family = "math",
  size = 12,
  res = 72,
  inline = TRUE,
  path = NULL,
  index = 0,
  color = "black",
  draw = TRUE
) {
  g <- math_grob(
    math = math,
    x = x,
    y = y,
    hjust = hjust,
    vjust = vjust,
    default.units = default.units,
    name = name,
    gp = gp,
    vp = vp,
    family = family,
    size = size,
    res = res,
    inline = inline,
    path = path,
    index = index,
    color = color
  )
  if (draw) {
    grid::grid.draw(g)
    invisible(g)
  } else {
    g
  }
}

#' @exportS3Method grid::makeContent
makeContent.math_grob <- function(x) {
  # Check for glyph support
  has_glyph_support <- grDevices::dev.capabilities()$glyphs
  if (is.na(has_glyph_support)) {
    warning("The device does not report whether it supports rendering glyphs")
  } else if (!isTRUE(has_glyph_support)) {
    stop("The current device doesn't support rendering glyphs", call. = FALSE)
  }

  # Get internal functions
  glyphFont <- utils::getFromNamespace("glyphFont", "grDevices")
  glyphFontList <- utils::getFromNamespace("glyphFontList", "grDevices")
  glyphInfo <- utils::getFromNamespace("glyphInfo", "grDevices")
  glyphAnchor <- utils::getFromNamespace("glyphAnchor", "grDevices")
  glyphGrob <- utils::getFromNamespace("glyphGrob", "grid")

  # Shape the math expression

  shaped <- shape_math(
    math = x$math,
    family = x$family,
    size = x$size,
    res = x$res,
    inline = x$inline,
    path = x$path,
    index = x$index,
    color = x$color
  )

  if (nrow(shaped$shape) == 0) {
    return(grid::nullGrob())
  }

  shape_df <- shaped$shape
  lines_df <- shaped$lines
  boxes_df <- shaped$boxes
  metrics <- shaped$metrics[1, ]

  # Calculate offset based on justification
  width <- metrics$width
  height <- metrics$height
  depth <- metrics$depth
  total_height <- height + depth

  # Convert character hjust to numeric

  hjust <- x$hjust
  if (is.character(hjust)) {
    hjust <- switch(
      hjust,
      left = 0,
      center = 0.5,
      centre = 0.5,
      right = 1,
      stop("Invalid hjust value: ", hjust, call. = FALSE)
    )
  }

  # Convert character vjust to numeric (or handle baseline specially)
  vjust <- x$vjust
  use_baseline <- identical(vjust, "baseline")
  if (is.character(vjust) && !use_baseline) {
    vjust <- switch(
      vjust,
      bottom = 0,
      center = 0.5,
      centre = 0.5,
      top = 1,
      stop("Invalid vjust value: ", vjust, call. = FALSE)
    )
  }

  x_offset <- -hjust * width
  if (use_baseline) {
    # Baseline at y position
    y_offset <- 0
  } else {
    # Box-based justification
    y_offset <- -vjust * total_height + depth
  }

  # Set up fonts
  fonts <- glyphFont(
    shape_df$font_path[1],
    shape_df$font_index[1],
    "",
    0,
    ""
  )
  fonts <- glyphFontList(fonts)

  # Create glyphInfo object
  glyph_info <- glyphInfo(
    id = shape_df$index,
    x = shape_df$x_offset + x_offset,
    y = shape_df$y_offset + y_offset,
    font = rep(1L, nrow(shape_df)),
    size = shape_df$font_size,
    fontList = fonts,
    width = width,
    height = total_height,
    hAnchor = glyphAnchor(x_offset, "left"),
    vAnchor = glyphAnchor(y_offset - depth, "bottom"),
    col = shape_df$color
  )

  box_grob <- if (nrow(boxes_df) == 0) grid::nullGrob() else grid::rectGrob(
    x = grid::unit(boxes_df$x + x_offset, "bigpts"),
    y = grid::unit(boxes_df$y + y_offset, "bigpts"),
    width = grid::unit(boxes_df$width, "bigpts"),
    height = grid::unit(boxes_df$height, "bigpts"),
    hjust = 0,
    vjust = 0,
    gp = grid::gpar(fill = boxes_df$color, col = NA)
  )


  line_grob <- if (nrow(lines_df) == 0) grid::nullGrob()  else grid::segmentsGrob(
    x0 = grid::unit(lines_df$x1 + x_offset, "bigpts"),
    y0 = grid::unit(lines_df$y1 + y_offset, "bigpts"),
    x1 = grid::unit(lines_df$x2 + x_offset, "bigpts"),
    y1 = grid::unit(lines_df$y2 + y_offset, "bigpts"),
    gp = grid::gpar(
      col = lines_df$color %||% "black",
      lwd = lines_df$thickness * 96 / 72,
      lineend = "butt"
    )
  )

  glyph_grob <- glyphGrob(
    glyph_info,
    x = 0,
    y = 0,
    hjust = 0,
    vjust = 0
  )

  # Create a gTree with all children positioned at the specified x, y
  grid::setChildren(
    grid::gTree(
      x = x$x,
      y = x$y,
      vp = grid::viewport(x = x$x, y = x$y, just = c(0, 0))
    ),
    grid::gList(box_grob, line_grob, glyph_grob)
  )
}
