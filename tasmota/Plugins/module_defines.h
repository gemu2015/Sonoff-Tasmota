
typedef struct {
  bool          grpflg;
  bool          usridx;
  uint16_t      command_code;
  uint32_t      index;
  uint32_t      data_len;
  int32_t       payload;
  char         *topic;
  char         *data;
  char         *command;
}XdrvMailbox;

extern void AddLog(uint32_t loglevel, PGM_P formatP, ...);

#ifdef ESP8266
#define portMUX_TYPE void
#endif

// vector table calls
#define jWire                           ( TwoWire*)                                    jt[0]
#define jWire1                          ( TwoWire*)                                    jt[1]
#define jSerial                         ( HardwareSerial*)                             jt[2]
#define jI2cSetDevice(ADDR,BUS)         (( bool (*)(uint32_t,uint32_t) )               jt[3])(ADDR,BUS)
#define jI2cSetActiveFound(A,B,C)       (( void (*)(uint32_t,const char *, uint32_t) ) jt[4])(A,B,C)
#define jAddLog(A,...)                  (( void (*)(uint32_t,const char *, ...) )      jt[5])(A,##__VA_ARGS__)
#define jResponseAppend_P(A,...)        (( void (*)(const char *, ...) )               jt[6])(A,##__VA_ARGS__)
#define jWSContentSend_PD(A,...)        (( void (*)(const char *, ...) )               jt[7])(A,##__VA_ARGS__)
#define jftostrfd(A,B,C)                (( char *(*)(float, uint8_t, char*) )          jt[8])(A,B,C)
#define jcalloc(A,B)                    (( void *(*)(size_t, size_t) )                 jt[9])(A,B)
// 10
#define jfscale(A,B,C)                  (( float (*)(int32_t, float, float) )          jt[10])(A,B,C)
#define sprint(A)                       (( void (*)(const char*) )                     jt[11])(A)
#define jbeginTransmission(BUS,ADDR)    (( void (*)(TwoWire*,uint8_t) )                jt[12])(BUS,ADDR)
#define jwrite(BUS,VAL)                 (( size_t (*)(TwoWire*,uint8_t) )              jt[13])(BUS,VAL)
#define jendTransmission(BUS,VAL)       (( uint8_t (*)(TwoWire*,bool) )                jt[14])(BUS,VAL)
#define jrequestFrom(BUS,ADDR,NUM)      (( size_t (*)(TwoWire*,uint8_t,size_t) )       jt[15])(BUS,ADDR,NUM)
#define jread(BUS)                      (( int (*)(TwoWire*) )                         jt[16])(BUS)
#define fshowhex(VAL)                   (( void (*)(uint32_t) )                        jt[17])(VAL)
#define jfree(MEM)                      (( void (*)(void*) )                           jt[18])(MEM)
#define jI2cWrite16(ADDR,REG,VAL,BUS)   (( bool (*)(uint8_t, uint8_t, uint16_t, uint8_t) )      jt[19])(ADDR,REG,VAL,BUS)
// 20
#define jI2cRead16(ADDR,REG,BUS)        (( uint16_t (*)(uint8_t, uint8_t, uint8_t) )  jt[20])(ADDR,REG,BUS)
#define jI2cValidRead16(DATA,ADDR,REG,BUS)  (( bool (*)(uint16_t *,uint8_t,uint8_t,uint8_t) )   jt[21])(DATA,ADDR,REG,BUS)
#define jsnprintf_P(A,B,C,...)          (( void (*)(char *,size_t,const char *,...) )  jt[22])(A,B,C,##__VA_ARGS__)
#define jXdrvRulesProcess(A)            (( bool (*)(bool) )                            jt[23])(A)
#define jResponseJsonEnd                (( void (*)(void) )                            jt[24])
#define jdelay(A)                       (( void (*)(uint32_t) )                        jt[25])(A)
#define jI2cActive(A)                   (( bool (*)(uint32_t) )                        jt[26])(A)
#define jResponseJsonEndEnd             (( void (*)(void) )                            jt[27])
#define jIndexSeparator                 (( char (*)(void) )                            jt[28])
#define jResponse_P(A,...)                (( int (*)(const char * formatP, ...) )      jt[29])(A,##__VA_ARGS__)
// 30
#define jI2cResetActive(REG,BUS)        (( void (*)(uint32_t, uint32_t) )              jt[30])(REG,BUS)
#define jisnan(FVAL)                    (( bool (*)(float) )                           jt[31])(FVAL)
#define jConvertTemp(FVAL)              (( float (*)(float) )                          jt[32])(FVAL)
#define jConvertHumidity(FVAL)          (( float (*)(float) )                          jt[33])(FVAL)
#define jTempHumDewShow(JSON,PASS,TYPES,TEMP,HUM)(( bool (*)(bool,bool,const char *,float,float) ) jt[34])(JSON,PASS,TYPES,TEMP,HUM)
#define jstrlcpy(DST,SRC,SIZE)          (( size_t (*)(char *,const char *,size_t) )                jt[35])(DST,SRC,SIZE)
#define jGetTextIndexed(DST,DSIZE,INDEX,HSTCK)(( char *(*)(char*,size_t,uint32_t,const char*) )    jt[36])(DST,DSIZE,INDEX,HSTCK)
#define JGetTasmotaGlobal(SEL)          ((uint32_t (*)(uint32_t) )                     jt[37])(SEL)
#define jiseq(FVAL)                     (( bool (*)(float) )                           jt[38])(FVAL)
#define jfdiv(P1,P2)                    (( float (*)(float,float) )                    jt[39])(P1,P2)
// 40
#define jfmul(P1,P2)                    (( float (*)(float,float) )                    jt[40])(P1,P2)
#define jfdiff(P1,P2)                   (( float (*)(float,float) )                    jt[41])(P1,P2)
#define jtofloat(P1)                    (( float (*)(uint64_t) )                       jt[42])(P1)
#define jfadd(P1,P2)                    (( float (*)(float,float) )                    jt[43])(P1,P2)
#define jI2cRead8(ADDR,REG)             (( uint8_t (*)(uint8_t,uint8_t) )              jt[44])(ADDR,REG)
#define jI2cWrite8(ADDR,REG,VAL)        (( bool (*)(uint8_t,uint8_t,uint8_t) )         jt[45])(ADDR,REG,VAL)
#define javailable(WIRE)                (( uint8_t (*)(TwoWire*) )                     jt[46])(WIRE)
#define jAddLogMissed(SENS,MISS)        (( void (*)(const char*,uint32_t) )            jt[47])(SENS,MISS)
#define jNAN                            (( float (*)(void) )                           jt[48])()
#define jgtsf2(P1,P2)                   (( bool (*)(float,float) )                     jt[49])(P1,P2)
// 50
#define jltsf2(P1,P2)                   (( bool (*)(float,float) )                     jt[50])(P1,P2)
#define jeqsf2(P1,P2)                   (( bool (*)(float,float) )                     jt[51])(P1,P2)
#define jPin(PIN,INDEX)                 (( int (*)(uint32_t,uint32_t) )                jt[52])(PIN,INDEX)
#define jnewTS(RPIN,TPIN)               (( void* (*)(int32_t,int32_t) )                jt[53])(RPIN,TPIN)
#define jwriteTS(TSER,BUF,SIZE)         (( size_t (*)(void*,uint8_t*,uint32_t) )       jt[54])(TSER,BUF,SIZE)
#define jflushTS(TSER)                  (( void (*)(void*) )                           jt[55])(TSER)
#define jbeginTS(TSER,BAUD)             (( int (*)(void*,uint32_t) )                   jt[56])(TSER,BAUD)
#define jXdrvMailbox                    ((XdrvMailbox*)                                 jt[57])
#define jGetCommandCode(DST,DSIZE,NEEDLE,HSTCK)(( int (*)(char*,size_t,const char*,const char*) )    jt[58])(DST,DSIZE,NEEDLE,HSTCK)
#define jstrlen(STR)                    (( uint32_t (*)(char*) )                       jt[59])(STR)
// 60
#define jstrncasecmp_P(S1,S2,SIZE)      (( int (*)(const char*,const char *, size_t) ) jt[60])(S1,S2,SIZE)
#define jtoupper(CHAR)                  (( int (*)( int c ) )                          jt[61])(CHAR)
#define jiscale(A,B,C)                  (( int32_t (*)(int32_t, int32_t, int32_t) )    jt[62])(A,B,C)
#define jdeleteTS(TSER)                 (( void (*)(void*) )                           jt[63])(TSER)
#define jreadTS(TSER,BUF,SIZE)          (( size_t (*)(void*,uint8_t*,uint32_t) )       jt[64])(TSER,BUF,SIZE)
#define jread1TS(TSER)                  (( int (*)(void*) )                            jt[65])(TSER)
#define javailTS(TSER)                  (( uint8_t (*)(void*) )                        jt[66])(TSER)
#define jMqttPublishTeleSensor          (( void (*)(void) )                            jt[67])
#define jstrtoul(A,B,C)                 (( uint32_t (*)(const char *,char **, int) )   jt[68])(A,B,C)
#define jAddLogBuffer(A,B,C)            (( void (*)(uint32_t,uint8_t*, uint32_t) )     jt[69])(A,B,C)
// 70
#define jResponseTime_P(A,...)            (( int (*)(const char*, ...) )                 jt[70])(A,##__VA_ARGS__)
#define jClaimSerial                    (( void (*)(void) )                            jt[71])
#define jhardwareSerial(TSER)           (( bool (*)(void*) )                           jt[72])(TSER)
#define jmillis                         (( uint32_t (*)(void) )                        jt[73])
#define jsprintf_P(A,B,...)             (( void (*)(char*,const char * formatP,... ) ) jt[74])(A,B,##__VA_ARGS__)
#define jAddlogT(TXT)                   (( void (*)(char*) )                           jt[75])(TXT)
#define jtmod__divsi3(A,B)              (( int32_t (*)(int32_t,int32_t) )              jt[76])(A,B)
#define jtmod__udivsi3(A,B)             (( uint32_t (*)(uint32_t,uint32_t) )           jt[77])(A,B)
#define jtmod__floatsisf(A)             (( float (*)(int32_t) )                        jt[78])(A)
#define jtmod__floatunsisf(A)           (( float (*)(uint32_t) )                       jt[79])(A)
// 80
#define jFastPrecisePowf(A,B)           (( float (*)(float, float) )                   jt[80])(A,B)
#define JGetTasmotaGf(SEL)              (( float (*)(uint32_t) )                       jt[81])(SEL)
#define jtmod__muldi3(A,B)              (( int64_t (*)(int64_t,int64_t) )              jt[82])(A,B)
#define jtmod__fixunssfsi(A)            (( uint32_t (*)(float) )                       jt[83])(A)
#define jtmod__umodsi3(A,B)             (( uint32_t (*)(uint32_t,uint32_t) )           jt[84])(A,B)
#define jtwi_readFrom(A,B,C,D)(( unsigned char (*)(uint8_t,uint8_t*,unsigned int,uint8_t) ) jt[85])(A,B,C,D)
#ifdef ESP8266
#define jDecodeCommand(A,B,C)           (( bool (*)(const char*, void (* const x[])(void),MODULES_TABLE* )) jt[86])(A,B,C)
#else
#define jDecodeCommand(A,B,C)           (( bool (*)(const char*, void (* const x[])(void),volatile MODULES_TABLE* )) jt[86])(A,B,C)
#endif

