
#define AGPIO(x) ((x)<<5)
#define BGPIO(x) ((x)>>5)

#include "i18n.h"

#define SerConfu8 uint8_t
#include "tasmota.h"
#include "tasmota_template.h"
#include "settings.h"

// vector table calls
#define jWire                           ( TwoWire*)                                    jt[0]
#define jWire1                          ( TwoWire*)                                    jt[1]
#define jSerial                         ( HardwareSerial*)                             jt[2]
#define jI2cSetDevice(A)                (( bool (*)(uint32_t) )                        jt[3])(A)
#define jI2cSetActiveFound(A,B,C)       (( void (*)(uint32_t,const char *, uint32_t) ) jt[4])(A,B,C)
#define jAddLog(A,B)                    (( void (*)(uint32_t, PGM_P, ...) )            jt[5])(A,B)
#define jResponseAppend_P(...)          (( void (*)(const char * formatP, ...) )       jt[6])(__VA_ARGS__)
#define jWSContentSend_PD(...)          (( void (*)(const char * formatP, ...) )       jt[7])(__VA_ARGS__)
#define jftostrfd(A,B,C)                (( char *(*)(float, uint8_t, char*) )          jt[8])(A,B,C)
#define jcalloc(A,B)                    (( void *(*)(size_t, size_t) )                 jt[9])(A,B)
// 10
#define jfscale(A,B,C)                  (( float (*)(int32_t, float, float) )          jt[10])(A,B,C)
#define sprint(A)                       (( void (*)(const char*) )                     jt[11])(A)
#define jbeginTransmission(BUS,ADDR)    (( void (*)(TwoWire*,uint8_t) )                jt[12])(BUS,ADDR)
#define jwrite(BUS,VAL)                 (( void (*)(TwoWire*,uint8_t) )                jt[13])(BUS,VAL)
#define jendTransmission(BUS,VAL)       (( uint8_t (*)(TwoWire*,bool) )                jt[14])(BUS,VAL)
#define jrequestFrom(BUS,ADDR,NUM)      (( void (*)(TwoWire*,uint8_t,uint8_t) )        jt[15])(BUS,ADDR,NUM)
#define jread(BUS)                      (( uint8_t (*)(TwoWire*) )                     jt[16])(BUS)
#define fshowhex(VAL)                   (( void (*)(uint32_t) )                        jt[17])(VAL)
#define jfree(MEM)                      (( void (*)(void*) )                           jt[18])(MEM)
#define jI2cWrite16(ADDR,REG,VAL)       (( bool (*)(uint8_t, uint8_t, uint16_t) )      jt[19])(ADDR,REG,VAL)
// 20
#define jI2cRead16(ADDR,REG)            (( uint16_t (*)(uint8_t, uint8_t) )            jt[20])(ADDR,REG)
#define jI2cValidRead16(DATA,ADDR,REG)  (( bool (*)(uint16_t *,uint8_t,uint8_t) )      jt[21])(DATA,ADDR,REG)
#define jsnprintf_P(...)                (( void (*)(...) )                             jt[22])(__VA_ARGS__)
#define jXdrvRulesProcess(A)            (( bool (*)(bool) )                            jt[23])(A)
#define jResponseJsonEnd                (( void (*)(void) )                            jt[24])
#define jdelay(A)                       (( void (*)(uint32_t) )                        jt[25])(A)
#define jI2cActive(A)                   (( bool (*)(uint32_t) )                        jt[26])(A)
#define jResponseJsonEndEnd             (( void (*)(void) )                            jt[27])
#define jIndexSeparator                 (( char (*)(void) )                            jt[28])
#define jResponse_P(...)                (( int (*)(const char * formatP, ...) )        jt[29])(__VA_ARGS__)
// 30
#define jI2cResetActive(REG,CNT)        (( void (*)(uint32_t, uint32_t) )              jt[30])(REG,CNT)
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
#define jwriteTS(TSER,BUF,SIZE)         (( void (*)(void*,uint8_t*,uint32_t) )         jt[54])(TSER,BUF,SIZE)
#define jflushTS(TSER)                  (( void (*)(void*) )                           jt[55])(TSER)
#define jbeginTS(TSER,BAUD)             (( int (*)(void*,uint32_t) )                   jt[56])(TSER,BAUD)
#define jXDRVMAILBOX                    (XDRVMAILBOX*)                                 jt[57]
#define jGetCommandCode(DST,DSIZE,NEEDLE,HSTCK)(( int (*)(char*,size_t,const char*,const char*) )    jt[58])(DST,DSIZE,NEEDLE,HSTCK)
#define jstrlen(STR)                    (( uint32_t (*)(char*) )                       jt[59])(STR)
#define jstrncasecmp_P(S1,S2,SIZE)      (( int (*)(const char*,const char *, size_t) ) jt[60])(S1,S2,SIZE)
#define jtoupper(CHAR)                  (( int (*)( int c ) )                          jt[61])(CHAR)
#define jiscale(A,B,C)                  (( int32_t (*)(int32_t, int32_t, int32_t) )    jt[62])(A,B,C)
#define jdeleteTS(TSER)                 (( void (*)(void*) )                           jt[63])(TSER)
#define jreadTS(TSER,BUF,SIZE)          (( size_t (*)(void*,uint8_t*,uint32_t) )       jt[64])(TSER,BUF,SIZE)


// Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define fldsiz(name, field) (sizeof(((name *)0)->field))

#define MODULE_DESC __attribute__((section(".text.mod_desc"))) extern const FLASH_MODULE
#define MODULE_PART __attribute__((section(".text.mod_part")))
#define MODULE_END __attribute__((section(".text.mod_end"))) void  end_of_module(void) {__asm__ __volatile__(".word 0x4AFCAA55");}

#define CAT2(a,b) a##b
#define CAT(a,b) CAT2(a,b)
#define UNIQUE_ID CAT(_uid_,__COUNTER__)


#define DPSTR(LABEL,TEXT) extern "C" {  const char *LABEL(void);} __asm__  (\
  ".section .text.mod_string\n"\
  ".align 4\n"\
  ".global " #LABEL "\n"\
  #LABEL": .asciz "#TEXT" \n"\
);

#define GPSTR(VAR,FUNC) const char *VAR = (const char*)&FUNC + mt->execution_offset; fshowhex((uint32_t)VAR);
//#define jPSTR(LABEL) (__extension__({ (const char *)&LABEL[0]+mt->execution_offset;}))

#define jPSTR(LABEL) (const char *)LABEL+mt->execution_offset


#define SETREGS MODULE_MEMORY *mem = (MODULE_MEMORY*)mt->mod_memory;void (* const *jt)() = mt->jt;
#define ALLOCMEM void (* const *jt)() = mt->jt;mt->mem_size = sizeof(MODULE_MEMORY);mt->mem_size += mt->mem_size % 4;mt->mod_memory = jcalloc(mt->mem_size / 4, 4);if (!mt->mod_memory) {return -1;};MODULE_MEMORY *mem = (MODULE_MEMORY*)mt->mod_memory;SETTINGS *jsettings = mt->settings;
#define RETMEM if (mt->mem_size) {jfree(mt->mod_memory);mt->mem_size = 0;}
#define MODULE_SYNC_END __attribute__((section(".text.mod_end"))); __asm__ __volatile__ (".align 4");
#define MODULE_DESCRIPTOR(NAME,TYPE,REV)  __attribute__((section(".text.mod_desc"))) extern const FLASH_MODULE module_header = {MODULE_SYNC,CURR_ARCH,(TYPE),(REV),(NAME),mod_func_execute,end_of_module,0,0};


#define   beginTransmission(ADDR) jbeginTransmission(jWire, ADDR)
#define   write(CMD) jwrite(jWire, CMD)
#define   endTransmission(BUS) jendTransmission(jWire, BUS)
#define   requestFrom(ADDR,NUM)  jrequestFrom(jWire, ADDR, NUM);
#define   read() jread(jWire)
#define   I2cRead8 jI2cRead8
#define   I2cWrite8 jI2cWrite8
#define   delay jdelay
#define   available() javailable(jWire)
#define   ConvertHumidity jConvertHumidity
#define   GetTextIndexed jGetTextIndexed
#define   I2cSetActiveFound jI2cSetActiveFound
#define   I2cActive jI2cActive
#define   AddLogMissed jAddLogMissed
#define   TempHumDewShow jTempHumDewShow
#define   GetTasmotaGlobal JGetTasmotaGlobal
#define   ConvertTemp jConvertTemp
#define   strlcpy jstrlcpy
#define   snprintf_P jsnprintf_P
#define   TempHumDewShow jTempHumDewShow
#define   IndexSeparator jIndexSeparator
#define   ResponseAppend_P jResponseAppend_P
#define   Response_P jResponse_P
#define   ResponseJsonEndEnd jResponseJsonEndEnd
#define   ResponseJsonEnd jResponseJsonEnd
#define   XdrvRulesProcess jXdrvRulesProcess
#define   WSContentSend_PD jWSContentSend_PD
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
#define   XdrvMailbox (jXDRVMAILBOX)
#define   GetCommandCode jGetCommandCode
#define   strlen jstrlen
#define   strncasecmp_P jstrncasecmp_P
#define   toupper jtoupper
#define   iscale jiscale
#define   deleteTS jdeleteTS
#define   readTS jreadTS
