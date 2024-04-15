// Copyright (C) 2021-2024 Theo Niessink <theo@taletn.com>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WDL/heapbuf.h"
#include "WDL/wdlendian.h"
#include "WDL/wavwrite.h"

int read_firmware(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	void* const ptr = buf->ResizeOK(size);
	if (!ptr) return 0;

	FILE* const fp = fopen(filename, "rb");
	if (!fp) return 0;

	const int n = (int)fread(ptr, 1, size, fp);
	fclose(fp);

	return n == size ? size : 0;
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
		'M', 'T', 'r', 'k', 0x00, 0x00, 0x00, 0x07, 0x00, 0xFF, 0x51, 0x03, 0x07, 0x59, 0x9A // ~124.56 BPM
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

	static const int m = sizeof(hdr) - 7;
	int n = (int)ftell(fp) - m;
	n = WDL_bswap32_if_le(n);

	fseek(fp, m - 4, SEEK_SET);
	fwrite(&n, 4, 1, fp);
	fclose(fp);

	n = WDL_bswap32_if_le(n);
	n += m;

	return n;
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
	static const unsigned short block[4] =
	{
		' ',    // Space
		0x2580, // Upper half block
		0x2584, // Lower half block
		0x2588  // Full block
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

		for (int j = 0; j < 4; ++j)
		{
			const int line1 = ptr[0];
			const int line2 = ptr[1];

			static const unsigned short space = ' ';
			fwrite(&space, 2, 1, fp);

			for (int k = 0x10; k; k >>= 1)
			{
				const int bit1 = !!(line1 & k);
				const int bit2 = !!(line2 & k);

				fwrite(&block[(bit2 << 1) | bit1], 2, 1, fp);
			}

			fwrite(eol, 2, n, fp);
			ptr += 2;
		}

		fwrite(eol, 2, n, fp);
		ofs += 8;
	}

	return num;
}

int read_wavetbl(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	return read_firmware(filename, buf, size);
}

void print_drum_bank(const WDL_HeapBuf* const firmware, const int ofs)
{
	printf("Drum Bank (+%d)\n\n", ofs);

	const unsigned char* const buf = (const unsigned char*)firmware->Get() + ofs;

	printf("Prog# "); for (int i = 1; i <= 10; ++i) printf("| %-3d ", i);
	printf("\n------"); for (int i = 1; i <= 10; ++i) printf("+-----");

	for (int i = 0; i < 128; ++i)
	{
		if (!(i % 10)) printf("\n%-5d ", i);

		const int drum_kit_no = buf[i];
		printf("| %-3d ", drum_kit_no);
	}

	putchar('\n');
}

void print_drum_kits(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Drum Kits (+%d)\n", ofs);

	static const char* const name[] =
	{
		"Standard Kit",
		"Room Kit",
		"Power Kit",
		"Electronic Kit",
		"Analog Kit",
		"Brush Kit",
		"Orchestra Kit",
		"Clavinova Kit",
		"RX Kit",
		"C/M Kit"
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

			const int drum_voice_ofs = ((ptr[0] << 8) | ptr[1]) * 6;
			printf("| %-+6d ", drum_voice_ofs);

			ptr += 2;
		}

		putchar('\n');
	}
}

