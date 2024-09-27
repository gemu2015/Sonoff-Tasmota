// floating point intrinsics hook

// 36 bytes code about 60 bytes in total with vector
// 16 instructions 30 in total on esp32 == 4,166 ns * 30 = 130 ns addition takes 8,7 us = 15 % slower
MODULE_PART float __addsf3(float a, float b) {
SETMINREGS
  return fadd(a,b);
}

MODULE_PART float __subsf3(float a, float b) {
SETMINREGS
  return fdiff(a,b);
}

MODULE_PART float __mulsf3(float a, float b) {
SETMINREGS
  return fmul(a,b);
}

MODULE_PART float __divsf3(float a, float b) {
SETMINREGS
  return fdiv(a,b);
}

MODULE_PART void *_Znwj(uint32_t size) {
SETMINREGS
    return calloc(size, 1);
}

void  _ZdlPv(void* ptr) {
SETMINREGS
    free(ptr);
}

float __floatsisf(int32_t in) {
SETMINREGS
    return float_i32(in);
}

float __floatunsisf(uint32_t in) {
SETMINREGS
    return float_ui32(in);
}

float __floatundisf(uint64_t in) {
SETMINREGS
    return float_ui64(in);
}

int32_t __fixsfsi(float in) {
SETMINREGS
    return i32_float(in);
}

uint32_t __fixunssfsi(float in) {
SETMINREGS
    return ui32_float(in);
}



/* 36 bytes __addsf3
40205a30:	e0c112               	addi	a1, a1, -32
40205a33:	076102               	s32i	a0, a1, 28
40205a36:	0661c2               	s32i	a12, a1, 24
40205a39:	006132               	s32i	a3, a1, 0
40205a3c:	02cd                	mov.n	a12, a2
40205a3e:	fb6c05               	call0	40201100 <gettbl>
40205a41:	1228                	l32i.n	a2, a2, 4
40205a43:	0138                	l32i.n	a3, a1, 0
40205a45:	2b2242               	l32i	a4, a2, 172
40205a48:	0c2d                	mov.n	a2, a12
40205a4a:	0004c0               	callx0	a4
40205a4d:	7108                	l32i.n	a0, a1, 28
40205a4f:	61c8                	l32i.n	a12, a1, 24
40205a51:	20c112               	addi	a1, a1, 32
40205a54:	f00d                	ret.n
*/

// not yet in jumptable
MODULE_PART int __nesf2(float a, float b) {
SETMINREGS
  return 1;
}
