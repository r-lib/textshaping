This is a minor release with a full rewrite of the R facing shaping algorithm. .
It breaks one reverse dependency where the maintainer are waiting for this 
version to arrive on CRAN before fixing it

## revdepcheck results

We checked 3 reverse dependencies, comparing R CMD check results across CRAN and dev versions of this package.

 * We saw 1 new problems
 * We failed to check 0 packages

Issues with CRAN packages are summarised below.

### New problems
(This reports the first line of each new failure)

* geomtextpath
  checking examples ... ERROR
  checking tests ... ERROR
  checking running R code from vignettes ... ERROR
  checking re-building of vignette outputs ... NOTE