/* Drum Voices

Offset  | Size | Data            | Parameter                    | Description              | Default
--------+------+-----------------+------------------------------+--------------------------+---------
+0      | 2    | 0000   - 03FF   | Wavetable#                   | 0 - 1023                 |
+2      | 1    | 80     - 7F     | Pitch coarse                 | -128 - +127 [semitone]   | 00
+3      | 1    | 00     - 63     | Pitch fine                   | 0 - 99 [cent]            | 00
+4      | 1    | 00     - 7F     | Attenuation                  | 0 - 127                  | 00
+5      | 1    | Bit 4  - 7      | Reverb depth                 | 0 - 8                    | 00
        |      | Bit 0  - 3      | Panpot                       | L7 - R7                  |

*/
void print_drum_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Drum Voices (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | Wavetbl | PC   | PF  | Atn | Rev | Pan ";
	static const char* const line = "--------+---------+------+-----+-----+-----+-----";

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

		const int wavetbl_ofs = ((ptr[0] << 8) | ptr[1]) * 12;
		printf("%-+7d | ", wavetbl_ofs);

		printf("%-+4d | %-+3d | ", (signed char)ptr[2], ptr[3]);
		printf("%-3d | ", ptr[4]);
		printf("%-3d | %-+3d \n", ptr[5] >> 4, ((int)ptr[5] << 28) >> 28);

		ptr += 6;
		ofs += 6;
	}
}

void print_bank_list(const WDL_HeapBuf* const firmware, const int ofs)
{
	printf("Bank List (+%d)\n\n", ofs);

	const unsigned char* const buf = (const unsigned char*)firmware->Get() + ofs;

	printf("Bank# "); for (int i = 0; i < 10; ++i) printf("| %-3d ", i);
	printf("\n------"); for (int i = 0; i < 10; ++i) printf("+-----");

	for (int i = 0; i < 128; ++i)
	{
		if (!(i % 10)) printf("\n%-5d ", i);

		const int bank_no = buf[i];
		printf("| %-3d ", bank_no);
	}

	putchar('\n');
}

void print_program_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num, const char* const title)
{
	printf("%s (+%d)\n", title, ofs);

	static const char* const name[] =
	{
		"General MIDI",
		"Disk Orchestra",
		"C/M Parts 1 - 10",
		"C/M Parts 11 - 16"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		putchar('\n');

		if (num > 1)
		{
			printf("Bank %d", i);
			if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
			puts("\n");
		}

		printf("Prog# "); for (int j = 1; j <= 10; ++j) printf("| %-7d ", j);
		printf("\n------"); for (int j = 1; j <= 10; ++j) printf("+---------");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int normal_voice_ofs = ((ptr[0] << 4) | ptr[1]) * 96;
			printf("| %-+7d ", normal_voice_ofs);

			ptr += 2;
		}

		putchar('\n');
	}
}

