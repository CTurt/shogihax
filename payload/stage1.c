#define stage2 ((__attribute__((noreturn)) void (*)(void))0x801bd91c)

__attribute__((noreturn)) void _start(void) {
	void *addr = stage2;
	unsigned int length = 0x1000;

    for (;length>0;length-=4,addr+=4) {\
    	asm ("\tcache %0, (%1)\n"::"i" (0x15), "r" (addr)); // Hit Writeback Invalidate (primary data cache)
    	asm ("\tcache %0, (%1)\n"::"i" (0x10), "r" (addr)); // Hit Invalidate (primary instruction cache)
    }

	stage2();
}
