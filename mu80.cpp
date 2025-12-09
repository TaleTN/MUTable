// Copyright (C) 2019-2025 Theo Niessink <theo@taletn.com>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WDL/heapbuf.h"
#include "WDL/wdlendian.h"
#include "WDL/wdltypes.h"
#include "WDL/wavwrite.h"

int read_firmware(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK(size);
	if (!ptr) return 0;

	FILE* const fp = fopen(filename, "rb");
	if (!fp) return 0;

	const int n = (int)fread(ptr, 1, size, fp) & ~1;
	fclose(fp);

	for (int i = 0; i < n; i += 2)
	{
		unsigned short word;
		memcpy(&word, ptr, 2);

		word = WDL_bswap16(word);
		memcpy(ptr, &word, 2);

		ptr += 2;
	}

	return n == size ? size : 0;
}

int write_firmware(const char* const filename, const WDL_HeapBuf* const buf)
{
	FILE* const fp = fopen(filename, "wb");
	if (!fp) return 0;

	const int n = (int)fwrite(buf->Get(), 1, buf->GetSize(), fp);
	fclose(fp);

	return n;
}

int write_var_len(const int len, FILE* const fp)
{
	assert(len >= 0 && len <= 0x0FFFFFFF);

	if (len >= 0x200000) fputc((len >> 21) | 0x80, fp);
	if (len >= 0x4000) fputc(((len >> 14) & 0x7F) | 0x80, fp);
	if (len >= 0x80) fputc(((len >> 7) & 0x7F) | 0x80, fp);
	fputc(len & 0x7F, fp);

	return len;
}

int write_midi(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs)
{
	FILE* const fp = fopen(filename, "wb");
	if (!fp) return 0;

	static const unsigned char hdr[] =
	{
		'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0xC0, // 192 TPQN
		'M', 'T', 'r', 'k', 0x00, 0x00, 0x00, 0x00
	};

	fwrite(hdr, sizeof(hdr), 1, fp);

	const unsigned char* const buf = (const unsigned char*)firmware->Get();
	const int size = firmware->GetSize();

	for (int i = ofs, delta = 0; i < size;)
	{
		const int status = buf[i++];

		// The End
		if (status == 0xF2) break;

		// Delay
		if (status == 0xF3 || status == 0xF4)
		{
			for (int j = 0; j <= 7; j += 7)
			{
				assert(i < size);
				delta += buf[i++] << j;
				if (status == 0xF3) break;
			}
			continue;
		}

		write_var_len(delta, fp);
		delta = 0;

		// SysEx
		if (status == 0xF0)
		{
			int j = i;
			do
			{
				assert(j < size);
			}
			while (buf[j++] != 0xF7);

			fputc(status, fp);
			write_var_len(j - i, fp);

			do
			{
				fputc(buf[i], fp);
			}
			while (buf[i++] != 0xF7);

			continue;
		}

		switch (status & 0xF0)
		{
			// 2-byte Note Off
			case 0x80:
			{
				fputc(status | 0x10, fp);

				assert(i < size);
				fputc(buf[i++], fp);

				fputc(0, fp);
				break;
			}

			// 3-byte MIDI message
			case 0x90: case 0xA0: case 0xB0: case 0xE0:
			{
				fputc(status, fp);

				for (int j = 0; j < 2; ++j)
				{
					assert(i < size);
					fputc(buf[i++], fp);
				}
				break;
			}

			// 2-byte MIDI message
			case 0xC0: case 0xD0:
			{
				fputc(status, fp);

				assert(i < size);
				fputc(buf[i++], fp);
				break;
			}

			default: assert(false);
		}
	}

	static const int m = sizeof(hdr);
	int n = (int)ftell(fp) - m;
	n = WDL_bswap32_if_le(n);

	fseek(fp, m - 4, SEEK_SET);
	fwrite(&n, 4, 1, fp);
	fclose(fp);

	n = WDL_bswap32_if_le(n);
	n += m;

	return n;
}

void print_drum_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Drum Banks (+%d)\n", ofs);

	static const char* const name[] =
	{
		"TG300B",
		"XG",
		"SFX"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nDrum Bank %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Prog# "); for (int j = 1; j <= 10; ++j) printf("| %-3d ", j);
		printf("\n------"); for (int j = 1; j <= 10; ++j) printf("+-----");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int drum_kit_no = *ptr++;
			printf("| %-3d ", drum_kit_no);
		}

		putchar('\n');
	}
}

