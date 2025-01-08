// Generated by cpp11: do not edit by hand
// clang-format off


#include "cpp11/declarations.hpp"
#include <R_ext/Visibility.h>

// string_metrics.h
list get_string_shape_c(strings string, integers id, strings path, integers index, list_of<list> features, doubles size, doubles res, doubles lineheight, integers align, doubles hjust, doubles vjust, doubles width, doubles tracking, doubles indent, doubles hanging, doubles space_before, doubles space_after, integers direction, list_of<integers> soft_wrap, list_of<integers> hard_wrap);
extern "C" SEXP _textshaping_get_string_shape_c(SEXP string, SEXP id, SEXP path, SEXP index, SEXP features, SEXP size, SEXP res, SEXP lineheight, SEXP align, SEXP hjust, SEXP vjust, SEXP width, SEXP tracking, SEXP indent, SEXP hanging, SEXP space_before, SEXP space_after, SEXP direction, SEXP soft_wrap, SEXP hard_wrap) {
  BEGIN_CPP11
    return cpp11::as_sexp(get_string_shape_c(cpp11::as_cpp<cpp11::decay_t<strings>>(string), cpp11::as_cpp<cpp11::decay_t<integers>>(id), cpp11::as_cpp<cpp11::decay_t<strings>>(path), cpp11::as_cpp<cpp11::decay_t<integers>>(index), cpp11::as_cpp<cpp11::decay_t<list_of<list>>>(features), cpp11::as_cpp<cpp11::decay_t<doubles>>(size), cpp11::as_cpp<cpp11::decay_t<doubles>>(res), cpp11::as_cpp<cpp11::decay_t<doubles>>(lineheight), cpp11::as_cpp<cpp11::decay_t<integers>>(align), cpp11::as_cpp<cpp11::decay_t<doubles>>(hjust), cpp11::as_cpp<cpp11::decay_t<doubles>>(vjust), cpp11::as_cpp<cpp11::decay_t<doubles>>(width), cpp11::as_cpp<cpp11::decay_t<doubles>>(tracking), cpp11::as_cpp<cpp11::decay_t<doubles>>(indent), cpp11::as_cpp<cpp11::decay_t<doubles>>(hanging), cpp11::as_cpp<cpp11::decay_t<doubles>>(space_before), cpp11::as_cpp<cpp11::decay_t<doubles>>(space_after), cpp11::as_cpp<cpp11::decay_t<integers>>(direction), cpp11::as_cpp<cpp11::decay_t<list_of<integers>>>(soft_wrap), cpp11::as_cpp<cpp11::decay_t<list_of<integers>>>(hard_wrap)));
  END_CPP11
}
// string_metrics.h
doubles get_line_width_c(strings string, strings path, integers index, doubles size, doubles res, logicals include_bearing, list_of<list> features);
extern "C" SEXP _textshaping_get_line_width_c(SEXP string, SEXP path, SEXP index, SEXP size, SEXP res, SEXP include_bearing, SEXP features) {
  BEGIN_CPP11
    return cpp11::as_sexp(get_line_width_c(cpp11::as_cpp<cpp11::decay_t<strings>>(string), cpp11::as_cpp<cpp11::decay_t<strings>>(path), cpp11::as_cpp<cpp11::decay_t<integers>>(index), cpp11::as_cpp<cpp11::decay_t<doubles>>(size), cpp11::as_cpp<cpp11::decay_t<doubles>>(res), cpp11::as_cpp<cpp11::decay_t<logicals>>(include_bearing), cpp11::as_cpp<cpp11::decay_t<list_of<list>>>(features)));
  END_CPP11
}

extern "C" {
static const R_CallMethodDef CallEntries[] = {
    {"_textshaping_get_line_width_c",   (DL_FUNC) &_textshaping_get_line_width_c,    7},
    {"_textshaping_get_string_shape_c", (DL_FUNC) &_textshaping_get_string_shape_c, 20},
    {NULL, NULL, 0}
};
}

void export_string_metrics(DllInfo* dll);

extern "C" attribute_visible void R_init_textshaping(DllInfo* dll){
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  export_string_metrics(dll);
  R_forceSymbols(dll, TRUE);
}
