#include "reciter.h"
#include "ReciterTabs.h"
#include "SamData.h"

#define inputtemp (samdata->reciter.inputtemp)

//#define REG_A samdata->A
//#define REG_X samdata->X
//#define REG_Y samdata->Y


MODULE_PART uint8_t Code37055(unsigned char IN) {
	SETMEMREGS
/*
	REG_X = mem59;
	REG_X--;
	REG_A = inputtemp[REG_X];
	REG_Y = REG_A;
*/
	//REG_A = pgm_read_byte_inlined(tab36376 + REG_Y); //tab36376[Y];
	const uint8_t *cp = tab36376 + inputtemp[IN - 1];
	cp += EXEC_OFFSET;
	return pgm_read_byte(cp);
}


MODULE_PART uint8_t Code37066(unsigned char IN) {
	SETMEMREGS	
	//REG_A = pgm_read_byte_inlined(tab36376 + REG_Y); //tab36376[Y];
	const uint8_t *cp = tab36376 + inputtemp[IN + 1];
	cp += EXEC_OFFSET;
	return pgm_read_byte(cp);
}

MODULE_PART unsigned char GetRuleByte(unsigned short mem62, unsigned char Y) {
	SETMEMREGS
	unsigned int address = mem62;

#if 0
	if (mem62 >= 37541) {
		address -= 37541;
		return pgm_read_byte_inlined(rules2+address+Y); //rules2[address+Y];
	}
	address -= 32000;
	return pgm_read_byte_inlined(rules+address+Y); //rules[address+Y];
#else
	const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);

	if (mem62 >= icp[4]) {
		address -= icp[4];
		//return pgm_read_byte(rules2 + address + Y); //rules2[address+Y];
		const char *cp = rules2 + address + Y;
		cp += EXEC_OFFSET;
		return pgm_read_byte(cp);

	}
	address -= icp[5];
	//return pgm_read_byte(rules + address + Y); //rules[address+Y];
	const char *cp = rules + address + Y;
	cp += EXEC_OFFSET;
	return pgm_read_byte(cp);
#endif
}

MODULE_PART int TextToPhonemes(char *_input) { // Code36484
	SETMEMREGS
	//unsigned char *tab39445 = &mem[39445];   //input and output
	//unsigned char mem29;
	uint8_t REG_A,REG_X,REG_Y;

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
	REG_X = 1;
	REG_Y = 0;
	do {
		//pos36499:
		REG_A = _input[REG_Y] & 127;
		if ( REG_A >= 112) {
			REG_A = REG_A & 95;
		}	else if ( REG_A >= 96) {
			REG_A = REG_A & 79;
		}
		inputtemp[REG_X] = REG_A;
		REG_X++;
		REG_Y++;
	} while (REG_Y != 255);


	REG_X = 255;
	inputtemp[REG_X] = 27;
	mem61 = 255;

pos36550:
	REG_A = 255;
	mem56 = 255;

pos36554:
	while(1) {
		mem61++;
		REG_X = mem61;
		REG_A = inputtemp[REG_X];
		mem64 = REG_A;
		if (REG_A == '[') {
			mem56++;
			REG_X = mem56;
			REG_A = 155;
			_input[REG_X] = 155;
			//goto pos36542;
			//			Code39771(); 	//Code39777();
			return 1;
		}

		//pos36579:
		if (REG_A != '.') {
			break;
		}
		REG_X++;
		REG_Y = inputtemp[REG_X];
		//REG_A = pgm_read_byte_inlined(tab36376+REG_Y)/*tab36376[Y]*/ & 1;
		const uint8_t *cp = tab36376 + REG_Y;
		cp += EXEC_OFFSET;
		REG_A = pgm_read_byte(cp) & 1;

		if (REG_A != 0) {
			break;
		}
		mem56++;
		REG_X = mem56;
		REG_A = '.';
		_input[REG_X] = '.';
	} //while


	//pos36607:
	REG_A = mem64;
	REG_Y = REG_A;
	//REG_A = pgm_read_byte_inlined(tab36376+REG_A); //tab36376[A];
	const uint8_t *cp = tab36376 + REG_A;
	cp += EXEC_OFFSET;
	REG_A = pgm_read_byte(cp);

	mem57 = REG_A;
	if ((REG_A&2) != 0) {
		//mem62 = 37541;
		const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);
		mem62 = icp[4];
		goto pos36700;
	}

	//pos36630:
	REG_A = mem57;
	if (REG_A != 0) goto pos36677;
	REG_A = 32;
	inputtemp[REG_X] = ' ';
	mem56++;
	REG_X = mem56;
	if (REG_X > 120) goto pos36654;
	_input[REG_X] = REG_A;
	goto pos36554;

	// -----

	//36653 is unknown. Contains position