void print_drum_kits(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Drum Kits (+%d)\n", ofs);

	static const char* const name[] =
	{
		"TG300B Standard Kit",
		"TG300B Room Kit",
		"TG300B Power Kit",
		"TG300B Electro Kit",
		"TG300B Analog Kit",
		"TG300B Jazz Kit",
		"TG300B Brush Kit",
		"TG300B Orchestra Kit",
		"TG300B C/M Kit",
		"TG300B SFX Set",
		"XG Standard Kit",
		"XG Room Kit",
		"XG Rock Kit",
		"XG Electro Kit",
		"XG Analog Kit",
		"XG Jazz Kit",
		"XG Brush Kit",
		"XG Symphony Kit",
		"XG SFX Kit 1",
		"XG SFX Kit 2",
		"Silent Kit",
		"XG Standard Kit 2"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nDrum Kit %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Note# "); for (int j = 0; j < 10; ++j) printf("| %-6d ", j);
		printf("\n------"); for (int j = 0; j < 10; ++j) printf("+--------");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int drum_voice_ofs = (ptr[0] << 8) | ptr[1];
			printf("| %-+6d ", drum_voice_ofs);

			ptr += 2;
		}

		putchar('\n');
	}
}

/* Drum Voices

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 2    | 0000   - FFFF   | SFX/normal voice offset   | 0:drum, 1 - 65535:offset/2 | 0000
+2      | 1    | 00     - 7F     | Playback rate             | -64 - +63 [semitone]       | 40
+3      | 2    | 0000   - FFFF   | Attack length             | 0 - 65535 [samples]        | 0000
+5      | 1    | Bit 6  - 7      | Sample format             | 0:DPCM, 2:16-bit, 3:16-bit | 00
+6      | 2    | 0000   - FFFF   | Loop length               | 0 - 65535 [samples]        | 0000
+8      | 3    | 000000 - 3FFFFF | Loop point address        | 8 MB wave ROM address/2    | 000000
+11     | 1    | Bit 2  - 4      | DPCM scale                | 2^0 - 2^7                  | 00
        |      | Bit 0  - 1      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0      |
+12     | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]         | 00
+13     | 1    | 80     - 7F     | Pitch coarse              | -128 - +127 [semitone]     | 00
--------+------+-----------------+---------------------------+----------------------------+---------
+14     | 1    | 00     - 7F     | Pitch coarse              | 0 - 127 [note]             | 3C
+15     | 1    | 00     - 7F     | Pitch fine                | -64 - +63 [cent]           | 40
+16     | 1    | 00     - 7F     | Instrument level          | 0 - 127                    | 7F
+17     | 1    | 00     - 7F     | Alternate group           | 0:off, 1 - 127             | 00
+18     | 1    | 00     - 7F     | Pan                       | 0:random, 1 - 127          | 40
+19     | 1    | 00     - 7F     | Reverb send               | 0 - 127                    | 7F
+20     | 1    | 00     - 7F     | Chorus send               | 0 - 127                    | 7F
+21     | 1    | 00     - 7F     | Variation send            | 0 - 127                    | 7F
+22     | 1    | 00     - 01     | Key assign                | 0:single, 1:multi          | 00
+23     | 1    | 00     - 01     | Receive Note Off          | 0:off, 1:on                | 01
+24     | 1    | 00     - 01     | Receive Note On           | 0:off, 1:on                | 01
+25     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                    | 7F
+26     | 1    | 00     - 7F     | Filter resonance          | 0 - 127                    | 10
+27     | 1    | 00     - 7F     | Amp EG attack rate        | 0 - 127                    | 7F
+28     | 1    | 00     - 7F     | Amp EG decay 1 rate       | 0 - 127                    | 40
+29     | 1    | 00     - 7F     | Amp EG decay 2 rate       | 0 - 127                    | 20

*/
void print_drum_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Drum Voices (+%d)\n", ofs);

	static const char* const hdr  = "                                  Sample                                  |                                          Voice                                          \n"
	                                "Offset  | SFX     | Rat | Attack | F | Loop   | Addr   | D8 | PF   | PC   | PC  | PF  | Lvl | Alt | Pan | Rev | Cho | Var | K | 0 | 1 | Cut | Q   | A   | D1  | D2  ";
	static const char* const line = "--------+---------+-----+--------+---+--------+--------+----+------+------+-----+-----+-----+-----+-----+-----+-----+-----+---+---+---+-----+-----+-----+-----+-----";

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	ofs = 0;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 25))
		{
			if (!i) putchar('\n'); else puts(line);
			puts(hdr);
			puts(line);
		}

		printf("%-+7d | ", ofs);

		// Sample
		const int sfx_ofs = ((ptr[0] << 8) | ptr[1]) << 1;
		printf("%-+7d | ", sfx_ofs);

		printf("%-3d | ", ptr[2]);

		const int attack = (ptr[3] << 8) | ptr[4];
		const int format = ptr[5] >> 6;
		const int loop = (ptr[6] << 8) | ptr[7];
		const int addr = ((ptr[8] << 16) | (ptr[9] << 8) | ptr[10]) << 1;
		const int dpcm = ptr[11] /* & 0x1F */;

		printf("%-6d | %d | %-6d | ", attack, format, loop);
		printf("%06X | %02X | ", addr, dpcm);
		printf("%-+4d | %-+4d | ", (signed char)ptr[12], (signed char)ptr[13]);

		// Voice
		printf("%-3d | %-+3d | ", ptr[14], ptr[15] - 64);
		printf("%-3d | %-3d | %-3d | ", ptr[16], ptr[17], ptr[18]);
		printf("%-3d | %-3d | %-3d | ", ptr[19], ptr[20], ptr[21]);
		printf("%d | %d | %d | ", ptr[22], ptr[23], ptr[24]);
		printf("%-3d | %-3d | ", ptr[25], ptr[26]);
		printf("%-3d | %-3d | %-3d \n", ptr[27], ptr[28], ptr[29]);

		ptr += 30;
		ofs += 30;
	}
}

