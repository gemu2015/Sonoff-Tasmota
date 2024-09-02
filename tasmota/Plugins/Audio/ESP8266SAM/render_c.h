#include "render.h"
#include "RenderTabs.h"
#include "SamData.h"


void AddInflection(unsigned char mem48, unsigned char phase1);
unsigned char trans(unsigned char mem39212, unsigned char mem39213);


//timetable for more accurate c64 simulation
const unsigned char timetable[5][5] PROGMEM =
{
	{162, 167, 167, 127, 128},
	{226, 60, 60, 0, 0},
	{225, 60, 59, 0, 0},
	{200, 0, 0, 54, 55},
	{199, 0, 0, 54, 54}
};

MODULE_PART void Output8BitAry(int index, unsigned char ary[5]) {
	SETMEMREGS
	int newbufferpos =  bufferpos + pgm_read_byte(&timetable[oldtimetableindex][index]);
	int bp0 = __divsi3(bufferpos, 50);
	int bp1 = __divsi3(newbufferpos, 50);
	int k = 0;
	for (int i = bp0; i < bp1; i++, k++) {
		outcb(outcbdata, lastAry[k]);
	}
	memmove(lastAry, ary, 5);
	bufferpos = newbufferpos;
	oldtimetableindex = index;
}
MODULE_PART void Output8Bit(int index, unsigned char A) {
	unsigned char ary[5] = {A,A,A,A,A};
	Output8BitAry(index, ary);
}


//written by me because of different table positions.
// mem[47] = ...
// 168=pitches
// 169=frequency1
// 170=frequency2
// 171=frequency3
// 172=amplitude1
// 173=amplitude2
// 174=amplitude3


MODULE_PART unsigned char Read(unsigned char p, unsigned char Y) {
	SETMEMREGS
	unsigned char *tabtab[] = {pitches, frequency1, frequency2, frequency3, amplitude1, amplitude2, amplitude3};
	unsigned char res = 0;
	unsigned char *rp;
	unsigned char **rpt = tabtab;
	if (p < 168 || p > 174) {
		return 0;
	}
	p -= 168;
	rp = rpt[p];
	return rp[Y];
}

MODULE_PART void Write(unsigned char p, unsigned char Y, unsigned char value) {
	SETMEMREGS
	unsigned char *tabtab[] = {pitches, frequency1, frequency2, frequency3, amplitude1, amplitude2, amplitude3};
	unsigned char res = 0;
	unsigned char *rp;
	unsigned char **rpt = tabtab;
	if (p < 168 || p > 174) {
		return;
	}
	p -= 168;
	rp = rpt[p];
	rp[p] = Y;
}



// -------------------------------------------------------------------------
//Code48227
// Render a sampled sound from the sampleTable.
//
//   Phoneme   Sample Start   Sample End
//   32: S*    15             255
//   33: SH    257            511
//   34: F*    559            767
//   35: TH    583            767
//   36: /H    903            1023
//   37: /X    1135           1279
//   38: Z*    84             119
//   39: ZH    340            375
//   40: V*    596            639
//   41: DH    596            631
//
//   42: CH
//   43: **    399            511
//
//   44: J*
//   45: **    257            276
//   46: **
//
//   66: P*
//   67: **    743            767
//   68: **
//
//   69: T*
//   70: **    231            255
//   71: **
//
// The SampledPhonemesTable[] holds flags indicating if a phoneme is
// voiced or not. If the upper 5 bits are zero, the sample is voiced.
//
// Samples in the sampleTable are compressed, with bits being converted to
// bytes from high bit to low, as follows:
//
//   unvoiced 0 bit   -> X
//   unvoiced 1 bit   -> 5
//
//   voiced 0 bit     -> 6
//   voiced 1 bit     -> 24
//
// Where X is a value from the table:
//
//   { 0x18, 0x1A, 0x17, 0x17, 0x17 };
//
// The index into this table is determined by masking off the lower
// 3 bits from the SampledPhonemesTable:
//
//        index = (SampledPhonemesTable[i] & 7) - 1;
//
// For voices samples, samples are interleaved between voiced output.