#define jResponseCmndDone               (( void (*)(void) )                            jt[87])
#define jbwriteTS(TSER,VAL)             (( size_t (*)(void*,uint8_t) )                 jt[88])(TSER,VAL)
#define jmemcmp(A,B,SIZE)               (( int (*)(const void*,const void*,int) )      jt[89])(A,B,SIZE)
#define jToHex_P(A,B,C,D,E)             (( char* (*)(const unsigned char *, size_t, char *, size_t, char) ) jt[90])(A,B,C,D,E)
#define jmemset(A,B,C)                  (( void* (*)(void *,int,size_t) )              jt[91])(A,B,C)
#define jmemmove(A,B,C)                 (( void* (*)(void *,const void *,size_t) )     jt[92])(A,B,C)
#define jResponseCmndNumber(A)          (( void (*)(int))                              jt[93])(A)
#define jResponseCmndFloat(A,B)         (( void (*)(float,uint32_t))                   jt[94])(A,B)
#define jResponseAppendTHD(A,B)         (( int (*)(float,float))                       jt[95])(A,B)
#define jWSContentSend_THD(A,B,C)       (( void (*)(const char *,float,float))         jt[96])(A,B,C)
#define jstrncpy(A,B,C)                 (( char *(*)(char *, const char *, size_t) )   jt[97])(A,B,C)   
#define jisprint(A)                     (( int (*)(int) )                              jt[98])(A)
#define jisinf(A)                       (( bool (*)(float) )                           jt[99])(A)
#define jcopyStr(A)                     (( char *(*)(const char *) )                   jt[100])(A)
#define jsetClockStretchLimit(BUS,A)    (( void (*)(TwoWire*,uint32_t) )               jt[101])(BUS,A)
#define jwriten(BUS,BUF,LEN)            (( void (*)(TwoWire*,uint8_t*,uint32_t) )      jt[102])(BUS,BUF,LEN)
#define jmodff(A,B)                     (( float (*)(float,float*) )                   jt[103])(A,B)                     
#define jfl_const(A,B)                  (( float (*)(int32_t,int32_t) )                jt[104])(A,B) 
#define jWSContentSend_Temp(A,B)        (( void (*)(const char *, float) )             jt[105])(A,B)
#define jdelayMicroseconds(A)           (( void (*)(uint32_t) )                        jt[106])(A)
#define jdigitalRead(A)                 (( int (*)(uint8_t) )                          jt[107])(A)
#define jdigitalWrite(A,B)              (( void (*)(uint8_t, uint8_t) )                jt[108])(A,B)
#define jpinMode(A,B)                   (( void (*)(uint8_t, uint8_t) )                jt[109])(A,B)
#define jstrchr(A,B)                    (( char *(*)(char *, char) )                   jt[110])(A,B)
#define jtrimm(A)                       (( char *(*)(char *) )                         jt[111])(A)
#define jvTaskEnterCritical(A)          (( void (*)(portMUX_TYPE *) )                  jt[112])(A)
#define jvTaskExitCritical(A)           (( void (*)(portMUX_TYPE *) )                  jt[113])(A)
#define jdirectRead(A)                  (( uint32_t (*)(uint32_t) )                    jt[114])(A)
#define jdirectWriteLow(A)              (( void (*)(uint32_t) )                        jt[115])(A)
#define jdirectWriteHigh(A)             (( void (*)(uint32_t) )                        jt[116])(A)
#define jdirectModeInput(A)             (( void (*)(uint32_t) )                        jt[117])(A)
#define jdirectModeOutput(A)            (( void (*)(uint32_t) )                        jt[118])(A)
#define jCalcTempHumToAbsHum(A,B)       (( float (*)(float,float) )                    jt[119])(A,B)
#define jWSContentSend_P(A,...)         (( void (*)(const char *, ...) )               jt[120])(A,##__VA_ARGS__)
#define jHttpCheckPriviledgedAccess(A)  (( bool (*)(void) )                            jt[121])
#define jWSContentStart_P(A)            (( void (*)(const char *) )                    jt[122])(A)
#define jWSContentSendStyle             (( void (*)(void) )                            jt[123])
#define jWSContentSpaceButton(A,B)      (( void (*)(uint32_t,bool) )                   jt[124])(A,B)
#define jWSContentStop                  (( void (*)(void) )                            jt[125])
#define jWebGetArg(A,B,C)               (( void (*)(const char*,char*,size_t) )        jt[126])(A,B,C)
#define jWebRestart(A)                  (( void (*)(uint32_t) )                        jt[127])(A)
#define jWebServer_hasArg(A)            (( bool (*)(const char *) )                    jt[128])(A)
#define jWebServer_on(A,B,C)          (( void (*)(const char *, void (*)(void),uint8_t) ) jt[129])(A,B,C)
#define jatoi(A)                        (( int (*)(const char *) )                     jt[130])(A)
#define jstrcpy_P(A,B)                  (( char *(*)(char *, const char *) )           jt[131])(A,B)
#define SetTasmotaGlobal(A,B)           (( void (*)(uint32_t,uint32_t) )               jt[132])(A,B)
#define fixsfti(A)                      (( int32_t (*)(float) )                        jt[133])(A)
#define gtgtbl                          (( void *(*)(void) )                           jt[134])
#define asettings                       ( SETTINGS **)                                 jt[135]

// Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#ifndef bitWrite
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#endif
#define fldsiz(name, field) (sizeof(((name *)0)->field))

#ifdef ESP8266
#define PLUGIN_CODE_TEXT
#endif

// essential defines -----------------------------------------------------------------------
// linker sections
#ifdef PLUGIN_CODE_TEXT
#define SECTION_DESC ".text.mod_desc"
#define SECTION_STRING ".text.mod_string"
#define SECTION_PART ".text.mod_part"
#define SECTION_END ".text.mod_end"
#else
#define SECTION_DESC ".plugin.mod_desc"
#define SECTION_STRING ".plugin.mod_string"
#define SECTION_PART ".plugin.mod_part"
#define SECTION_END ".plugin.mod_end"
#endif
//KEEP (*(SORT(.text.mod.*)))


#ifndef MODULE_HEADER
#define MODULE_HEADER module_header
#endif

#define MODULE_FUNCTION_EXECUTE mod_func_execute
#define END_OF_MODULE end_of_module

//#define MODULE_DESC __attribute__((section(SECTION_DESC))) extern const FLASH_MODULE
#ifdef ESP32
//#define MODULE_PART __attribute__( (section(SECTION_PART),aligned(4))) 
#define MODULE_PART __attribute__( (section(SECTION_PART),aligned(4))) __attribute__( (optimize("no-stack-protector")) ) 
#define MODULE_END __attribute__((section(SECTION_END),aligned(4))) static void  END_OF_MODULE(void) {__asm__ __volatile__(".align 4\n.word 0x4AFCAA55");}
#else
#define MODULE_PART __attribute__((section(SECTION_PART)))
#define MODULE_END __attribute__((section(SECTION_END))) static void  END_OF_MODULE(void) {__asm__ __volatile__(".word 0x4AFCAA55");}
#endif


#ifdef ESP32
#ifdef __riscv
#undef MODULE_PSTART

#define MODULE_PSTART
 //   _Pragma("GCC options push") \
 //    _Pragma("GCC optimize ("-O1")")

#undef MODULE_PEND
#define MODULE_PEND
  //  _Pragma("GCC options pop") \

#else
#undef MODULE_PSTART
#define MODULE_PSTART
#undef MODULE_PEND
#define MODULE_PEND
#endif
#endif

#ifdef ESP8266
#undef MODULE_PSTART
#define MODULE_PSTART
#undef MODULE_PEND
#define MODULE_PEND
#endif


// #pragma GCC optimize ("-fno-stack-protector")

//redefine_extname oldname newname
//#pragma redefine_extname myroutine __fixed_myroutine

//#define GET_MTABLE static uint32_t  GetmTbl(void) {
//  return 0x12345678;
//}

//#pragma GCC push_options
//#pragma GCC optimize ("-Og")
//#pragma GCC optimize ("-O3")
//#pragma GCC optimize ("-fno-stack-protector")
//#pragma GCC pop_options

/*
xtensa-esp32-elf-objdump -d ./.pio/build/tasmota32-4M/firmware.elf >dissasm.txt
riscv32-esp-elf-objdump -d ./.pio/build/tasmota32c3-4M/firmware.elf >dissasm.txt

riscv_save - riscv_restore

gcc -Q --help=optimizers
gcc -Q --help=target

riscv32-esp-elf-gcc -Q -O0 --help=optimizers >opts_0
riscv32-esp-elf-gcc -Q -O3 --help=optimizers >opts_3
diff opts_0 opts_3 | grep enabled

*/
/*
__asm__  (\
  ".section .text.mod_part\n"\
  ".literal .xyz, 9600\n"\
  ".align 4\n"\
  "l32r	a2, .xyz	#,\n"\
  "ret.n\n"\
);\
};
*/


