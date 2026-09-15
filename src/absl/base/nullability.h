#ifndef ABSL_BASE_NULLABILITY_H_
#define ABSL_BASE_NULLABILITY_H_

// Stub: Clang nullability annotations. Outside Clang they expand to nothing.

#if !defined(absl_nonnull)
#if defined(__clang__) && defined(__has_feature)
#if __has_feature(nullability)
#define absl_nonnull _Nonnull
#else
#define absl_nonnull
#endif
#else
#define absl_nonnull
#endif
#endif

#if !defined(absl_nullable)
#if defined(__clang__) && defined(__has_feature)
#if __has_feature(nullability)
#define absl_nullable _Nullable
#else
#define absl_nullable
#endif
#else
#define absl_nullable
#endif
#endif

// Stub: `ABSL_NULLABILITY_COMPATIBLE` marks a class as nullability-compatible.
#define ABSL_NULLABILITY_COMPATIBLE

#endif  // ABSL_BASE_NULLABILITY_H_