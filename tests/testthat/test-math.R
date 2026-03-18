# Comprehensive LaTeX math expression tests for visual validation
# Run with: plot_math(shape_math(expr)) for each expression

test_expressions <- c(
  # === BASIC ELEMENTS ===
  # Variables (should be italic)
  "x + y = z",
  "a^2 + b^2 = c^2",
  "f(x) = mx + b",

  # Numbers (should be upright)
  "x = 42",
  "y = 3.14159",

  # Greek letters
  r"(\alpha + \beta = \gamma)",
  r"(\Gamma \Delta \Theta \Lambda \Xi \Pi \Sigma \Phi \Psi \Omega)",
  r"(\epsilon \varepsilon \theta \vartheta \pi \varpi \rho \varrho \sigma \varsigma \phi \varphi)",

  # === OPERATORS AND RELATIONS ===
  # Binary operators
  r"(a \pm b \mp c \times d \div e)",
  r"(A \cap B \cup C \setminus D)",
  r"(a \oplus b \otimes c \odot d)",
  r"(a \ltimes b \rtimes c)",
  r"(a \boxplus b \boxminus c \boxtimes d)",

  # Relations
  r"(a \leq b \geq c \neq d)",
  r"(a \equiv b \approx c \sim d \cong e)",
  r"(A \subset B \supset C \subseteq D \supseteq E)",
  r"(a \in A \ni b \notin C)",
  r"(a \leqslant b \geqslant c \lesssim d \gtrsim e)",
  r"(a \prec b \succ c \preceq d \succeq e)",
  r"(a \vDash b \Vdash c \triangleq d)",

  # Arrows

  r"(a \to b \leftarrow c \Rightarrow d \Leftarrow e)",
  r"(a \leftrightarrow b \Leftrightarrow c)",
  r"(\uparrow \downarrow \Uparrow \Downarrow \updownarrow)",
  r"(\nearrow \searrow \swarrow \nwarrow)",
  r"(\nleftarrow \nrightarrow \nLeftarrow \nRightarrow)",
  r"(\leftarrowtail \rightarrowtail \twoheadleftarrow \twoheadrightarrow)",
  r"(\hookleftarrow \hookrightarrow \mapsto)",

  # === SCRIPTS ===
  "x^2",
  "x_i",
  "x_i^2",
  "x^{2n+1}",
  "x_{i,j}^{k}",
  r"(e^{i\pi} + 1 = 0)",

  # === FRACTIONS ===
  r"(\frac{a}{b})",
  r"(\frac{x+1}{x-1})",
  r"(\dfrac{a}{b} \text{ vs } \tfrac{a}{b})",
  r"(\frac{\frac{a}{b}}{\frac{c}{d}})",
  r"(\cfrac{1}{1+\cfrac{1}{1+\cfrac{1}{x}}})",

  # === ROOTS ===
  r"(\sqrt{x})",
  r"(\sqrt{x^2 + y^2})",
  r"(\sqrt[3]{x})",
  r"(\sqrt[n]{a^n})",

  # === BIG OPERATORS ===
  # Inline style
  r"(\sum_{i=1}^n x_i)",
  r"(\prod_{k=1}^n k)",
  r"(\int_0^\infty e^{-x} dx)",

  # With limits control
  r"(\sum\limits_{i=1}^n x_i)",
  r"(\sum\nolimits_{i=1}^n x_i)",

  # Various big operators
  r"(\bigcup_{i=1}^n A_i)",
  r"(\bigcap_{i=1}^n A_i)",
  r"(\bigoplus_{i=1}^n V_i)",
  r"(\iint_D f(x,y) \, dA)",
  r"(\iiint_V f \, dV)",
  r"(\oint_C F \cdot dr)",

  # === DELIMITERS ===
  # Auto-sizing
  r"(\left( \frac{a}{b} \right))",
  r"(\left[ \frac{x}{y} \right])",
  r"(\left\{ \frac{p}{q} \right\})",
  r"(\left| x \right|)",
  r"(\left\| v \right\|)",
  r"(\left\langle x, y \right\rangle)",
  r"(\left\lfloor x \right\rfloor)",
  r"(\left\lceil x \right\rceil)",

  # Manual sizing
  r"(\big( \Big( \bigg( \Bigg()",
  r"(\big[ \Big] \bigg\{ \Bigg\})",
  r"(\bigl( x \bigr))",

  # Corner brackets
  r"(\ulcorner P \urcorner)",

  # === ACCENTS ===
  r"(\hat{x} \check{y} \tilde{z})",
  r"(\acute{a} \grave{e} \breve{u})",
  r"(\dot{x} \ddot{x} \dddot{x} \ddddot{x})",
  r"(\bar{x} \vec{v} \mathring{a})",

  # Wide accents
  r"(\widehat{abc} \widetilde{xyz})",
  r"(\overline{x+y} \underline{a+b})",
  r"(\overbrace{a+b+c}^{n} \underbrace{x+y+z}_{m})",

  # Over/under arrows
  r"(\overrightarrow{AB})",
  r"(\overleftarrow{BA})",
  r"(\overleftrightarrow{AB})",
  r"(\underrightarrow{v})",

  # === FUNCTIONS ===
  r"(\sin x \cos y \tan z)",
  r"(\log_2 x \ln e \exp(x))",
  r"(\lim_{x \to 0} \frac{\sin x}{x})",
  r"(\max_{x \in S} f(x))",
  r"(\gcd(a, b) \det(A))",
  r"(\operatorname{Var}(X))",
  r"(\operatorname*{argmax}_x f(x))",

  # === MATH STYLES ===
  r"(\displaystyle \sum_{i=1}^n x_i)",
  r"(\textstyle \sum_{i=1}^n x_i)",
  r"(\scriptstyle x + y)",
  r"(\scriptscriptstyle a + b)",
  r"(x + \displaystyle\frac{a}{b} + y)",

  # === FONT STYLES ===
  r"(\mathrm{d}x)",
  r"(\mathbf{v} \cdot \mathbf{w})",
  r"(\mathit{italic})",
  r"(\mathsf{sans})",
  r"(\mathtt{mono})",
  r"(\mathcal{L} \mathscr{H})",
  r"(\mathfrak{g})",
  r"(\mathbb{R} \mathbb{C} \mathbb{Z} \mathbb{Q} \mathbb{N})",
  r"(\boldsymbol{\alpha} \bm{\beta})",

  # === TEXT ===
  r"(x \text{ if } x > 0)",
  r"(\text{where } n \in \mathbb{N})",

  # === MATRICES ===
  r"(\begin{matrix} a & b \\ c & d \end{matrix})",
  r"(\begin{pmatrix} a & b \\ c & d \end{pmatrix})",
  r"(\begin{bmatrix} 1 & 0 \\ 0 & 1 \end{bmatrix})",
  r"(\begin{vmatrix} a & b \\ c & d \end{vmatrix})",
  r"(\begin{Vmatrix} a & b \\ c & d \end{Vmatrix})",
  r"(\begin{Bmatrix} x \\ y \end{Bmatrix})",
  r"(\begin{smallmatrix} a & b \\ c & d \end{smallmatrix})",

  # === ALIGNMENT ENVIRONMENTS ===
  r"(\begin{aligned} x &= 1 \\ y &= 2 \end{aligned})",
  r"(\begin{gathered} a + b \\ c + d \end{gathered})",
  r"(\begin{array}{lcr} a & b & c \\ d & e & f \end{array})",

  # === CASES ===
  r"(f(x) = \begin{cases} 1 & x > 0 \\ 0 & x \leq 0 \end{cases})",
  r"(|x| = \begin{cases} x & x \geq 0 \\ -x & x < 0 \end{cases})",

  # === STACKED EXPRESSIONS ===
  r"(\binom{n}{k})",
  r"(\tbinom{n}{k} \dbinom{n}{k})",
  r"(\overset{?}{=})",
  r"(\underset{x \to 0}{\lim})",
  r"(\stackrel{f}{\longrightarrow})",
  r"(a \atop b)",

  # === EXTENSIBLE ARROWS ===
  r"(A \xrightarrow{f} B)",
  r"(A \xleftarrow[g]{f} B)",
  r"(A \xRightarrow{\phi} B)",
  r"(A \xleftrightarrow{iso} B)",
  r"(x \xmapsto{T} y)",
  r"(a \xlongequal{def} b)",

  # === SPACING ===
  r"(a\!b\,c\:d\;e\ f)",
  r"(x \quad y \qquad z)",

  # === COLORS ===
  r"(\textcolor{red}{x} + \textcolor{blue}{y})",
  r"(\colorbox{yellow}{highlighted})",

  # === CANCELLATION ===
  r"(\cancel{x})",
  r"(\bcancel{y})",
  r"(\xcancel{z})",
  r"(\cancelto{0}{x})",
  r"(\frac{\cancel{2}x}{\cancel{2}} = x)",

  # === PHANTOMS ===
  r"(a + \phantom{b} + c)",
  r"(\sqrt{\vphantom{b}a} \sqrt{b})",

  # === BOXING ===
  r"(\boxed{E = mc^2})",

  # === SMASH ===
  r"(\smash{\frac{a}{b}} + x)",
  r"(\sqrt{\smash[b]{\frac{a}{b}}})",

  # === MODULAR ARITHMETIC ===
  r"(a \equiv b \pmod{n})",
  r"(a \bmod b)",
  r"(17 \equiv 2 \pod{5})",

  # === SUBSTACK ===
  r"(\sum_{\substack{i=1 \\ j=1}}^n a_{ij})",

  # === NEGATION ===
  r"(a \not= b)",
  r"(A \not\subset B)",

  # === RULE ===
  r"(x \rule{2em}{0.4pt} y)",

  # === MISCELLANEOUS SYMBOLS ===
  r"(\infty \partial \nabla)",
  r"(\forall x \exists y)",
  r"(\emptyset \varnothing)",
  r"(\aleph_0 \hbar \ell)",
  r"(\Re(z) \Im(z))",
  r"(\angle \triangle \square)",
  r"(\neg P \land Q \lor R)",
  r"(\bot \top)",
  r"(\dagger \ddagger \diamond)",
  r"(\imath \jmath)",
  r"(\complement \eth \hslash \mho)",

  # === DOTS ===
  r"(a_1, a_2, \ldots, a_n)",
  r"(a_1 + a_2 + \cdots + a_n)",
  r"(\vdots \ddots)",
  r"(a \dotsb b \dotsc c)",

  # === CASE VARIANTS ===
  r"(x = \begin{rcases} a \\ b \end{rcases} y)",
  r"(f(x) = \begin{dcases} 1 & x > 0 \\ 0 & x \leq 0 \end{dcases})",
  r"(x = \begin{drcases} a \\ b \end{drcases} y)",

  # === SPLIT ENVIRONMENT ===
  r"(\begin{split} a &= b + c \\ &= d + e \end{split})",

  # === MORE EXTENSIBLE ARROWS ===
  r"(A \xLeftarrow{\phi} B)",
  r"(A \xLeftrightarrow{iso} B)",
  r"(A \xhookleftarrow{f} B)",
  r"(A \xhookrightarrow{g} B)",

  # === UNDER ARROWS ===
  r"(\underleftarrow{AB})",
  r"(\underleftrightarrow{AB})",

  # === MORE PHANTOMS ===
  r"(a \hphantom{bbb} c)",

  # === COLOR COMMAND ===
  r"(\color{red} x + y)",

  # === MORE SYMBOLS ===
  r"(\exists x \nexists y)",
  r"(\wp(S))",
  r"(f' = f^\prime)",
  r"(\lnot P)",
  r"(\checkmark)",

  # === MORE BIG OPERATORS ===
  r"(\coprod_{i=1}^n X_i)",
  r"(\bigvee_{i} P_i \bigwedge_{j} Q_j)",
  r"(\bigodot \biguplus \bigsqcup)",

  # === MORE FUNCTIONS ===
  r"(\cot x \sec x \csc x)",
  r"(\arcsin x \arccos x \arctan x)",
  r"(\sinh x \cosh x \tanh x)",
  r"(\liminf_{n} a_n \leq \limsup_{n} a_n)",
  r"(\min S \leq \max S)",
  r"(\sup S \geq \inf S)",
  r"(\dim V \ker T \hom(A,B))",
  r"(\arg z \deg f)",
  r"(\Pr(A) a \mod b)",

  # === MORE RELATIONS ===
  r"(a \ll b \gg c)",
  r"(a \perp b \parallel c \mid d)",
  r"(a \propto b \simeq c)",

  # === COMPLEX EXPRESSIONS ===
  # Quadratic formula
  r"(x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a})",

  # Euler's identity
  r"(e^{i\pi} + 1 = 0)",

  # Taylor series
  r"(e^x = \sum_{n=0}^{\infty} \frac{x^n}{n!})",

  # Gaussian integral
  r"(\int_{-\infty}^{\infty} e^{-x^2} dx = \sqrt{\pi})",

  # Matrix determinant
  r"(\det\begin{pmatrix} a & b \\ c & d \end{pmatrix} = ad - bc)",

  # Binomial theorem
  r"((x+y)^n = \sum_{k=0}^{n} \binom{n}{k} x^{n-k} y^k)",

  # Cauchy-Schwarz
  r"(\left| \sum_{i=1}^n a_i b_i \right|^2 \leq \left( \sum_{i=1}^n a_i^2 \right) \left( \sum_{i=1}^n b_i^2 \right))",

  # Fourier transform
  r"(\hat{f}(\xi) = \int_{-\infty}^{\infty} f(x) e^{-2\pi i x \xi} dx)",

  # Stokes' theorem
  r"(\oint_{\partial \Omega} \omega = \int_\Omega d\omega)",

  # Schrodinger equation
  r"(i\hbar \frac{\partial}{\partial t} \Psi = \hat{H} \Psi)"
)