typedef struct { 
  uint16_t *tele_period;
  uint32_t *global_update;
  float *temperature_celsius;
  float *humidity;
  uint32_t *uptime;
  power_t *rel_inverted;
  uint8_t *devices_present;
} GTBL;

#define STGLOB  GTBL *tgbl = (GTBL*) gtgtbl();

#define TasmotaGlobal  *tgbl

//#define PROGMEM  __attribute__((section(".irom.text")))
#undef PROGMEM
#ifdef PLUGIN_CODE_TEXT
#define PROGMEM  __attribute__((section(".text.mod_string"),aligned(4)))
#else
#define PROGMEM  __attribute__((section(".plugin.mod_string"),aligned(4)))
#endif


//#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[EXEC_OFFSET];}))
#define GSTR(LABEL) (const char *)LABEL+EXEC_OFFSET
#define GU8(LABEL) (const uint8_t *)LABEL+EXEC_OFFSET
#define GFLT(LABEL) (float *) ((char *)LABEL+EXEC_OFFSET)

// all floating point constants must be in progmem and named FP_CONST
#define FLTC(INDEX) *(float *) ((char *)&FP_CONST[INDEX]+EXEC_OFFSET)


//#define VTABLE(A) void (*const A[])(MODULES_TABLE*) PROGMEM
#define VTABLE(A) void (*const A[])(void) PROGMEM

#define GVT(LABEL) ( void (**)(MODULES_TABLE*) ) ((char *)LABEL+EXEC_OFFSET)

#define FSTRING(A) const char A[] PROGMEM 
#define FU8ARRAY(A) const uint8_t A[] PROGMEM 
#define GU8A(LABEL) (const uint8_t *)LABEL+EXEC_OFFSET

#define PARRAY(A,...) (__extension__({static const unsigned char __c[] PROGMEM = {A,...}; &__c[EXEC_OFFSET];}))

#define GFB(A)  pgm_read_byte(&A[EXEC_OFFSET])

extern "C" { MODULES_TABLE *gettbl(void); };


//extern "C" {  const uint32_t xmodule_end;}
/*
__asm__  (\
  ".section .text.mod_end\n"\
  ".align 4\n"\
  ".global xmodule_end\n"\
  "xmodule_end:"\
  ".word 0x4AFCAA55"
);
*/

#ifdef ESP32
//#if 0
extern const FLASH_MODULE module_header;
MODULE_PART MODULES_TABLE *gettbl();

//MODULES_TABLE *gettbl() {
//  return (MODULES_TABLE*)*(uint32_t*)GLOB_MOD_REG;
//}

  //const FLASH_MODULE *mh = &module_header;
  //return (MODULES_TABLE*)mh->mtv;
  //return (MODULES_TABLE*)*((uint32_t*)&module_header+12);
  //{__asm__ __volatile__("l32r	a2, module_header + 48"); };
  //{__asm__ __volatile__(".align 4");}
  //{__asm__ __volatile__("entry a1,32");}
  //{__asm__ __volatile__("l32r	a2, module_header+48");}
  //{__asm__ __volatile__("retw.n");}



/*
{__asm__ __volatile__(".align 4");}
{__asm__ __volatile__(".global gettbl");}
{__asm__ __volatile__(".type   gettbl,@function");}
{__asm__ __volatile__(".section .plugin.mod_part");}
{__asm__ __volatile__(".align 4");}
{__asm__ __volatile__("gettbl:");}
{__asm__ __volatile__("entry a1,32");}
{__asm__ __volatile__("l32r	a2, module_header+48");}
{__asm__ __volatile__("retw.n");}
*/
#endif

extern "C" {
 extern void (* const MODULE_JUMPTABLE[])(void);
}
extern MODULES_TABLE modules[];

// counter 7 config 2  R/W = 0x3FF5705C

#ifdef ESP32
// esp32
#ifdef __riscv
#undef GET_MTBL
//#define GET_MTBL volatile MODULES_TABLE *mt = (MODULES_TABLE*)*(uint32_t*)GLOB_MOD_REG;
#define GET_MTBL volatile MODULES_TABLE *mt = gettbl()
#else
#undef GET_MTBL
#define GET_MTBL volatile MODULES_TABLE *mt = gettbl()
#endif

#undef GET_JT
#define GET_JT void (* const *jt)() = mt->jt

#else
// esp8266
#undef GET_MTBL
#define GET_MTBL MODULES_TABLE *mt = gettbl()
#undef GET_JT
#define GET_JT void (* const *jt)() = mt->jt
#endif

#define SETREGS GET_MTBL; MODULE_MEMORY *mem = (MODULE_MEMORY*)mt->mod_memory;GET_JT;FLASH_MODULE *mp = (FLASH_MODULE*)mt->mod_addr;SETTINGS *jsettings = *asettings;
#define ALLOCMEM GET_MTBL; GET_JT; mt->mem_size = sizeof(MODULE_MEMORY);mt->mem_size += mt->mem_size % 4;mt->mod_memory = jcalloc(mt->mem_size / 4, 4);if (!mt->mod_memory) {return -1;};MODULE_MEMORY *mem = (MODULE_MEMORY*)mt->mod_memory;SETTINGS *jsettings = *asettings;FLASH_MODULE *mp = (FLASH_MODULE*)mt->mod_addr;
#define RETMEM if (mt->mem_size) {jfree(mt->mod_memory);mt->mem_size = 0;}
#define MODULE_DESCRIPTOR(NAME,TYPE,REV,GPIO1,PIN1,GPIO2,PIN2,GPIO3,PIN3,GPIO4,PIN4)  __attribute__((section(SECTION_DESC))) extern const FLASH_MODULE MODULE_HEADER = {MODULE_SYNC,CURR_ARCH,(TYPE),(REV),(NAME),mod_func_execute,END_OF_MODULE,0,0,(uint32_t)&modules,(uint32_t)&MODULE_JUMPTABLE,{GPIO1,PIN1,GPIO2,PIN2,GPIO3,PIN3,GPIO4,PIN4}};
#define MOD_FUNC(A, ...) A(MODULES_TABLE *mt, ##__VA_ARGS__)
//#define MOD_FUNC(A, ...) A(##__VA_ARGS__)

#define CALL_MOD_FUNC(A, ...) A(mt, ##__VA_ARGS__)


#define MOD_RESULT int32_t

#define STRBUFFER


    
/*

//#pragma GCC optimize ("O0")

 // TwoWire *wire;
//#define wire mem->wire
 // INITWIRE(wire)

 // wire->xbeginTransmission(0xaa);
 // wire->xwrite(0xaa);
 // wire->xendTransmission(false);




MODULE_PART void MOD_FUNC(cmd1);
MODULE_PART void MOD_FUNC(cmd2);

void MOD_FUNC(cmd1) {
  SETREGS
 AddLog(LOG_LEVEL_INFO,PSTR("cmd 1"));
 ResponseCmndDone();
}

void MOD_FUNC(cmd2) {
  SETREGS
  AddLog(LOG_LEVEL_INFO,PSTR("cmd 2"));
  ResponseCmndDone();
}

const char ksps30Commands[] PROGMEM = "mlx|start|stop";
VTABLE(ksps30Command) = {&cmd1, &cmd2};

case FUNC_COMMAND:
      result = DecodeCommand(GSTR(ksps30Commands), GVT(ksps30Command));
      break;
      
*/