/* Normal Voices

Offset  | Size | Data            | Parameter                    | Description              | Default
--------+------+-----------------+------------------------------+--------------------------+---------
+0      | 1    | 00     - 01     | Voice mode                   | 0:1 element, 1:2 element | 00
+1      | 1    | 00     - 7F     | Element 1 level              | 0 - 127                  | 7F
+2      | 1    | 00     - 7F     | Element 2 level              | 0 - 127                  | 7F
+3      | 1    | 20     - 5F     | Element 1 detune             | -32 - +31                | 40
+4      | 1    | 20     - 5F     | Element 2 detune             | -32 - +31                | 40
+5      | 1    | 00     - 7F     | Portamento time              | 0 - 127                  | 01
+6      | 1    | 00     - 0F     | Mod LFO pitch depth          | 0 - 15                   | 0F
+7      | 1    | 00     - 7F     | Don't care                   | 00                       | 00
+8      | 1    | 00     - 0F     | CAF LFO pitch depth          | 0 - 15                   | 00
+9      | 1    | 00     - 7F     | Don't care                   | 00                       | 00
+10     | 1    | 00     - 05     | Element 1 pitch rate scaling | 0 - 5: 100/50/20/10/5/0% | 00
+11     | 1    | 00     - 7F     | Element 1 pitch center note  | 0 - 127 [note]           | 3C
+12     | 1    | 28     - 58     | Element 1 note shift         | -24 - +24 [semitone]     | 40
+13     | 1    | 28     - 58     | Element 2 note shift         | -24 - +24 [semitone]     | 40
+14     | 1    | 00     - 05     | Element 2 pitch rate scaling | 0 - 5: 100/50/20/10/5/0% | 00
+15     | 1    | 00     - 7F     | Element 2 pitch center note  | 0 - 127 [note]           | 3C
+16     | 8    | 20     - 7F     | Voice name                   | ASCII, space padded      | 20
--------+------+-----------------+------------------------------+--------------------------+---------
+24     | 36   | See below       | Element 1 parameters         | See below                |
+60     | 36   | See below       | Element 2 parameters         | See below                |

Element 1/2 Parameters

Offset  | Size | Data            | Parameter                    | Description              | Default
--------+------+-----------------+------------------------------+--------------------------+---------
+0      | 1    | 00     - 0F     | Waveform# MSB                | 0 - 139                  | 00
+1      | 1    | 00     - 0F     | Waveform# LSB                |                          | 00
+2      | 1    | 31     - 4F     | EG attack rate               | -15..+15                 | 40
+3      | 1    | 31     - 4F     | EG release rate              | -15..+15                 | 40
+4      | 1    | 00     - 7F     | Level scaling break point 1  | 0 - 127 [note]           | 40
+5      | 1    | 00     - 7F     | Level scaling break point 2  | 0 - 127 [note]           | 40
+6      | 1    | 00     - 7F     | Level scaling break point 3  | 0 - 127 [note]           | 40
+7      | 1    | 00     - 7F     | Level scaling break point 4  | 0 - 127 [note]           | 40
+8      | 1    | 00     - 0F     | Level scaling offset 1 MSB   | -128 - +127              | 08
+9      | 1    | 00     - 0F     | Level scaling offset 1 LSB   |                          | 00
+10     | 1    | 00     - 0F     | Level scaling offset 2 MSB   | -128 - +127              | 08
+11     | 1    | 00     - 0F     | Level scaling offset 2 LSB   |                          | 00
+12     | 1    | 00     - 0F     | Level scaling offset 3 MSB   | -128 - +127              | 08
+13     | 1    | 00     - 0F     | Level scaling offset 3 LSB   |                          | 00
+14     | 1    | 00     - 0F     | Level scaling offset 4 MSB   | -128 - +127              | 08
+15     | 1    | 00     - 0F     | Level scaling offset 4 LSB   |                          | 00
+16     | 1    | 00     - 0F     | Panpot                       | L7 - R7                  | 00
--------+------+-----------------+------------------------------+--------------------------+---------
+17     | 1    | 00     - 07     | LFO speed                    | 0 - 7                    | 04
+18     | 1    | 00     - 7F     | LFO delay                    | 0 - 127                  | 00
+19     | 1    | 00     - 7F     | Don't care                   | 00                       | 00
+20     | 1    | 00     - 0F     | LFO pitch mod depth          | 0 - 15                   | 00
+21     | 1    | 00     - 07     | LFO amp mod depth            | 0 - 7                    | 00
+22     | 1    | 00     - 01     | Pitch LFO wave               | 0:tri, 1:s&h             | 00
--------+------+-----------------+------------------------------+--------------------------+---------
+23     | 1    | 00     - 02     | Pitch EG range               | 0:0.5, 1:1, 2:2 [oct]    | 01
+24     | 1    | 00     - 01     | Pitch EG velocity switch     | 0:on, 1:off              | 01
+25     | 1    | 00     - 07     | Pitch EG rate scaling        | 0 - 7                    | 00
+26     | 1    | 00     - 3F     | Pitch EG rate 1              | 0 - 63                   | 3F
+27     | 1    | 00     - 3F     | Pitch EG rate 2              | 0 - 63                   | 3F
+28     | 1    | 00     - 3F     | Pitch EG rate 3              | 0 - 63                   | 3F
+29     | 1    | 00     - 3F     | Pitch EG release rate        | 0 - 63                   | 3F
+30     | 1    | 00     - 7F     | Pitch EG level 0             | -64 - +63                | 40
+31     | 1    | 00     - 7F     | Pitch EG level 1             | -64 - +63                | 40
+32     | 1    | 00     - 7F     | Pitch EG level 2             | -64 - +63                | 40
+33     | 1    | 00     - 7F     | Pitch EG level 3             | -64 - +63                | 40
+34     | 1    | 00     - 7F     | Pitch EG release level       | -64 - +63                | 40
+35     | 1    | 00     - 07     | Velocity curve               | 0 - 7: curve 1 - 8       | 00

*/
void print_normal_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Normal Voices (+%d)\n", ofs);

	static const char* const hdr  = "                                               Voice                                               |                                   Element                                   |          LFO           |                             Pitch EG                              \n"
	                                "Offset  | E | Lv1 | Lv2 | Dt1 | Dt2 | PT  | Mod | CAF | S1 | Ns1 | NS1 | NS2 | S2 | Ns2 | Name     | Wave# | AR  | RR  | BP1 | BP2 | BP3 | BP4 | Ofs1 | Ofs2 | Ofs3 | Ofs4 | Pan | Sp | Del | PM | AM | W | R | V | RS | R1 | R2 | R3 | RR | L0  | L1  | L2  | L3  | RL  | VC ";
	static const char* const line = "--------+---+-----+-----+-----+-----+-----+-----+-----+----+-----+-----+-----+----+-----+----------+-------+-----+-----+-----+-----+-----+-----+------+------+------+------+-----+----+-----+----+----+---+---+---+----+----+----+----+----+-----+-----+-----+-----+-----+----";
	static const char* const skip = "        |   |     |     |     |     |     |     |     |    |     |     |     |    |     |          | ";

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
		printf("%d | ", el);

		printf("%-3d | %-3d | ", ptr[1], ptr[2]);
		printf("%-+3d | %-+3d | ", ptr[3] - 64, ptr[4] - 64);
		printf("%-3d | %-3d | %-3d | ", ptr[5], ptr[6], ptr[8]);
		printf("%-2d | %-3d | ", ptr[10], ptr[11]);
		printf("%-+3d | %-+3d | ", ptr[12] - 64, ptr[13] - 64);
		printf("%-2d | %-3d | ", ptr[14], ptr[15]);

		for (int k = 16; k < 24; ++k) putchar(ptr[k]);
		printf(" | ");
		ptr += 24;

		for (int k = 0; k < el; ++k)
		{
			if (k) printf(skip);

			// Element
			printf("%-5d | ", (ptr[0] << 4) | ptr[1]);

			// Amplitude
			printf("%-+3d | %-+3d | ", ptr[2] - 64, ptr[3] - 64);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[4 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+4d | ", ((ptr[8 + 2*l] << 4) | ptr[9 + 2*l]) - 128);
			printf("%-+3d | ", ((int)ptr[16] << 28) >> 28);

			// LFO
			printf("%-2d | %-3d | ", ptr[17] & 0x07, ptr[18]);
			printf("%-2d | %-2d | %d | ", ptr[20], ptr[21], ptr[22]);

			// Pitch
			printf("%d | %d | %d  | ", ptr[23], ptr[24], ptr[25]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[26 + l]);
			for (int l = 0; l < 5; ++l) printf("%-3d | ", ptr[30 + l]);

			printf("%-2d \n", ptr[35]);
			ptr += 36;
		}

		ptr += (2 - el) * 36;
		ofs += 96;

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

		const int sample_ofs = ((ptr[0] << 8) | ptr[1]) * 9;
		printf("| %-+6d ", sample_ofs);

		ptr += 2;
	}

	putchar('\n');
}

