































#ifndef PICORSRC_H_
#define PICORSRC_H_

#include "picodefs.h"
#include "picoos.h"
#include "picoknow.h"





#define PICORSRC_MAX_RSRC_NAME_SIZ PICO_MAX_RESOURCE_NAME_SIZE  

#define PICORSRC_MAX_NUM_VOICES 64

 
#define PICORSRC_KB_ARRAY_SIZE 64

typedef picoos_char picorsrc_resource_name_t[PICORSRC_MAX_RSRC_NAME_SIZ];

typedef enum picorsrc_resource_type {
    PICORSRC_TYPE_NULL,
    PICORSRC_TYPE_TEXTANA,
    PICORSRC_TYPE_SIGGEN,
    PICORSRC_TYPE_USER_LEX,
    PICORSRC_TYPE_USER_PREPROC,
    PICORSRC_TYPE_OTHER
} picorsrc_resource_type_t;


#define PICORSRC_FIELD_VALUE_TEXTANA (picoos_char *) PICO_S(22)
#define PICORSRC_FIELD_VALUE_SIGGEN (picoos_char *) PICO_S(23)
#define PICORSRC_FIELD_VALUE_USERLEX (picoos_char *) PICO_S(24)
#define PICORSRC_FIELD_VALUE_USERTPP (picoos_char *) PICO_S(25)



typedef struct picorsrc_resource_manager * picorsrc_ResourceManager;
typedef struct picorsrc_voice            * picorsrc_Voice;
typedef struct picorsrc_resource         * picorsrc_Resource;








#define PICO_BIN_EXTENSION      PICO_S(26)
#define PICO_INPLACE_EXTENSION  PICO_S(27)









 

picorsrc_ResourceManager picorsrc_newResourceManager(picoos_MemoryManager mm, picoos_Common common  );

void picorsrc_disposeResourceManager(picoos_MemoryManager mm, picorsrc_ResourceManager * thiz);











picoos_int16 picoctrl_isValidResourceHandle(picorsrc_Resource resource);



pico_status_t picorsrc_loadResource(picorsrc_ResourceManager thiz,
        picoos_char * fileName, picorsrc_Resource * resource);

 
pico_status_t picorsrc_unloadResource(picorsrc_ResourceManager thiz, picorsrc_Resource * rsrc);


pico_status_t picorsrc_createDefaultResource(picorsrc_ResourceManager thiz 
);


pico_status_t picorsrc_rsrcGetName(picorsrc_Resource resource,
        picoos_char * name, picoos_uint32 maxlen);








pico_status_t picorsrc_createVoiceDefinition(picorsrc_ResourceManager thiz,
        picoos_char * voiceName);


pico_status_t picorsrc_releaseVoiceDefinition(picorsrc_ResourceManager thiz,
        picoos_char * voiceName);

pico_status_t picorsrc_addResourceToVoiceDefinition(picorsrc_ResourceManager thiz,
        picoos_char * voiceName, picoos_char * resourceName);












typedef struct picorsrc_voice {

    picorsrc_Voice next;

    picoknow_KnowledgeBase kbArray[PICORSRC_KB_ARRAY_SIZE];

    picoos_uint8 numResources;

    picorsrc_Resource resourceArray[PICO_MAX_NUM_RSRC_PER_VOICE];


} picorsrc_voice_t;



 
pico_status_t picorsrc_createVoice(picorsrc_ResourceManager thiz, const picoos_char * voiceName, picorsrc_Voice * voice);

 
pico_status_t picorsrc_releaseVoice(picorsrc_ResourceManager thiz, picorsrc_Voice * voice);





#endif  
