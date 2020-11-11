This version includes a "fix" that allows the package to build on Solaris even
without harfbuzz and fribidi (as is the situation on CRAN). It further updates 
the C api to prepare for future font substitution functionality

## Test environments
* local R installation, R 4.0.1
* ubuntu 16.04 (on travis-ci), R 4.0.1
* win-builder (devel)

## R CMD check results

0 errors | 0 warnings | 0 note