// Code48227()
MODULE_PART void RenderSample(unsigned char *mem66) {
	SETMEMREGS
	int tempA;
	// current phoneme's index
	samdata->mem49 = samdata->Y;

	// mask low three bits and subtract 1 get value to
	// convert 0 bits on unvoiced samples.
	samdata->A = samdata->mem39&7;
	samdata->X = samdata->A - 1;

    // store the result
	samdata->mem56 = samdata->X;

	// determine which offset to use from table { 0x18, 0x1A, 0x17, 0x17, 0x17 }
	// T, S, Z                0          0x18
	// CH, J, SH, ZH          1          0x1A
	// P, F*, V, TH, DH       2          0x17
	// /H                     3          0x17
	// /X                     4          0x17

    // get value from the table
	samdata->mem53 = pgm_read_byte(tab48426+samdata->X); //tab48426[X];
	samdata->mem47 = samdata->X;      //46016+mem[56]*256

	// voiced sample?
	samdata->A = samdata->mem39 & 248;
	if(samdata->A == 0) {
        // voiced phoneme: Z*, ZH, V*, DH
		samdata->Y = samdata->mem49;
		samdata->A = pitches[samdata->mem49] >> 4;

		// jump to voiced portion
		goto pos48315;
	}

	samdata->Y = samdata->A ^ 255;
pos48274:

    // step through the 8 bits in the sample
	samdata->mem56 = 8;

	// get the next sample from the table
    // mem47*256 = offset to start of samples
	samdata->A = pgm_read_byte(sampleTable + samdata->mem47*256+samdata->Y); // sampleTable[mem47*256+Y];
pos48280:

    // left shift to get the high bit
	tempA = samdata->A;
	samdata->A = samdata->A << 1;
	//48281: BCC 48290

	// bit not set?
	if ((tempA & 128) == 0) {
        // convert the bit to value from table
		samdata->X = samdata->mem53;
		//mem[54296] = X;
        // output the byte
		Output8Bit(1, (samdata->X&0xf)*16);
		// if X != 0, exit loop
		if(samdata->X != 0) goto pos48296;
	}

	// output a 5 for the on bit
	Output8Bit(2, 5*16);

	//48295: NOP
pos48296:

	samdata->X = 0;

    // decrement counter
	samdata->mem56--;

	// if not done, jump to top of loop
	if (samdata->mem56 != 0) goto pos48280;

	// increment position
	samdata->Y++;
	if (samdata->Y != 0) goto pos48274;

	// restore values and return
	samdata->mem44 = 1;
	samdata->Y = samdata->mem49;
	return;

	unsigned char phase1;

pos48315:
// handle voiced samples here

   // number of samples?
	phase1 = samdata->A ^ 255;

	samdata->Y = *mem66;
	do {
		//pos48321:

        // shift through all 8 bits
		samdata->mem56 = 8;
		//A = Read(mem47, Y);

		// fetch value from table
		samdata->A = pgm_read_byte(sampleTable + samdata->mem47*256+samdata->Y); //sampleTable[mem47*256+Y];

        // loop 8 times
		//pos48327:
		do {
			//48327: ASL A
			//48328: BCC 48337

			// left shift and check high bit
			tempA = samdata->A;
			samdata->A = samdata->A << 1;
			if ((tempA & 128) != 0) {
                // if bit set, output 26
				samdata->X = 26;
				Output8Bit(3, (samdata->X&0xf)*16);
			} else {
				//timetable 4
				// bit is not set, output a 6
				samdata->X=6;
				Output8Bit(4, (samdata->X&0xf)*16);
			}
			samdata->mem56--;
		} while(samdata->mem56 != 0);

        // move ahead in the table
		samdata->Y++;

		// continue until counter done
		phase1++;

	} while (phase1 != 0);
	//	if (phase1 != 0) goto pos48321;

	// restore values and return
	samdata->A = 1;
	samdata->mem44 = 1;
	*mem66 = samdata->Y;
	samdata->Y = samdata->mem49;
	return;
}



// RENDER THE PHONEMES IN THE LIST
//
// The phoneme list is converted into sound through the steps:
//
// 1. Copy each phoneme <length> number of times into the frames list,
//    where each frame represents 10 milliseconds of sound.
//
// 2. Determine the transitions lengths between phonemes, and linearly
//    interpolate the values across the frames.
//
// 3. Offset the pitches by the fundamental frequency.
//
// 4. Render the each frame.



