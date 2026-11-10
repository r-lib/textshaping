# Comprehensive math expression tests for visual validation
# Run with: plot_math(shape_math(expr)) for each expression
# Tests both LaTeX and Typst syntax

# =============================================================================
# Typst test expressions
# =============================================================================

typst_expressions <- c(
  # === BASIC ELEMENTS ===
  # Variables
  "x + y = z",
  "a^2 + b^2 = c^2",

  # Numbers
  "x = 42",
  "y = 3.14159",

  # Greek letters
  "alpha + beta = gamma",
  "Gamma Delta Theta Lambda Xi Pi Sigma Phi Psi Omega",

  # Greek variants (dotted)
  "epsilon.alt",
  "phi.alt",
  "theta.alt",

  # === DOTTED/NAMESPACED SYMBOLS ===
  # Arrows
  "arrow.r",
  "arrow.l",
  "arrow.lr",
  "arrow.r.double",
  "arrow.r.long",
  "arrow.hook.r",
  "arrow.twohead.r",
  "arrow.bar.r",

  # Relations
  "eq.not",
  "lt.eq",
  "gt.eq",
  "approx.eq",
  "subset.eq",
  "prec.eq",

  # Operators
  "plus.minus",
  "times.circle",
  "dot.op",

  # Sets
  "union.big",
  "sect.big",

  # Logic
  "exists.not",

  # Miscellaneous
  "planck.reduce",
  "dots.h",
  "dots.c",

  # === SHORTHAND SEQUENCES ===
  # Arrows
  "a -> b",
  "a <- b",
  "a <-> b",
  "a => b",
  "a <=> b",
  "a --> b",
  "a <-- b",
  "a ==> b",
  "a <==> b",
  "f: A |-> B",

  # Relations
  "a <= b",
  "a >= b",
  "a != b",
  "a << b",
  "a >> b",

  # Operators
  "a * b",
  "a - b",
  "a ~ b",

  # Definitions
  "x := 1",
  "f ::= g",

  # === OPERATORS AND RELATIONS ===
  "a + b - c * d / e",

  # === SCRIPTS ===
  "x^2",
  "x_i",
  "x_i^2",
  "x^(2n+1)",

  # === FRACTIONS ===
  "frac(a, b)",
  "frac(x+1, x-1)",
  "frac(frac(a, b), frac(c, d))",

  # Fraction shorthand
  "a/b",
  "1/2",
  "x^2/y^2",
  "a/b/c",

  # === ROOTS ===
  "sqrt(x)",
  "sqrt(x^2 + y^2)",
  "root(3, x)",
  "root(n, a^n)",

  # === DELIMITERS ===
  "abs(x)",
  "norm(v)",
  "floor(x)",
  "ceil(x)",
  "lr((frac(a, b)))",

  # Explicit delimiter sizes
  "lr(size: 2em, (x))",
  "lr(size: 3, [a + b])",
  "lr(size: 24pt, (y))",
  "lr(size: 10mm, [z])",
  "lr(size: 1cm, {w})",
  "lr(size: 0.5in, (v))",
  "lr(size: 150%, (frac(a, b)))",

  # Non-scaling (escaped) delimiters
  "\\(x\\)",
  "\\[a + b\\]",
  "f\\(x\\) = x^2",

  # === ACCENTS ===
  "hat(x) tilde(y) bar(z)",
  "dot(x) ddot(x) vec(v)",
  "acute(a) grave(e) breve(u)",

  # Wide accents
  "widehat(abc) widetilde(xyz)",
  "overline(x+y) underline(a+b)",
  "overbrace(a+b+c, n) underbrace(x+y+z, m)",
  "overrightarrow(AB)",

  # Generic accent function
  "accent(x, hat)",
  "accent(x, tilde)",
  "accent(a, acute)",
  'accent(x, "\u0302")',
  'accent(v, "\u20D7")',

  # === FUNCTIONS ===
  "sin x cos y tan z",
  "log x ln e exp(x)",
  "lim max min",

  # === DIFFERENTIAL OPERATORS ===
  "dif",
  "Dif",
  "dd",
  "integral dif x",
  "integral_0^1 f(x) dif x",

  # === PRIMES ===
  "f'",
  "f''",
  "f'''",
  "f''''",
  "f^2'",
  "f'(x)",

  # === FONT STYLES ===
  "upright(d) x",
  "bold(v) + bold(w)",
  "italic(text)",
  "sans(text)",
  "mono(code)",
  "cal(L) frak(g)",
  "bb(R) bb(C) bb(Z) bb(Q) bb(N)",

  # === TEXT ===
  'x "if" x > 0',

  # === MATRICES ===
  "mat(a, b; c, d)",
  "mat(1, 0; 0, 1)",

  # === CASES ===
  "cases(1 &\"if\" x > 0, 0 &\"if\" x = 0)",

  # === STACKED EXPRESSIONS ===
  "binom(n, k)",

  # === ATTACH (four-corner scripts) ===
  "attach(T, t: mu, b: nu)",
  "attach(T, tl: a, tr: b, bl: c, br: d)",
  "attach(Gamma, tl: mu, tr: nu, bl: rho, br: sigma)",
  "attach(R, tr: i, br: j, bl: k, tl: l)",

  # === SPACING ===
  "a thin b med c thick d",
  "x quad y",

  # Explicit spacing with h()
  "a h(1em) b",
  "a h(2em) b",
  "a h(10pt) b",
  "a h(5mm) b",
  "a h(0.5cm) b",
  "a h(0.25in) b",

  # === CANCELLATION ===
  "cancel(x)",
  "cancel(x, inverted: true)",
  "cancel(x, cross: true)",

  # === PHANTOMS ===
  "a + hide(b) + c",

  # === DISPLAY STYLES ===
  "display(sum_i^n x_i)",
  "inline(sum_i^n x_i)",

  # === NEW FUNCTIONS ===
  # Round brackets shorthand
  "round(x + y)",

  # Primes function
  "primes(1)",
  "primes(2)",
  "primes(3)",

  # Additional style functions
  "serif(x)",
  "scr(L)",

  # Mid delimiter (for set-builder notation)
  "lr({ x mid(|) x > 0 })",

  # Under/over bracket variants
  "overbracket(a + b, n)",
  "underbracket(x + y, m)",
  "overparen(f(x))",
  "underparen(g(x))",
  "overshell(abc)",
  "undershell(xyz)",

  # === NEW FEATURES ===
  # stretch() function
  "stretch(=, size: 3em)",
  "stretch(arrow.r, size: 2em)",

  # class() function
  'class("relation", x)',
  'class("binary", +)',

  # Matrix augment parameter
  "mat(augment: 1, a, b; c, d)",
  "mat(augment: #(vline: 1), a, b; c, d)",
  "mat(augment: #(hline: 0), a, b; c, d)",
  "mat(augment: #(vline: 1, hline: 0), a, b; c, d)",

  # cramped parameter for display styles
  "display(x^2, cramped: true)",
  "inline(x^2, cramped: true)",
  "script(x^2, cramped: true)",
  "sscript(x^2, cramped: true)",

  # accent size parameter
  "accent(x, hat, size: 150%)",
  "accent(x, hat, size: 2)",

  # accent dotless parameter
  "accent(i, hat, dotless: true)",
  "accent(j, hat, dotless: true)",

  # gap parameters (using non-hyphenated names; hyphenated row-gap/column-gap also work)
  "mat(rowgap: 1em, a, b; c, d)",
  "mat(columngap: 2em, a, b; c, d)",

  # === MULTILINE (Typst linebreak and alignment) ===
  "x &= 1 \\ &= 2",
  "a &= b + c \\ &= d",

  # === COMPLEX EXPRESSIONS ===
  # Quadratic formula
  "x = frac(-b + sqrt(b^2 - 4 a c), 2 a)",

  # Taylor series (partial)
  "e^x = sum_(n=0)^infinity frac(x^n, n!)",

  # Matrix determinant
  "det mat(a, b; c, d) = a d - b c"
)

