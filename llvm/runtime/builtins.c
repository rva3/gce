/* Compatibility amalgamation for standalone compilation and analysis only.
 * Build runtime/builtins/*.c separately when creating libgce.a, so an
 * application may supply individual integer or memory helpers.
 * Wide helpers use 32-bit limbs to avoid recursive lowering. Division by
 * zero parks the thread; signed overflow follows C's contract.
 */
#include "builtins/udivsi3.c"
#include "builtins/umodsi3.c"
#include "builtins/divsi3.c"
#include "builtins/modsi3.c"
#include "builtins/udivdi3.c"
#include "builtins/umoddi3.c"
#include "builtins/divdi3.c"
#include "builtins/moddi3.c"
#include "builtins/ashldi3.c"
#include "builtins/lshrdi3.c"
#include "builtins/ashrdi3.c"
#include "builtins/muldi3.c"
#include "builtins/memcpy.c"
#include "builtins/memset.c"
#include "builtins/memmove.c"
#include "builtins/memcmp.c"