//void Code47574()
MODULE_PART void Render() {
	SETMEMREGS

	unsigned char phase1 = 0;  //mem43
	unsigned char phase2 = 0;
	unsigned char phase3 = 0;
	unsigned char mem66 = 0;
	unsigned char mem38 = 0;
	unsigned char mem40 = 0;
	unsigned char speedcounter = 0; //mem45
	unsigned char mem48 = 0;
	int i;
	if (phonemeIndexOutput[0] == 255) {
		return; //exit if no data
	}
	samdata->A = 0;
	samdata->X = 0;
	samdata->mem44 = 0;


// CREATE FRAMES
//
// The length parameter in the list corresponds to the number of frames
// to expand the phoneme to. Each frame represents 10 milliseconds of time.
// So a phoneme with a length of 7 = 7 frames = 70 milliseconds duration.
//
// The parameters are copied from the phoneme to the frame verbatim.


// pos47587:
do {
    // get the index
	samdata->Y = samdata->mem44;
	// get the phoneme at the index
	samdata->A = phonemeIndexOutput[samdata->mem44];
	samdata->mem56 = samdata->A;

	// if terminal phoneme, exit the loop
	if (samdata->A == 255) break;

	// period phoneme *.
	if (samdata->A == 1) {
       // add rising inflection
		samdata->A = 1;
		mem48 = 1;
		//goto pos48376;
		AddInflection(mem48, phase1);
	}
	/*
	if (A == 2) goto pos48372;
	*/

	// question mark phoneme?
	if (samdata->A == 2) {
        // create falling inflection
		mem48 = 255;
		AddInflection(mem48, phase1);
	}
	//	pos47615:

    // get the stress amount (more stress = higher pitch)
	phase1 = pgm_read_byte(tab47492 + stressOutput[samdata->Y] + 1); // tab47492[stressOutput[Y] + 1];

    // get number of frames to write
	phase2 = phonemeLengthOutput[samdata->Y];
	samdata->Y = samdata->mem56;

	// copy from the source to the frames list
	do {
		frequency1[samdata->X] = freq1data[samdata->Y];     // F1 frequency
		frequency2[samdata->X] = freq2data[samdata->Y];     // F2 frequency
		frequency3[samdata->X] = freq3data[samdata->Y];     // F3 frequency
		amplitude1[samdata->X] = pgm_read_byte(&ampl1data[samdata->Y]);     // F1 amplitude
		amplitude2[samdata->X] = pgm_read_byte(&ampl2data[samdata->Y]);     // F2 amplitude
		amplitude3[samdata->X] = pgm_read_byte(&ampl3data[samdata->Y]);     // F3 amplitude
		sampledConsonantFlag[samdata->X] = pgm_read_byte(&sampledConsonantFlags[samdata->Y]);        // phoneme data for sampled consonants
		pitches[samdata->X] = pitch + phase1;      // pitch
		samdata->X++;
		phase2--;
	} while(phase2 != 0);
	samdata->mem44++;
} while(samdata->mem44 != 0);
yield();
if (DEBUG_ESP8266SAM_LIB)
{
        PrintOutput(sampledConsonantFlag, frequency1, frequency2, frequency3, amplitude1, amplitude2, amplitude3, pitches);
}
// -------------------
//pos47694:

// CREATE TRANSITIONS
//
// Linear transitions are now created to smoothly connect the
// end of one sustained portion of a phoneme to the following
// phoneme.
//
// To do this, three tables are used:
//
//  Table         Purpose
//  =========     ==================================================
//  blendRank     Determines which phoneme's blend values are used.
//
//  blendOut      The number of frames at the end of the phoneme that
//                will be used to transition to the following phoneme.
//
//  blendIn       The number of frames of the following phoneme that
//                will be used to transition into that phoneme.
//
// In creating a transition between two phonemes, the phoneme
// with the HIGHEST rank is used. Phonemes are ranked on how much
// their identity is based on their transitions. For example,
// vowels are and diphthongs are identified by their sustained portion,
// rather than the transitions, so they are given low values. In contrast,
// stop consonants (P, B, T, K) and glides (Y, L) are almost entirely
// defined by their transitions, and are given high rank values.
//
// Here are the rankings used by SAM:
//
//     Rank    Type                         Phonemes
//     2       All vowels                   IY, IH, etc.
//     5       Diphthong endings            YX, WX, ER
//     8       Terminal liquid consonants   LX, WX, YX, N, NX
//     9       Liquid consonants            L, RX, W
//     10      Glide                        R, OH
//     11      Glide                        WH
//     18      Voiceless fricatives         S, SH, F, TH
//     20      Voiced fricatives            Z, ZH, V, DH
//     23      Plosives, stop consonants    P, T, K, KX, DX, CH
//     26      Stop consonants              J, GX, B, D, G
//     27-29   Stop consonants (internal)   **
//     30      Unvoiced consonants          /H, /X and Q*
//     160     Nasal                        M
//
// To determine how many frames to use, the two phonemes are
// compared using the blendRank[] table. The phoneme with the
// higher rank is selected. In case of a tie, a blend of each is used:
//
//      if blendRank[phoneme1] ==  blendRank[phomneme2]
//          // use lengths from each phoneme
//          outBlendFrames = outBlend[phoneme1]
//          inBlendFrames = outBlend[phoneme2]
//      else if blendRank[phoneme1] > blendRank[phoneme2]
//          // use lengths from first phoneme
//          outBlendFrames = outBlendLength[phoneme1]
//          inBlendFrames = inBlendLength[phoneme1]
//      else
//          // use lengths from the second phoneme
//          // note that in and out are SWAPPED!
//          outBlendFrames = inBlendLength[phoneme2]
//          inBlendFrames = outBlendLength[phoneme2]
//
// Blend lengths can't be less than zero.
//
// Transitions are assumed to be symetrical, so if the transition
// values for the second phoneme are used, the inBlendLength and
// outBlendLength values are SWAPPED.
//
// For most of the parameters, SAM interpolates over the range of the last
// outBlendFrames-1 and the first inBlendFrames.
//
// The exception to this is the Pitch[] parameter, which is interpolates the
// pitch from the CENTER of the current phoneme to the CENTER of the next
// phoneme.
//
// Here are two examples. First, For example, consider the word "SUN" (S AH N)
//
//    Phoneme   Duration    BlendWeight    OutBlendFrames    InBlendFrames
//    S         2           18             1                 3
//    AH        8           2              4                 4
//    N         7           8              1                 2
//
// The formant transitions for the output frames are calculated as follows:
//
//     flags ampl1 freq1 ampl2 freq2 ampl3 freq3 pitch
//    ------------------------------------------------
// S
//    241     0     6     0    73     0    99    61   Use S (weight 18) for transition instead of AH (weight 2)
//    241     0     6     0    73     0    99    61   <-- (OutBlendFrames-1) = (1-1) = 0 frames
// AH
//      0     2    10     2    66     0    96    59 * <-- InBlendFrames = 3 frames
//      0     4    14     3    59     0    93    57 *
//      0     8    18     5    52     0    90    55 *
//      0    15    22     9    44     1    87    53
//      0    15    22     9    44     1    87    53
//      0    15    22     9    44     1    87    53   Use N (weight 8) for transition instead of AH (weight 2).
//      0    15    22     9    44     1    87    53   Since N is second phoneme, reverse the IN and OUT values.
//      0    11    17     8    47     1    98    56 * <-- (InBlendFrames-1) = (2-1) = 1 frames
// N
//      0     8    12     6    50     1   109    58 * <-- OutBlendFrames = 1
//      0     5     6     5    54     0   121    61
//      0     5     6     5    54     0   121    61
//      0     5     6     5    54     0   121    61
//      0     5     6     5    54     0   121    61
//      0     5     6     5    54     0   121    61
//      0     5     6     5    54     0   121    61
//
// Now, consider the reverse "NUS" (N AH S):
//
//     flags ampl1 freq1 ampl2 freq2 ampl3 freq3 pitch
//    ------------------------------------------------
// N
//     0     5     6     5    54     0   121    61
//     0     5     6     5    54     0   121    61
//     0     5     6     5    54     0   121    61
//     0     5     6     5    54     0   121    61
//     0     5     6     5    54     0   121    61
//     0     5     6     5    54     0   121    61   Use N (weight 8) for transition instead of AH (weight 2)
//     0     5     6     5    54     0   121    61   <-- (OutBlendFrames-1) = (1-1) = 0 frames
// AH
//     0     8    11     6    51     0   110    59 * <-- InBlendFrames = 2
//     0    11    16     8    48     0    99    56 *
//     0    15    22     9    44     1    87    53   Use S (weight 18) for transition instead of AH (weight 2)
//     0    15    22     9    44     1    87    53   Since S is second phoneme, reverse the IN and OUT values.
//     0     9    18     5    51     1    90    55 * <-- (InBlendFrames-1) = (3-1) = 2
//     0     4    14     3    58     1    93    57 *
// S
//   241     2    10     2    65     1    96    59 * <-- OutBlendFrames = 1
//   241     0     6     0    73     0    99    61

	samdata->A = 0;
	samdata->mem44 = 0;
	samdata->mem49 = 0; // mem49 starts at as 0
	samdata->X = 0;
	while(1) //while No. 1
	{

        // get the current and following phoneme
		samdata->Y = phonemeIndexOutput[samdata->X];
		samdata->A = phonemeIndexOutput[samdata->X+1];
		samdata->X++;

		// exit loop at end token
		if (samdata->A == 255) break;//goto pos47970;


        // get the ranking of each phoneme
		samdata->X = samdata->A;
		samdata->mem56 = pgm_read_byte(blendRank+samdata->A); //blendRank[A];
		samdata->A = pgm_read_byte(blendRank+samdata->Y); //blendRank[Y];

		// compare the rank - lower rank value is stronger
		if (samdata->A == samdata->mem56) {
            // same rank, so use out blend lengths from each phoneme
			phase1 = pgm_read_byte(outBlendLength+samdata->Y);//outBlendLength[Y];
			phase2 = pgm_read_byte(outBlendLength+samdata->X);//outBlendLength[X];
		} else if (samdata->A < samdata->mem56) {
            // first phoneme is stronger, so us it's blend lengths
			phase1 = pgm_read_byte(inBlendLength+samdata->X);//inBlendLength[X];
			phase2 = pgm_read_byte(outBlendLength+samdata->X);//outBlendLength[X];
		} else {
            // second phoneme is stronger, so use it's blend lengths
            // note the out/in are swapped
			phase1 = pgm_read_byte(outBlendLength+samdata->Y);//outBlendLength[Y];
			phase2 = pgm_read_byte(inBlendLength+samdata->Y);//inBlendLength[Y];
		}

		samdata->Y = samdata->mem44;
		samdata->A = samdata->mem49 + phonemeLengthOutput[samdata->mem44]; // A is mem49 + length
		samdata->mem49 = samdata->A; // mem49 now holds length + position
		samdata->A = samdata->A + phase2; //Maybe Problem because of carry flag

		//47776: ADC 42
		speedcounter = samdata->A;
		samdata->mem47 = 168;
		phase3 = samdata->mem49 - phase1; // what is mem49
		samdata->A = phase1 + phase2; // total transition?
		mem38 = samdata->A;

		samdata->X = samdata->A;
		samdata->X -= 2;
		if ((samdata->X & 128) == 0)
		do   //while No. 2
		{
			//pos47810:

          // mem47 is used to index the tables:
          // 168  pitches[]
          // 169  frequency1
          // 170  frequency2
          // 171  frequency3
          // 172  amplitude1
          // 173  amplitude2
          // 174  amplitude3

			mem40 = mem38;

			if (samdata->mem47 == 168)     // pitch
			{

               // unlike the other values, the pitches[] interpolates from
               // the middle of the current phoneme to the middle of the
               // next phoneme

				unsigned char mem36, mem37;
				// half the width of the current phoneme
				mem36 = phonemeLengthOutput[samdata->mem44] >> 1;
				// half the width of the next phoneme
				mem37 = phonemeLengthOutput[samdata->mem44+1] >> 1;
				// sum the values
				mem40 = mem36 + mem37; // length of both halves
				mem37 += samdata->mem49; // center of next phoneme
				mem36 = samdata->mem49 - mem36; // center index of current phoneme
				samdata->A = Read(samdata->mem47, mem37); // value at center of next phoneme - end interpolation value
				//A = mem[address];

				samdata->Y = mem36; // start index of interpolation
				samdata->mem53 = samdata->A - Read(samdata->mem47, mem36); // value to center of current phoneme
			} else {
                // value to interpolate to
				samdata->A = Read(samdata->mem47, speedcounter);
				// position to start interpolation from
				samdata->Y = phase3;
				// value to interpolate from
				samdata->mem53 = samdata->A - Read(samdata->mem47, phase3);
			}

			//Code47503(mem40);
			// ML : Code47503 is division with remainder, and mem50 gets the sign

			// calculate change per frame
			signed char m53 = (signed char)samdata->mem53;
			samdata->mem50 = samdata->mem53 & 128;
			unsigned char m53abs = abs(m53);
			//samdata->mem51 = m53abs % mem40; //abs((char)m53) % mem40;
			//samdata->mem53 = (unsigned char)((signed char)(m53) / mem40);
			samdata->mem51 = __umodsi3(m53abs, mem40); //abs((char)m53) % mem40;
			samdata->mem53 = (unsigned char)(__divsi3((signed char)(m53), mem40));

            // interpolation range
			samdata->X = mem40; // number of frames to interpolate over
			samdata->Y = phase3; // starting frame


            // linearly interpolate values

			samdata->mem56 = 0;
			//47907: CLC
			//pos47908:
			while(1)     //while No. 3
			{
				samdata->A = Read(samdata->mem47, samdata->Y) + samdata->mem53; //carry alway cleared

				mem48 = samdata->A;
				samdata->Y++;
				samdata->X--;
				if(samdata->X == 0) break;

				samdata->mem56 += samdata->mem51;
				if (samdata->mem56 >= mem40)  //???
				{
					samdata->mem56 -= mem40; //carry? is set
					//if ((mem56 & 128)==0)
					if ((samdata->mem50 & 128)==0)
					{
						//47935: BIT 50
						//47937: BMI 47943
						if(mem48 != 0) mem48++;
					} else mem48--;
				}
				//pos47945:
				Write(samdata->mem47, samdata->Y, mem48);
			} //while No. 3

			//pos47952:
			samdata->mem47++;
			//if (mem47 != 175) goto pos47810;
		} while (samdata->mem47 != 175);     //while No. 2
		//pos47963:
		samdata->mem44++;
		samdata->X = samdata->mem44;
	}  //while No. 1
  yield();
	//goto pos47701;
	//pos47970:

    // add the length of this phoneme
	mem48 = samdata->mem49 + phonemeLengthOutput[samdata->mem44];


// ASSIGN PITCH CONTOUR
//
// This subtracts the F1 frequency from the pitch to create a
// pitch contour. Without this, the output would be at a single
// pitch level (monotone).


	// don't adjust pitch if in sing mode
	if (!singmode) {
        // iterate through the buffer
		for(i=0; i<256; i++) {
            // subtract half the frequency of the formant 1.
            // this adds variety to the voice
    		pitches[i] -= (frequency1[i] >> 1);
        }
	}

	phase1 = 0;
	phase2 = 0;
	phase3 = 0;
	samdata->mem49 = 0;
	speedcounter = 72; //sam standard speed

// RESCALE AMPLITUDE
//
// Rescale volume from a linear scale to decibels.
//

	//amplitude rescaling
	for (i = 255; i >= 0; i--) {
		amplitude1[i] = pgm_read_byte(amplitudeRescale + amplitude1[i]);//amplitudeRescale[amplitude1[i]];
		amplitude2[i] = pgm_read_byte(amplitudeRescale + amplitude2[i]);//amplitudeRescale[amplitude2[i]];
		amplitude3[i] = pgm_read_byte(amplitudeRescale + amplitude3[i]);//amplitudeRescale[amplitude3[i]];
	}

	samdata->Y = 0;
	samdata->A = pitches[0];
	samdata->mem44 = samdata->A;
	samdata->X = samdata->A;
	mem38 = samdata->A - (samdata->A>>2);     // 3/4*A ???
	yield();
	if (DEBUG_ESP8266SAM_LIB)
	{
        PrintOutput(sampledConsonantFlag, frequency1, frequency2, frequency3, amplitude1, amplitude2, amplitude3, pitches);
	}

// PROCESS THE FRAMES
//
// In traditional vocal synthesis, the glottal pulse drives filters, which
// are attenuated to the frequencies of the formants.
//
// SAM generates these formants directly with sin and rectangular waves.
// To simulate them being driven by the glottal pulse, the waveforms are
// reset at the beginning of each glottal pulse.

	//finally the loop for sound output
	//pos48078:
	while(1) {
        // get the sampled information on the phoneme
		samdata->A = sampledConsonantFlag[samdata->Y];
		samdata->mem39 = samdata->A;

		// unvoiced sampled phoneme?
		samdata->A = samdata->A & 248;
		if(samdata->A != 0) {
            // render the sample for the phoneme
			RenderSample(&mem66);
			// skip ahead two in the phoneme buffer
			samdata->Y += 2;
			mem48 -= 2;
		} else {
            // simulate the glottal pulse and formants
			unsigned char ary[5];
			unsigned int p1 = phase1 * 256; // Fixed point integers because we need to divide later on
			unsigned int p2 = phase2 * 256;
			unsigned int p3 = phase3 * 256;

			for (int k=0; k<5; k++) {
				signed char sp1 = (signed char)pgm_read_byte(&sinus[0xff & (p1>>8)]);
				signed char sp2 = (signed char)pgm_read_byte(&sinus[0xff & (p2>>8)]);
				signed char rp3 = (signed char)pgm_read_byte(&rectangle[0xff & (p3>>8)]);
				signed int sin1 = sp1 * ((unsigned char)amplitude1[samdata->Y] & 0x0f);
				signed int sin2 = sp2 * ((unsigned char)amplitude2[samdata->Y] & 0x0f);
				signed int rect = rp3 * ((unsigned char)amplitude3[samdata->Y] & 0x0f);
				signed int mux = sin1 + sin2 + rect;
				mux /= 32;
				mux += 128; // Go from signed to unsigned amplitude
				ary[k] = mux;
				p1 += ((int)frequency1[samdata->Y]) * 256 / 4; // Compromise, this becomes a shift and works well
				p2 += ((int)frequency2[samdata->Y]) * 256 / 4;
				p3 += ((int)frequency3[samdata->Y]) * 256 / 4;
			}
			// output the accumulated value
			Output8BitAry(0, ary);
			speedcounter--;
			if (speedcounter != 0) goto pos48155;
			samdata->Y++; //go to next amplitude

			// decrement the frame count
			mem48--;
		}

		// if the frame count is zero, exit the loop
		if(mem48 == 0) 	return;
		speedcounter = speed;
pos48155:

        // decrement the remaining length of the glottal pulse
		samdata->mem44--;

		// finished with a glottal pulse?
		if(samdata->mem44 == 0) {
pos48159:
            // fetch the next glottal pulse length
			samdata->A = pitches[samdata->Y];
			samdata->mem44 = samdata->A;
			samdata->A = samdata->A - (samdata->A>>2);
			mem38 = samdata->A;

			// reset the formant wave generators to keep them in
			// sync with the glottal pulse
			phase1 = 0;
			phase2 = 0;
			phase3 = 0;
			continue;
		}

		// decrement the count
		mem38--;

		// is the count non-zero and the sampled flag is zero?
		if((mem38 != 0) || (samdata->mem39 == 0)) {
            // reset the phase of the formants to match the pulse
			phase1 += frequency1[samdata->Y];
			phase2 += frequency2[samdata->Y];
			phase3 += frequency3[samdata->Y];
			continue;
		}

		// voiced sampled phonemes interleave the sample with the
		// glottal pulse. The sample flag is non-zero, so render
		// the sample for the phoneme.
		RenderSample(&mem66);
		goto pos48159;
	} //while

}