void print_bank_lists(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Bank Lists (+%d)\n", ofs);

	static const char* const name[] =
	{
		"TG300B",
		"XG",
		"SFX"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nBank List %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Bank# "); for (int j = 0; j < 10; ++j) printf("| %-3d ", j);
		printf("\n------"); for (int j = 0; j < 10; ++j) printf("+-----");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int prog_bank_no = *ptr++;
			printf("| %-3d ", prog_bank_no);
		}

		putchar('\n');
	}
}

void print_program_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Program Banks (+%d)\n", ofs);

	static const char* const name[] =
	{
		"TG300B Bank 0",
		"TG300B Bank 1",
		"TG300B Bank 2",
		"TG300B Bank 3",
		"TG300B Bank 4",
		"TG300B Bank 5",
		"TG300B Bank 6",
		"TG300B Bank 7",
		"TG300B Bank 8",
		"TG300B Bank 9",
		"TG300B Bank 10",
		"TG300B Bank 11",
		"TG300B Bank 16",
		"TG300B Bank 17",
		"TG300B Bank 18",
		"TG300B Bank 19",
		"TG300B Bank 24",
		"TG300B Bank 25",
		"TG300B Bank 26",
		"TG300B Bank 32",
		"TG300B Bank 33",
		"TG300B Bank 40",
		"?",
		"TG300B Bank 126",
		"TG300B Bank 127",
		"MU Basic",
		"Key Scale Panning",
		"Stereo",
		"Single",
		"Slow",
		"Fast Decay",
		"Double Attack",
		"Bright 1",
		"Bright 2",
		"Dark 1",
		"Dark 2",
		"Resonant",
		"Attack",
		"Release",
		"Resonant Sweep",
		"Muted",
		"Detune 1",
		"Detune 2",
		"Detune 3",
		"Octave 1",
		"Octave 2",
		"5th 1",
		"5th 2",
		"Bend",
		"Tutti 1",
		"Tutti 2",
		"Tutti 3",
		"Velocity Switch",
		"Velocity Crossfade",
		"Other Waves 1",
		"Other Waves 2",
		"Other Waves 3",
		"Other Waves 4",
		"Other Waves 5",
		"Other Waves 6",
		"Other Waves 7",
		"Other Waves 8",
		"Other Waves 9",
		"Other Instrument 1",
		"Other Instrument 2",
		"Other Instrument 3",
		"Other Instrument 4",
		"Other Instrument 5",
		"Other Instrument 6",
		"SFX",
		"Silence"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nBank %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Prog# "); for (int j = 1; j <= 10; ++j) printf("| %-7d ", j);
		printf("\n------"); for (int j = 1; j <= 10; ++j) printf("+---------");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int normal_voice_ofs = ((ptr[0] << 8) | ptr[1]) << 1;
			printf("| %-+7d ", normal_voice_ofs);

			ptr += 2;
		}

		putchar('\n');
	}
}