/* Samples

Offset  | Size | Data            | Parameter                    | Description              | Default
--------+------+-----------------+------------------------------+--------------------------+---------
+0      | 2    | 0000   - 03FF   | Wavetable#                   | 0 - 1023                 |
+2      | 1    | 00     - 7F     | Note limit low               | 0 - 127 [note]           | 00
+3      | 1    | 00     - 7F     | Note limit high              | 0 - 127 [note]           | 7F
+4      | 1    | 00     - 7F     | Pitch coarse                 | ?                        | 3C
+5      | 1    | 00     - 7F     | Pitch fine                   | ?                        | 00
+6      | 1    | 00     - 7F     | Attenuation                  | 0 - 127                  | 00
+7      | 1    | Bit 4  - 7      | Attack rate                  | 0 - 15                   | F0
        |      | Bit 0  - 3      | Decay 1 rate                 | 0 - 15                   |
+8      | 1    | Bit 4  - 7      | Rate correction              | 0 - 15                   | 0F
        |      | Bit 0  - 3      | Release rate                 | 0 - 15                   |

*/
void print_samples(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Samples (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | Wavetbl | N1  | N2  | PC  | PF  | Atn | AR | D1R | RC | RR ";
	static const char* const line = "--------+---------+-----+-----+-----+-----+-----+----+-----+----+----";

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

		const int wavetbl_ofs = ((ptr[0] << 8) | ptr[1]) * 12;
		printf("%-+7d | ", wavetbl_ofs);

		printf("%-3d | %-3d | ", ptr[2], ptr[3]);
		printf("%-3d | %-3d | ", ptr[4], ptr[5]);
		printf("%-3d | ", ptr[6]);
		printf("%-2d | %-2d  | ", ptr[7] >> 4, ptr[7] & 0x0F);
		printf("%-2d | %-2d \n", ptr[8] >> 4, ptr[8] & 0x0F);

		ptr += 9;
		ofs += 9;
	}
}