// Create a rising or falling inflection 30 frames prior to
// index X. A rising inflection is used for questions, and
// a falling inflection is used for statements.

MODULE_PART void AddInflection(unsigned char mem48, unsigned char phase1) {
	SETMEMREGS
	//pos48372:
	//	mem48 = 255;
//pos48376:

    // store the location of the punctuation
	samdata->mem49 = samdata->X;
	samdata->A = samdata->X;
	int Atemp = samdata->A;

	// backup 30 frames
	samdata->A = samdata->A - 30;
	// if index is before buffer, point to start of buffer
	if (Atemp <= 30) samdata->A=0;
	samdata->X = samdata->A;

	// FIXME: Explain this fix better, it's not obvious
	// ML : A =, fixes a problem with invalid pitch with '.'
	while( (samdata->A=pitches[samdata->X]) == 127) samdata->X++;


pos48398:
	//48398: CLC
	//48399: ADC 48

	// add the inflection direction
	samdata->A += mem48;
	phase1 = samdata->A;

	// set the inflection
	pitches[samdata->X] = samdata->A;
pos48406:

    // increment the position
	samdata->X++;

	// exit if the punctuation has been reached
	if (samdata->X == samdata->mem49) return; //goto pos47615;
	if (pitches[samdata->X] == 255) goto pos48406;
	samdata->A = phase1;
	goto pos48398;
}