/*
#define DATAMEM  __attribute__((section(".text.mod_table"),aligned(4)))
#define DPSTR(LABEL,TEXT) extern "C" {  const char *LABEL(void);} __asm__  (\
  ".section .text.mod_string\n"\
  ".align 4\n"\
  ".global " #LABEL "\n"\
  #LABEL": .asciz "#TEXT" \n"\
);
#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(lbl_, a)
#define UNAME LABEL_(__LINE__)
#define SUNAME "UNAME"
#define GPSTR(VAR,FUNC) const char *VAR = (const char*)&FUNC + EXEC_OFFSET; fshowhex((uint32_t)VAR);
//#define jPSTR(LABEL) (__extension__({ (const char *)&LABEL[0]+EXEC_OFFSET;}))
#define CAT2(a,b) a##b
#define CAT(a,b) CAT2(a,b)
#define UNIQUE_ID CAT(_uid_,__COUNTER__)

//#undef PSTR
// on esp8266 passing of PGMP strings works, on ESP32 fails and must be copied to ram buffer before passing pointer
// this implementation only supports one PSTR per call
#ifdef ESP8266
#define yPSTR(LABEL) (const char *)LABEL+EXEC_OFFSET
#define STRBUFFER
#else
#define yPSTR(LABEL) __extension__( {_copy32((uint32_t*)((const char *)LABEL+EXEC_OFFSET), mem->cbuffer); (const char *)mem->cbuffer;} )
#define STRBUFFER uint32_t cbuffer[STRBUFFSIZE];
#endif



#ifdef ESP32
uint32_t _strlen32(uint32_t *sp) {
  uint8_t len = 1;
  while (1) {
    uint32_t val = *sp++;
    if (!(val & 0xff000000)) break;
    if (!(val & 0x00ff0000)) break;
    if (!(val & 0x0000ff00)) break;
    if (!(val & 0x000000ff)) break;
    len++;
  };
  return len;
}

#define STRBUFFSIZE 32
void _copy32(uint32_t *src, uint32_t *dst) {
  uint8_t len = _strlen32(src);
  if (len > STRBUFFSIZE) len = STRBUFFSIZE;
  for (uint8_t cnt = 0; cnt < len; cnt++) {
    *dst++ = *src++;
  }
}
#endif

//#define SHIFT(cmd, bits) (((uint32_t)(cmd)) << (bits))
//#define PACK1(c1,...)          ( SHIFT(c1, 0) )
//#define PACK2(c1,c2,...)       ( SHIFT(c1, 8) | SHIFT(c2, 0) )
//#define PACK3(c1,c2,c3,...)    ( SHIFT(c1,16) | SHIFT(c2, 8) | SHIFT(c3,0) )
//#define PACK4(c1,c2,c3,c4,...) ( SHIFT(c1,24) | SHIFT(c2,16) | SHIFT(c3,8) | SHIFT(c4,0) )

//#define MODULE_SYNC_END __attribute__((section(".text.mod_end"))); __asm__ __volatile__ (".align 4");
//#define MODULE_STORAGE(IND,NAME,VALUE)  __attribute__((section(SECTION_DESC))) extern const MODULE_STORE storage[IND] = {NAME,VALUE};

*/

typedef struct {
  void (*xbeginTransmission)(uint8_t);
  uint8_t (*xendTransmission)(bool); 
  uint8_t (*xread)(); 
  void (*xwrite)(uint8_t);
  void (*xrequestFrom)(uint8_t,uint8_t);
}  xTwoWire;

#define INITWIRE(A) A->xbeginTransmission = ( void (*)(uint8_t) ) jt[12];A->xendTransmission = ( uint8_t (*)(bool) ) jt[14];A->xread = ( uint8_t (*)() ) jt[16];A->xwrite = ( void (*)(uint8_t) ) jt[13];A->xrequestFrom = ( void (*)(uint8_t,uint8_t) ) jt[15];


#define initialized mt->flags.initialized
#define TasmotaSerial  void
//#define TwoWire xTwoWire

#define   beginTransmission(ADDR) jbeginTransmission(mem->xWire, ADDR)
#define   write(CMD) jwrite(mem->xWire, CMD)
#define   endTransmission(BUS) jendTransmission(mem->xWire, BUS)
#define   requestFrom(ADDR,NUM)  jrequestFrom(mem->xWire, ADDR, NUM)
#define   read() jread(mem->xWire)
#define   I2cRead8 jI2cRead8
#define   I2cRead16 jI2cRead16
#define   I2cWrite16 jI2cWrite16
#define   I2cWrite8 jI2cWrite8
#define   delay jdelay
#define   available() javailable(mem->xWire)
#define   ConvertHumidity jConvertHumidity
#define   GetTextIndexed jGetTextIndexed
#define   I2cSetActiveFound jI2cSetActiveFound
#define   I2cActive jI2cActive
#define   AddLogMissed jAddLogMissed
#define   TempHumDewShow jTempHumDewShow
#define   GetTasmotaGlobal JGetTasmotaGlobal
#define   ConvertTemp jConvertTemp
#define   strlcpy jstrlcpy
#undef   snprintf_P
#define   snprintf_P jsnprintf_P
#define   TempHumDewShow jTempHumDewShow
#define   IndexSeparator jIndexSeparator
#define   ResponseAppend_P jResponseAppend_P
#define   Response_P jResponse_P
#define   ResponseJsonEndEnd jResponseJsonEndEnd
#define   ResponseJsonEnd jResponseJsonEnd
#define   XdrvRulesProcess jXdrvRulesProcess
#define   WSContentSend_PD jWSContentSend_PD
#define   WSContentSend_P jWSContentSend_P

//#define   WSContentSend_P(A,...) {char *xyz=jcopyStr(A); jWSContentSend_P(xyz,__VA_ARGS__); free(xyz);}

