/* Generated wolfSSL user_settings.h file for Arduino */
#ifndef ARDUINO_USER_SETTINGS_H
#define ARDUINO_USER_SETTINGS_H

/* Platform */
#define WOLFSSL_ARDUINO

/* Math library (remove this to use normal math)*/
#define USE_FAST_MATH
#define TFM_NO_ASM

/* RNG DEFAULT !!FOR TESTING ONLY!! */
/* comment out the error below to get started w/ bad entropy source
 * This will need fixed before distribution but is OK to test with */
#error "needs solved, see: https://www.wolfssl.com/docs/porting-guide/"
#define WOLFSSL_GENSEED_FORTEST

#endif /* ARDUINO_USER_SETTINGS_H */