pos36654:
	_input[REG_X] = 155;
	REG_A = mem61;
	mem36653 = REG_A;
	//	mem29 = A; // not used
	//	Code36538(); das ist eigentlich
	return 1;
	//Code39771();
	//go on if there is more input ???
	mem61 = mem36653;
	goto pos36550;

pos36677:
	REG_A = mem57 & 128;
	if (REG_A == 0) {
		//36683: BRK
		return 0;
	}

	// go to the right rules for this character.
	REG_X = mem64 - 'A';
	{
		//mem62 = pgm_read_byte_inlined(&tab37489[REG_X]) | (pgm_read_byte_inlined(&tab37515[REG_X])<<8);
		const uint8_t *cp1 = &tab37489[REG_X];
		const uint8_t *cp2 = &tab37515[REG_X];
		cp1 += EXEC_OFFSET;
		cp2 += EXEC_OFFSET;
		mem62 = pgm_read_byte(cp1) | (pgm_read_byte(cp2)<<8);
	}

	// -------------------------------------
	// go to next rule
	// -------------------------------------

pos36700:

	// find next rule
	REG_Y = 0;
	do {
		mem62 += 1;
		REG_A = GetRuleByte(mem62, REG_Y);
	} while ((REG_A & 128) == 0);
	REG_Y++;

	//pos36720:
	// find '('
	while(1) {
		REG_A = GetRuleByte(mem62, REG_Y);
		if (REG_A == '(') break;
		REG_Y++;
	}
	mem66 = REG_Y;

	//pos36732:
	// find ')'
	do {
		REG_Y++;
		REG_A = GetRuleByte(mem62, REG_Y);
	} while(REG_A != ')');
	mem65 = REG_Y;

	//pos36741:
	// find '='
	do {
		REG_Y++;
		REG_A = GetRuleByte(mem62, REG_Y);
		REG_A = REG_A & 127;
	} while (REG_A != '=');
	mem64 = REG_Y;

	REG_X = mem61;
	mem60 = REG_X;

	// compare the string within the bracket
	REG_Y = mem66;
	REG_Y++;
	//pos36759:
	while(1) {
		mem57 = inputtemp[REG_X];
		REG_A = GetRuleByte(mem62, REG_Y);
		if (REG_A != mem57) goto pos36700;
		REG_Y++;
		if(REG_Y == mem65) break;
		REG_X++;
		mem60 = REG_X;
	}

// the string in the bracket is correct

//pos36787:
	REG_A = mem61;
	mem59 = mem61;

pos36791:
	while(1) {
		mem66--;
		REG_Y = mem66;
		REG_A = GetRuleByte(mem62, REG_Y);
		mem57 = REG_A;
		//36800: BPL 36805
		if ((REG_A & 128) != 0) goto pos37180;
		REG_X = REG_A & 127;
		//REG_A = pgm_read_byte_inlined(tab36376+REG_X)/*tab36376[X]*/ & 128;
		const uint8_t *cp = tab36376 + REG_X;
		cp += EXEC_OFFSET;
		REG_A = pgm_read_byte(cp) & 128;

		if (REG_A == 0) break;
		REG_X = mem59-1;
		REG_A = inputtemp[REG_X];
		if (REG_A != mem57) goto pos36700;
		mem59 = REG_X;
	}

