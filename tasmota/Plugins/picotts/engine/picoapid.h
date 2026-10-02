






























#ifndef PICOAPID_H_
#define PICOAPID_H_

#include "picodefs.h"
#include "picoapi.h"
#include "picoos.h"
#include "picorsrc.h"
#include "picoctrl.h"





 
typedef struct pico_system {
    picoos_uint32 magic;         
    picoos_Common common;
    picorsrc_ResourceManager rm;
    picoctrl_Engine engine;
} pico_system_t;


 
extern int is_valid_system_handle(pico_System system);
extern picoos_Common pico_sysGetCommon(pico_System thiz);





#endif  
