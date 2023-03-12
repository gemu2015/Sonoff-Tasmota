/***************************************************************************//**
 * @file    user_settings.h
 * @brief   Arduino wolfSSL library user settings header
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

#ifdef __has_include
    #if __has_include("user_settings_wolfssl.h")
        #include "user_settings_wolfssl.h"
    #else
        #include "user_settings_default.h"
        #if __has_include("user_settings_extra.h")
            #include "user_settings_extra.h"
        #endif
    #endif
#else /* __has_include */
    #include "user_settings_default.h"
#endif /* __has_include */

#endif /* __USER_SETTINGS_H__ */
