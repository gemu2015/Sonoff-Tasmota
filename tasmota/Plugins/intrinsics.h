// floating point and other intrinsics hook

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

MODULE_PART void  _ZdlPv(void* ptr) {
SETMINREGS
    free(ptr);
}

MODULE_PART float __floatsisf(int32_t in) {
SETMINREGS
    return float_i32(in);
}

MODULE_PART float __floatunsisf(uint32_t in) {
SETMINREGS
    return float_ui32(in);
}

MODULE_PART float __floatundisf(uint64_t in) {
SETMINREGS
    return float_ui64(in);
}

MODULE_PART int32_t __fixsfsi(float in) {
SETMINREGS
    return i32_float(in);
}

MODULE_PART uint32_t __fixunssfsi(float in) {
SETMINREGS
    return ui32_float(in);
}

// a > b
MODULE_PART int __gtsf2 (float a, float b) {
SETMINREGS
  return jgtsf2(a, b);
}

// a >= b
MODULE_PART int __gesf2 (float a, float b) {
SETMINREGS
  return jgtsf2(a, b) | jeqsf2(a, b);
}

// a < b
MODULE_PART int __ltsf2 (float a, float b) {
SETMINREGS
  return jltsf2(a, b);
}

// a <= b
MODULE_PART int __lesf2 (float a, float b) {
SETMINREGS
  return jltsf2(a, b) | jeqsf2(a, b);
}

// a == b
MODULE_PART int __eqsf2 (float a, float b) {
SETMINREGS
  return jeqsf2(a, b);
}

// a != b
MODULE_PART int __nesf2(float a, float b) {
SETMINREGS
  return !jeqsf2(a, b);
}

MODULE_PART int __unordsf2 (float a, float b) {
SETMINREGS
  return jisnan(a) | jisnan(b);
}





/* 36 bytes __addsf3 example code
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

#ifdef USE_BP_DOUBLES

MODULE_PART double __floattidf(int64_t in) {
SETMINREGS
  return p__floattidf(in);
}

MODULE_PART double __floatuntidf(uint64_t in) {
SETMINREGS
  return p__floatuntidf(in);
}

MODULE_PART double __floatsidf(int32_t in) {
SETMINREGS
  return p__floatsidf(in);
}

MODULE_PART double __floatunsidf(uint32_t in) {
SETMINREGS
  return p__floatunsidf(in);
}

MODULE_PART int32_t __fixdfdi(double in) {
SETMINREGS
  return p__fixdfdi(in);
}

MODULE_PART uint32_t __fixunsdfsi(double in) {
SETMINREGS
  return p__fixunsdfsi(in);
}

MODULE_PART int64_t __fixdfti(double in) {
SETMINREGS
  return i642d(in);
}

MODULE_PART double __extendsfdf2(float in) {
SETMINREGS
  return p__extendsfdf2(in);
}

MODULE_PART double __adddf3(double a, double b) {
SETMINREGS
  return dadd(a,b);
}

MODULE_PART double __subdf3(double a, double b) {
SETMINREGS
  return ddiff(a,b);
}

MODULE_PART double __muldf3(double a, double b) {
SETMINREGS
  return dmul(a,b);
}

MODULE_PART double __divdf3(double a, double b) {
SETMINREGS
  return ddiv(a,b);
}


// a < b
MODULE_PART int __ltdf2(double a, double b) {
SETMINREGS
  return double_cdispatch(0,a,b);
}

// a > b
MODULE_PART int __gtdf2(double a, double b) {
SETMINREGS
  return double_cdispatch(2,a,b);
}

// a != b
MODULE_PART int __nedf2(double a, double b) {
SETMINREGS
  return double_cdispatch(1,a,b);
}

// a == b
MODULE_PART int __eqdf2(double a, double b) {
SETMINREGS
  return double_cdispatch(4,a,b);
}

#endif // USE_BP_DOUBLES

//#define USE_BP_TWOWIRE

#ifdef USE_BP_TWOWIRE

// i2c class
MODULE_PART void TWI_beginTransmission(TwoWire *mp, uint8_t addr) {
SETMINREGS 
  jbeginTransmission(mp, addr);
}

MODULE_PART void TWI_endTransmission(TwoWire *mp, uint8_t sendstop) {
SETMINREGS
  jendTransmission(mp, sendstop);
}

MODULE_PART uint8_t TWI_requestFrom(TwoWire *mp, uint8_t address, uint8_t quantity) {
SETMINREGS
  return jrequestFrom(mp, address, quantity);
}

/* virtuals go via vectors anyhow
MODULE_PART size_t TWI_write(TwoWire *mp, uint8_t data) {
SETMINREGS
  return jwrite(mp, data);
}

MODULE_PART int TWI_read(TwoWire *mp) {
SETMINREGS
  return jread(mp);
}
*/

#endif


// 744 bytes overhead with double, 216 without double

