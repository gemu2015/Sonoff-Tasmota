/* host test shim (variant "flt"): route every double transcendental the engine uses
 * through the single precision functions that the plugin jump table offers
 * (jsinf/jcosf/jsqrtf/jexpf, module_defines.h) - this is what a plugin build must do. */
#include <math.h>
#define sqrt(x) ((double)sqrtf((float)(x)))
#define sin(x)  ((double)sinf((float)(x)))
#define cos(x)  ((double)cosf((float)(x)))
#define exp(x)  ((double)expf((float)(x)))
#define fabs(x) ((double)fabsf((float)(x)))
