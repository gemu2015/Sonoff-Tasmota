//#include <stdio.h> // define printf()
//#include <string.h> // strlen()
//#include <stdlib.h>
//#include <stddef.h> // define NULL
//#include "esp8266sam_debug.h"
#include "sam.h"
#include "render.h"
#include "SamTabs.h"
#include "SamData.h"

//standard sam sound

typedef struct {
	unsigned char speed;
	unsigned char pitch;
	static unsigned char mouth;
	static unsigned char throat;
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
	static unsigned char A, X, Y;
// contains the final soundbuffer
	int bufferpos;
//char *buffer = NULL;
} SAM_MEM;

SAM_MEM sam;

#define input (samdata->sam.input)
#define stress (samdata->sam.stress)
#define phonemeLength (samdata->sam.phonemeLength)
#define phonemeindex (samdata->sam.phonemeindex)
#define phonemeIndexOutput (samdata->sam.phonemeIndexOutput)
#define stressOutput (samdata->sam.stressOutput)
#define phonemeLengthOutput (samdata->sam.phonemeLengthOutput)


void SetInput(char *_input) {
	SETREGS
	int i, l;
	l = strlen(_input);
	if (l > 254) l = 254;
	for(i = 0; i < l; i++)
		input[i] = _input[i];
	input[l] = 0;
}

void SetSpeed(unsigned char _speed) {
SETMEMREGS
	sam.speed = _speed;
};
void SetPitch(unsigned char _pitch) {
SETMEMREGS
	sam.pitch = _pitch;
};
void SetMouth(unsigned char _mouth) {
SETMEMREGS
	sam.mouth = _mouth;
};
void SetThroat(unsigned char _throat) {
SETMEMREGS
	sam.throat = _throat;
};
void EnableSingmode(int x) {
SETMEMREGS
	sam.singmode = x;
};
//char* GetBuffer(){return buffer;};
int GetBufferLength(){
SETMEMREGS
	return sam.bufferpos;
};

MODULE_PART void Init();
MODULE_PART int Parser1();
MODULE_PART void Parser2();
MODULE_PART int SAMMain();
MODULE_PART void CopyStress();
MODULE_PART void SetPhonemeLength();
MODULE_PART void AdjustLengths();
MODULE_PART void Code41240();
MODULE_PART void Insert(unsigned char position, unsigned char mem60, unsigned char mem59, unsigned char mem58);
MODULE_PART void InsertBreath();
MODULE_PART void PrepareOutput();
MODULE_PART void SetMouthThroat(unsigned char mouth, unsigned char throat);

// 168=pitches
// 169=frequency1
// 170=frequency2
// 171=frequency3
// 172=amplitude1
// 173=amplitude2
// 174=amplitude3


void Init() {
	SETMEMREGS
	int i;
	SetMouthThroat( sam.mouth, sam.throat);

	sam.bufferpos = 0;
	// TODO, check for free the memory, 10 seconds of output should be more than enough
//	buffer = malloc(22050*10);

	/*
	freq2data = &mem[45136];
	freq1data = &mem[45056];
	freq3data = &mem[45216];
	*/
	//pitches = &mem[43008];
	/*
	frequency1 = &mem[43264];
	frequency2 = &mem[43520];
	frequency3 = &mem[43776];
	*/
	/*
	amplitude1 = &mem[44032];
	amplitude2 = &mem[44288];
	amplitude3 = &mem[44544];
	*/
	//phoneme = &mem[39904];
	/*
	ampl1data = &mem[45296];
	ampl2data = &mem[45376];
	ampl3data = &mem[45456];
	*/

	for(i = 0; i < 256; i++) {
		stress[i] = 0;
		phonemeLength[i] = 0;
	}

	for(i = 0; i < 60; i++) {
		phonemeIndexOutput[i] = 0;
		stressOutput[i] = 0;
		phonemeLengthOutput[i] = 0;
	}
	phonemeindex[255] = 255; //to prevent buffer overflow // ML : changed from 32 to 255 to stop freezing with long inputs

}

void (*outcb)(void *, unsigned char) = NULL;
void *outcbdata = NULL;

//int Code39771()
int SAMMain( void (*cb)(void *, unsigned char), void *cbd ) {
	SETMEMREGS
  	outcb = cb;
  	outcbdata = cbd;
	Init();
	phonemeindex[255] = 32; //to prevent buffer overflow

	if (!Parser1()) return 0;
	if (DEBUG_ESP8266SAM_LIB)
		PrintPhonemes(phonemeindex, phonemeLength, stress);
	Parser2();
	CopyStress();
	SetPhonemeLength();
	AdjustLengths();
	Code41240();
	do
	{
		sam.A = phonemeindex[sam.X];
		if (sam.A > 80)
		{
			phonemeindex[sam.X] = 255;
			break; // error: delete all behind it
		}
		sam.X++;
	} while (sam.X != 0);

	//pos39848:
	InsertBreath();

	//mem[40158] = 255;
	if (DEBUG_ESP8266SAM_LIB)
	{
		PrintPhonemes(phonemeindex, phonemeLength, stress);
	}

	PrepareOutput();

	return 1;
}

int SAMPrepare() {
  Init();
  phonemeindex[255] = 32; //to prevent buffer overflow

  if (!Parser1()) return 0;
  Parser2();
  CopyStress();
  SetPhonemeLength();
  AdjustLengths();
  Code41240();
  do
  {
    sam.A = phonemeindex[sam.X];
    if (sam.A > 80)
    {
      phonemeindex[sam.X] = 255;
      break; // error: delete all behind it
    }
    sam.X++;
  } while (sam.X != 0);

  InsertBreath();
  return 1;
}



