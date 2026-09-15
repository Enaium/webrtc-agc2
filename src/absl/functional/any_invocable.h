#ifndef ABSL_FUNCTIONAL_ANY_INVOCABLE_H_
#define ABSL_FUNCTIONAL_ANY_INVOCABLE_H_

#include <memory>
#include <type_traits>
#include <utility>

namespace absl {

// Stub: type-erased wrapper for move-only callables.
//
// Mirrors the subset of `absl::AnyInvocable` used by WebRTC: construction from
// any compatible callable, move semantics, boolean conversion and invocation.
template <typename Signature>
class AnyInvocable;

template <typename R, typename... Args>
class AnyInvocable<R(Args...)> {
 public:
  AnyInvocable() = default;
  AnyInvocable(std::nullptr_t) {}

  AnyInvocable(AnyInvocable&&) = default;
  AnyInvocable& operator=(AnyInvocable&&) = default;

  template <typename F,
            typename = std::enable_if_t<
                !std::is_same_v<std::decay_t<F>, AnyInvocable> &&
                std::is_invocable_r_v<R, std::decay_t<F>&, Args...>>>
  AnyInvocable(F&& f)
      : impl_(std::make_unique<Impl<std::decay_t<F>>>(std::forward<F>(f))) {}

  explicit operator bool() const { return impl_ != nullptr; }

  R operator()(Args... args) {
    return impl_->Invoke(std::forward<Args>(args)...);
  }

 private:
  struct Base {
    virtual ~Base() = default;
    virtual R Invoke(Args...) = 0;
  };

  template <typename F>
  struct Impl final : Base {
    explicit Impl(F&& f) : callable_(std::move(f)) {}
    R Invoke(Args... args) override {
      return callable_(std::forward<Args>(args)...);
    }
    F callable_;
  };

  std::unique_ptr<Base> impl_;
};

// Ref-qualified signatures, as used for task queue tasks.
template <typename R, typename... Args>
class AnyInvocable<R(Args...) &&> : public AnyInvocable<R(Args...)> {
 public:
  using AnyInvocable<R(Args...)>::AnyInvocable;
  AnyInvocable() = default;
  AnyInvocable(AnyInvocable&&) = default;
  AnyInvocable& operator=(AnyInvocable&&) = default;

  R operator()(Args... args) && {
    return AnyInvocable<R(Args...)>::operator()(std::forward<Args>(args)...);
  }
};

}  // namespace absl

#endif  // ABSL_FUNCTIONAL_ANY_INVOCABLE_H_