/* Wavetables

Offset  | Size | Data            | Parameter                    | Description              | Default
--------+------+-----------------+------------------------------+--------------------------+---------
+0      | 1    | Bit 6  - 7      | Sample format                | 3:12-bit                 |
+0      | 3    | 000000 - 3FFFFF | Start address                | 2 MB wave ROM address    | 000000
+3      | 2    | 0000   - FFFF   | Loop address                 | 0 - 65535 [samples]      | 0000
+5      | 2    | 0000   - FFFF   | End address                  | 1 - 65536 [samples]      | 0000
+7      | 1    | Bit 3  - 5      | LFO speed                    | 0 - 7                    | 00
        |      | Bit 0  - 2      | Vibrato depth                | 0 - 7                    |
+8      | 1    | Bit 4  - 7      | Attack rate                  | 0 - 15                   | 00
        |      | Bit 0  - 3      | Decay 1 rate                 | 0 - 15                   |
+9      | 1    | Bit 4  - 7      | Decay level                  | 0 - 15                   | 00
        |      | Bit 0  - 3      | Decay 2 rate                 | 0 - 15                   |
+10     | 1    | Bit 4  - 7      | Rate correction              | 0 - 15                   | 00
        |      | Bit 0  - 3      | Release rate                 | 0 - 15                   |
+11     | 1    | Bit 0  - 2      | Tremolo depth                | 0 - 7                    | 00

*/
void print_wavetbl(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Wavetables (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | F | Addr   | Loop  | End   | Sp | PM | AR | D1R | DL | D2R | RC | RR | AM ";
	static const char* const line = "--------+---+--------+-------+-------+----+----+----+-----+----+-----+----+----+----";

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

		const int format = ptr[0] >> 6;
		const int addr = ((ptr[0] & 0x3F) << 16) | (ptr[1] << 8) | ptr[2];
		const int loop = (ptr[3] << 8) | ptr[4];

		int end = (ptr[5] << 8) | ptr[6];
		end ^= addr ? 0xFFFF : 0;

		printf("%d | %06X | ", format, addr);
		printf("%-5d | %-5d | ", loop, end);

		printf("%d  | %d  | ", (ptr[7] >> 3) & 0x07, ptr[7] & 0x07);
		printf("%-2d | %-2d  | ", ptr[8] >> 4, ptr[8] & 0x0F);
		printf("%-2d | %-2d  | ", ptr[9] >> 4, ptr[9] & 0x0F);
		printf("%-2d | %-2d | ", ptr[10] >> 4, ptr[10] & 0x0F);
		printf("%-2d \n", ptr[11] & 0x07);

		ptr += 12;
		ofs += 12;
	}
}