//void Code48547()
void PrepareOutput() {
	sam.A = 0;
	sam.X = 0;
	sam.Y = 0;

	//pos48551:
	while(1)
	{
		sam.A = phonemeindex[sam.X];
		if (sam.A == 255)
		{
			sam.A = 255;
			phonemeIndexOutput[sam.Y] = 255;
			Render();
			return;
		}
		if (sam.A == 254)
		{
			sam.X++;
			int temp = sam.X;
			//mem[48546] = X;
			phonemeIndexOutput[sam.Y] = 255;
			Render();
			//X = mem[48546];
			sam.X=temp;
			sam.Y = 0;
			continue;
		}

		if (sam.A == 0)
		{
			sam.X++;
			continue;
		}

		phonemeIndexOutput[sam.Y] = sam.A;
		phonemeLengthOutput[sam.Y] = phonemeLength[sam.X];
		stressOutput[sam.Y] = stress[sam.X];
		sam.X++;
		sam.Y++;
	}
}

//void Code48431()
void InsertBreath() {
	unsigned char mem54;
	unsigned char mem55;
	unsigned char index; //variable Y
	mem54 = 255;
	sam.X++;
	mem55 = 0;
	unsigned char mem66 = 0;
	while(1)
	{
		//pos48440:
		sam.X = mem66;
		index = phonemeindex[sam.X];
		if (index == 255) return;
		mem55 += phonemeLength[sam.X];

		if (mem55 < 232)
		{
			if (index != 254) // ML : Prevents an index out of bounds problem
			{
				sam.A = flags2[index]&1;
				if(sam.A != 0)
				{
					sam.X++;
					mem55 = 0;
					Insert(sam.X, 254, sam.mem59, 0);
					mem66++;
					mem66++;
					continue;
				}
			}
			if (index == 0) mem54 = sam.X;
			mem66++;
			continue;
		}
		sam.X = mem54;
		phonemeindex[sam.X] = 31;   // 'Q*' glottal stop
		phonemeLength[sam.X] = 4;
		stress[sam.X] = 0;
		sam.X++;
		mem55 = 0;
		Insert(sam.X, 254, sam.mem59, 0);
		sam.X++;
		mem66 = sam.X;
	}

}

// Iterates through the phoneme buffer, copying the stress value from
// the following phoneme under the following circumstance:

//     1. The current phoneme is voiced, excluding plosives and fricatives
//     2. The following phoneme is voiced, excluding plosives and fricatives, and
//     3. The following phoneme is stressed
//
//  In those cases, the stress value+1 from the following phoneme is copied.
//
// For example, the word LOITER is represented as LOY5TER, with as stress
// of 5 on the diphtong OY. This routine will copy the stress value of 6 (5+1)
// to the L that precedes it.


//void Code41883()
void CopyStress() {
    // loop thought all the phonemes to be output
	unsigned char pos=0; //mem66
	while(1)
	{
        // get the phomene
		sam.Y = phonemeindex[pos];

	    // exit at end of buffer
		if (sam.Y == 255) return;

		// if CONSONANT_FLAG set, skip - only vowels get stress
		if ((flags[sam.Y] & 64) == 0) {pos++; continue;}
		// get the next phoneme
		sam.Y = phonemeindex[pos+1];
		if (sam.Y == 255) //prevent buffer overflow
		{
			pos++; continue;
		} else
		// if the following phoneme is a vowel, skip
		if ((flags[sam.Y] & 128) == 0)  {pos++; continue;}

        // get the stress value at the next position
		sam.Y = stress[pos+1];

		// if next phoneme is not stressed, skip
		if (sam.Y == 0)  {pos++; continue;}

		// if next phoneme is not a VOWEL OR ER, skip
		if ((sam.Y & 128) != 0)  {pos++; continue;}

		// copy stress from prior phoneme to this one
		stress[pos] = sam.Y+1;

		// advance pointer
		pos++;
	}

}


//void Code41014()
void Insert(unsigned char position/*var57*/, unsigned char mem60, unsigned char mem59, unsigned char mem58) {
	int i;
	for(i=253; i >= position; i--) // ML : always keep last safe-guarding 255
	{
		phonemeindex[i+1] = phonemeindex[i];
		phonemeLength[i+1] = phonemeLength[i];
		stress[i+1] = stress[i];
	}

	phonemeindex[position] = mem60;
	phonemeLength[position] = mem59;
	stress[position] = mem58;
	return;
}

// The input[] buffer contains a string of phonemes and stress markers along
// the lines of:
//
//     DHAX KAET IHZ AH5GLIY. <0x9B>
//
// The byte 0x9B marks the end of the buffer. Some phonemes are 2 bytes
// long, such as "DH" and "AX". Others are 1 byte long, such as "T" and "Z".
// There are also stress markers, such as "5" and ".".
//
// The first character of the phonemes are stored in the table signInputTable1[].
// The second character of the phonemes are stored in the table signInputTable2[].
// The stress characters are arranged in low to high stress order in stressInputTable[].
//
// The following process is used to parse the input[] buffer:
//
// Repeat until the <0x9B> character is reached:
//
//        First, a search is made for a 2 character match for phonemes that do not
//        end with the '*' (wildcard) character. On a match, the index of the phoneme
//        is added to phonemeIndex[] and the buffer position is advanced 2 bytes.
//
//        If this fails, a search is made for a 1 character match against all
//        phoneme names ending with a '*' (wildcard). If this succeeds, the
//        phoneme is added to phonemeIndex[] and the buffer position is advanced
//        1 byte.
//
//        If this fails, search for a 1 character match in the stressInputTable[].
//        If this succeeds, the stress value is placed in the last stress[] table
//        at the same index of the last added phoneme, and the buffer position is
//        advanced by 1 byte.
//
//        If this fails, return a 0.
//
// On success:
//
//    1. phonemeIndex[] will contain the index of all the phonemes.
//    2. The last index in phonemeIndex[] will be 255.
//    3. stress[] will contain the stress value for each phoneme

