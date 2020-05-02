// Uncomment if copying a .N64 file with a header, leave uncommented if just copying a binary
//#define STAGE3_INCLUDES_0x400_HEADER

#ifdef STAGE3_INCLUDES_0x400_HEADER
#define STAGE3_OFFSET 0
#else
#define STAGE3_OFFSET 0x400
#endif

// Uncomment to enable compression
#define LZSS

#ifdef LZSS
// From CUE (https://gbatemp.net/threads/nintendo-ds-gba-compressors.313278/)

#define LZS_SHIFT     1          // bits to shift
#define LZS_MASK      0x80       // bits to check:
#define LZS_THRESHOLD 2          // max number of bytes to not encode

unsigned int LZS_Decode(unsigned char *pak_buffer, unsigned int pak_len, unsigned char *raw_buffer) {
  //unsigned char *pak_buffer, *raw_buffer
  unsigned char *pak, *raw, *pak_end, *raw_end;
  //unsigned int   pak_len
  unsigned int raw_len, header, len, pos;
  unsigned char  flags, mask;

  //header = *pak_buffer;
  //if (header != CMD_CODE_10) {
  //  free(pak_buffer);
  //  BREAK(", WARNING: file is not LZSS encoded!\n");
  //}

  //raw_len = *(unsigned int *)pak_buffer >> 8;
  raw_len = (pak_buffer[1]) + (pak_buffer[2] << 8) + (pak_buffer[3] << 16);
  
  pak = pak_buffer + 4;
  raw = raw_buffer;
  pak_end = pak_buffer + pak_len;
  raw_end = raw_buffer + raw_len;

  mask = 0;

  while (raw < raw_end) {
    if (!(mask >>= LZS_SHIFT)) {
      if (pak == pak_end) break;
      flags = *pak++;
      mask = LZS_MASK;
    }

    if (!(flags & mask)) {
      if (pak == pak_end) break;
      *raw++ = *pak++;
    } else {
      if (pak + 1 >= pak_end) break;
      pos = *pak++;
      pos = (pos << 8) | *pak++;
      len = (pos >> 12) + LZS_THRESHOLD + 1;
      if (raw + len > raw_end) {
        //printf(", WARNING: wrong decoded length!");
        len = raw_end - raw;
      }
      pos = (pos & 0xFFF) + 1;
      while (len--) *raw++ = *(raw - pos);
    }
  }

  raw_len = raw - raw_buffer;

  //if (raw != raw_end) printf(", WARNING: unexpected end of encoded file!");

  //Save("saved.bin", raw_buffer, raw_len);

  //free(raw_buffer);
  //free(pak_buffer);

  //printf("\n");

  return raw_len;
}
#endif

#define getCharFromModem ((int (*)(void))0x8001dd48)
//#define readModemData ((unsigned int (*)(void *destination, unsigned int max))0x8001dfbc)
#define sendModem ((void *(*)(void *, unsigned int))0x8001df54)

// Download stage 3 to expansion RAM
#define stage3size (*(unsigned int *)(0x80400000 - 4))
#define downloadDest ((void *)0x80400000 - 4)
#define stage3 ((void *)0x80400000)

__attribute__((noreturn)) void _start(void) {
	unsigned int i;
	unsigned int downloaded = 0;

	stage3size = 0xffffffff;

	while(1) {
		while((i = getCharFromModem()) == 0xffffffff);
		((unsigned char *)downloadDest)[downloaded++] = (unsigned char)i;

		if(downloaded % 0x100 == 0) {
			char c = (char)i;
			sendModem(&c, 1);
		}

		if(downloaded == sizeof(unsigned int) + stage3size) break;
	}

	//char done[] = "done";
	//sendModem(done, 5);
	//while(1);

	// Disable interrupts
  __asm__ __volatile__(
    "mfc0 $t0, $12\n"
   "addiu $t1, $zero, 0xfffe\n"
  "and $t1, $t0, $t1\n"
  "mtc0 $t1, $12\n"
  ::: "t0", "t1"
  );

	void *addr = (void *)0x80000000 + STAGE3_OFFSET;
	
	int length = 
	#ifndef LZSS
	stage3size;
	#else
	LZS_Decode((void *)(0x80400000), stage3size, addr);
  #endif

  for (; length > 0; length -= 4, addr += 4) {
  	#ifndef LZSS
  	*(unsigned int *)addr = *((unsigned int *)(addr + 0x400000 - STAGE3_OFFSET));
  	#endif

  	asm ("\tcache %0, (%1)\n"::"i" (0x15), "r" (addr)); // Hit Writeback Invalidate (primary data cache)
  	asm ("\tcache %0, (%1)\n"::"i" (0x10), "r" (addr)); // Hit Invalidate (primary instruction cache)
  }

	(((__attribute__((noreturn)) void (*)(void))0x80000400)());
}
