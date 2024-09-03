#include<stdio.h>

extern unsigned const char signInputTable1[];
extern unsigned const char signInputTable2[];

#define printf Serial.printf

MODULE_PART void PrintPhonemes(unsigned char *, unsigned char *, unsigned char *);

#define SAM_NO_DEBUG

void PrintPhonemes(unsigned char *_phonemeindex, unsigned char *_phonemeLength, unsigned char *_stress) {
SETMEMREGS

#ifndef SAM_NO_DEBUG
	int i = 0;
	printf("===========================================\n");

	printf("Internal Phoneme presentation:\n\n");
	printf(" idx    phoneme  length  _stress\n");
	printf("------------------------------\n");

	while((_phonemeindex[i] != 255) && (i < 255)) {
		if (_phonemeindex[i] < 81) {
			printf(" %3i      %c%c      %3i       %i\n",
			_phonemeindex[i],
			pgm_read_byte(&signInputTable1[_phonemeindex[i]]),
			pgm_read_byte(&signInputTable2[_phonemeindex[i]]),
			_phonemeLength[i],
			_stress[i]
			);
		} else {
			printf(" %3i      ??      %3i       %i\n", _phonemeindex[i], _phonemeLength[i], _stress[i]);
		}
		i++;
	}
	printf("===========================================\n");
	printf("\n");
	OsWatchLoop();
#endif
}

MODULE_PART void PrintOutput(
	unsigned char *flag,
	unsigned char *f1,
	unsigned char *f2,
	unsigned char *f3,
	unsigned char *a1,
	unsigned char *a2,
	unsigned char *a3,
	unsigned char *p)
{

#ifndef SAM_NO_DEBUG
	printf("===========================================\n");
	printf("Final data for speech output:\n\n");
	int i = 0;
	printf(" flags ampl1 freq1 ampl2 freq2 ampl3 freq3 pitch\n");
	printf("------------------------------------------------\n");
	while(i < 255) {
		printf("%5i %5i %5i %5i %5i %5i %5i %5i\n", flag[i], a1[i], f1[i], a2[i], f2[i], a3[i], f3[i], p[i]);
		i++;
	}
	printf("===========================================\n");
#endif
}

extern unsigned char GetRuleByte(unsigned short mem62, unsigned char Y);

MODULE_PART void PrintRule(int offset) {
#ifndef SAM_NO_DEBUG
	int i = 1;
	unsigned char A = 0;
	printf("Applying rule: ");
	do {
		A = GetRuleByte(offset, i);
		if ((A&127) == '=') printf(" -> "); else printf("%c", A&127);
		i++;
	} while ((A&128)==0);
	printf("\n");
#endif
}
