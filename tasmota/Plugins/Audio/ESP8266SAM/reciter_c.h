//#include <stdio.h>
//#include <string.h>
#include "reciter.h"
#include "ReciterTabs.h"
//#include "esp8266sam_debug.h"
#include "SamData.h"

//unsigned char A, X, Y;
//extern int debug;

#define inputtemp (samdata->reciter.inputtemp)

MODULE_PART void Code37055(unsigned char mem59) {
	SETMEMREGS
	samdata->X = mem59;
	samdata->X--;
	samdata->A = inputtemp[samdata->X];
	samdata->Y = samdata->A;
	samdata->A = pgm_read_byte(tab36376+samdata->Y); //tab36376[Y];
	return;
}

MODULE_PART void Code37066(unsigned char mem58) {
	SETMEMREGS
	samdata->X = mem58;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	samdata->Y = samdata->A;
	samdata->A = pgm_read_byte(tab36376+samdata->Y); //tab36376[Y];
}

MODULE_PART unsigned char GetRuleByte(unsigned short mem62, unsigned char Y) {
	SETMEMREGS
	unsigned int address = mem62;

	const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);

	if (mem62 >= icp[4]) {
		address -= icp[4];
		return pgm_read_byte(rules2 + address + Y); //rules2[address+Y];
	}
	address -= icp[5];
	return pgm_read_byte(rules + address + Y); //rules[address+Y];
}

// Code36484
MODULE_PART int TextToPhonemes(char *inbuff) {
	SETMEMREGS


	const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);

	//unsigned char *tab39445 = &mem[39445];   //input and output
	//unsigned char mem29;
	unsigned char mem56;      //output position for phonemes
	unsigned char mem57;
	unsigned char mem58;
	unsigned char mem59;
	unsigned char mem60;
	unsigned char mem61;
	unsigned short mem62;     // memory position of current rule

	unsigned char mem64;      // position of '=' or current character
	unsigned char mem65;     // position of ')'
	unsigned char mem66;     // position of '('
	unsigned char mem36653;

	inputtemp[0] = 32;

	// secure copy of input
	// because input will be overwritten by phonemes
	samdata->X = 1;
	samdata->Y = 0;
	do {
		//pos36499:
		samdata->A = inbuff[samdata->Y] & 127;
		if ( samdata->A >= 112) samdata->A = samdata->A & 95;
		else if ( samdata->A >= 96) samdata->A = samdata->A & 79;

		inputtemp[samdata->X] = samdata->A;
		samdata->X++;
		samdata->Y++;
	} while (samdata->Y != 255);


	samdata->X = 255;
	inputtemp[samdata->X] = 27;
	mem61 = 255;


pos36550:
	samdata->A = 255;
	mem56 = 255;


pos36554:
	while(1) {
		mem61++;
		samdata->X = mem61;
		samdata->A = inputtemp[samdata->X];
		mem64 = samdata->A;
		if (samdata->A == '[') {
			mem56++;
			samdata->X = mem56;
			samdata->A = 155;
			inbuff[samdata->X] = 155;
			//goto pos36542;
			//			Code39771(); 	//Code39777();
			return 1;
		}

		//pos36579:
		if (samdata->A != '.') break;
		samdata->X++;
		samdata->Y = inputtemp[samdata->X];
		samdata->A = pgm_read_byte(tab36376+samdata->Y)/*tab36376[Y]*/ & 1;
		if (samdata->A != 0) break;
		mem56++;
		samdata->X = mem56;
		samdata->A = '.';
		inbuff[samdata->X] = '.';
	} //while


	//pos36607:
	samdata->A = mem64;
	samdata->Y = samdata->A;
	samdata->A = pgm_read_byte(tab36376 + samdata->A); //tab36376[A];
	mem57 = samdata->A;
	if ((samdata->A & 2) != 0) {
		mem62 = icp[4];
		goto pos36700;
	}

	//pos36630:
	samdata->A = mem57;
	if(samdata->A != 0) goto pos36677;
	samdata->A = 32;
	inputtemp[samdata->X] = ' ';
	mem56++;
	samdata->X = mem56;
	if (samdata->X > 120) goto pos36654;
	inbuff[samdata->X] = samdata->A;
	goto pos36554;

	// -----

	//36653 is unknown. Contains position

pos36654:
	inbuff[samdata->X] = 155;
	samdata->A = mem61;
	mem36653 = samdata->A;
	//	mem29 = A; // not used
	//	Code36538(); das ist eigentlich
	return 1;
	//Code39771();
	//go on if there is more input ???
	mem61 = mem36653;
	goto pos36550;

pos36677:
	samdata->A = mem57 & 128;
	if(samdata->A == 0) {
		//36683: BRK
		return 0;
	}

	// go to the right rules for this character.
	samdata->X = mem64 - 'A';
	mem62 = pgm_read_byte(&tab37489[samdata->X]) | (pgm_read_byte(&tab37515[samdata->X])<<8);

	// -------------------------------------
	// go to next rule
	// -------------------------------------

