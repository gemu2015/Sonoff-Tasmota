

























#ifndef PICOCTRL_H_
#define PICOCTRL_H_

#include "picodefs.h"
#include "picoos.h"
#include "picorsrc.h"
#include "picodata.h"




#define PICOCTRL_MAX_PROC_UNITS 25




#define PICOCTRL_DEFAULT_ENGINE_SIZE 1000000

typedef struct picoctrl_engine * picoctrl_Engine;

picoos_int16 picoctrl_isValidEngineHandle(picoctrl_Engine thiz);

picoctrl_Engine picoctrl_newEngine (
        picoos_MemoryManager mm,
        picorsrc_ResourceManager rm,
        const picoos_char * voiceName
        );

void picoctrl_disposeEngine(
        picoos_MemoryManager mm,
        picorsrc_ResourceManager rm,
        picoctrl_Engine * thiz
        );

pico_status_t picoctrl_engFeedText(
        picoctrl_Engine engine,
        picoos_char * text,
        picoos_int16  textSize,
        picoos_int16 * bytesPut);

pico_status_t picoctrl_engReset(
        picoctrl_Engine engine,
        picoos_int32 resetMode);

picoos_Common picoctrl_engGetCommon(picoctrl_Engine thiz);

picodata_step_result_t picoctrl_engFetchOutputItemBytes(
        picoctrl_Engine engine,
        picoos_char * buffer,
        picoos_int16 bufferSize,
        picoos_int16  * bytesReceived
);

void picoctrl_engResetExceptionManager(
        picoctrl_Engine thiz
        );


picodata_step_result_t picoctrl_getLastScheduledPU(
        picoctrl_Engine engine
        );

picodata_step_result_t picoctrl_getLastProducedItemType(
        picoctrl_Engine engine
        );



#endif  
