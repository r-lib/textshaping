#pragma once

#include <cpp11/strings.hpp>
#include <cpp11/integers.hpp>
#include <cpp11/list.hpp>

[[cpp11::register]]
cpp11::writable::list get_face_features_c(cpp11::strings path, cpp11::integers index);
