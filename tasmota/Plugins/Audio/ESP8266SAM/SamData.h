/*
 * dataspace.h
 *
 *  Created on: Feb 24, 2019
 *      Author: chris.l
 */

#ifndef SAMDATA_H_
#define SAMDATA_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SAMDATA

/*
typedef struct {
	unsigned char speed;
	unsigned char pitch;
	unsigned char mouth;
	unsigned char throat;
	int singmode = 0;
	unsigned char mem39;
	unsigned char mem44;
	unsigned char mem47;
	unsigned char mem49;
	unsigned char mem50;
	unsigned char mem51;
	unsigned char mem53;
	unsigned char mem56;
	unsigned char mem59;
	unsigned char A, X, Y;
    unsigned char oldtimetableindex;
    unsigned char lastAry[5];
    void (*outcb)(void *, unsigned char);
    void *outcbdata;

// contains the final soundbuffer
	int bufferpos;
//char *buffer = NULL;
} sam_mem;
*/

typedef struct s_samdata {
    struct render {
        unsigned char pitches[256]; // tab43008
        unsigned char frequency1[256];
        unsigned char frequency2[256];
        unsigned char frequency3[256];
        unsigned char amplitude1[256];
        unsigned char amplitude2[256];
        unsigned char amplitude3[256];
        unsigned char sampledConsonantFlag[256]; // tab44800
    } render;
    struct reciter {
        unsigned char inputtemp[256];
    } reciter;
    struct sam {
        char input[256]; //tab39445
        unsigned char stress[256]; //numbers from 0 to 8
        unsigned char phonemeLength[256]; //tab40160
        unsigned char phonemeindex[256];
        unsigned char phonemeIndexOutput[60]; //tab47296
        unsigned char stressOutput[60]; //tab47365
        unsigned char phonemeLengthOutput[60]; //tab47416
    } sam;

    uint8_t freq1data[80];
    uint8_t freq2data[80];
    uint8_t freq3data[80];
    
	unsigned char speed;
	unsigned char pitch;
	unsigned char mouth;
	unsigned char throat;
	int singmode = 0;
	unsigned char mem39;
	unsigned char mem44;
	unsigned char mem47;
	unsigned char mem49;
	unsigned char mem50;
	unsigned char mem51;
	unsigned char mem53;
	unsigned char mem56;
	unsigned char mem59;
	unsigned char A, X, Y;
    unsigned char oldtimetableindex;
    unsigned char lastAry[5];
    void (*outcb)(void *, unsigned char);
    void *outcbdata;
	int bufferpos;
} SamData;



#define input samdata->sam.input
#define stress samdata->sam.stress
#define phonemeLength samdata->sam.phonemeLength
#define phonemeindex samdata->sam.phonemeindex
#define phonemeIndexOutput samdata->sam.phonemeIndexOutput
#define stressOutput samdata->sam.stressOutput
#define phonemeLengthOutput samdata->sam.phonemeLengthOutput

#define oldtimetableindex samdata->oldtimetableindex
#define lastAry samdata->lastAry

#define freq1data    samdata->freq1data
#define freq2data    samdata->freq2data
#define freq3data    samdata->freq3data

#define pitches    samdata->render.pitches
#define frequency1 samdata->render.frequency1
#define frequency2 samdata->render.frequency2
#define frequency3 samdata->render.frequency3
#define amplitude1 samdata->render.amplitude1
#define amplitude2 samdata->render.amplitude2
#define amplitude3 samdata->render.amplitude3
#define sampledConsonantFlag samdata->render.sampledConsonantFlag
#define pitch    samdata->pitch
#define speed    samdata->speed
#define singmode    samdata->singmode
#define mouth   samdata->mouth
#define throat   samdata->throat
#define bufferpos samdata->bufferpos
#define outcb samdata->outcb
#define outcbdata samdata->outcbdata

#ifdef __cplusplus
}
#endif


const int32_t uconst[] PROGMEM  = {2559,3071,3073};


#endif /* SAMDATA_H_ */