// input[] holds the string of phonemes, each two bytes wide
// signInputTable1[] holds the first character of each phoneme
// signInputTable2[] holds te second character of each phoneme
// phonemeIndex[] holds the indexes of the phonemes after parsing input[]
//
// The parser scans through the input[], finding the names of the phonemes
// by searching signInputTable1[] and signInputTable2[]. On a match, it
// copies the index of the phoneme into the phonemeIndexTable[].
//
// The character <0x9B> marks the end of text in input[]. When it is reached,
// the index 255 is placed at the end of the phonemeIndexTable[], and the
// function returns with a 1 indicating success.
int Parser1() {
	int i;
	unsigned char sign1;
	unsigned char sign2;
	unsigned char position = 0;
	sam.X = 0;
	sam.A = 0;
	sam.Y = 0;

	// CLEAR THE STRESS TABLE
	for(i=0; i<256; i++)
		stress[i] = 0;

  // THIS CODE MATCHES THE PHONEME LETTERS TO THE TABLE
	// pos41078:
	while(1)
	{
        // GET THE FIRST CHARACTER FROM THE PHONEME BUFFER
		sign1 = input[sam.X];
		// TEST FOR 155 (�) END OF LINE MARKER
		if (sign1 == 155)
		{
           // MARK ENDPOINT AND RETURN
			phonemeindex[position] = 255;      //mark endpoint
			// REACHED END OF PHONEMES, SO EXIT
			return 1;       //all ok
		}

		// GET THE NEXT CHARACTER FROM THE BUFFER
		sam.X++;
		sign2 = input[sam.X];

		// NOW sign1 = FIRST CHARACTER OF PHONEME, AND sign2 = SECOND CHARACTER OF PHONEME

       // TRY TO MATCH PHONEMES ON TWO TWO-CHARACTER NAME
       // IGNORE PHONEMES IN TABLE ENDING WITH WILDCARDS

       // SET INDEX TO 0
		sam.Y = 0;
pos41095:

         // GET FIRST CHARACTER AT POSITION Y IN signInputTable
         // --> should change name to PhonemeNameTable1
		sam.A = pgm_read_byte(signInputTable1+sam.Y);//signInputTable1[Y];

		// FIRST CHARACTER MATCHES?
		if (sam.A == sign1)
		{
           // GET THE CHARACTER FROM THE PhonemeSecondLetterTable
			sam.A = pgm_read_byte(signInputTable2+sam.Y);//signInputTable2[Y];
			// NOT A SPECIAL AND MATCHES SECOND CHARACTER?
			if ((sam.A != '*') && (sam.A == sign2))
			{
               // STORE THE INDEX OF THE PHONEME INTO THE phomeneIndexTable
				phonemeindex[position] = sam.Y;

				// ADVANCE THE POINTER TO THE phonemeIndexTable
				position++;
				// ADVANCE THE POINTER TO THE phonemeInputBuffer
				sam.X++;

				// CONTINUE PARSING
				continue;
			}
		}

		// NO MATCH, TRY TO MATCH ON FIRST CHARACTER TO WILDCARD NAMES (ENDING WITH '*')

		// ADVANCE TO THE NEXT POSITION
		sam.Y++;
		// IF NOT END OF TABLE, CONTINUE
		if (sam.Y != 81) goto pos41095;

// REACHED END OF TABLE WITHOUT AN EXACT (2 CHARACTER) MATCH.
// THIS TIME, SEARCH FOR A 1 CHARACTER MATCH AGAINST THE WILDCARDS

// RESET THE INDEX TO POINT TO THE START OF THE PHONEME NAME TABLE
		sam.Y = 0;
pos41134:
// DOES THE PHONEME IN THE TABLE END WITH '*'?
		if (pgm_read_byte(signInputTable2+sam.Y)/*signInputTable2[Y]*/ == '*')
		{
// DOES THE FIRST CHARACTER MATCH THE FIRST LETTER OF THE PHONEME
			if (pgm_read_byte(signInputTable1+sam.Y)/*]signInputTable1[Y]*/ == sign1)
			{
                // SAVE THE POSITION AND MOVE AHEAD
				phonemeindex[position] = sam.Y;

				// ADVANCE THE POINTER
				position++;

				// CONTINUE THROUGH THE LOOP
				continue;
			}
		}
		sam.Y++;
		if (sam.Y != 81) goto pos41134; //81 is size of PHONEME NAME table

// FAILED TO MATCH WITH A WILDCARD. ASSUME THIS IS A STRESS
// CHARACTER. SEARCH THROUGH THE STRESS TABLE

        // SET INDEX TO POSITION 8 (END OF STRESS TABLE)
		sam.Y = 8;

       // WALK BACK THROUGH TABLE LOOKING FOR A MATCH
		while( (sign1 != pgm_read_byte(stressInputTable+sam.Y)/*stressInputTable[Y]*/) && (sam.Y>0))
		{
  // DECREMENT INDEX
			sam.Y--;
		}

        // REACHED THE END OF THE SEARCH WITHOUT BREAKING OUT OF LOOP?
		if (sam.Y == 0)
		{
			//mem[39444] = X;
			//41181: JSR 42043 //Error
           // FAILED TO MATCH ANYTHING, RETURN 0 ON FAILURE
			return 0;
		}
// SET THE STRESS FOR THE PRIOR PHONEME
		stress[position-1] = sam.Y;
	} //while
}