#define   I2cValidRead16 jI2cValidRead16
#define   I2cResetActive jI2cResetActive
#define   ftostrfd jftostrfd
#define   fscale jfscale
#define   I2cSetDevice jI2cSetDevice
#define   Pin jPin
#define   NewTS jnewTS
#define   writeTS jwriteTS
#define   flushTS jflushTS
#define   beginTS jbeginTS
#define   XdrvMailbox (jXdrvMailbox)
#define   GetCommandCode jGetCommandCode
#define   strlen jstrlen
#undef strncasecmp_P
#define   strncasecmp_P jstrncasecmp_P
#define   toupper jtoupper
#define   iscale jiscale
#define   deleteTS jdeleteTS
#define   readTS jreadTS
#define   readbTS jread1TS
#define   availTS javailTS
#define   IndexSeparator jIndexSeparator
#define   AddLog jAddLog
#define   MqttPublishTeleSensor jMqttPublishTeleSensor
#define   strtoul jstrtoul
#define   AddLogBuffer jAddLogBuffer
#define   ResponseTime_P jResponseTime_P
#define   ClaimSerial jClaimSerial
#define   hardwareSerial jhardwareSerial
#define   millis jmillis
#undef    sprintf_P
#define   sprintf_P jsprintf_P
#define   AddLogT jAddlogT
#define   tmod__divsi3 jtmod__divsi3
#define   tmod__udivsi3 jtmod__udivsi3
#define   tmod__floatsisf jtmod__floatsisf
#define   tmod__floatunsisf jtmod__floatunsisf
#define   FastPrecisePowf  jFastPrecisePowf
#define   isnan jisnan
#define   isinf jisinf
#define   copyStr jcopyStr
#define   tmod__mulsf3  jfmul
#define   tmod__divsf3  jfdiv
#define   tmod__addsf3  jfadd
#define   tmod__subsf3  jfdiff
#define   fadd  jfadd
#define   fdiff  jfdiff
#define   GetTasmotaGlobalf JGetTasmotaGlobalf
#define   tmod__muldi3 jtmod__muldi3
#define   tmod__fixunssfsi jtmod__fixunssfsi
#define   tmod__umodsi3 jtmod__umodsi3
#define   twi_readFrom jtwi_readFrom
#define   DecodeCommand(A,B) jDecodeCommand(A,B,mt)
#define   ResponseCmndDone jResponseCmndDone
#define   bwriteTS jbwriteTS
#define   memcmp jmemcmp
#undef memcmp_P
#define   memcmp_P jmemcmp
#define   ToHex_P(A,B,C,D) jToHex_P(A,B,C,D,'\0') 
#define   memset jmemset
#define   memmove jmemmove
#define   memmove_P jmemmove
#define   ResponseCmndNumber jResponseCmndNumber
#define   ResponseCmndFloat jResponseCmndFloat
#define   ResponseAppendTHD jResponseAppendTHD
#define   WSContentSend_THD jWSContentSend_THD
#undef memcpy_P
#define   memcpy_P jmemmove
#define   strncpy jstrncpy
#define   isprint jisprint
#define   setClockStretchLimit(VAL) jsetClockStretchLimit(mem->xWire, VAL)
#define   writen(BUF,LEN) jwriten(mem->xWire,BUF,LEN)
#define free jfree
#define modff jmodff
#define fl_const jfl_const
#define WSContentSend_Temp jWSContentSend_Temp
#define delayMicroseconds jdelayMicroseconds
#define digitalRead jdigitalRead
#define digitalWrite jdigitalWrite
#define pinMode jpinMode
#define sprintf jsprintf_P
#define Settings jsettings
#define strchr jstrchr
#define trimm jtrimm
#define vTaskEnterCritical jvTaskEnterCritical
#define vTaskExitCritical jvTaskExitCritical
#define directRead jdirectRead
#define directWriteLow jdirectWriteLow
#define directWriteHigh jdirectWriteHigh
#define directModeInput jdirectModeInput
#define directModeOutput jdirectModeOutput
#define CalcTempHumToAbsHum jCalcTempHumToAbsHum
#define tofloat jtofloat


#define fdiv jfdiv
#define iseq jiseq
#define fmul jfmul
#define fixunssfsi tmod__fixunssfsi
#define ltsf2 jltsf2
#define gtsf2 jgtsf2
#define floatunsisf jtmod__floatunsisf
#define udivsi3 jtmod__udivsi3
#define HttpCheckPriviledgedAccess jHttpCheckPriviledgedAccess
#define WSContentStart_P jWSContentStart_P
#define WebServer jWebServer
#define WSContentSendStyle jWSContentSendStyle
#define WSContentSpaceButton jWSContentSpaceButton
#define WSContentStop jWSContentStop
#define WebGetArg jWebGetArg
#define WebRestart jWebRestart
#define WebServer_hasArg jWebServer_hasArg
#define WebServer_on(A,B) jWebServer_on(A,(void (*)(void)) ((uint32_t)B + EXEC_OFFSET),HTTP_ANY)
#define atoi jatoi
#undef strcpy_P
#define strcpy_P jstrcpy_P


#define FPC(A,B) jfl_const(A,B)

// floating point constants must be defined here
#ifdef __riscv
#define FPC_n999 jfl_const(-999,1)
#define FPC_0x01 jfl_const(1,100)
#define FPC_0x02 jfl_const(2,100)
#define FPC_273x15 jfl_const(27315,100)
#define FPC_0x00097656 jfl_const(97656,100000000)
#define FPC_0 jfl_const(0,0)
#else
#define FPC_n999 -999
#define FPC_0x01 0.01
#define FPC_0x02 0.02
#define FPC_273x15 273.15
#define FPC_0x00097656 0.00097656
#define FPC_0 jfl_const(0,0)

#endif

// floating point zero is always global symbol on esp32 
//FPC_0


// tensilica immediate is only -2048 to 2047
// all others must be coded with ICONST

#ifdef ESP8266
#define ICONST(A) A
#else
#ifdef __riscv
#define ICONST(A) A
#else
//#define ICONST(A) fixunssfsi(A)
#define ICONST(A) fixsfti(A)
#endif
#endif

#define SETWIRE(A) if (A==0) {mem->xWire = jWire;} else {mem->xWire = jWire1;} 

#ifdef __riscv
#define PUSH_OPTIONS _Pragma("GCC push_options")\
_Pragma("GCC optimize (\"-Og\")")
#define PULL_OPTIONS _Pragma("GCC pop_options")
#else
#ifdef ESP32
#define PUSH_OPTIONS _Pragma("GCC push_options")\
_Pragma("GCC optimize (\"-Og\")")
#define PULL_OPTIONS _Pragma("GCC pop_options")
#else
#define PUSH_OPTIONS
#define PULL_OPTIONS
#endif
#endif

//#pragma GCC optimize ("Og")

/*
#define PUSH_OPTIONS \
#ifdef __riscv \
#pragma GCC push_options \
#pragma GCC optimize ("-Og") \
#endif
*/

/*
#define RENAME_LIBRARY(GCC_NAME, AEABI_NAME)		\
  __asm__ (".globl\t__aeabi_" #AEABI_NAME "\n"		\
	   RENAME_LIBRARY_SET "\t__aeabi_" #AEABI_NAME 	\
	     ", __" #GCC_NAME "\n");
       */

