




























#ifndef PICODEFS_H_
#define PICODEFS_H_




 
 
 


#define PICO_MAX_VOICE_NAME_SIZE        32



#define PICO_MAX_RESOURCE_NAME_SIZE     32



#define PICO_MAX_DATAPATH_NAME_SIZE    128



#define PICO_MAX_FILE_NAME_SIZE         64

 
#define PICO_MAX_NUM_RESOURCES          64

 
#define PICO_MAX_NUM_VOICE_DEFINITIONS  64

 
#define PICO_MAX_NUM_RSRC_PER_VOICE     16



#define PICO_MAX_FOREIGN_HEADER_LEN     64



 
 
 

typedef signed int pico_Status;


 
 

#define PICO_OK                         (pico_Status)     0


 





#define PICO_EXC_NUMBER_FORMAT          (pico_Status)   -10
#define PICO_EXC_MAX_NUM_EXCEED         (pico_Status)   -11
#define PICO_EXC_NAME_CONFLICT          (pico_Status)   -12
#define PICO_EXC_NAME_UNDEFINED         (pico_Status)   -13
#define PICO_EXC_NAME_ILLEGAL           (pico_Status)   -14

 
#define PICO_EXC_BUF_OVERFLOW           (pico_Status)   -20
#define PICO_EXC_BUF_UNDERFLOW          (pico_Status)   -21
#define PICO_EXC_BUF_IGNORE             (pico_Status)   -22

 
#define PICO_EXC_OUT_OF_MEM             (pico_Status)   -30

 
#define PICO_EXC_CANT_OPEN_FILE         (pico_Status)   -40
#define PICO_EXC_UNEXPECTED_FILE_TYPE   (pico_Status)   -41
#define PICO_EXC_FILE_CORRUPT           (pico_Status)   -42
#define PICO_EXC_FILE_NOT_FOUND         (pico_Status)   -43

 
#define PICO_EXC_RESOURCE_BUSY          (pico_Status)   -50
#define PICO_EXC_RESOURCE_MISSING       (pico_Status)   -51

 
#define PICO_EXC_KB_MISSING             (pico_Status)   -60

 
#define PICO_ERR_NULLPTR_ACCESS         (pico_Status)  -100
#define PICO_ERR_INVALID_HANDLE         (pico_Status)  -101
#define PICO_ERR_INVALID_ARGUMENT       (pico_Status)  -102
#define PICO_ERR_INDEX_OUT_OF_RANGE     (pico_Status)  -103

 
#define PICO_ERR_OTHER                  (pico_Status)  -999


 

 
#define PICO_WARN_INCOMPLETE            (pico_Status)    10
#define PICO_WARN_FALLBACK              (pico_Status)    11
#define PICO_WARN_OTHER                 (pico_Status)    19

 
#define PICO_WARN_KB_OVERWRITE          (pico_Status)    50
#define PICO_WARN_RESOURCE_DOUBLE_LOAD  (pico_Status)    51

 
#define PICO_WARN_INVECTOR              (pico_Status)    60
#define PICO_WARN_CLASSIFICATION        (pico_Status)    61
#define PICO_WARN_OUTVECTOR             (pico_Status)    62

 
#define PICO_WARN_PU_IRREG_ITEM         (pico_Status)    70
#define PICO_WARN_PU_DISCARD_BUF        (pico_Status)    71



 
 
 

#define PICO_STEP_IDLE                  (pico_Status)   200
#define PICO_STEP_BUSY                  (pico_Status)   201

#define PICO_STEP_ERROR                 (pico_Status)  -200

 
 
 

#define PICO_RESET_FULL                                 0
#define PICO_RESET_SOFT                                 0x10


 
 
 

 
#define PICO_DATA_PCM_16BIT             (pico_Int16)  1




#endif  