/*
    SAM's voice can be altered by changing the frequencies of the
    mouth formant (F1) and the throat formant (F2). Only the voiced
    phonemes (5-29 and 48-53) are altered.
*/
// mouth formants (F1) 5..29
const unsigned char mouthFormants5_29[30] PROGMEM = {
                0, 0, 0, 0, 0, 10,
                14, 19, 24, 27, 23, 21, 16, 20, 14, 18, 14, 18, 18,
                16, 13, 15, 11, 18, 14, 11, 9, 6, 6, 6};

// throat formants (F2) 5..29
const unsigned char throatFormants5_29[30] PROGMEM = {
        255, 255,
        255, 255, 255, 84, 73, 67, 63, 40, 44, 31, 37, 45, 73, 49,
        36, 30, 51, 37, 29, 69, 24, 50, 30, 24, 83, 46, 54, 86};

        // there must be no zeros in this 2 tables
        // formant 1 frequencies (mouth) 48..53
const unsigned char mouthFormants48_53[6] PROGMEM = {19, 27, 21, 27, 18, 13};

        // formant 2 frequencies (throat) 48..53
const unsigned char throatFormants48_53[6] PROGMEM = {72, 39, 31, 43, 30, 34};

void SetMouthThroat(unsigned char _mouth, unsigned char _throat) {
	SETMEMREGS
	unsigned char initialFrequency;
	unsigned char newFrequency = 0;
	//unsigned char mouth; //mem38880
	//unsigned char throat; //mem38881

	unsigned char pos = 5; //mem39216
//pos38942:
	// recalculate formant frequencies 5..29 for the mouth (F1) and throat (F2)
	while(pos != 30) {
		// recalculate mouth frequency
		initialFrequency = pgm_read_byte(&mouthFormants5_29[pos]);
		if (initialFrequency != 0) newFrequency = trans(_mouth, initialFrequency);
		freq1data[pos] = newFrequency;

		// recalculate throat frequency
		initialFrequency = pgm_read_byte(&throatFormants5_29[pos]);
		if(initialFrequency != 0) newFrequency = trans(_throat, initialFrequency);
		freq2data[pos] = newFrequency;
		pos++;
	}

//pos39059:
	// recalculate formant frequencies 48..53
	pos = 48;
	samdata->Y = 0;
    while(pos != 54) {
		// recalculate F1 (mouth formant)
		initialFrequency = pgm_read_byte(&mouthFormants48_53[samdata->Y]);
		newFrequency = trans(_mouth, initialFrequency);
		freq1data[pos] = newFrequency;

		// recalculate F2 (throat formant)
		initialFrequency = pgm_read_byte(&throatFormants48_53[samdata->Y]);
		newFrequency = trans(_throat, initialFrequency);
		freq2data[pos] = newFrequency;
		samdata->Y++;
		pos++;
	}
}


