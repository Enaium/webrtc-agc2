#ifndef ABSL_STRINGS_STR_CAT_H_
#define ABSL_STRINGS_STR_CAT_H_

#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/string_view.h"

namespace absl {

namespace strings_internal {

template <typename T>
void AppendValue(std::string* out, const T& value) {
  if constexpr (HasAbslStringify<T>::value) {
    StringifySink sink(*out);
    AbslStringify(sink, value);
  } else if constexpr (std::is_same_v<T, absl::string_view>) {
    out->append(value.data(), value.size());
  } else {
    std::ostringstream os;
    os << value;
    out->append(os.str());
  }
}

}  // namespace strings_internal

// Stub: concatenates the string representations of all arguments.
template <typename... Ts>
std::string StrCat(const Ts&... values) {
  std::string out;
  (strings_internal::AppendValue(&out, values), ...);
  return out;
}

// Stub: appends the string representations of all arguments to `*out`.
template <typename... Ts>
void StrAppend(std::string* out, const Ts&... values) {
  (strings_internal::AppendValue(out, values), ...);
}

}  // namespace absl

#endif  // ABSL_STRINGS_STR_CAT_H_