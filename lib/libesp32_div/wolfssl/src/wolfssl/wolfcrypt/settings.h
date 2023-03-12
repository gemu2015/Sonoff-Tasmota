/***************************************************************************//**
 * @file    settings.h
 * @brief   Replace the original file to insert defines
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#ifdef ARDUINO
#define WOLFSSL_USER_SETTINGS
#endif

#include <wolfssl/wolfcrypt/wolf_crypt_settings.h>

#endif /* __SETTINGS_H__ */