void print_velocity_curves(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Velocity Curves (+%d)\n", ofs);

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nCurve %d\n\n", i + 1);

		printf("    "); for (int j = 0; j < 10; ++j) printf("| %-3d ", j);
		printf("\n----"); for (int j = 0; j < 10; ++j) printf("+-----");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-3d ", j);

			const int y = *ptr++;
			printf("| %-3d ", y);
		}

		putchar('\n');
	}
}

void print_pitch_tbl(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Pitch Table (+%d)\n", ofs);

	static const char* const hdr  = "      | 0     | 1     | 2     | 3     | 4     | 5     | 6     | 7     | 8     | 9     ";
	static const char* const line = "------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------";

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 10))
		{
			putchar('\n');

			if (!(i % 250))
			{
				if (i) puts(line);
				puts(hdr);
				puts(line);
			}

			printf("%-5d ", i);
		}

		// y = round(2^(i/1200 + 10) - 1024)
		const int y = (ptr[0] << 8) | ptr[1];
		printf("| %-5d ", y);

		ptr += 2;
	}

	putchar('\n');
}

void print_byte_tbl(const WDL_HeapBuf* const firmware, int ofs, const int num, const char* const title)
{
	printf("%s (+%d)\n\n", title, ofs);

	printf("    "); for (int i = 0; i < 10; ++i) printf("| %-3d ", i);
	printf("\n----"); for (int i = 0; i < 10; ++i) printf("+-----");

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 10)) printf("\n%-3d ", i);

		const int y = *ptr++;
		printf("| %-3d ", y);
	}

	putchar('\n');
}

// https://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c

void write_cue_points(WaveWriter* const wav, const int loop)
{
	static const int num = 1;
	int cue[3 + num * 6];

	cue[0] = WDL_bswap32_if_le('cue ');
	cue[1] = WDL_bswap32_if_be((1 + num * 6) * sizeof(int));
	cue[2] = WDL_bswap32_if_be(num);

	cue[3] = 0;
	cue[4] = 0;
	cue[5] = WDL_bswap32_if_le('data');
	cue[6] = 0;
	cue[7] = 0;
	cue[8] = WDL_bswap32_if_be(loop);

	wav->WriteChunk(cue, (3 + num * 6) * sizeof(int));
}

