












































































































#ifndef PICOAPI_H_
#define PICOAPI_H_



#include "picodefs.h"





#ifdef _WIN32
#  define PICO_EXPORT  __declspec( dllexport )
#else
#  define PICO_EXPORT extern
#endif

#define PICO_FUNC PICO_EXPORT pico_Status



 
 
 

 

typedef struct pico_system   *pico_System;
typedef struct pico_resource *pico_Resource;
typedef struct pico_engine   *pico_Engine;


 

#define PICO_INT16_MAX   32767
#define PICO_UINT16_MAX  0xffff
#define PICO_INT32_MAX   2147483647
#define PICO_UINT32_MAX  0xffffffff

#include <limits.h>

#if (SHRT_MAX == PICO_INT16_MAX)
typedef short pico_Int16;
#else
#error "platform not supported"
#endif

#if (USHRT_MAX == PICO_UINT16_MAX)
typedef unsigned short pico_Uint16;
#else
#error "platform not supported"
#endif

#if (INT_MAX == PICO_INT32_MAX)
typedef int pico_Int32;
#else
#error "platform not supported"
#endif

#if (UINT_MAX == PICO_UINT32_MAX)
typedef unsigned int pico_Uint32;
#else
#error "platform not supported"
#endif


 

typedef unsigned char pico_Char;


 

#define PICO_RETSTRINGSIZE 200   

typedef char pico_Retstring[PICO_RETSTRINGSIZE];



 
 
 

 











PICO_FUNC pico_initialize(
        void *memory,
        const pico_Uint32 size,
        pico_System *outSystem
        );










PICO_FUNC pico_terminate(
        pico_System *system
        );


 






PICO_FUNC pico_getSystemStatusMessage(
        pico_System system,
        pico_Status errCode,
        pico_Retstring outMessage
        );





PICO_FUNC pico_getNrSystemWarnings(
        pico_System system,
        pico_Int32 *outNrOfWarnings
        );








PICO_FUNC pico_getSystemWarning(
        pico_System system,
        const pico_Int32 warningIndex,
        pico_Status *outCode,
        pico_Retstring outMessage
        );


 












PICO_FUNC pico_loadResource(
        pico_System system,
        const pico_Char *resourceFileName,
        pico_Resource *outResource
        );







PICO_FUNC pico_unloadResource(
        pico_System system,
        pico_Resource *inoutResource
        );

 




PICO_FUNC pico_getResourceName(
        pico_System system,
        pico_Resource resource,
        pico_Retstring outName);


 








PICO_FUNC pico_createVoiceDefinition(
        pico_System system,
        const pico_Char *voiceName
        );







PICO_FUNC pico_addResourceToVoiceDefinition(
        pico_System system,
        const pico_Char *voiceName,
        const pico_Char *resourceName
        );






PICO_FUNC pico_releaseVoiceDefinition(
        pico_System system,
        const pico_Char *voiceName
        );


 






PICO_FUNC pico_newEngine(
        pico_System system,
        const pico_Char *voiceName,
        pico_Engine *outEngine
        );






PICO_FUNC pico_disposeEngine(
        pico_System system,
        pico_Engine *inoutEngine
        );



 
 
 
















PICO_FUNC pico_putTextUtf8(
        pico_Engine engine,
        const pico_Char *text,
        const pico_Int16 textSize,
        pico_Int16 *outBytesPut
        );


















PICO_FUNC pico_getData(
        pico_Engine engine,
        void *outBuffer,
        const pico_Int16 bufferSize,
        pico_Int16 *outBytesReceived,
        pico_Int16 *outDataType
        );







PICO_FUNC pico_resetEngine(
        pico_Engine engine,
        pico_Int32 resetMode
);


 






PICO_FUNC pico_getEngineStatusMessage(
        pico_Engine engine,
        pico_Status errCode,
        pico_Retstring outMessage
        );





PICO_FUNC pico_getNrEngineWarnings(
        pico_Engine engine,
        pico_Int32 *outNrOfWarnings
        );








PICO_FUNC pico_getEngineWarning(
        pico_Engine engine,
        const pico_Int32 warningIndex,
        pico_Status *outCode,
        pico_Retstring outMessage
        );



#endif  