pos36700:

	// find next rule
	samdata->Y = 0;
	do {
		mem62 += 1;
		samdata->A = GetRuleByte(mem62, samdata->Y);
	} while ((samdata->A & 128) == 0);
	samdata->Y++;

	//pos36720:
	// find '('
	while(1) {
		samdata->A = GetRuleByte(mem62, samdata->Y);
		if (samdata->A == '(') break;
		samdata->Y++;
	}
	mem66 = samdata->Y;

	//pos36732:
	// find ')'
	do {
		samdata->Y++;
		samdata->A = GetRuleByte(mem62, samdata->Y);
	} while(samdata->A != ')');
	mem65 = samdata->Y;

	//pos36741:
	// find '='
	do {
		samdata->Y++;
		samdata->A = GetRuleByte(mem62, samdata->Y);
		samdata->A = samdata->A & 127;
	} while (samdata->A != '=');
	mem64 = samdata->Y;

	samdata->X = mem61;
	mem60 = samdata->X;

	// compare the string within the bracket
	samdata->Y = mem66;
	samdata->Y++;
	//pos36759:
	while(1) {
		mem57 = inputtemp[samdata->X];
		samdata->A = GetRuleByte(mem62, samdata->Y);
		if (samdata->A != mem57) goto pos36700;
		samdata->Y++;
		if (samdata->Y == mem65) break;
		samdata->X++;
		mem60 = samdata->X;
	}

// the string in the bracket is correct

//pos36787:
	samdata->A = mem61;
	mem59 = mem61;

pos36791:
	while(1) {
		mem66--;
		samdata->Y = mem66;
		samdata->A = GetRuleByte(mem62, samdata->Y);
		mem57 = samdata->A;
		//36800: BPL 36805
		if ((samdata->A & 128) != 0) goto pos37180;
		samdata->X = samdata->A & 127;
		samdata->A = pgm_read_byte(tab36376+samdata->X)/*tab36376[X]*/ & 128;
		if (samdata->A == 0) break;
		samdata->X = mem59-1;
		samdata->A = inputtemp[samdata->X];
		if (samdata->A != mem57) goto pos36700;
		mem59 = samdata->X;
	}

//pos36833:
	samdata->A = mem57;
	if (samdata->A == ' ') goto pos36895;
	if (samdata->A == '#') goto pos36910;
	if (samdata->A == '.') goto pos36920;
	if (samdata->A == '&') goto pos36935;
	if (samdata->A == '@') goto pos36967;
	if (samdata->A == '^') goto pos37004;
	if (samdata->A == '+') goto pos37019;
	if (samdata->A == ':') goto pos37040;
	//	Code42041();    //Error
	//36894: BRK
	return 0;

	// --------------

pos36895:
	Code37055(mem59);
	samdata->A = samdata->A & 128;
	if(samdata->A != 0) goto pos36700;
pos36905:
	mem59 = samdata->X;
	goto pos36791;

	// --------------

pos36910:
	Code37055(mem59);
	samdata->A = samdata->A & 64;
	if (samdata->A != 0) goto pos36905;
	goto pos36700;

	// --------------


pos36920:
	Code37055(mem59);
	samdata->A = samdata->A & 8;
	if (samdata->A == 0) goto pos36700;
pos36930:
	mem59 = samdata->X;
	goto pos36791;

	// --------------

pos36935:
	Code37055(mem59);
	samdata->A = samdata->A & 16;
	if (samdata->A != 0) goto pos36930;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 72) goto pos36700;
	samdata->X--;
	samdata->A = inputtemp[samdata->X];
	if ((samdata->A == 67) || (samdata->A == 83)) goto pos36930;
	goto pos36700;

	// --------------

pos36967:
	Code37055(mem59);
	samdata->A = samdata->A & 4;
	if(samdata->A != 0) goto pos36930;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 72) goto pos36700;
	if ((samdata->A != 84) && (samdata->A != 67) && (samdata->A != 83)) goto pos36700;
	mem59 = samdata->X;
	goto pos36791;

	// --------------


pos37004:
	Code37055(mem59);
	samdata->A = samdata->A & 32;
	if(samdata->A == 0) goto pos36700;

pos37014:
	mem59 = samdata->X;
	goto pos36791;

	// --------------

pos37019:
	samdata->X = mem59;
	samdata->X--;
	samdata->A = inputtemp[samdata->X];
	if ((samdata->A == 'E') || (samdata->A == 'I') || (samdata->A == 'Y')) goto pos37014;
	goto pos36700;
	// --------------

pos37040:
	Code37055(mem59);
	samdata->A = samdata->A & 32;
	if(samdata->A == 0) goto pos36791;
	mem59 = samdata->X;
	goto pos37040;

//---------------------------------------


pos37077:
	samdata->X = mem58+1;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 'E') goto pos37157;
	samdata->X++;
	samdata->Y = inputtemp[samdata->X];
	samdata->X--;
	samdata->A = pgm_read_byte(tab36376+samdata->Y)/*tab36376[Y]*/ & 128;
	if(samdata->A == 0) goto pos37108;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 'R') goto pos37113;
