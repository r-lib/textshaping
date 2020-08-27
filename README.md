
<!-- README.md is generated from README.Rmd. Please edit that file -->

# textshaping

<!-- badges: start -->

[![Codecov test
coverage](https://codecov.io/gh/r-lib/textshaping/branch/master/graph/badge.svg)](https://codecov.io/gh/r-lib/textshaping?branch=master)
[![R build
status](https://github.com/r-lib/textshaping/workflows/R-CMD-check/badge.svg)](https://github.com/r-lib/textshaping/actions)
<!-- badges: end -->

This package is a low level package that provides advanced text shaping
capabilities to graphic devices. It is based on the
[FriBidi](https://github.com/fribidi/fribidi) and
[HarfBuzz](https://harfbuzz.github.io) libraries and provides full
support for correctly shaping left-to-right, right-to-left, and
bidirectional text with full support for all OpenType features such as
ligatures, stylistic glyph substitutions, etc.

For an example of a package that uses textshaping to support advanced
text layout see [ragg](https://ragg.r-lib.org).

A big thanks to [Behdad Esfahbod](behdad.org) who is the main author of
both FriBidi and HarfBuzz and has been very helpful answering questions
during the cause of development.

## Installation

For now you can install the package with devtools:

``` r
devtools::install_github("r-lib/textshaping")
```

Note that you will need both the development versions of FriBidi and
HarfBuzz to successfully compile it.

## Code of Conduct

Please note that the textshaping project is released with a [Contributor
Code of
Conduct](https://contributor-covenant.org/version/2/0/CODE_OF_CONDUCT.html).
By contributing to this project, you agree to abide by its terms.