/* Normal Voices

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - 01     | Element switch            | 0:1, 1:1+2                 | 00
+1      | 1    | 00     - 7F     | Voice level               | y =~ (x/127)^2             |
+2      | 8    | 20     - 7F     | Voice name                | ASCII, space padded        |
+10     | 70   | See below       | Element 1                 | See below                  |
+80     | 70   | See below       | Element 2 (optional)      | See below                  |

Element 1/2

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - FF     | Wave#                     | 0 - 245                    |
+1      | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]             | 00
+2      | 1    | 00     - 7F     | Note limit high           | 0 - 127 [note]             | 7F
+3      | 1    | 01     - 7F     | Velocity limit low        | 1 - 127                    | 01
+4      | 1    | 01     - 7F     | Velocity limit high       | 1 - 127                    | 7F
--------+------+-----------------+---------------------------+----------------------------+---------
+5      | 1    | Bit 6  - 7      | LFO wave                  | 0:saw, 1:tri, 2:s&h        | 5F
        |      | Bit 0  - 5      | LFO speed                 | 0 - 63                     |
+6      | 1    | Bit 7           | LFO phase init            | 0:off, 1:on                | 00
        |      | Bit 0  - 6      | Vibrato delay time        | 0 - 127                    |
+7      | 1    | Bit 7           | Filter EG velocity curve  | 0:linear, 1:exponential    | 80
        |      | Bit 0  - 6      | Vibrato fade time         | 0 - 127                    |
+8      | 1    | Bit 6  - 7      | Pitch EG depth            | 0:0.5, 1:1, 2:2, 3:4 [oct] | 40
        |      | Bit 0  - 5      | LFO pitch mod depth       | 0 - 63                     |
+9      | 1    | Bit 4  - 7      | PEG velocity level sens.  | -7 - +7                    | 70
        |      | Bit 0  - 3      | LFO filter mod depth      | 0 - 15                     |
+10     | 1    | Bit 5  - 7      | Pitch scaling depth       | 0 - 5: 0/5/10/20/50/100%   | 00
        |      | Bit 0  - 4      | LFO amp mod depth         | 0 - 31                     |
--------+------+-----------------+---------------------------+----------------------------+---------
+11     | 1    | 20     - 60     | Note shift                | -32 - +32 [semitone]       | 40
+12     | 1    | 0E     - 72     | Detune                    | -50 - +50 [cent]           | 40
+13     | 1    | 00     - 7F     | Pitch scaling center note | 0 - 127 [note]             | 3C
+14     | 1    | Bit 4  - 7      | PEG velocity rate sens.   | -7 - +7                    | 77
        |      | Bit 0  - 3      | Pitch EG rate scaling     | -7 - +7                    |
+15     | 1    | 00     - 7F     | Pitch EG RS center note   | 0 - 127 [note]             | 3C
+16     | 1    | 00     - 3F     | Pitch EG attack rate      | 0 - 63                     | 3F
+17     | 1    | 00     - 3F     | Pitch EG decay 1 rate     | 0 - 63                     | 3F
+18     | 1    | 00     - 3F     | Pitch EG decay 2 rate     | 0 - 63                     | 3F
+19     | 1    | 00     - 3F     | Pitch EG release rate     | 0 - 63                     | 3F
+20     | 1    | 00     - 7F     | Pitch EG initial level    | -64 - +63                  | 40
+21     | 1    | 00     - 7F     | Pitch EG attack level     | -64 - +63                  | 40
+22     | 1    | 00     - 7F     | Pitch EG decay 1 level    | -64 - +63                  | 40
+23     | 1    | 00     - 7F     | Pitch EG decay 2 level    | -64 - +63                  | 40
+24     | 1    | 00     - 7F     | Pitch EG release level    | -64 - +63                  | 40
--------+------+-----------------+---------------------------+----------------------------+---------
+25     | 1    | 00     - 3F     | Filter resonance          | 0 - 63                     | 00
+26     | 1    | 00     - 07     | Velocity sensitivity      | 0 - 7                      | 00
+27     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                    | 7F
+28     | 1    | 00     - 7F     | Cutoff scaling break pt 1 | 0 - 127 [note]             | 18
+29     | 1    | 00     - 7F     | Cutoff scaling break pt 2 | 0 - 127 [note]             | 30
+30     | 1    | 00     - 7F     | Cutoff scaling break pt 3 | 0 - 127 [note]             | 48
+31     | 1    | 00     - 7F     | Cutoff scaling break pt 4 | 0 - 127 [note]             | 60
+32     | 1    | 00     - 7F     | Cutoff scaling offset 1   | -64 - +63                  | 40
+33     | 1    | 00     - 7F     | Cutoff scaling offset 2   | -64 - +63                  | 40
+34     | 1    | 00     - 7F     | Cutoff scaling offset 3   | -64 - +63                  | 40
+35     | 1    | 00     - 7F     | Cutoff scaling offset 4   | -64 - +63                  | 40
+36     | 1    | Bit 4  - 7      | FEG velocity level sens.  | -7 - +7                    | 77
        |      | Bit 0  - 3      | FEG velocity rate sens.   | -7 - +7                    |
+37     | 1    | Bit 4  - 7      | Velocity curve            | 0 - 6                      | 07
        |      | Bit 0  - 3      | Filter EG rate scaling    | -7 - +7                    |
+38     | 1    | 00     - 7F     | Filter EG RS center note  | 0 - 127 [note]             | 3C
+39     | 1    | 00     - 3F     | Filter EG attack rate     | 0 - 63                     | 3F
+40     | 1    | 00     - 3F     | Filter EG decay 1 rate    | 0 - 63                     | 3F
+41     | 1    | 00     - 3F     | Filter EG decay 2 rate    | 0 - 63                     | 3F
+42     | 1    | 00     - 3F     | Filter EG release rate    | 0 - 63                     | 3F
+43     | 1    | 00     - 7F     | Filter EG initial level   | -64 - +63                  | 40
+44     | 1    | 00     - 7F     | Filter EG attack level    | -64 - +63                  | 40
+45     | 1    | 00     - 7F     | Filter EG decay 1 level   | -64 - +63                  | 40
+46     | 1    | 00     - 7F     | Filter EG decay 2 level   | -64 - +63                  | 40
+47     | 1    | 00     - 7F     | Filter EG release level   | -64 - +63                  | 40
--------+------+-----------------+---------------------------+----------------------------+---------
+48     | 1    | 00     - 7F     | Element level             | 0 - 127, y =~ (x/127)^2    |
+49     | 1    | 00     - 7F     | Level scaling break pt 1  | 0 - 127 [note]             | 18
+50     | 1    | 00     - 7F     | Level scaling break pt 2  | 0 - 127 [note]             | 30
+51     | 1    | 00     - 7F     | Level scaling break pt 3  | 0 - 127 [note]             | 48
+52     | 1    | 00     - 7F     | Level scaling break pt 4  | 0 - 127 [note]             | 60
+53     | 1    | 00     - 7F     | Level scaling offset 1    | -64 - +63                  | 40
+54     | 1    | 00     - 7F     | Level scaling offset 2    | -64 - +63                  | 40
+55     | 1    | 00     - 7F     | Level scaling offset 3    | -64 - +63                  | 40
+56     | 1    | 00     - 7F     | Level scaling offset 4    | -64 - +63                  | 40
+57     | 1    | Bit 4  - 7      | Amp EG rate scaling       | -7 - +7                    | 77
        |      | Bit 0  - 3      | Pan                       | 0 - 14, 15:scaling         |
+58     | 1    | 00     - 7F     | Amp EG RS center note     | 0 - 127 [note]             | 3C
+59     | 1    | Bit 4  - 7      | Resonance sensitivity     | -7 - +7                    | 70
        |      | Bit 0  - 3      | Amp EG key on delay       | 0 - 15                     |
+60     | 1    | 00     - 3F     | Amp EG attack rate        | 0 - 63                     | 3F
+61     | 1    | 00     - 3F     | Amp EG decay 1 rate       | 0 - 63                     | 3F
+62     | 1    | 00     - 3F     | Amp EG decay 2 rate       | 0 - 63                     | 3F
+63     | 1    | 00     - 3F     | Amp EG release rate       | 0 - 63                     | 3F
+64     | 1    | 00     - 7F     | Amp EG decay 1 level      | -64 - +63                  | 40
+65     | 1    | 00     - 7F     | Amp EG decay 2 level      | -64 - +63                  | 40
+66     | 1    | 00     - 7F     | Address offset MSB        | Attack offset [samples]    | 00
+67     | 1    | 00     - 7F     | Address offset LSB        |                            | 00

*/
void print_normal_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Normal Voices (+%d)\n", ofs);

	static const char* const hdr  = "           Voice             |            Element            |                          LFO                           |                                       Pitch                                       |                                                                  Filter                                                                   |                                                         Amplitude                                                          \n"
	                                "Offset  | E | Lvl | Name     | Wave# | N1  | N2  | V1  | V2  | W | Sp | I | Del | F | Fad | D | PM | VL | FM | S | AM | NS  | Det | Ns  | VR | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Q  | VS | Cut | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VL | VR | VC | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Lvl | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | RS | P  | Nrs | QS | R0 | R1  | R2  | R3  | R4  | L2  | L3  | Addr   ";
	static const char* const line = "--------+---+-----+----------+-------+-----+-----+-----+-----+---+----+---+-----+---+-----+---+----+----+----+---+----+-----+-----+-----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+-----+----+----+-----+-----+-----+-----+-----+-----+--------";
	static const char* const skip = "        |   |     |          | ";

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	ofs = 0;

	for (int i = 0, j = 0; i < num; ++i)
	{
		if (!j)
		{
			if (!i) putchar('\n'); else puts(line);
			puts(hdr);
			puts(line);
		}

		printf("%-+7d | ", ofs);

		// Voice
		const int el = ptr[0] + 1;
		printf("%d | %-3d | ", el, ptr[1]);

		for (int k = 2; k < 10; ++k) putchar(ptr[k]);
		printf(" | ");
		ptr += 10;

		for (int k = 0; k < el; ++k)
		{
			if (k) printf(skip);

			// Element
			printf("%-5d | ", ptr[0]);
			printf("%-3d | %-3d | ", ptr[1], ptr[2]);
			printf("%-3d | %-3d | ", ptr[3], ptr[4]);

			// LFO
			printf("%d | %-2d | ", ptr[5] >> 6, ptr[5] & 0x3F);
			printf("%d | %-3d | ", ptr[6] >> 7, ptr[6] & 0x7F);
			printf("%d | %-3d | ", ptr[7] >> 7, ptr[7] & 0x7F);
			printf("%d | %-2d | ", ptr[8] >> 6, ptr[8] & 0x3F);
			printf("%-+2d | %-2d | ", (ptr[9] >> 4) - 7, ptr[9] & 0x0F);
			printf("%d | %-2d | ", ptr[10] >> 5, ptr[10] & 0x1F);

			// Pitch
			printf("%-+3d | %-+3d | ", ptr[11] - 64, ptr[12] - 64);
			printf("%-3d | %-+2d | ", ptr[13], (ptr[14] >> 4) - 7);
			printf("%-+2d | %-3d | ", (ptr[14] & 0x0F) - 7, ptr[15]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[16 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[20 + l] - 64);

			// Filter
			printf("%-2d | %-2d | %-3d | ", ptr[25], ptr[26], ptr[27]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[28 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[32 + l] - 64);
			printf("%-+2d | %-+2d | %-2d | ", (ptr[36] >> 4) - 7, (ptr[36] & 0x0F) - 7, ptr[37] >> 4);
			printf("%-+2d | %-3d | ", (ptr[37] & 0x0F) - 7, ptr[38]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[39 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[43 + l] - 64);

			// Amplitude
			printf("%-3d | ", ptr[48]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[49 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[53 + l] - 64);
			printf("%-+2d | %-2d | %-3d | ", (ptr[57] >> 4) - 7, ptr[57] & 0x0F, ptr[58]);
			printf("%-+2d | %-2d | ", (ptr[59] >> 4) - 7, ptr[59] & 0x0F);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[60 + l]);
			for (int l = 0; l < 2; ++l) printf("%-+3d | ", ptr[64 + l] - 64);
			printf("%-+6d \n", (ptr[66] << 7) | ptr[67]);

			ptr += 68;
		}

		ofs += 10 + el * 68;
		if ((j += el) >= 25) j = 0;
	}
}

void print_sample_sets(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Sample Sets (+%d)\n\n", ofs);

	printf("Wave# "); for (int i = 0; i < 10; ++i) printf("| %-6d ", i);
	printf("\n------"); for (int i = 0; i < 10; ++i) printf("+--------");

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 10)) printf("\n%-5d ", i);

		const int sample_ofs = (ptr[0] << 8) | ptr[1];
		printf("| %-+6d ", sample_ofs);

		ptr += 2;
	}

	putchar('\n');
}

