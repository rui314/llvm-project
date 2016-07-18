// RUN: %clang_cc1 -fsyntax-only -triple x86_64-pc-linux-gnu -verify %s
// RUN: %clang_cc1 -fsyntax-only -triple powerpc64le-linux-gnu -verify %s

extern void a(const char *);

extern const char *str;

int main() {
#ifdef __x86_64__
  if (__builtin_cpu_supports("ss")) // expected-error {{invalid cpu feature string}}
    a("sse4.2");

  if (__builtin_cpu_supports(str)) // expected-error {{expression is not a string literal}}
    a(str);
#else
  if (__builtin_cpu_supports("vsx")) // expected-error {{use of unknown builtin}}
    a("vsx");
#endif

  return 0;
}