pos37108:
	mem58 = samdata->X;
	goto pos37184;
pos37113:
	if ((samdata->A == 83) || (samdata->A == 68)) goto pos37108;  // 'S' 'D'
	if (samdata->A != 76) goto pos37135; // 'L'
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 89) goto pos36700;
	goto pos37108;

pos37135:
	if (samdata->A != 70) goto pos36700;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 85) goto pos36700;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A == 76) goto pos37108;
	goto pos36700;

pos37157:
	if (samdata->A != 73) goto pos36700;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 78) goto pos36700;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A == 71) goto pos37108;
	//pos37177:
	goto pos36700;

	// -----------------------------------------

pos37180:

	samdata->A = mem60;
	mem58 = samdata->A;

pos37184:
	samdata->Y = mem65 + 1;

	//37187: CPY 64
	//	if(? != 0) goto pos37194;
	if(samdata->Y == mem64) goto pos37455;
	mem65 = samdata->Y;
	//37196: LDA (62),y
	samdata->A = GetRuleByte(mem62,samdata->Y);
	mem57 = samdata->A;
	samdata->X = samdata->A;
	samdata->A = pgm_read_byte(tab36376+samdata->X)/*tab36376[X]*/ & 128;
	if(samdata->A == 0) goto pos37226;
	samdata->X = mem58+1;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != mem57) goto pos36700;
	mem58 = samdata->X;
	goto pos37184;
pos37226:
	samdata->A = mem57;
	if (samdata->A == 32) goto pos37295;   // ' '
	if (samdata->A == 35) goto pos37310;   // '#'
	if (samdata->A == 46) goto pos37320;   // '.'
	if (samdata->A == 38) goto pos37335;   // '&'
	if (samdata->A == 64) goto pos37367;   // ''
	if (samdata->A == 94) goto pos37404;   // ''
	if (samdata->A == 43) goto pos37419;   // '+'
	if (samdata->A == 58) goto pos37440;   // ':'
	if (samdata->A == 37) goto pos37077;   // '%'
	//pos37291:
	//	Code42041(); //Error
	//37294: BRK
	return 0;

	// --------------
pos37295:
	Code37066(mem58);
	samdata->A = samdata->A & 128;
	if(samdata->A != 0) goto pos36700;
pos37305:
	mem58 = samdata->X;
	goto pos37184;

	// --------------

pos37310:
	Code37066(mem58);
	samdata->A = samdata->A & 64;
	if(samdata->A != 0) goto pos37305;
	goto pos36700;

	// --------------


pos37320:
	Code37066(mem58);
	samdata->A = samdata->A & 8;
	if(samdata->A == 0) goto pos36700;

pos37330:
	mem58 = samdata->X;
	goto pos37184;

	// --------------

pos37335:
	Code37066(mem58);
	samdata->A = samdata->A & 16;
	if(samdata->A != 0) goto pos37330;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 72) goto pos36700;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if ((samdata->A == 67) || (samdata->A == 83)) goto pos37330;
	goto pos36700;

	// --------------


pos37367:
	Code37066(mem58);
	samdata->A = samdata->A & 4;
	if(samdata->A != 0) goto pos37330;
	samdata->A = inputtemp[samdata->X];
	if (samdata->A != 72) goto pos36700;
	if ((samdata->A != 84) && (samdata->A != 67) && (samdata->A != 83)) goto pos36700;
	mem58 = samdata->X;
	goto pos37184;

	// --------------

pos37404:
	Code37066(mem58);
	samdata->A = samdata->A & 32;
	if(samdata->A == 0) goto pos36700;
pos37414:
	mem58 = samdata->X;
	goto pos37184;

	// --------------

pos37419:
	samdata->X = mem58;
	samdata->X++;
	samdata->A = inputtemp[samdata->X];
	if ((samdata->A == 69) || (samdata->A == 73) || (samdata->A == 89)) goto pos37414;
	goto pos36700;

// ----------------------

pos37440:

	Code37066(mem58);
	samdata->A = samdata->A & 32;
	if(samdata->A == 0) goto pos37184;
	mem58 = samdata->X;
	goto pos37440;
pos37455:
	samdata->Y = mem64;
	mem61 = mem60;

	if (DEBUG_ESP8266SAM_LIB)
		PrintRule(mem62);

pos37461:
	//37461: LDA (62),y
	samdata->A = GetRuleByte(mem62, samdata->Y);
	mem57 = samdata->A;
	samdata->A = samdata->A & 127;
	if (samdata->A != '=') {
		mem56++;
		samdata->X = mem56;
		inbuff[samdata->X] = samdata->A;
	}

	//37478: BIT 57
	//37480: BPL 37485  //not negative flag
	if ((mem57 & 128) == 0) goto pos37485; //???
	goto pos36554;
pos37485:
	samdata->Y++;
	goto pos37461;
}