//return = (mem39212*mem39213) >> 1
MODULE_PART unsigned char trans(unsigned char mem39212, unsigned char mem39213) {
	SETMEMREGS
	//pos39008:
	unsigned char carry;
	int temp;
	unsigned char mem39214, mem39215;
	samdata->A = 0;
	mem39215 = 0;
	mem39214 = 0;
	samdata->X = 8;
	do {
		carry = mem39212 & 1;
		mem39212 = mem39212 >> 1;
		if (carry != 0) {
			/*
						39018: LSR 39212
						39021: BCC 39033
						*/
			carry = 0;
			samdata->A = mem39215;
			temp = (int)samdata->A + (int)mem39213;
			samdata->A = samdata->A + mem39213;
			if (temp > 255) carry = 1;
			mem39215 = samdata->A;
		}
		temp = mem39215 & 1;
		mem39215 = (mem39215 >> 1) | (carry?128:0);
		carry = temp;
		//39033: ROR 39215
		samdata->X--;
	} while (samdata->X != 0);
	temp = mem39214 & 128;
	mem39214 = (mem39214 << 1) | (carry?1:0);
	carry = temp;
	temp = mem39215 & 128;
	mem39215 = (mem39215 << 1) | (carry?1:0);
	carry = temp;

	return mem39215;
}
