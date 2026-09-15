#ifndef ABSL_BASE_CONFIG_H_
#define ABSL_BASE_CONFIG_H_

// Stub: feature detection normally provided by Abseil's generated
// `absl/base/config.h`. Only the subset used by the extracted sources is
// defined here.

#if !defined(ABSL_HAVE_THREAD_LOCAL)
#if __cplusplus >= 201103L
#define ABSL_HAVE_THREAD_LOCAL 1
#endif
#endif

#endif  // ABSL_BASE_CONFIG_H_