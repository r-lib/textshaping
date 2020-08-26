#include "face_feature.h"
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

  for (int i = 0; i < path.size(); ++i) {
    hb_blob_t* blob = hb_blob_create_from_file(Rf_translateCharUTF8(path[i]));
    hb_face_t* face = hb_face_create(blob, index[i]);
    writable::strings font_tags;
    // Get GPOS tags
    n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GPOS, 0, NULL, NULL);
    tags.clear();
    tags.reserve(n_tags);
    for (size_t j = 0; j < n_tags; ++j) {
      hb_tag_t tag;
      tags.push_back(tag);
    }
    n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GPOS, 0, &n_tags, &tags[0]);
    for (size_t j = 0; j < n_tags; ++j) {
      hb_tag_to_string(tags[j], tag_temp);
      font_tags.push_back(tag_temp);
    }
    // Get GSUB tags — same as above
    n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GSUB, 0, NULL, NULL);
    tags.clear();
    tags.reserve(n_tags);
    for (size_t j = 0; j < n_tags; ++j) {
      hb_tag_t tag;
      tags.push_back(tag);
    }
    n_tags = hb_ot_layout_table_get_feature_tags(face, HB_OT_TAG_GSUB, 0, &n_tags, &tags[0]);
    for (size_t j = 0; j < n_tags; ++j) {
      hb_tag_to_string(tags[j], tag_temp);
      font_tags.push_back(tag_temp);
    }
    features[i] = (SEXP) font_tags;
    hb_face_destroy(face);
  }
  return features;
}
