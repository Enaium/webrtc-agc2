#ifndef ABSL_ALGORITHM_CONTAINER_H_
#define ABSL_ALGORITHM_CONTAINER_H_

#include <algorithm>

namespace absl {

// Stub: fills the container with the given value.
template <typename Container, typename Value>
void c_fill(Container& container, const Value& value) {
  std::fill(container.begin(), container.end(), value);
}

}  // namespace absl

#endif  // ABSL_ALGORITHM_CONTAINER_H_