/* Samples

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - 7F     | Attenuation               | y =~ 2^(-x/8)              | 00
+1      | 1    | 80     - 7F     | Pitch coarse              | -128 - 127 [note]          | 3C
+2      | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]         | 40
+3      | 2    | 0000   - FFFF   | Attack length             | 0 - 65535 [samples]        | 0000
+5      | 1    | Bit 6  - 7      | Sample format             | 0:DPCM, 2:16-bit, 3:16-bit | 00
+6      | 2    | 0000   - FFFF   | Loop length               | 0 - 65535 [samples]        | 0000
+8      | 3    | 000000 - 3FFFFF | Loop point address        | 8 MB wave ROM address/2    | 000000
+11     | 1    | Bit 2  - 4      | DPCM scale                | 2^0 - 2^7                  | 00
        |      | Bit 0  - 1      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0      |
+12     | 1    | 00              | Not used                  | 00                         | 00
+13     | 1    | 00     - FF     | Note limit high           | 0 - 126, 255 [note]        | FF

*/
void print_samples(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Samples (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | Atn | PC   | PF   | Attack | F | Loop   | Addr   | D8 | -  | N   ";
	static const char* const line = "--------+-----+------+------+--------+---+--------+--------+----+----+-----";

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	ofs = 0;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 25))
		{
			if (!i) putchar('\n'); else puts(line);
			puts(hdr);
			puts(line);
		}

		printf("%-+7d | ", ofs);

		printf("%-3d | ", ptr[0]);
		printf("%-4d | %-+4d | ", (signed char)ptr[1], (signed char)ptr[2]);

		const int attack = (ptr[3] << 8) | ptr[4];
		const int format = ptr[5] >> 6;
		const int loop = (ptr[6] << 8) | ptr[7];
		const int addr = ((ptr[8] << 16) | (ptr[9] << 8) | ptr[10]) << 1;
		const int dpcm = ptr[11] /* & 0x1F */;

		printf("%-6d | %d | %-6d | ", attack, format, loop);
		printf("%06X | %02X | ", addr, dpcm);
		printf("%02X | %-3d \n", ptr[12], ptr[13]);

		ptr += 14;
		ofs += 14;
	}
}