# Test that all expressions can be shaped without error
test_that("All test expressions shape without error", {
  for (i in seq_along(test_expressions)) {
    expr <- test_expressions[i]
    result <- tryCatch(
      shape_math(expr),
      error = function(e) e
    )
    expect_false(
      inherits(result, "error"),
      label = sprintf("Expression %d: %s", i, expr)
    )
  }
})

# Test display mode for expressions with big operators
test_that("Display mode works for big operators", {
  display_expressions <- c(
    r"(\sum_{i=1}^n x_i)",
    r"(\prod_{k=1}^n k)",
    r"(\int_0^\infty e^{-x} dx)"
  )

  for (expr in display_expressions) {
    result <- tryCatch(
      shape_math(expr, inline = FALSE),
      error = function(e) e
    )
    expect_false(inherits(result, "error"))
  }
})

# Helper function for manual visual validation
# Usage: validate_math(1:10) to view expressions 1-10
# Usage: validate_math(1:12, nrow = 3, ncol = 4) for grid layout
validate_math <- function(
  indices = seq_along(test_expressions),
  nrow = 1,
  ncol = 1,
  ...
) {
  if (
    !requireNamespace("grDevices", quietly = TRUE) ||
      utils::packageVersion("grDevices") < package_version("4.3.0")
  ) {
    stop("Visual validation requires grDevices 4.3.0+")
  }
  if (!requireNamespace("grid", quietly = TRUE)) {
    stop("Visual validation requires grid package")
  }

  per_page <- nrow * ncol

  # Process indices in chunks based on grid size
  for (page_start in seq(1, length(indices), by = per_page)) {
    page_indices <- indices[
      page_start:min(page_start + per_page - 1, length(indices))
    ]

    grid::grid.newpage()

    # Create layout viewport
    layout_vp <- grid::viewport(
      layout = grid::grid.layout(
        nrow = nrow * 2, # Double rows: one for label, one for plot
        ncol = ncol,
        heights = grid::unit(rep(c(1, 5), nrow), c("lines", "null"))
      )
    )
    grid::pushViewport(layout_vp)

    for (j in seq_along(page_indices)) {
      i <- page_indices[j]
      expr <- test_expressions[i]

      # Calculate grid position
      grid_col <- ((j - 1) %% ncol) + 1
      grid_row_base <- ((j - 1) %/% ncol) * 2 + 1

      # Draw expression label
      label_vp <- grid::viewport(
        layout.pos.row = grid_row_base,
        layout.pos.col = grid_col
      )
      grid::pushViewport(label_vp)
      # Truncate long expressions for display
      label_text <- if (nchar(expr) > 40) {
        paste0(substr(expr, 1, 37), "...")
      } else {
        expr
      }
      grid::grid.text(
        sprintf("[%d] %s", i, label_text),
        gp = grid::gpar(fontsize = 8, fontfamily = "mono"),
        just = "left",
        x = grid::unit(0.02, "npc")
      )
      grid::popViewport()

      # Draw math expression
      plot_vp <- grid::viewport(
        layout.pos.row = grid_row_base + 1,
        layout.pos.col = grid_col
      )
      grid::pushViewport(plot_vp)

      tryCatch(
        {
          grid::grid.rect(gp = grid::gpar(fill = "grey98", col = "grey80"))
          grid.math(
            expr,
            x = 0.5,
            y = 0.5,
            hjust = "center",
            vjust = "center",
            ...
          )
        },
        error = function(e) {
          grid::grid.text(
            sprintf("ERROR: %s", e$message),
            gp = grid::gpar(col = "red", fontsize = 8)
          )
        }
      )

      grid::popViewport()
    }

    grid::popViewport()
    dev.flush()

    # If there are more pages, wait for user input
    if (page_start + per_page <= length(indices)) {
      #readline("Press Enter for next page...")
    }
  }
}