//change phonemelength depedendent on stress
//void Code41203()
void SetPhonemeLength() {
	unsigned char A;
	int position = 0;
	while(phonemeindex[position] != 255 )
	{
		A = stress[position];
		//41218: BMI 41229
		if ((A == 0) || ((A&128) != 0))
		{
			phonemeLength[position] = pgm_read_byte(&phonemeLengthTable[phonemeindex[position]]);
		} else
		{
			phonemeLength[position] = pgm_read_byte(&phonemeStressedLengthTable[phonemeindex[position]]);
		}
		position++;
	}
}


void Code41240() {
	unsigned char pos=0;

	while(phonemeindex[pos] != 255)
	{
		unsigned char index; //register AC
		sam.X = pos;
		index = phonemeindex[pos];
		if ((flags[index]&2) == 0)
		{
			pos++;
			continue;
		} else
		if ((flags[index]&1) == 0)
		{
			Insert(pos+1, index+1, pgm_read_byte(&phonemeLengthTable[index+1]), stress[pos]);
			Insert(pos+2, index+2, pgm_read_byte(&phonemeLengthTable[index+2]), stress[pos]);
			pos += 3;
			continue;
		}

		do
		{
			sam.X++;
			sam.A = phonemeindex[sam.X];
		} while(sam.A==0);

		if (sam.A != 255)
		{
			if ((flags[sam.A] & 8) != 0)  {pos++; continue;}
			if ((sam.A == 36) || (sam.A == 37)) {pos++; continue;} // '/H' '/X'
		}

		Insert(pos+1, index+1, pgm_read_byte(&phonemeLengthTable[index+1]), stress[pos]);
		Insert(pos+2, index+2, pgm_read_byte(&phonemeLengthTable[index+2]), stress[pos]);
		pos += 3;
	};

}

// Rewrites the phonemes using the following rules:
//
//       <DIPHTONG ENDING WITH WX> -> <DIPHTONG ENDING WITH WX> WX
//       <DIPHTONG NOT ENDING WITH WX> -> <DIPHTONG NOT ENDING WITH WX> YX
//       UL -> AX L
//       UM -> AX M
//       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
//       T R -> CH R
//       D R -> J R
//       <VOWEL> R -> <VOWEL> RX
//       <VOWEL> L -> <VOWEL> LX
//       G S -> G Z
//       K <VOWEL OR DIPHTONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPHTONG NOT ENDING WITH IY>
//       G <VOWEL OR DIPHTONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPHTONG NOT ENDING WITH IY>
//       S P -> S B
//       S T -> S D
//       S K -> S G
//       S KX -> S GX
//       <ALVEOLAR> UW -> <ALVEOLAR> UX
//       CH -> CH CH' (CH requires two phonemes to represent it)
//       J -> J J' (J requires two phonemes to represent it)
//       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
//       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>