# =============================================================================
# LaTeX test expressions
# =============================================================================

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

  # Differential operators
  r"(\dif)",
  r"(\Dif)",
  r"(\dd)",
  r"(\int \dif x)",
  r"(\int_0^1 f(x) \dif x)",

  # Primes
  "f'",
  "f''",
  "f'''",
  "f''''",
  "f^2'",
  r"(f^\prime)",
  r"(f^{\prime\prime})",
  "f'(x)",

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

# Test that all LaTeX expressions can be shaped without error
test_that("All LaTeX test expressions shape without error", {
  for (i in seq_along(test_expressions)) {
    expr <- test_expressions[i]
    result <- tryCatch(
      shape_math(expr),
      error = function(e) e
    )
    expect_false(
      inherits(result, "error"),
      label = sprintf("LaTeX expression %d: %s", i, expr)
    )
  }
})

# Test that all Typst expressions can be shaped without error
test_that("All Typst test expressions shape without error", {
  for (i in seq_along(typst_expressions)) {
    expr <- typst_expressions[i]
    result <- tryCatch(
      shape_math(expr),
      error = function(e) e
    )
    expect_false(
      inherits(result, "error"),
      label = sprintf("Typst expression %d: %s", i, expr)
    )
  }
})

# Test syntax detection - LaTeX vs Typst should produce equivalent results
test_that("LaTeX and Typst produce equivalent results for simple expressions", {
  # Simple fraction
  latex_result <- shape_math(r"(\frac{a}{b})")
  typst_result <- shape_math("frac(a, b)")
  expect_equal(nrow(latex_result$shape), nrow(typst_result$shape))

  # Square root
  latex_result <- shape_math(r"(\sqrt{x})")
  typst_result <- shape_math("sqrt(x)")
  expect_equal(nrow(latex_result$shape), nrow(typst_result$shape))

  # Binomial
  latex_result <- shape_math(r"(\binom{n}{k})")
  typst_result <- shape_math("binom(n, k)")
  expect_equal(nrow(latex_result$shape), nrow(typst_result$shape))
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
# Regression tests for parser bugs found in the 2026-11 C++ review

test_that("typst brace groups with unexpected tokens error instead of hanging", {
  expect_error(shape_math("arrow.r {a)}"))
})

test_that("typst augment dictionary with unexpected tokens errors instead of hanging", {
  expect_error(shape_math("mat(augment: #(1), a)"))
  expect_no_error(shape_math("mat(augment: #(vline: -1), a)"))
  expect_no_error(shape_math("mat(augment: #(vline: (1, 2)), a)"))
})

test_that("typst parser reports trailing content instead of silently dropping it", {
  expect_error(shape_math("arrow.r a, b"))
  expect_error(shape_math("arrow.r f: A -> B"))
})

test_that("typst parenthesised comma lists work", {
  expect_no_error(shape_math("f(x, y)"))
})

test_that("typst scripts, primes, and identifiers work", {
  expect_no_error(shape_math("x_1 + y"))
  expect_no_error(shape_math("xy + z"))
  expect_no_error(shape_math("f'(x) + y"))
  expect_no_error(shape_math("x^(a+b)"))
})

test_that("typst root, matrices, and cases handle ragged and empty cells", {
  expect_no_error(shape_math("root(3, x)"))
  expect_no_error(shape_math("mat(a; c, d)"))
  expect_no_error(shape_math("mat(a, b; c, d)"))
  expect_no_error(shape_math("cases(a & b, c & d)"))
  expect_no_error(shape_math("mat(row-gap: 0.5em, a)"))
  expect_no_error(shape_math("mat(column-gap: 1em, a)"))
})

test_that("typst delimiters and attachments work", {
  expect_no_error(shape_math("lr(|x|)"))
  expect_no_error(shape_math("lr((x + y))"))
  expect_no_error(shape_math("attach(x, t: a, b: c)"))
  expect_no_error(shape_math("attach(x, t: a)"))
  expect_no_error(shape_math("attach(x, tl: a, br: b)"))
})

test_that("typst backslash escapes and line breaks work", {
  expect_no_error(shape_math("arrow.r a \\ b"))
  expect_no_error(shape_math("\\(a\\)"))
})

test_that("deeply nested input errors instead of overflowing the stack", {
  expect_error(
    shape_math(paste0(strrep("{", 2000), "x", strrep("}", 2000))),
    "nested"
  )
  expect_error(
    shape_math(paste0(strrep(r"(\frac{)", 1000), "1", strrep("}{1}", 1000))),
    "nested"
  )
})

test_that("latex displaystyle stops at \\right and friends", {
  expect_no_error(shape_math(r"(\left( \displaystyle a \right))"))
})

test_that("latex non-ascii input is rendered", {
  expect_no_error(shape_math(r"(\alpha + é)"))
})

test_that("latex primes combine with scripts in order", {
  expect_no_error(shape_math("f'^2"))
  expect_no_error(shape_math("f^2'"))
  # f'^2 should produce a superscript containing both glyphs
  s1 <- shape_math("f'^2")
  s2 <- shape_math("f'")
  expect_gt(nrow(s1$shape), nrow(s2$shape))
})

test_that("latex \\text preserves spaces", {
  s <- shape_math(r"(\text{a b})")
  expect_gte(nrow(s$shape), 2)
})

test_that("math glyph variants and assemblies are returned", {
  loc <- "/System/Library/Fonts/Supplemental/STIXTwoMath.otf"
  skip_if_not(file.exists(loc))
  stopifnot(has_math_table(loc, 0L))
  gid <- shape_math("(", path = loc)$shape$index[1]
  variants <- get_math_glyph_variants(loc, 0L, as.integer(gid), TRUE, 12, 72)
  expect_gt(nrow(variants), 0)
  assembly <- get_math_glyph_assembly(loc, 0L, as.integer(gid), TRUE, 12, 72)
  expect_gt(nrow(assembly), 0)
})
