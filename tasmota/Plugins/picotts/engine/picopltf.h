





























#if !defined(__PICOPLTF_H__)
#define __PICOPLTF_H__

#define ENDIANNESS_BIG 1
#define ENDIANNESS_LITTLE 2

 
#define PICO_Windows    1    
#define PICO_MacOSX     5    
#define PICO_Linux      7    

#define PICO_GENERIC    99   

 
#if !defined(PICO_PLATFORM)
#if defined(_WIN32)
#define PICO_PLATFORM    PICO_Windows
#elif defined(__APPLE__) && defined(__MACH__)
#define PICO_PLATFORM    PICO_MacOSX
#elif defined(linux) || defined(__linux__) || defined(__linux)
#define PICO_PLATFORM    PICO_Linux
#else
#define PICO_PLATFORM    PICO_GENERIC
#endif
#endif  


 
#if (PICO_PLATFORM == PICO_Windows)
#define PICO_PLATFORM_STRING PICO_S(18)
#elif (PICO_PLATFORM == PICO_MacOSX)
#define PICO_PLATFORM_STRING PICO_S(19)
#elif (PICO_PLATFORM == PICO_Linux)
#define PICO_PLATFORM_STRING PICO_S(20)
#elif (PICO_PLATFORM == PICO_GENERIC)
#define PICO_PLATFORM_STRING PICO_S(21)
#endif

#define PICO_ENDIANNESS ENDIANNESS_LITTLE



#endif  