void write_utf16_str(const char* const str, FILE* const fp)
{
	for (int i = 0;; ++i)
	{
		const unsigned short c = str[i];
		if (!c) break;

		#ifdef _WIN32
		if (c == '\n')
		{
			static const unsigned short cr = '\r';
			fwrite(&cr, 2, 1, fp);
		}
		#endif

		fwrite(&c, 2, 1, fp);
	}
}

int print_bitmaps(FILE* const fp, const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	static const unsigned short block[4][2] =
	{
		{ ' ',    ' '    }, // Space
		{ 0x2580, 0x2580 }, // Upper half block
		{ 0x2584, 0x2584 }, // Lower half block
		{ 0x2588, 0x2588 }  // Full block
	};

	#ifdef _WIN32
	static const size_t n = 2;
	const unsigned short eol[n] = { '\r', '\n' };
	#else
	static const size_t n = 1;
	const unsigned short eol[n] = { '\n' };
	#endif

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		char str[16];
		sprintf(str, "+%d\n\n", ofs);

		write_utf16_str(str, fp);

		for (int j = 0; j < 8; ++j)
		{
			const int line1 = (ptr[0] << 8) | ptr[1];
			const int line2 = (ptr[2] << 8) | ptr[3];

			static const unsigned short space = ' ';
			fwrite(&space, 2, 1, fp);

			for (int k = 0x8000; k; k >>= 1)
			{
				const int bit1 = !!(line1 & k);
				const int bit2 = !!(line2 & k);

				fwrite(block[(bit2 << 1) | bit1], 2, 2, fp);
			}

			fwrite(eol, 2, n, fp);
			ptr += 4;
		}

		fwrite(eol, 2, n, fp);
		ofs += 32;
	}

	return num;
}

