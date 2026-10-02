



























#ifndef PICOEXTAPI_H_
#define PICOEXTAPI_H_

#include "picodefs.h"
#include "picodbg.h"





 
 
 
 

 

 

#define PICO_STRENC_UTF8    0
#define PICO_STRENC_UTF16   1





typedef char        *PICO_STRING_UTF8;
typedef pico_Uint16 *PICO_STRING_UTF16;






typedef void *PICO_STRING_PTR;


 
 
 

 




PICO_FUNC picoext_initialize(
        void *memory,
        const pico_Uint32 size,
        pico_Int16 enableMemProt,
        pico_System *outSystem
        );


 

 

PICO_FUNC picoext_getVersionInfo(
        pico_Retstring outInfo,
        const pico_Int16 outInfoMaxLen
    );

 








 




PICO_FUNC picoext_setTraceLevel(
        pico_System system,
        pico_Int32 level
        );




PICO_FUNC picoext_setTraceFilterFN(
        pico_System system,
        const pico_Char *name
        );




PICO_FUNC picoext_setLogFile(
        pico_System system,
        const pico_Char *name
        );


 

PICO_FUNC picoext_getSystemMemUsage(
        pico_System system,
        pico_Int16 resetIncremental,
        pico_Int32 *outUsedBytes,
        pico_Int32 *outIncrUsedBytes,
        pico_Int32 *outMaxUsedBytes
        );

PICO_FUNC picoext_getEngineMemUsage(
        pico_Engine engine,
        pico_Int16 resetIncremental,
        pico_Int32 *outUsedBytes,
        pico_Int32 *outIncrUsedBytes,
        pico_Int32 *outMaxUsedBytes
        );

PICO_FUNC picoext_getLastScheduledPU(
        pico_Engine engine
        );

PICO_FUNC picoext_getLastProducedItemType(
        pico_Engine engine
        );




#endif  
