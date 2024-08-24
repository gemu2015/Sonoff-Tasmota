#ifndef SAM_H
#define SAM_H

#ifdef __cplusplus
extern "C" {
#endif

MODULE_PART void SetInput(char *_input);
MODULE_PART void SetSpeed(unsigned char _speed);
MODULE_PART void SetPitch(unsigned char _pitch);
MODULE_PART void SetMouth(unsigned char _mouth);
MODULE_PART void SetThroat(unsigned char _throat);
MODULE_PART void EnableSingmode(int x);

MODULE_PART int SAMMain( void (*cb)(void *, unsigned char), void *cbdata );

MODULE_PART int GetBufferLength();

MODULE_PART int SAMPrepare();



#ifdef __cplusplus
}
#endif


#endif
