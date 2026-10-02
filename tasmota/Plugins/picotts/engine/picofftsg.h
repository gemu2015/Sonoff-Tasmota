


























#ifndef PICOFFTSG_H_
#define PICOFFTSG_H_

#include "picoos.h"
#include "picodsp.h"





#define PICOFFTSG_FFTTYPE picoos_int32

extern void rdft(int n, int isgn, PICOFFTSG_FFTTYPE *a);
extern void dfct(int n, float *a, int VAL_SHIFT);
extern void dfct_nmf(int n, int *a);
extern float norm_result(int m2, PICOFFTSG_FFTTYPE *tmpX, PICOFFTSG_FFTTYPE *norm_window);



#endif
