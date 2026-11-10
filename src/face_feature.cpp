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
#include <cpp11/protect.hpp>

using namespace cpp11;

writable::list get_face_features_c(strings path, integers index) {
  if (path.size() != index.size()) {
    cpp11::stop("`path` and `index` must be the same length");
  }

  writable::list features(path.size());
  if (path.size() == 0) {
    return features; // Early exit
  }

  std::vector<hb_tag_t> tags;
  char tag_temp[5];
  tag_temp[4] = '\0';

  // RAII guard so error paths (exceptions) can't leak the library
  struct FTLibraryGuard {
    FT_Library library = nullptr;
    ~FTLibraryGuard() {
      if (library != nullptr) FT_Done_FreeType(library);
    }
  } guard;

  FT_Error error = FT_Init_FreeType(&guard.library);
  if (error != 0) {
    stop("Freetype could not be initialised");
  }

  for (int i = 0; i < path.size(); ++i) {
    cpp11::check_user_interrupt();
    if (path[i] == NA_STRING || index[i] == NA_INTEGER) {
      stop("`path` and `index` cannot contain missing values");
    }
    FT_Face ft_face = nullptr;
    error = FT_New_Face(guard.library,
                        cpp11::safe[Rf_translateCharUTF8](path[i]),
                        index[i],
                        &ft_face);
    if (error != 0 || ft_face == nullptr) {
      stop("Font could not be loaded (freetype error %i)", (int) error);
    }
    // hb_ft_face_create_referenced takes its own reference, so we can release
    // ours immediately; hb_face_destroy below releases the rest
    hb_face_t *face = hb_ft_face_create_referenced(ft_face);
    FT_Done_Face(ft_face);
    writable::strings font_tags;

    auto collect_tags = [&](hb_tag_t table) {
      unsigned int n_tags = hb_ot_layout_table_get_feature_tags(face, table, 0, NULL, NULL);
      if (n_tags == 0) return;
      tags.assign(n_tags, 0);
      unsigned int count = n_tags;
      hb_ot_layout_table_get_feature_tags(face, table, 0, &count, tags.data());
      for (unsigned int j = 0; j < count; ++j) {
        hb_tag_to_string(tags[j], tag_temp);
        font_tags.push_back(tag_temp);
      }
    };

    collect_tags(HB_OT_TAG_GPOS);
    collect_tags(HB_OT_TAG_GSUB);

    features[i] = (SEXP) font_tags;
    hb_face_destroy(face);
  }

  return features;
}

#endif
