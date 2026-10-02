
































































































































































#if !defined(__PICODBG_H__)
#define __PICODBG_H__





 
#if !defined(__FUNCTION__) && !defined(__GNUC__)
#define __FUNCTION__ PICO_S(14)
#endif


 
#define PICODBG_LOG_LEVEL_ERROR     1
#define PICODBG_LOG_LEVEL_WARN      2
#define PICODBG_LOG_LEVEL_INFO      3
#define PICODBG_LOG_LEVEL_DEBUG     4
#define PICODBG_LOG_LEVEL_TRACE     5

 
#define PICODBG_SHOW_LEVEL          0x0001
#define PICODBG_SHOW_DATE           0x0002
#define PICODBG_SHOW_TIME           0x0004
#define PICODBG_SHOW_SRCNAME        0x0008
#define PICODBG_SHOW_SRCLINE        0x0010
#define PICODBG_SHOW_SRCALL         (PICODBG_SHOW_SRCNAME | PICODBG_SHOW_SRCLINE)
#define PICODBG_SHOW_FUNCTION       0x0020
#define PICODBG_SHOW_POS            (PICODBG_SHOW_SRCALL | PICODBG_SHOW_FUNCTION)

 
#if defined(PICO_DEBUG)

#define PICODBG_INITIALIZE(level) \
    picodbg_initialize(level)

#define PICODBG_TERMINATE() \
    picodbg_terminate()

#define PICODBG_SET_LOG_LEVEL(level) \
    picodbg_setLogLevel(level)

#define PICODBG_SET_LOG_FILTERFN(name) \
    picodbg_setLogFilterFN(name)

#define PICODBG_SET_LOG_FILE(name) \
    picodbg_setLogFile(name)

#define PICODBG_ENABLE_COLORS(flag) \
    picodbg_enableColors(flag)

#define PICODBG_SET_OUTPUT_FORMAT(format) \
    picodbg_setOutputFormat(format)


#define PICODBG_ASSERT(expr) \
    for (;!(expr);picodbg_assert(__FILE__, __LINE__, __FUNCTION__, #expr))

#define PICODBG_ASSERT_RANGE(val, min, max) \
    PICODBG_ASSERT(((val) >= (min)) && ((val) <= (max)))


#define PICODBG_LOG(level, msg) \
    picodbg_log(level, 1,  __FILE__, __LINE__, __FUNCTION__, picodbg_varargs msg)

#define PICODBG_ERROR(msg) \
    PICODBG_LOG(PICODBG_LOG_LEVEL_ERROR, msg)

#define PICODBG_WARN(msg) \
    PICODBG_LOG(PICODBG_LOG_LEVEL_WARN, msg)

#define PICODBG_INFO(msg) \
    PICODBG_LOG(PICODBG_LOG_LEVEL_INFO, msg)

#define PICODBG_DEBUG(msg) \
    PICODBG_LOG(PICODBG_LOG_LEVEL_DEBUG, msg)

#define PICODBG_TRACE(msg) \
    PICODBG_LOG(PICODBG_LOG_LEVEL_TRACE, msg)


#define PICODBG_INFO_CTX() \
    picodbg_log(PICODBG_LOG_LEVEL_INFO, 0, __FILE__, __LINE__, __FUNCTION__, PICO_S(14))

#define PICODBG_INFO_MSG(msg) \
    picodbg_log_msg(PICODBG_LOG_LEVEL_INFO, __FILE__, picodbg_varargs msg)

#define PICODBG_INFO_MSG_F(filterfn, msg) \
    picodbg_log_msg(PICODBG_LOG_LEVEL_INFO, (const char *)filterfn, picodbg_varargs msg)



 

void picodbg_initialize(int level);
void picodbg_terminate();

void picodbg_setLogLevel(int level);
void picodbg_setLogFilterFN(const char *name);
void picodbg_setLogFile(const char *name);
void picodbg_enableColors(int flag);
void picodbg_setOutputFormat(unsigned int format);

const char *picodbg_varargs(const char *format, ...);

void picodbg_log(int level, int donewline, const char *file, int line,
                 const char *func, const char *msg);
void picodbg_assert(const char *file, int line, const char *func,
                    const char *expr);

void picodbg_log_msg(int level, const char *file, const char *msg);


#else   

#define PICODBG_INITIALIZE(level)
#define PICODBG_TERMINATE()
#define PICODBG_SET_LOG_LEVEL(level)
#define PICODBG_SET_LOG_FILTERFN(name)
#define PICODBG_SET_LOG_FILE(name)
#define PICODBG_ENABLE_COLORS(flag)
#define PICODBG_SET_OUTPUT_FORMAT(format)

#define PICODBG_ASSERT(expr)
#define PICODBG_ASSERT_RANGE(val, min, max)

#define PICODBG_LOG(level, msg)
#define PICODBG_ERROR(msg)
#define PICODBG_WARN(msg)
#define PICODBG_INFO(msg)
#define PICODBG_DEBUG(msg)
#define PICODBG_TRACE(msg)

#define PICODBG_INFO_CTX()
#define PICODBG_INFO_MSG(msg)
#define PICODBG_INFO_MSG_F(filterfn, msg)


#endif  




#endif  