int read_wavetbl(const char* const filenames[4], WDL_HeapBuf* const buf, const int num, const int size)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK(num * size);
	if (!ptr) return 0;

	int total = 0;

	for (int i = 0; i < num; ++i)
	{
		FILE* const fp = fopen(filenames[i], "rb");
		if (!fp) return 0;

		const int n = (int)fread(ptr, 1, size, fp);
		fclose(fp);

		total += n;
		ptr += n;
	}

	return total == buf->GetSize() ? total : 0;
}

// https://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c

void write_cue_points(WaveWriter* const wav, const int num, const int loop, const int end)
{
	assert(num == 1 || num == 2);

	static const int max_num = 2;
	int cue[3 + max_num * 6];

	cue[0] = WDL_bswap32_if_le('cue ');
	cue[1] = WDL_bswap32_if_be((1 + num * 6) * sizeof(int));
	cue[2] = WDL_bswap32_if_be(num);

	int* ptr = &cue[3];
	int ofs = num == 2 ? loop : end;

	for (int i = 0; i < num; ++i)
	{
		ptr[0] = WDL_bswap32_if_be(i);
		ptr[1] = 0;
		ptr[2] = WDL_bswap32_if_le('data');
		ptr[3] = 0;
		ptr[4] = 0;
		ptr[5] = WDL_bswap32_if_be(ofs);

		ofs = end;
		ptr += 6;
	}

	wav->WriteChunk(cue, (3 + num * 6) * sizeof(int));
}

int write_sample(const char* const filename, const WDL_HeapBuf* const wavetbl, const int format, const int addr, const int attack, const int loop, const int dpcm = 0)
{
	const unsigned char* const buf = (const unsigned char*)wavetbl->Get() + addr;

	static const int extra = 3;
	const int len = attack + loop + extra;

	WaveWriter wav;
	if (!wav.Open(filename, 16, 1, 44100, 0)) return 0;

	switch (format)
	{
		// 8-bit signed log DPCM
		case 0:
		{
			static const unsigned short log_tbl[128] =
			{
				0, 8, 16, 24, 32, 40, 48, 56, 64, 72,
				80, 88, 96, 104, 112, 120, 128, 136, 144, 152,
				160, 168, 176, 184, 192, 200, 208, 216, 224, 232,
				240, 248, 256, 272, 288, 304, 320, 336, 352, 368,
				384, 400, 416, 432, 448, 464, 480, 496, 512, 528,
				544, 560, 576, 592, 608, 624, 640, 656, 672, 688,
				704, 720, 736, 752, 768, 800, 832, 864, 896, 928,
				960, 992, 1024, 1056, 1088, 1120, 1152, 1184, 1216, 1248,
				1280, 1312, 1344, 1376, 1408, 1440, 1472, 1504, 1536, 1568,
				1600, 1632, 1664, 1696, 1728, 1760, 1792, 1856, 1920, 1984,
				2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496, 2560, 2624,
				2688, 2752, 2816, 2880, 2944, 3008, 3072, 3136, 3200, 3264,
				3328, 3392, 3456, 3520, 3584, 3648, 3712, 3776
			};

			static const unsigned char ofs_tbl[4] = { 7, 6, 4, 0 };
			const int ofs = ofs_tbl[dpcm & 3], scale = (dpcm >> 2) & 7;

			const unsigned char* const ptr = buf - attack;
			int sum = 0;

			for (int i = 0; i < len; ++i)
			{
				const int step = ptr[i];

				const int delta = log_tbl[step & 0x7F];
				const int min_delta = -delta;

				sum += step & 0x80 ? min_delta : delta;
				sum -= ofs;

				int y = (sum << scale) >> 3;

				y = wdl_max(y, -32768);
				y = wdl_min(y, +32767);

				short sample = WDL_bswap16_if_be((short)y);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		// 16-bit signed linear PCM
		case 2: case 3:
		{
			const unsigned char* ptr = buf - (attack << 1);

			for (int i = 0; i < len; ++i)
			{
				short sample = ptr[0] | (ptr[1] << 8);

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);

				ptr += 2;
			}
			break;
		}

		default: assert(false);
	}

	wav.EndDataChunk();
	write_cue_points(&wav, loop ? 2 : 1, attack, len - extra);
	wav.Close();

	return len;
}

int extract_sample(const char* const filename, const int ofs, const WDL_HeapBuf* const wavetbl, const unsigned char* const ptr)
{
	const int attack = (ptr[0] << 8) | ptr[1];
	const int format = ptr[2] >> 6;
	const int loop = (ptr[3] << 8) | ptr[4];

	if (!(attack || loop)) return 0;

	const int addr = ((ptr[5] << 16) | (ptr[6] << 8) | ptr[7]) << 1;
	const int dpcm = ptr[8] & 0x1F;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 925;
	static unsigned char sample_list[max_samples][8];

	static int num_samples = 0;
	unsigned char hash[8];

	memcpy(hash, ptr, 8);
	hash[2] = (format << 6) | (format == 0 ? dpcm : 0);

	for (int i = 0; i < num_samples; ++i)
	{
		if (!memcmp(sample_list[i], hash, 8)) return 0;
	}

	assert(num_samples < max_samples);
	memcpy(sample_list[num_samples++], hash, 8);

	#endif

	char fn[128];
	sprintf(fn, filename, ofs);

	return write_sample(fn, wavetbl, format, addr, attack, loop, dpcm);
}

int extract_drum_samples(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs, const int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		const int sfx_ofs = (ptr[0] << 8) | ptr[1];

		if (!sfx_ofs)
		{
			n += extract_sample(filename, i * 30, wavetbl, &ptr[3]) > 0;
		}

		ptr += 30;
	}

	return n;
}