//pos36833:
	REG_A = mem57;
	if (REG_A == ' ') goto pos36895;
	if (REG_A == '#') goto pos36910;
	if (REG_A == '.') goto pos36920;
	if (REG_A == '&') goto pos36935;
	if (REG_A == '@') goto pos36967;
	if (REG_A == '^') goto pos37004;
	if (REG_A == '+') goto pos37019;
	if (REG_A == ':') goto pos37040;
	//	Code42041();    //Error
	//36894: BRK
	return 0;

	// --------------

pos36895:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 128;
	if (REG_A != 0) goto pos36700;
pos36905:
	mem59 = REG_X;
	goto pos36791;

	// --------------

pos36910:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 64;
	if (REG_A != 0) goto pos36905;
	goto pos36700;

	// --------------


pos36920:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 8;
	if (REG_A == 0) goto pos36700;
pos36930:
	mem59 = REG_X;
	goto pos36791;

	// --------------

pos36935:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 16;
	if (REG_A != 0) goto pos36930;
	REG_A = inputtemp[REG_X];
	if (REG_A != 72) goto pos36700;
	REG_X--;
	REG_A = inputtemp[REG_X];
	if ((REG_A == 67) || (REG_A == 83)) goto pos36930;
	goto pos36700;

	// --------------

pos36967:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 4;
	if (REG_A != 0) goto pos36930;
	REG_A = inputtemp[REG_X];
	if (REG_A != 72) goto pos36700;
	if ((REG_A != 84) && (REG_A != 67) && (REG_A != 83)) goto pos36700;
	mem59 = REG_X;
	goto pos36791;

	// --------------


pos37004:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 32;
	if (REG_A == 0) goto pos36700;

pos37014:
	mem59 = REG_X;
	goto pos36791;

	// --------------

pos37019:
	REG_X = mem59;
	REG_X--;
	REG_A = inputtemp[REG_X];
	if ((REG_A == 'E') || (REG_A == 'I') || (REG_A == 'Y')) goto pos37014;
	goto pos36700;
	// --------------

pos37040:
	REG_A = Code37055(mem59); REG_X = mem59 - 1;
	REG_A = REG_A & 32;
	if (REG_A == 0) goto pos36791;
	mem59 = REG_X;
	goto pos37040;

//---------------------------------------


pos37077:
	REG_X = mem58+1;
	REG_A = inputtemp[REG_X];
	if (REG_A != 'E') goto pos37157;
	REG_X++;
	REG_Y = inputtemp[REG_X];
	REG_X--;
	//REG_A = pgm_read_byte_inlined(tab36376+REG_Y)/*tab36376[Y]*/ & 128;
	cp = tab36376 + REG_Y;
	cp += EXEC_OFFSET;
	REG_A = pgm_read_byte(cp) & 128;

	if (REG_A == 0) goto pos37108;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if (REG_A != 'R') goto pos37113;
pos37108:
	mem58 = REG_X;
	goto pos37184;
pos37113:
	if ((REG_A == 83) || (REG_A == 68)) goto pos37108;  // 'S' 'D'
	if (REG_A != 76) goto pos37135; // 'L'
	REG_X++;
	REG_A = inputtemp[REG_X];
	if (REG_A != 89) goto pos36700;
	goto pos37108;

pos37135:
	if (REG_A != 70) goto pos36700;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if (REG_A != 85) goto pos36700;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if (REG_A == 76) goto pos37108;
	goto pos36700;

pos37157:
	if (REG_A != 73) goto pos36700;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if (REG_A != 78) goto pos36700;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if (REG_A == 71) goto pos37108;
	//pos37177:
	goto pos36700;

	// -----------------------------------------

pos37180:

	REG_A = mem60;
	mem58 = REG_A;