//void Code41397()
void Parser2() {
	if (DEBUG_ESP8266SAM_LIB) printf("Parser2\n");
	unsigned char pos = 0; //mem66;
	unsigned char mem58 = 0;


  // Loop through phonemes
	while(1)
	{
// SET X TO THE CURRENT POSITION
		sam.X = pos;
// GET THE PHONEME AT THE CURRENT POSITION
		sam.A = phonemeindex[pos];

// DEBUG: Print phoneme and index
		if (DEBUG_ESP8266SAM_LIB && sam.A != 255) printf("%d: %c%c\n", sam.X, signInputTable1[sam.A], signInputTable2[sam.A]);

// Is phoneme pause?
		if (sam.A == 0)
		{
// Move ahead to the
			pos++;
			continue;
		}

// If end of phonemes flag reached, exit routine
		if (sam.A == 255) return;

// Copy the current phoneme index to Y
		sam.Y = sam.A;

// RULE:
//       <DIPHTONG ENDING WITH WX> -> <DIPHTONG ENDING WITH WX> WX
//       <DIPHTONG NOT ENDING WITH WX> -> <DIPHTONG NOT ENDING WITH WX> YX
// Example: OIL, COW


// Check for DIPHTONG
		if ((flags[sam.A] & 16) == 0) goto pos41457;

// Not a diphthong. Get the stress
		mem58 = stress[pos];

// End in IY sound?
		sam.A = flags[sam.Y] & 32;

// If ends with IY, use YX, else use WX
		if (sam.A == 0) sam.A = 20; else sam.A = 21;    // 'WX' = 20 'YX' = 21
		//pos41443:
// Insert at WX or YX following, copying the stress

		if (DEBUG_ESP8266SAM_LIB) if (sam.A==20) printf("RULE: insert WX following diphtong NOT ending in IY sound\n");
		if (DEBUG_ESP8266SAM_LIB) if (sam.A==21) printf("RULE: insert YX following diphtong ending in IY sound\n");
		Insert(pos+1, sam.A, sam.mem59, mem58);
		sam.X = pos;
// Jump to ???
		goto pos41749;



pos41457:

// RULE:
//       UL -> AX L
// Example: MEDDLE

// Get phoneme
		sam.A = phonemeindex[sam.X];
// Skip this rule if phoneme is not UL
		if (sam.A != 78) goto pos41487;  // 'UL'
		sam.A = 24;         // 'L'                 //change 'UL' to 'AX L'

		if (DEBUG_ESP8266SAM_LIB) printf("RULE: UL -> AX L\n");

pos41466:
// Get current phoneme stress
		mem58 = stress[sam.X];

// Change UL to AX
		phonemeindex[sam.X] = 13;  // 'AX'
// Perform insert. Note code below may jump up here with different values
		Insert(sam.X+1, sam.A, sam.mem59, mem58);
		pos++;
// Move to next phoneme
		continue;

pos41487:

// RULE:
//       UM -> AX M
// Example: ASTRONOMY

// Skip rule if phoneme != UM
		if (sam.A != 79) goto pos41495;   // 'UM'
		// Jump up to branch - replaces current phoneme with AX and continues
		sam.A = 27; // 'M'  //change 'UM' to  'AX M'
		if (DEBUG_ESP8266SAM_LIB) printf("RULE: UM -> AX M\n");
		goto pos41466;
pos41495:

// RULE:
//       UN -> AX N
// Example: FUNCTION


// Skip rule if phoneme != UN
		if (sam.A != 80) goto pos41503; // 'UN'

		// Jump up to branch - replaces current phoneme with AX and continues
		sam.A = 28;         // 'N' //change UN to 'AX N'
		if (DEBUG_ESP8266SAM_LIB) printf("RULE: UN -> AX N\n");
		goto pos41466;
pos41503:

// RULE:
//       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
// EXAMPLE: AWAY EIGHT

		sam.Y = sam.A;
// VOWEL set?
		sam.A = flags[sam.A] & 128;

// Skip if not a vowel
		if (sam.A != 0)
		{
// Get the stress
			sam.A = stress[sam.X];

// If stressed...
			if (sam.A != 0)
			{
// Get the following phoneme
				sam.X++;
				sam.A = phonemeindex[sam.X];
// If following phoneme is a pause

				if (sam.A == 0)
				{
// Get the phoneme following pause
					sam.X++;
					sam.Y = phonemeindex[sam.X];

// Check for end of buffer flag
					if (sam.Y == 255) //buffer overflow
// ??? Not sure about these flags
     					sam.A = 65&128;
					else
// And VOWEL flag to current phoneme's flags
     					sam.A = flags[sam.Y] & 128;

// If following phonemes is not a pause
					if (sam.A != 0)
					{
// If the following phoneme is not stressed
						sam.A = stress[sam.X];
						if (sam.A != 0)
						{
// Insert a glottal stop and move forward
							if (DEBUG_ESP8266SAM_LIB) printf("RULE: Insert glottal stop between two stressed vowels with space between them\n");
							// 31 = 'Q'
							Insert(sam.X, 31, sam.mem59, 0);
							pos++;
							continue;
						}
					}
				}
			}
		}


// RULES FOR PHONEMES BEFORE R
//        T R -> CH R
// Example: TRACK


// Get current position and phoneme
		sam.X = pos;
		sam.A = phonemeindex[pos];
		if (sam.A != 23) goto pos41611;     // 'R'

// Look at prior phoneme
		sam.X--;
		sam.A = phonemeindex[pos-1];
		//pos41567:
		if (sam.A == 69)                    // 'T'
		{
// Change T to CH
			if (DEBUG_ESP8266SAM_LIB) printf("RULE: T R -> CH R\n");
			phonemeindex[pos-1] = 42;
			goto pos41779;
		}


// RULES FOR PHONEMES BEFORE R
//        D R -> J R
// Example: DRY

// Prior phonemes D?
		if (sam.A == 57)                    // 'D'
		{
// Change D to J
			phonemeindex[pos-1] = 44;
			if (DEBUG_ESP8266SAM_LIB) printf("RULE: D R -> J R\n");
			goto pos41788;
		}

// RULES FOR PHONEMES BEFORE R
//        <VOWEL> R -> <VOWEL> RX
// Example: ART


// If vowel flag is set change R to RX
		sam.A = flags[sam.A] & 128;
		if (DEBUG_ESP8266SAM_LIB) printf("RULE: R -> RX\n");
		if (sam.A != 0) phonemeindex[pos] = 18;  // 'RX'

// continue to next phoneme
		pos++;
		continue;

pos41611:

// RULE:
//       <VOWEL> L -> <VOWEL> LX
// Example: ALL

// Is phoneme L?
		if (sam.A == 24)    // 'L'
		{
// If prior phoneme does not have VOWEL flag set, move to next phoneme
			if ((flags[phonemeindex[pos-1]] & 128) == 0) {pos++; continue;}
// Prior phoneme has VOWEL flag set, so change L to LX and move to next phoneme
			if (DEBUG_ESP8266SAM_LIB) printf("RULE: <VOWEL> L -> <VOWEL> LX\n");
			phonemeindex[sam.X] = 19;     // 'LX'
			pos++;
			continue;
		}

// RULE:
//       G S -> G Z
//
// Can't get to fire -
//       1. The G -> GX rule intervenes
//       2. Reciter already replaces GS -> GZ

// Is current phoneme S?
		if (sam.A == 32)    // 'S'
		{
// If prior phoneme is not G, move to next phoneme
			if (phonemeindex[pos-1] != 60) {pos++; continue;}
// Replace S with Z and move on
			if (DEBUG_ESP8266SAM_LIB) printf("RULE: G S -> G Z\n");
			phonemeindex[pos] = 38;    // 'Z'
			pos++;
			continue;
		}

// RULE:
//             K <VOWEL OR DIPHTONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPHTONG NOT ENDING WITH IY>
// Example: COW

// Is current phoneme K?
		if (sam.A == 72)    // 'K'
		{
// Get next phoneme
			sam.Y = phonemeindex[pos+1];
// If at end, replace current phoneme with KX
			if (sam.Y == 255) phonemeindex[pos] = 75; // ML : prevents an index out of bounds problem
			else
			{
// VOWELS AND DIPHTONGS ENDING WITH IY SOUND flag set?
				sam.A = flags[sam.Y] & 32;
				if (DEBUG_ESP8266SAM_LIB) if (sam.A==0) printf("RULE: K <VOWEL OR DIPHTONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPHTONG NOT ENDING WITH IY>\n");
// Replace with KX
				if (sam.A == 0) phonemeindex[pos] = 75;  // 'KX'
			}
		}
		else

// RULE:
//             G <VOWEL OR DIPHTONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPHTONG NOT ENDING WITH IY>
// Example: GO


// Is character a G?
		if (sam.A == 60)   // 'G'
		{
// Get the following character
			unsigned char index = phonemeindex[pos+1];

// At end of buffer?
			if (index == 255) //prevent buffer overflow
			{
				pos++; continue;
			}
			else
// If diphtong ending with YX, move continue processing next phoneme
			if ((flags[index] & 32) != 0) {pos++; continue;}
// replace G with GX and continue processing next phoneme
			if (DEBUG_ESP8266SAM_LIB) printf("RULE: G <VOWEL OR DIPHTONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPHTONG NOT ENDING WITH IY>\n");
			phonemeindex[pos] = 63; // 'GX'
			pos++;
			continue;
		}

// RULE:
//      S P -> S B
//      S T -> S D
//      S K -> S G
//      S KX -> S GX
// Examples: SPY, STY, SKY, SCOWL

		sam.Y = phonemeindex[pos];
		//pos41719:
// Replace with softer version?
		sam.A = flags[sam.Y] & 1;
		if (sam.A == 0) goto pos41749;
		sam.A = phonemeindex[pos-1];
		if (sam.A != 32)    // 'S'
		{
			sam.A = sam.Y;
			goto pos41812;
		}
		// Replace with softer version
		if (DEBUG_ESP8266SAM_LIB) printf("RULE: S* %c%c -> S* %c%c\n", signInputTable1[sam.Y], signInputTable2[sam.Y],signInputTable1[sam.Y-12], signInputTable2[sam.Y-12]);
		phonemeindex[pos] = sam.Y-12;
		pos++;
		continue;


pos41749:

// RULE:
//      <ALVEOLAR> UW -> <ALVEOLAR> UX
//
// Example: NEW, DEW, SUE, ZOO, THOO, TOO

//       UW -> UX

		sam.A = phonemeindex[sam.X];
		if (sam.A == 53)    // 'UW'
		{
// ALVEOLAR flag set?
			sam.Y = phonemeindex[sam.X-1];
			sam.A = flags2[sam.Y] & 4;
// If not set, continue processing next phoneme
			if (sam.A == 0) {pos++; continue;}
			if (DEBUG_ESP8266SAM_LIB) printf("RULE: <ALVEOLAR> UW -> <ALVEOLAR> UX\n");
			phonemeindex[sam.X] = 16;
			pos++;
			continue;
		}
pos41779:

// RULE:
//       CH -> CH CH' (CH requires two phonemes to represent it)
// Example: CHEW

		if (sam.A == 42)    // 'CH'
		{
			//        pos41783:
			if (DEBUG_ESP8266SAM_LIB) printf("CH -> CH CH+1\n");
			Insert(sam.X+1, sam.A+1, sam.mem59, stress[sam.X]);
			pos++;
			continue;
		}

pos41788:

// RULE:
//       J -> J J' (J requires two phonemes to represent it)
// Example: JAY


		if (sam.A == 44) // 'J'
		{
			if (DEBUG_ESP8266SAM_LIB) printf("J -> J J+1\n");
			Insert(sam.X+1, sam.A+1, sam.mem59, stress[sam.X]);
			pos++;
			continue;
		}

// Jump here to continue
pos41812:

// RULE: Soften T following vowel
// NOTE: This rule fails for cases such as "ODD"
//       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
//       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>
// Example: PARTY, TARDY


// Past this point, only process if phoneme is T or D

		if (sam.A != 69)    // 'T'
		if (sam.A != 57) {pos++; continue;}       // 'D'
		//pos41825:


// If prior phoneme is not a vowel, continue processing phonemes
		if ((flags[phonemeindex[sam.X-1]] & 128) == 0) {pos++; continue;}

// Get next phoneme
		sam.X++;
		sam.A = phonemeindex[sam.X];
		//pos41841
// Is the next phoneme a pause?
		if (sam.A != 0)
		{
// If next phoneme is not a pause, continue processing phonemes
			if ((flags[sam.A] & 128) == 0) {pos++; continue;}
// If next phoneme is stressed, continue processing phonemes
// FIXME: How does a pause get stressed?
			if (stress[sam.X] != 0) {pos++; continue;}
//pos41856:
// Set phonemes to DX
		if (DEBUG_ESP8266SAM_LIB) printf("RULE: Soften T or D following vowel or ER and preceding a pause -> DX\n");
		phonemeindex[pos] = 30;       // 'DX'
		} else
		{
			sam.A = phonemeindex[sam.X+1];
			if (sam.A == 255) //prevent buffer overflow
				sam.A = 65 & 128;
			else
// Is next phoneme a vowel or ER?
				sam.A = flags[sam.A] & 128;
			if (DEBUG_ESP8266SAM_LIB) if (sam.A != 0) printf("RULE: Soften T or D following vowel or ER and preceding a pause -> DX\n");
			if (sam.A != 0) phonemeindex[pos] = 30;  // 'DX'
		}

		pos++;

	} // while
}


