#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_ALT_FORM_FLAG 1
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 0
#define NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER 1
#define NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER 1
#define NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER 1

#include "shims.h"

int main(void) {
  volatile float a = 5.5f;
  volatile float b = 2.0f;

  printf("%.2f + %.2f = %.2f\n", a, b, a + b);
  printf("%.2f - %.2f = %.2f\n", a, b, a - b);
  printf("%.2f * %.2f = %.2f\n", a, b, a * b);
  printf("%.2f / %.2f = %.2f\n", a, b, a / b);

  volatile int converted_int = (int)a;
  volatile float converted_back = (float)converted_int;
  printf("float %2.2f -> int %d -> float %2.2f\n", a, converted_int, converted_back);

  printf("=== Soft-Float Comparison Tests ===\n");
  if (a > b) {
    printf("ok: a (%.1f) > b (%.1f)\n", a, b);
  } else {
    printf("FAIL: a > b\n");
  }

  if (b < a) {
    printf("ok: b (%.1f) < a (%.1f)\n", b, a);
  } else {
    printf("FAIL: b > a\n");
  }

  if (a != b) {
    printf("ok: a != b\n");
  } else {
    printf("FAIL: a == b\n");
  }

  return 0;
}
