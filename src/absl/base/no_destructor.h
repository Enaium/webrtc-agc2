#ifndef ABSL_BASE_NO_DESTRUCTOR_H_
#define ABSL_BASE_NO_DESTRUCTOR_H_

#include <new>
#include <type_traits>
#include <utility>

namespace absl {

// Stub: a static object that is never destroyed.
//
// The contained object is constructed in place on first use of the
// constructor and its destructor is never called, so that it remains valid
// until the process terminates.
template <typename T>
class NoDestructor {
 public:
  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    static_assert(std::is_constructible_v<T, Args&&...>,
                  "T must be constructible from the given arguments");
    new (storage()) T(std::forward<Args>(args)...);
  }

  ~NoDestructor() = default;

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  T* get() { return reinterpret_cast<T*>(storage()); }
  const T* get() const { return reinterpret_cast<const T*>(storage()); }

  T& operator*() { return *get(); }
  const T& operator*() const { return *get(); }

  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

 private:
  unsigned char* storage() {
    return reinterpret_cast<unsigned char*>(&storage_);
  }
  const unsigned char* storage() const {
    return reinterpret_cast<const unsigned char*>(&storage_);
  }

  alignas(T) unsigned char storage_[sizeof(T)];
};

}  // namespace absl

#endif  // ABSL_BASE_NO_DESTRUCTOR_H_