// Applies various rules that adjust the lengths of phonemes
//
//         Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5
//         <VOWEL> <RX | LX> <CONSONANT> - decrease <VOWEL> length by 1
//         <VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th
//         <VOWEL> <UNVOICED CONSONANT> - increase vowel by 1/2 + 1
//         <NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6
//         <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1
//         <LIQUID CONSONANT> <DIPHTONG> - decrease by 2


//void Code48619()
void AdjustLengths() {

    // LENGTHEN VOWELS PRECEDING PUNCTUATION
    //
    // Search for punctuation. If found, back up to the first vowel, then
    // process all phonemes between there and up to (but not including) the punctuation.
    // If any phoneme is found that is a either a fricative or voiced, the duration is
    // increased by (length * 1.5) + 1

    // loop index
	sam.X = 0;
	unsigned char index;

    // iterate through the phoneme list
	unsigned char loopIndex=0;
	while(1)
	{
        // get a phoneme
		index = phonemeindex[sam.X];

		// exit loop if end on buffer token
		if (index == 255) break;

		// not punctuation?
		if((flags2[index] & 1) == 0)
		{
            // skip
			sam.X++;
			continue;
		}

		// hold index
		loopIndex = sam.X;

		// Loop backwards from this point
pos48644:

        // back up one phoneme
		sam.X--;

		// stop once the beginning is reached
		if(sam.X == 0) break;

		// get the preceding phoneme
		index = phonemeindex[sam.X];

		if (index != 255) //inserted to prevent access overrun
		if((flags[index] & 128) == 0) goto pos48644; // if not a vowel, continue looping

		//pos48657:
		do
		{
            // test for vowel
			index = phonemeindex[sam.X];

			if (index != 255)//inserted to prevent access overrun
			// test for fricative/unvoiced or not voiced
			if(((flags2[index] & 32) == 0) || ((flags[index] & 4) != 0))     //nochmal �berpr�fen
			{
				//A = flags[Y] & 4;
				//if(A == 0) goto pos48688;

                // get the phoneme length
				sam.A = phonemeLength[sam.X];

				// change phoneme length to (length * 1.5) + 1
				sam.A = (sam.A >> 1) + sam.A + 1;
if (DEBUG_ESP8266SAM_LIB) printf("RULE: Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5\n");
if (DEBUG_ESP8266SAM_LIB) printf("PRE\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);

				phonemeLength[sam.X] = sam.A;

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);

			}
            // keep moving forward
			sam.X++;
		} while (sam.X != loopIndex);
		//	if (X != loopIndex) goto pos48657;
		sam.X++;
	}  // while

    // Similar to the above routine, but shorten vowels under some circumstances

    // Loop throught all phonemes
	loopIndex = 0;
	//pos48697

	while(1)
	{
        // get a phoneme
		sam.X = loopIndex;
		index = phonemeindex[sam.X];

		// exit routine at end token
		if (index == 255) return;

		// vowel?
		sam.A = flags[index] & 128;
		if (sam.A != 0)
		{
            // get next phoneme
			sam.X++;
			index = phonemeindex[sam.X];

			// get flags
			if (index == 255)
			sam.mem56 = 65; // use if end marker
			else
			sam.mem56 = flags[index];

            // not a consonant
			if ((flags[index] & 64) == 0)
			{
                // RX or LX?
				if ((index == 18) || (index == 19))  // 'RX' & 'LX'
				{
                    // get the next phoneme
					sam.X++;
					index = phonemeindex[sam.X];

					// next phoneme a consonant?
					if ((flags[index] & 64) != 0) {
                        // RULE: <VOWEL> RX | LX <CONSONANT>


if (DEBUG_ESP8266SAM_LIB) printf("RULE: <VOWEL> <RX | LX> <CONSONANT> - decrease length by 1\n");
if (DEBUG_ESP8266SAM_LIB) printf("PRE\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", loopIndex, signInputTable1[phonemeindex[loopIndex]], signInputTable2[phonemeindex[loopIndex]], phonemeLength[loopIndex]);

                        // decrease length of vowel by 1 frame
    					phonemeLength[loopIndex]--;

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", loopIndex, signInputTable1[phonemeindex[loopIndex]], signInputTable2[phonemeindex[loopIndex]], phonemeLength[loopIndex]);

                    }
                    // move ahead
					loopIndex++;
					continue;
				}
				// move ahead
				loopIndex++;
				continue;
			}


			// Got here if not <VOWEL>

            // not voiced
			if ((sam.mem56 & 4) == 0)
			{

                 // Unvoiced
                 // *, .*, ?*, ,*, -*, DX, S*, SH, F*, TH, /H, /X, CH, P*, T*, K*, KX

                // not an unvoiced plosive?
				if((sam.mem56 & 1) == 0) {
                    // move ahead
                    loopIndex++;
                    continue;
                }

                // P*, T*, K*, KX


                // RULE: <VOWEL> <UNVOICED PLOSIVE>
                // <VOWEL> <P*, T*, K*, KX>

                // move back
				sam.X--;

if (DEBUG_ESP8266SAM_LIB) printf("RULE: <VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th\n");
if (DEBUG_ESP8266SAM_LIB) printf("PRE\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]],  phonemeLength[sam.X]);

                // decrease length by 1/8th
				sam.mem56 = phonemeLength[sam.X] >> 3;
				phonemeLength[sam.X] -= sam.mem56;

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);

                // move ahead
				loopIndex++;
				continue;
			}

            // RULE: <VOWEL> <VOICED CONSONANT>
            // <VOWEL> <WH, R*, L*, W*, Y*, M*, N*, NX, DX, Q*, Z*, ZH, V*, DH, J*, B*, D*, G*, GX>

if (DEBUG_ESP8266SAM_LIB) printf("RULE: <VOWEL> <VOICED CONSONANT> - increase vowel by 1/2 + 1\n");
if (DEBUG_ESP8266SAM_LIB) printf("PRE\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X-1, signInputTable1[phonemeindex[sam.X-1]], signInputTable2[phonemeindex[sam.X-1]],  phonemeLength[sam.X-1]);

            // decrease length
			sam.A = phonemeLength[sam.X-1];
			phonemeLength[sam.X-1] = (sam.A >> 2) + sam.A + 1;     // 5/4*A + 1

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X-1, signInputTable1[phonemeindex[sam.X-1]], signInputTable2[phonemeindex[sam.X-1]], phonemeLength[sam.X-1]);

            // move ahead
			loopIndex++;
			continue;

		}


        // WH, R*, L*, W*, Y*, M*, N*, NX, Q*, Z*, ZH, V*, DH, J*, B*, D*, G*, GX

//pos48821:

        // RULE: <NASAL> <STOP CONSONANT>
        //       Set punctuation length to 6
        //       Set stop consonant length to 5

        // nasal?
        if((flags2[index] & 8) != 0)
        {

            // M*, N*, NX,

            // get the next phoneme
            sam.X++;
            index = phonemeindex[sam.X];

            // end of buffer?
            if (index == 255)
               sam.A = 65&2;  //prevent buffer overflow
            else
                sam.A = flags[index] & 2; // check for stop consonant


            // is next phoneme a stop consonant?
            if (sam.A != 0)

               // B*, D*, G*, GX, P*, T*, K*, KX

            {
if (DEBUG_ESP8266SAM_LIB) printf("RULE: <NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6\n");
if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X-1, signInputTable1[phonemeindex[sam.X-1]], signInputTable2[phonemeindex[sam.X-1]], phonemeLength[sam.X-1]);

                // set stop consonant length to 6
                phonemeLength[sam.X] = 6;

                // set nasal length to 5
                phonemeLength[sam.X-1] = 5;

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X-1, signInputTable1[phonemeindex[sam.X-1]], signInputTable2[phonemeindex[sam.X-1]], phonemeLength[sam.X-1]);

            }
            // move to next phoneme
            loopIndex++;
            continue;
        }


        // WH, R*, L*, W*, Y*, Q*, Z*, ZH, V*, DH, J*, B*, D*, G*, GX

        // RULE: <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
        //       Shorten both to (length/2 + 1)

        // (voiced) stop consonant?
        if((flags[index] & 2) != 0)
        {
            // B*, D*, G*, GX

            // move past silence
            do
            {
                // move ahead
                sam.X++;
                index = phonemeindex[sam.X];
            } while(index == 0);


            // check for end of buffer
            if (index == 255) //buffer overflow
            {
                // ignore, overflow code
                if ((65 & 2) == 0) {loopIndex++; continue;}
            } else if ((flags[index] & 2) == 0) {
                // if another stop consonant, move ahead
                loopIndex++;
                continue;
            }

            // RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
if (DEBUG_ESP8266SAM_LIB) printf("RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1\n");
if (DEBUG_ESP8266SAM_LIB) printf("PRE\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X-1, signInputTable1[phonemeindex[sam.X-1]], signInputTable2[phonemeindex[sam.X-1]], phonemeLength[sam.X-1]);
// X gets overwritten, so hold prior X value for debug statement
int debugX = sam.X;
            // shorten the prior phoneme length to (length/2 + 1)
            phonemeLength[sam.X] = (phonemeLength[sam.X] >> 1) + 1;
            sam.X = loopIndex;

            // also shorten this phoneme length to (length/2 +1)
            phonemeLength[loopIndex] = (phonemeLength[loopIndex] >> 1) + 1;

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", debugX, signInputTable1[phonemeindex[debugX]], signInputTable2[phonemeindex[debugX]], phonemeLength[debugX]);
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", debugX-1, signInputTable1[phonemeindex[debugX-1]], signInputTable2[phonemeindex[debugX-1]], phonemeLength[debugX-1]);


            // move ahead
            loopIndex++;
            continue;
        }


        // WH, R*, L*, W*, Y*, Q*, Z*, ZH, V*, DH, J*, **,

        // RULE: <VOICED NON-VOWEL> <DIPHTONG>
        //       Decrease <DIPHTONG> by 2

        // liquic consonant?
        if ((flags2[index] & 16) != 0)
        {
            // R*, L*, W*, Y*

            // get the prior phoneme
            index = phonemeindex[sam.X-1];

            // prior phoneme a stop consonant>
            if((flags[index] & 2) != 0)
                             // Rule: <LIQUID CONSONANT> <DIPHTONG>

if (DEBUG_ESP8266SAM_LIB) printf("RULE: <LIQUID CONSONANT> <DIPHTONG> - decrease by 2\n");
if (DEBUG_ESP8266SAM_LIB) printf("PRE\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);

             // decrease the phoneme length by 2 frames (20 ms)
             phonemeLength[sam.X] -= 2;

if (DEBUG_ESP8266SAM_LIB) printf("POST\n");
if (DEBUG_ESP8266SAM_LIB) printf("phoneme %d (%c%c) length %d\n", sam.X, signInputTable1[phonemeindex[sam.X]], signInputTable2[phonemeindex[sam.X]], phonemeLength[sam.X]);
         }

         // move to next phoneme
         loopIndex++;
         continue;
    }
//            goto pos48701;
}

// -------------------------------------------------------------------------
// ML : Code47503 is division with remainder, and mem50 gets the sign
void Code47503(unsigned char mem52) {

	sam.Y = 0;
	if ((sam.mem53 & 128) != 0)
	{
		sam.mem53 = -sam.mem53;
		sam.Y = 128;
	}
	sam.mem50 = sam.Y;
	sam.A = 0;
	for(sam.X=8; sam.X > 0; sam.X--)
	{
		int temp = sam.mem53;
		sam.mem53 = sam.mem53 << 1;
		sam.A = sam.A << 1;
		if (temp >= 128) sam.A++;
		if (sam.A >= mem52)
		{
			sam.A = sam.A - mem52;
			sam.mem53++;
		}
	}

	sam.mem51 = sam.A;
	if ((sam.mem50 & 128) != 0) sam.mem53 = -sam.mem53;

}
