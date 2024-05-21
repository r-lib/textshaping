# geomtextpath

<details>

* Version: 0.1.3
* GitHub: https://github.com/AllanCameron/geomtextpath
* Source code: https://github.com/cran/geomtextpath
* Date/Publication: 2024-03-12 16:30:03 UTC
* Number of recursive dependencies: 94

Run `revdepcheck::cloud_details(, "geomtextpath")` for more info

</details>

## Newly broken

*   checking examples ... ERROR
    ```
    Running examples in ‘geomtextpath-Ex.R’ failed
    The error most likely occurred in:
    
    > ### Name: coord_curvedpolar
    > ### Title: Polar coordinates with curved text on x axis
    > ### Aliases: coord_curvedpolar
    > 
    > ### ** Examples
    > 
    > 
    ...
     20. │                       └─tibble:::`[.tbl_df`(txt$shape, , shape_vars)
     21. │                         └─tibble:::vectbl_as_col_location(...)
     22. │                           ├─tibble:::subclass_col_index_errors(...)
     23. │                           │ └─base::withCallingHandlers(...)
     24. │                           └─vctrs::vec_as_location(j, n, names, missing = "error", call = call)
     25. └─vctrs (local) `<fn>`()
     26.   └─vctrs:::stop_subscript_oob(...)
     27.     └─vctrs:::stop_subscript(...)
     28.       └─rlang::abort(...)
    Execution halted
    ```

*   checking tests ... ERROR
    ```
      Running ‘testthat.R’
    Running the tests in ‘tests/testthat.R’ failed.
    Complete output:
      > library(testthat)
      > library(geomtextpath)
      Loading required package: ggplot2
      > 
      > test_check("geomtextpath")
      [ FAIL 35 | WARN 35 | SKIP 3 | PASS 318 ]
      
    ...
        9. │           │ └─base::withCallingHandlers(...)
       10. │           └─vctrs::vec_as_location(j, n, names, missing = "error", call = call)
       11. └─vctrs (local) `<fn>`()
       12.   └─vctrs:::stop_subscript_oob(...)
       13.     └─vctrs:::stop_subscript(...)
       14.       └─rlang::abort(...)
      
      [ FAIL 35 | WARN 35 | SKIP 3 | PASS 318 ]
      Error: Test failures
      Execution halted
    ```

*   checking running R code from vignettes ... ERROR
    ```
    Errors in running code in vignettes:
    when running code in ‘aesthetics.Rmd’
      ...
    
    > p + geom_textpath(size = 4)
    
      When sourcing ‘aesthetics.R’:
    Error: Problem while converting geom to grob.
    ℹ Error occurred in the 1st layer.
    Caused by error in `txt$shape[, shape_vars]`:
    ...
    Error: Problem while converting geom to grob.
    ℹ Error occurred in the 1st layer.
    Caused by error in `txt$shape[, shape_vars]`:
    ! Can't subset columns that don't exist.
    ✖ Column `x_midpoint` doesn't exist.
    Execution halted
    
      ‘aesthetics.Rmd’ using ‘UTF-8’... failed
      ‘curved_polar.Rmd’ using ‘UTF-8’... failed
      ‘geomtextpath.Rmd’ using ‘UTF-8’... failed
    ```

*   checking re-building of vignette outputs ... NOTE
    ```
    Error(s) in re-building vignettes:
    --- re-building ‘aesthetics.Rmd’ using rmarkdown
    
    Quitting from lines 40-53 [unnamed-chunk-2] (aesthetics.Rmd)
    Error: processing vignette 'aesthetics.Rmd' failed with diagnostics:
    Problem while converting geom to grob.
    ℹ Error occurred in the 1st layer.
    Caused by error in `txt$shape[, shape_vars]`:
    ! Can't subset columns that don't exist.
    ✖ Column `x_midpoint` doesn't exist.
    --- failed re-building ‘aesthetics.Rmd’
    
    --- re-building ‘curved_polar.Rmd’ using rmarkdown
    ```

