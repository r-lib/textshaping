#include "face_feature.h"

#ifdef NO_HARFBUZZ_FRIBIDI

using namespace cpp11;

writable::list get_face_features_c(strings path, integers index) {
  Rprintf("textshaping has been compiled without HarfBuzz and/or Fribidi. Please install system dependencies and recompile\n");
  return {};
}

#else

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>
#include <vector>
#include <cpp11/r_string.hpp>

using namespace cpp11;

writable::list get_face_features_c(strings path, integers index) {
  writable::list features(path.size());
  std::vector<hb_tag_t> tags;
  unsigned int n_tags = 0;
  char tag_temp[5];
  tag_temp[4] = '\0';

  FT_Library library;
  FT_Face ft_face;
  FT_Error error = FT_Init_FreeType(&library);
  if (error != 0) {
    stop("Freetype could not be initialised");
  }

  for (int i = 0; i < path.size(); ++i) {
    error = FT_New_Face(library,
                        Rf_translateCharUTF8(path[i]),
                        index[i],
                        &ft_face);
    if (error != 0) {
      stop("Font could not be loaded");
    }
    hb_face_t *face = hb_ft_face_create_referenced(ft_face);
    writable::strings font_tags;

    // Get GPOS tags
    n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GPOS, 0, NULL, NULL);
    tags.clear();
    tags.reserve(n_tags);
    for (size_t j = 0; j < n_tags; ++j) {
      hb_tag_t tag;
      tags.push_back(tag);
    }
    if (n_tags > 0) {
      n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GPOS, 0, &n_tags, &tags[0]);
      for (size_t j = 0; j < n_tags; ++j) {
        hb_tag_to_string(tags[j], tag_temp);
        font_tags.push_back(tag_temp);
      }
    }

    // Get GSUB tags — same as above
    n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GSUB, 0, NULL, NULL);
    tags.clear();
    tags.reserve(n_tags);
    for (size_t j = 0; j < n_tags; ++j) {
      hb_tag_t tag;
      tags.push_back(tag);
    }
    if (n_tags > 0) {
      n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GSUB, 0, &n_tags, &tags[0]);
      for (size_t j = 0; j < n_tags; ++j) {
        hb_tag_to_string(tags[j], tag_temp);
        font_tags.push_back(tag_temp);
      }
    }

    features[i] = (SEXP) font_tags;
    hb_face_destroy(face);
  }

  FT_Done_FreeType(library);
  return features;
}

#endif