pos37184:
	REG_Y = mem65 + 1;

	//37187: CPY 64
	//	if(? != 0) goto pos37194;
	if(REG_Y == mem64) goto pos37455;
	mem65 = REG_Y;
	//37196: LDA (62),y
	REG_A = GetRuleByte(mem62, REG_Y);
	mem57 = REG_A;
	REG_X = REG_A;
	//REG_A = pgm_read_byte_inlined(tab36376+REG_X)/*tab36376[X]*/ & 128;
	cp = tab36376 + REG_X;
	cp += EXEC_OFFSET;
	REG_A = pgm_read_byte(cp) & 128;

	if(REG_A == 0) goto pos37226;
	REG_X = mem58+1;
	REG_A = inputtemp[REG_X];
	if (REG_A != mem57) goto pos36700;
	mem58 = REG_X;
	goto pos37184;
pos37226:
	REG_A = mem57;
	if (REG_A == 32) goto pos37295;   // ' '
	if (REG_A == 35) goto pos37310;   // '#'
	if (REG_A == 46) goto pos37320;   // '.'
	if (REG_A == 38) goto pos37335;   // '&'
	if (REG_A == 64) goto pos37367;   // ''
	if (REG_A == 94) goto pos37404;   // ''
	if (REG_A == 43) goto pos37419;   // '+'
	if (REG_A == 58) goto pos37440;   // ':'
	if (REG_A == 37) goto pos37077;   // '%'
	//pos37291:
	//	Code42041(); //Error
	//37294: BRK
	return 0;

	// --------------
pos37295:
	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 128;
	if(REG_A != 0) goto pos36700;
pos37305:
	mem58 = REG_X;
	goto pos37184;

	// --------------

pos37310:
	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 64;
	if(REG_A != 0) goto pos37305;
	goto pos36700;

	// --------------


pos37320:
	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 8;
	if (REG_A == 0) goto pos36700;

pos37330:
	mem58 = REG_X;
	goto pos37184;

	// --------------

pos37335:
	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 16;
	if (REG_A != 0) goto pos37330;
	REG_A = inputtemp[REG_X];
	if (REG_A != 72) goto pos36700;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if ((REG_A == 67) || (REG_A == 83)) goto pos37330;
	goto pos36700;

	// --------------


pos37367:
	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 4;
	if (REG_A != 0) goto pos37330;
	REG_A = inputtemp[REG_X];
	if (REG_A != 72) goto pos36700;
	if ((REG_A != 84) && (REG_A != 67) && (REG_A != 83)) goto pos36700;
	mem58 = REG_X;
	goto pos37184;

	// --------------

pos37404:
	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 32;
	if (REG_A == 0) goto pos36700;
pos37414:
	mem58 = REG_X;
	goto pos37184;

	// --------------

pos37419:
	REG_X = mem58;
	REG_X++;
	REG_A = inputtemp[REG_X];
	if ((REG_A == 69) || (REG_A == 73) || (REG_A == 89)) goto pos37414;
	goto pos36700;

// ----------------------

pos37440:

	REG_A = Code37066(mem58); REG_X = mem58 + 1;
	REG_A = REG_A & 32;
	if (REG_A == 0) goto pos37184;
	mem58 = REG_X;
	goto pos37440;
pos37455:
	REG_Y = mem64;
	mem61 = mem60;

	if (DEBUG_ESP8266SAM_LIB)
		PrintRule(mem62);

pos37461:
	//37461: LDA (62),y
	REG_A = GetRuleByte(mem62, REG_Y);
	mem57 = REG_A;
	REG_A = REG_A & 127;
	if (REG_A != '=') {
		mem56++;
		REG_X = mem56;
		_input[REG_X] = REG_A;
	}

	//37478: BIT 57
	//37480: BPL 37485  //not negative flag
	if ((mem57 & 128) == 0) goto pos37485; //???
	goto pos36554;
pos37485:
	REG_Y++;
	goto pos37461;
}