int extract_samples(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs, const int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		n += extract_sample(filename, i * 14, wavetbl, &ptr[3]) > 0;
		ptr += 14;
	}

	return n;
}

int main(const int argc, const char* const* const argv)
{
	int opt = argc == 2 ? argv[1][0] : 0;
	if (opt == '-') opt = argv[1][1] | ('-' << 8);

	static const char* const roms[] =
	{
		// IC8 PROGRAM ROM 4M v1.04
		"mu80/xq556a0.ic8", // SHA1(a11bd4523cd8ff1e1744078c3b4c18112b73c61e)

		// IC31 XQ012B0 WAVE ROM 1 16M
		"mu80/xq012b0-822.bin", // SHA1(43dab164de5497df9203a1ac9e7ece478276e46d)
		// IC32 XQ013B0 WAVE ROM 2 16M
		"mu80/xq013b0-823.bin", // SHA1(fc603b7b7a3f3500521d4d9638a9562f90cc0354)

		// IC33 XQ089B0 WAVE ROM 3 16M
		"mu80/xq089b0-824.bin", // SHA1(ecc4c1cfb123d12bc3dad092c31bddc707bb4d07)
		// IC43 XQ090B0 WAVE ROM 4 16M
		"mu80/xq090b0-825.bin"  // SHA1(14073c41fbdf4faa9da9c83dafe4dc2d6b01b53b)
	};

	WDL_HeapBuf firmware;

	if (!read_firmware(roms[0], &firmware, 512*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-f')
	{
		const int size = write_firmware("build/mu80_firmware.bin", &firmware);
		return size == firmware.GetSize() ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (opt == '-m')
	{
		const int size = write_midi("midi/mu80_demo.mid", &firmware, +489152);
		const int n = size > 0;

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	if (opt == '-t')
	{
		printf("MU80 Data Tables\n\n");

		print_drum_banks(&firmware, +33842, 3); puts("\n--\n");
		print_drum_kits(&firmware, +28210, 22); puts("\n--\n");
		print_drum_voices(&firmware, +18926, 309); puts("\n--\n");

		print_bank_lists(&firmware, +52402, 3); puts("\n--\n");
		print_program_banks(&firmware, +34226, 71); puts("\n--\n");
		print_normal_voices(&firmware, +58284, 741); puts("\n--\n");

		print_sample_sets(&firmware, +18414, 248); puts("\n--\n");
		print_samples(&firmware, +256, 1295);

		return EXIT_SUCCESS;
	}

	if (opt == '-b')
	{
		FILE* const fp = fopen("table/mu80_bitmap.txt", "wb");
		if (!fp) return EXIT_FAILURE;

		static const unsigned short bom = 0xFEFF;
		fwrite(&bom, 2, 1, fp);

		write_utf16_str("MU80 Bitmaps\n\n", fp);

		int n = print_bitmaps(fp, &firmware, +388376, 23);
		n += print_bitmaps(fp, &firmware, +425086, 201);

		fclose(fp);
		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(&roms[1], &wavetbl, 4, 2*1024*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-w')
	{
		int n = extract_drum_samples("wave/mu80/drum_%05d.wav", &firmware, +18926, 309, &wavetbl);
		n += extract_samples("wave/mu80/sample_%05d.wav", &firmware, +256, 1295, &wavetbl);

		n += write_sample("wave/mu80/lost+found_00000.wav", &wavetbl, 0, 0x2F1AA6, 7309, 961, 0x1F) > 0;
		n += write_sample("wave/mu80/lost+found_00001.wav", &wavetbl, 0, 0x5D9760, 1530, 69, 0x17) > 0;
		n += write_sample("wave/mu80/lost+found_00002.wav", &wavetbl, 2, 0x7750CC, 66145, 0) > 0;
		n += write_sample("wave/mu80/lost+found_00003.wav", &wavetbl, 2, 0x797E24, 71337, 0) > 0;
		n += write_sample("wave/mu80/lost+found_00004.wav", &wavetbl, 2, 0x7FFFFA, 3025, 0) > 0;

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -f | -m | -t | -b | -w\n", argv[0]);
	return EXIT_FAILURE;
}
