// RUN: %clang_cc1 %s -verify -pedantic -fsyntax-only -Wno-unused-value

#pragma OPENCL EXTENSION cl_khr_fp16 : disable
constant float f = 1.0h; // expected-error{{half precision constant requires cl_khr_fp16}}

half half_disabled(half *p, // expected-error{{declaring function return value of type 'half' is not allowed}}
                   half h)  // expected-error{{declaring function parameter of type 'half' is not allowed}}
{
  half a[2]; // expected-error{{declaring variable of type 'half [2]' is not allowed}}
  half b;    // expected-error{{declaring variable of type 'half' is not allowed}}
  *p; // expected-error{{loading directly from pointer to type 'half' is not allowed}}
  p[1]; // expected-error{{loading directly from pointer to type 'half' is not allowed}}

  float c = 1.0f;
  b = (half) c;  // expected-error{{casting to type 'half' is not allowed}}
  c = (float) 1.0h;  // expected-error{{half precision constant requires cl_khr_fp16}}
  b = 1.0h; // expected-error{{half precision constant requires cl_khr_fp16}}

  half *allowed = &p[1];
  half *allowed2 = &*p;
  half *allowed3 = p + 1;

  return h;
}

// Exactly the same as above but with the cl_khr_fp16 extension enabled.
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
constant half a = 1.0h;
half half_enabled(half *p, half h)
{
  half a[2];
  half b;
  *p;
  p[1];

  float c = 1.0f;
  b = (half) c;
  c = (float) 1.0h;
  b = 1.0h;

  half *allowed = &p[1];
  half *allowed2 = &*p;
  half *allowed3 = p + 1;

  return h;
}
