__attribute__((weak, visibility("hidden"))) int g1 = 1, g2 = 2;

__attribute__((weak, visibility("hidden"))) int* f1() { return &g1 + g2; }
__attribute__((weak, visibility("hidden"))) int* f2() { return &g2; }
int h1 = 2;

void start() {
  f1();
  f2();
  //while (*(f2()) == 2);
  while (g2 == 2);
  __asm__("ud2");
}