int write_sample(const char* const filename, const WDL_HeapBuf* const wavetbl, const int format, const int addr, int loop, const int end)
{
	const unsigned char* const buf = (const unsigned char*)wavetbl->Get() + addr;
	int len = end + 1;

	WaveWriter wav;
	if (!wav.Open(filename, 16, 1, 41964, 0)) return 0;

	switch (format)
	{
		// 12-bit signed linear PCM
		case 3:
		{
			const unsigned char* ptr = buf;

			for (int i = 0; i < len; ++i)
			{
				short sample;

				if (!(i & 1))
				{
					sample = (ptr[0] << 8) | (ptr[1] & 0xF0);
				}
				else
				{
					sample = ((ptr[1] & 0x0F) << 4) | (ptr[2] << 8);
					ptr += 3;
				}

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		default: assert(false);
	}

	wav.EndDataChunk();
	if (loop) write_cue_points(&wav, loop);
	wav.Close();

	return len;
}

int extract_sample(const char* const filename, const int ofs, const WDL_HeapBuf* const wavetbl, const unsigned char* const ptr)
{
	const int format = ptr[0] >> 6;
	const int addr = ((ptr[0] & 0x3F) << 16) | (ptr[1] << 8) | ptr[2];

	if (!addr) return 0;

	const int loop = (ptr[3] << 8) | ptr[4];
	const int end = ((ptr[5] << 8) | ptr[6]) ^ 0xFFFF;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 328;
	static unsigned char sample_list[max_samples][7];

	static int num_samples = 0;
	const unsigned char* const& hash = ptr;

	for (int i = 0; i < num_samples; ++i)
	{
		if (!memcmp(sample_list[i], hash, 7)) return 0;
	}

	assert(num_samples < max_samples);
	memcpy(sample_list[num_samples++], hash, 7);

	#endif

	char fn[128];
	sprintf(fn, filename, ofs);

	return write_sample(fn, wavetbl, format, addr, loop, end);
}

int extract_samples(const char* const filename, const WDL_HeapBuf* const wavetbl, const int ofs, const int num)
{
	const unsigned char* ptr = (const unsigned char*)wavetbl->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		n += extract_sample(filename, i * 12, wavetbl, ptr) > 0;
		ptr += 12;
	}

	return n;
}

int main(const int argc, const char* const* const argv)
{
	int opt = argc == 2 ? argv[1][0] : 0;
	if (opt == '-') opt = argv[1][1] | ('-' << 8);

	static const char* const roms[] =
	{
		// IC4 XK731C0 EPROM 1M v1.10
		"tg100/xk731c0.ic4", // SHA1(483103a2ffc63a90a2086c597baa2b2745c3a1c2)

		// IC6 XK992A0 ROM 16M
		"tg100/xk992a0.ic6" // SHA1(32ec77a46f4d005538c735f56ad48fa7243c63be)
	};

	WDL_HeapBuf firmware;

	if (!read_firmware(roms[0], &firmware, 128*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-m')
	{
		const int size = write_midi("midi/tg100_demo.mid", &firmware, +103838);
		const int n = size > 0;

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	if (opt == '-b')
	{
		FILE* const fp = fopen("table/tg100_font.txt", "wb");
		if (!fp) return EXIT_FAILURE;

		static const unsigned short bom = 0xFEFF;
		fwrite(&bom, 2, 1, fp);

		write_utf16_str("TG100 Characters\n\n", fp);

		int n = print_bitmaps(fp, &firmware, +92613, 8);
		n += print_bitmaps(fp, &firmware, +92677, 8);

		fclose(fp);
		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(roms[1], &wavetbl, 2*1024*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-t')
	{
		printf("TG100 Data Tables\n\n");

		print_drum_bank(&firmware, +85008); puts("\n--\n");
		print_drum_kits(&firmware, +101278, 10); puts("\n--\n");
		print_drum_voices(&firmware, +99634, 274); puts("\n--\n");

		print_bank_list(&firmware, +85136); puts("\n--\n");
		print_program_banks(&firmware, +65536, 4, "Program Banks"); putchar('\n');
		print_program_banks(&firmware, +85264, 1, "Internal Bank"); puts("\n--\n");
		print_normal_voices(&firmware, +66576, 192); puts("\n--\n");

		print_sample_sets(&firmware, +99354, 140); puts("\n--\n");
		print_samples(&firmware, +95906, 383); puts("\n--\n");
		print_wavetbl(&wavetbl, +0, 512); puts("\n--\n");

		print_velocity_curves(&firmware, +88076, 8); puts("\n--\n");
		print_pitch_tbl(&firmware, +85548, 1200); puts("\n--\n");
		print_byte_tbl(&firmware, +87948, 128, "Volume Table"); puts("\n--\n");
		print_byte_tbl(&firmware, +91148, 128, "Portamento Time Table"); puts("\n--\n");
		print_byte_tbl(&firmware, +91304, 64, "Pitch EG Rate Table");

		return EXIT_SUCCESS;
	}

	if (opt == '-w')
	{
		const int n = extract_samples("wave/tg100/sample_%04d.wav", &wavetbl, +0, 512);
		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -m | -b | -t | -w\n", argv[0]);
	return EXIT_FAILURE;
}