#if 1
#define RENAME_LIBRARY_SET ".set"
#define RENAME_LIBRARY(GCC_NAME, AEABI_NAME)		\
  __asm__ (".globl\t__" #AEABI_NAME "\n"		\
	   RENAME_LIBRARY_SET "\t__" #AEABI_NAME 	\
	     ", __" #GCC_NAME "\n");

#else
#define RENAME_LIBRARY(GCC_NAME, AEABI_NAME)			\
  __asm__ (".globl\t__c6xabi_" #AEABI_NAME "\n"		\
	   ".set\t__c6xabi_" #AEABI_NAME			\
	   ", __gnu_" #GCC_NAME "\n");
#endif

#define DECLARE_LIBRARY_RENAMES RENAME_LIBRARY (__muldf3, murks)


//RENAME_LIBRARY (j_mulsf3, mulsf3)


/*
float my_mulsf3(float a, float b) {
  void (* const *jt)() = gettbl()->jt;
  return jfmul(a,b);
}
*/


/*
  TwoWire xwire;
  void (TwoWire::*pwire)();
  pwire = &TwoWire::begin;
  (xwire.*pwire)();
  */

//@code{DECLARE_LIBRARY_RENAMES} macro
//(@pxref{Library Calls}

/*
jnewTS(RPIN,TPIN)               (( void* (*)(int32_t,int32_t) )                jt[53])(RPIN,TPIN)
#define jwriteTS(TSER,BUF,SIZE)         (( size_t (*)(void*,uint8_t*,uint32_t) )       jt[54])(TSER,BUF,SIZE)
#define jflushTS(TSER)                  (( void (*)(void*) )                           jt[55])(TSER)
#define jbeginTS(TSER,BAUD)             (( int (*)(void*,uint32_t) )                   jt[56])(TSER,BAUD)
#define jdeleteTS(TSER)                 (( void (*)(void*) )                           jt[63])(TSER)
#define jreadTS(TSER,BUF,SIZE)          (( size_t (*)(void*,uint8_t*,uint32_t) )       jt[64])(TSER,BUF,SIZE)
#define jread1TS(TSER)                  (( int (*)(void*) )                            jt[65])(TSER)
#define javailTS(TSER)                  (( uint8_t (*)(void*) )                        jt[66])(TSER)


Next: Routines for decimal floating point emulation, Previous: Routines for integer arithmetic, Up: The GCC low-level runtime library   [Contents][Index]

4.2 Routines for floating point emulation

The software floating point library is used on machines which do not have hardware support for floating point. It is also used whenever -msoft-float is used to disable generation of floating point instructions. (Not all targets support this switch.)

For compatibility with other compilers, the floating point emulation routines can be renamed with the DECLARE_LIBRARY_RENAMES macro (see Implicit Calls to Library Routines). In this section, the default names are used.

Presently the library does not support XFmode, which is used for long double on some architectures.

Arithmetic functions
Conversion functions
Comparison functions
Other floating-point functions
4.2.1 Arithmetic functions

Runtime Function: float __addsf3 (float a, float b)
Runtime Function: double __adddf3 (double a, double b)
Runtime Function: long double __addtf3 (long double a, long double b)
Runtime Function: long double __addxf3 (long double a, long double b)
These functions return the sum of a and b.

Runtime Function: float __subsf3 (float a, float b)
Runtime Function: double __subdf3 (double a, double b)
Runtime Function: long double __subtf3 (long double a, long double b)
Runtime Function: long double __subxf3 (long double a, long double b)
These functions return the difference between b and a; that is, a - b.

Runtime Function: float __mulsf3 (float a, float b)
Runtime Function: double __muldf3 (double a, double b)
Runtime Function: long double __multf3 (long double a, long double b)
Runtime Function: long double __mulxf3 (long double a, long double b)
These functions return the product of a and b.

Runtime Function: float __divsf3 (float a, float b)
Runtime Function: double __divdf3 (double a, double b)
Runtime Function: long double __divtf3 (long double a, long double b)
Runtime Function: long double __divxf3 (long double a, long double b)
These functions return the quotient of a and b; that is, a / b.

Runtime Function: float __negsf2 (float a)
Runtime Function: double __negdf2 (double a)
Runtime Function: long double __negtf2 (long double a)
Runtime Function: long double __negxf2 (long double a)
These functions return the negation of a. They simply flip the sign bit, so they can produce negative zero and negative NaN.

4.2.2 Conversion functions

Runtime Function: double __extendsfdf2 (float a)
Runtime Function: long double __extendsftf2 (float a)
Runtime Function: long double __extendsfxf2 (float a)
Runtime Function: long double __extenddftf2 (double a)
Runtime Function: long double __extenddfxf2 (double a)
These functions extend a to the wider mode of their return type.

Runtime Function: double __truncxfdf2 (long double a)
Runtime Function: double __trunctfdf2 (long double a)
Runtime Function: float __truncxfsf2 (long double a)
Runtime Function: float __trunctfsf2 (long double a)
Runtime Function: float __truncdfsf2 (double a)
These functions truncate a to the narrower mode of their return type, rounding toward zero.

Runtime Function: int __fixsfsi (float a)
Runtime Function: int __fixdfsi (double a)
Runtime Function: int __fixtfsi (long double a)
Runtime Function: int __fixxfsi (long double a)
These functions convert a to a signed integer, rounding toward zero.

Runtime Function: long __fixsfdi (float a)
Runtime Function: long __fixdfdi (double a)
Runtime Function: long __fixtfdi (long double a)
Runtime Function: long __fixxfdi (long double a)
These functions convert a to a signed long, rounding toward zero.

Runtime Function: long long __fixsfti (float a)
Runtime Function: long long __fixdfti (double a)
Runtime Function: long long __fixtfti (long double a)
Runtime Function: long long __fixxfti (long double a)
These functions convert a to a signed long long, rounding toward zero.

Runtime Function: unsigned int __fixunssfsi (float a)
Runtime Function: unsigned int __fixunsdfsi (double a)
Runtime Function: unsigned int __fixunstfsi (long double a)
Runtime Function: unsigned int __fixunsxfsi (long double a)
These functions convert a to an unsigned integer, rounding toward zero. Negative values all become zero.

Runtime Function: unsigned long __fixunssfdi (float a)
Runtime Function: unsigned long __fixunsdfdi (double a)
Runtime Function: unsigned long __fixunstfdi (long double a)
Runtime Function: unsigned long __fixunsxfdi (long double a)
These functions convert a to an unsigned long, rounding toward zero. Negative values all become zero.

Runtime Function: unsigned long long __fixunssfti (float a)
Runtime Function: unsigned long long __fixunsdfti (double a)
Runtime Function: unsigned long long __fixunstfti (long double a)
Runtime Function: unsigned long long __fixunsxfti (long double a)
These functions convert a to an unsigned long long, rounding toward zero. Negative values all become zero.

Runtime Function: float __floatsisf (int i)
Runtime Function: double __floatsidf (int i)
Runtime Function: long double __floatsitf (int i)
Runtime Function: long double __floatsixf (int i)
These functions convert i, a signed integer, to floating point.

Runtime Function: float __floatdisf (long i)
Runtime Function: double __floatdidf (long i)
Runtime Function: long double __floatditf (long i)
Runtime Function: long double __floatdixf (long i)
These functions convert i, a signed long, to floating point.

Runtime Function: float __floattisf (long long i)
Runtime Function: double __floattidf (long long i)
Runtime Function: long double __floattitf (long long i)
Runtime Function: long double __floattixf (long long i)
These functions convert i, a signed long long, to floating point.

Runtime Function: float __floatunsisf (unsigned int i)
Runtime Function: double __floatunsidf (unsigned int i)
Runtime Function: long double __floatunsitf (unsigned int i)
Runtime Function: long double __floatunsixf (unsigned int i)
These functions convert i, an unsigned integer, to floating point.

Runtime Function: float __floatundisf (unsigned long i)
Runtime Function: double __floatundidf (unsigned long i)
Runtime Function: long double __floatunditf (unsigned long i)
Runtime Function: long double __floatundixf (unsigned long i)
These functions convert i, an unsigned long, to floating point.

Runtime Function: float __floatuntisf (unsigned long long i)
Runtime Function: double __floatuntidf (unsigned long long i)
Runtime Function: long double __floatuntitf (unsigned long long i)
Runtime Function: long double __floatuntixf (unsigned long long i)
These functions convert i, an unsigned long long, to floating point.

Runtime Function: void __fixsfbitint (UBILtype *r, int32_t rprec, float a)
Runtime Function: void __fixdfbitint (UBILtype *r, int32_t rprec, double a)
Runtime Function: void __fixxfbitint (UBILtype *r, int32_t rprec, __float80 a)
Runtime Function: void __fixtfbitint (UBILtype *r, int32_t rprec, _Float128 a)
These functions convert a to bit-precise integer r, rounding toward zero. If rprec is positive, it converts to unsigned bit-precise integer and negative values all become zero, if rprec is negative, it converts to signed bit-precise integer.

Runtime Function: float __floatbitintsf (UBILtype *i, int32_t iprec)
Runtime Function: double __floatbitintdf (UBILtype *i, int32_t iprec)
Runtime Function: __float80 __floatbitintxf (UBILtype *i, int32_t iprec)
Runtime Function: _Float128 __floatbitinttf (UBILtype *i, int32_t iprec)
Runtime Function: _Float16 __floatbitinthf (UBILtype *i, int32_t iprec)
Runtime Function: __bf16 __floatbitintbf (UBILtype *i, int32_t iprec)
These functions convert bit-precise integer i to floating point. If iprec is positive, it is conversion from unsigned bit-precise integer, otherwise from signed bit-precise integer.

4.2.3 Comparison functions

There are two sets of basic comparison functions.

Runtime Function: int __cmpsf2 (float a, float b)
Runtime Function: int __cmpdf2 (double a, double b)
Runtime Function: int __cmptf2 (long double a, long double b)
These functions calculate a <=> b. That is, if a is less than b, they return −1; if a is greater than b, they return 1; and if a and b are equal they return 0. If either argument is NaN they return 1, but you should not rely on this; if NaN is a possibility, use one of the higher-level comparison functions.

Runtime Function: int __unordsf2 (float a, float b)
Runtime Function: int __unorddf2 (double a, double b)
Runtime Function: int __unordtf2 (long double a, long double b)
These functions return a nonzero value if either argument is NaN, otherwise 0.

There is also a complete group of higher level functions which correspond directly to comparison operators. They implement the ISO C semantics for floating-point comparisons, taking NaN into account. Pay careful attention to the return values defined for each set. Under the hood, all of these routines are implemented as

  if (__unordXf2 (a, b))
    return E;
  return __cmpXf2 (a, b);
where E is a constant chosen to give the proper behavior for NaN. Thus, the meaning of the return value is different for each set. Do not rely on this implementation; only the semantics documented below are guaranteed.

Runtime Function: int __eqsf2 (float a, float b)
Runtime Function: int __eqdf2 (double a, double b)
Runtime Function: int __eqtf2 (long double a, long double b)
These functions return zero if neither argument is NaN, and a and b are equal.

Runtime Function: int __nesf2 (float a, float b)
Runtime Function: int __nedf2 (double a, double b)
Runtime Function: int __netf2 (long double a, long double b)
These functions return a nonzero value if either argument is NaN, or if a and b are unequal.

Runtime Function: int __gesf2 (float a, float b)
Runtime Function: int __gedf2 (double a, double b)
Runtime Function: int __getf2 (long double a, long double b)
These functions return a value greater than or equal to zero if neither argument is NaN, and a is greater than or equal to b.

Runtime Function: int __ltsf2 (float a, float b)
Runtime Function: int __ltdf2 (double a, double b)
Runtime Function: int __lttf2 (long double a, long double b)
These functions return a value less than zero if neither argument is NaN, and a is strictly less than b.

Runtime Function: int __lesf2 (float a, float b)
Runtime Function: int __ledf2 (double a, double b)
Runtime Function: int __letf2 (long double a, long double b)
These functions return a value less than or equal to zero if neither argument is NaN, and a is less than or equal to b.

Runtime Function: int __gtsf2 (float a, float b)
Runtime Function: int __gtdf2 (double a, double b)
Runtime Function: int __gttf2 (long double a, long double b)
These functions return a value greater than zero if neither argument is NaN, and a is strictly greater than b.

4.2.4 Other floating-point functions

Runtime Function: float __powisf2 (float a, int b)
Runtime Function: double __powidf2 (double a, int b)
Runtime Function: long double __powitf2 (long double a, int b)
Runtime Function: long double __powixf2 (long double a, int b)
These functions convert raise a to the power b.

Runtime Function: complex float __mulsc3 (float a, float b, float c, float d)
Runtime Function: complex double __muldc3 (double a, double b, double c, double d)
Runtime Function: complex long double __multc3 (long double a, long double b, long double c, long double d)
Runtime Function: complex long double __mulxc3 (long double a, long double b, long double c, long double d)
These functions return the product of a + ib and c + id, following the rules of C99 Annex G.

Runtime Function: complex float __divsc3 (float a, float b, float c, float d)
Runtime Function: complex double __divdc3 (double a, double b, double c, double d)
Runtime Function: complex long double __divtc3 (long double a, long double b, long double c, long double d)
Runtime Function: complex long double __divxc3 (long double a, long double b, long double c, long double d)
These functions return the quotient of a + ib and c + id (i.e., (a + ib) / (c + id)), following the rules of C99 Annex G.

Next: Routines for decimal floating point emulation, Previous: Routines for integer arithmetic, Up: The GCC low-level runtime library   [Contents][Index]


*/