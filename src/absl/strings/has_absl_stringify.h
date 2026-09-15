#ifndef ABSL_STRINGS_HAS_ABSL_STRINGIFY_H_
#define ABSL_STRINGS_HAS_ABSL_STRINGIFY_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"

namespace absl {

// Stub: the sink passed to `AbslStringify()` overloads. Abseil calls
// `sink.Append(...)` with the pieces of the string representation.
class StringifySink {
 public:
  explicit StringifySink(std::string& out) : out_(out) {}

  void Append(absl::string_view part) { out_.append(part.data(), part.size()); }
  void Append(size_t count, char c) { out_.append(count, c); }

 private:
  std::string& out_;
};

namespace strings_internal {

template <typename T, typename = void>
struct IsAbslStringifiable : std::false_type {};

template <typename T>
struct IsAbslStringifiable<
    T,
    std::void_t<decltype(AbslStringify(std::declval<StringifySink&>(),
                                       std::declval<const T&>()))>>
    : std::true_type {};

}  // namespace strings_internal

// Detects whether `T` provides an `AbslStringify()` overload.
template <typename T>
struct HasAbslStringify : strings_internal::IsAbslStringifiable<T> {};

}  // namespace absl

#endif  // ABSL_STRINGS_HAS_ABSL_STRINGIFY_H_