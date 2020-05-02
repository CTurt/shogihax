#define _piDmaStatus (read_volatile_4(0xa4600010))

#define waitForPi() do {} while ((_piDmaStatus & 3) != 0)

#define read_volatile_4(a) (*(volatile unsigned int *)a)
#define write_volatile_4(a, v) (*(volatile unsigned int *)a = v)

unsigned char modemData[512];
unsigned int offsetToWriteModemData = 0;
unsigned int bytesRecv = 0;
unsigned int modemReadOffset = 0;

unsigned int toSend = 0;
unsigned char sendBuffer[512];
unsigned int sendOffset = 0;

unsigned int sentBytes = 0;

// 0 or 0x20
unsigned int lengthToSendToCartOrFlags = 0;

int interruptExecuted = 0;

unsigned int piTransferCount = 0;

// decompilation of 0x8001e158
void modemInterrupt(void) {
  unsigned char *pbVar1;
  unsigned int uVar2;
  unsigned int uVar3;
  unsigned int uVar4;
  unsigned int _MODEM_STATUS;
  
    waitForPi();
    uVar2 = read_volatile_4(0xaffd0000);
    if ((uVar2 & 0x80) == 0) {
      //lengthBeforeModemData = lengthBeforeModemData + 1;
      //waitForPi();
      //  write_volatile_4(0xaffd0000,0x500070);
      
      //(*(code *)PTR_handleModemStatus0_800a7404)();
    }
    if ((uVar2 & 0x40) == 0) {
      waitForPi();
        uVar2 = read_volatile_4(0xaffd8044);
      
      _MODEM_STATUS = uVar2 & 4;
      if ((uVar2 & 0x80) != 0) {
        waitForPi();
          uVar3 = read_volatile_4(0xaffd8050);
        
        //DAT_8003a2fc = (uint)((uVar3 & 0x80) != 0);
        //DAT_8003a300 = (uint)((uVar3 & 0x40) != 0);
        //DAT_8003a304 = (uint)((uVar3 & 0x20) != 0);
        //if ((uVar2 & 0x10) != 0) {
        //  DAT_800a741c = DAT_800a741c + 1;
        //}
        waitForPi();
        uVar4 = read_volatile_4(0xaffd8040);
        if (_MODEM_STATUS == 0) {
          //DAT_8003a2f4 = DAT_800a7420 + 1;
          if (bytesRecv < 0x200) {
            bytesRecv = bytesRecv + 1;
            //DAT_800a7420 = DAT_8003a2f4;
            modemData[offsetToWriteModemData] = (char)uVar4;
            offsetToWriteModemData = offsetToWriteModemData + 1;
            if (offsetToWriteModemData == 0x200) {
              offsetToWriteModemData = 0;
            }
          }
          else {
            //DAT_800a741c = DAT_800a741c + 1;
            //DAT_800a7420 = DAT_8003a2f4;
          }
        }
        if (_MODEM_STATUS != 0) {
          //DAT_8003ec18 = DAT_8003ec18 + 1;
          waitForPi();
            write_volatile_4(0xaffd8044,0xc0);

        }
        //DAT_8003a308 = DAT_8003ec18;
        if (toSend != 0) {
          waitForPi();
            write_volatile_4(0xaffd8064,0xa0);
          
        }
        waitForPi();
        write_volatile_4(0xaffd0000,lengthToSendToCartOrFlags * 0x10001 | 0x400050);
        if (toSend != 0) {
          waitForPi();
            write_volatile_4(0xaffd8064,0xe0);
          
        }
      }
      if (toSend != 0) {
        waitForPi();
          uVar2 = read_volatile_4(0xaffd8064);
 
        if ((uVar2 & 0xc0) == 0xc0) {
          waitForPi();
          pbVar1 = sendBuffer + sendOffset;
          sendOffset = sendOffset + 1;
          write_volatile_4(0xaffd8060,(unsigned int)*pbVar1);
          sentBytes = sentBytes + 1;
          if (sendOffset == 0x200) {
            sendOffset = 0;
          }
          toSend = toSend + -1;
          waitForPi();
            write_volatile_4(0xaffd8064,0xa0);

		   waitForPi();
            write_volatile_4(0xaffd8044,0x80);
          
          waitForPi();
          write_volatile_4(0xaffd0000,lengthToSendToCartOrFlags * 0x10001 | 0x400050);

          waitForPi();
            write_volatile_4(0xaffd8044,0xc0);

          //DAT_8003a2f8 = sentBytes;
          if (toSend != 0) {
            waitForPi();
              write_volatile_4(0xaffd8064,0xe0);
              //DAT_8003a2f8 = sentBytes;
          }
        }
      }
    }
    waitForPi();
}

unsigned long long disableInterrupts(void);  
  __asm__ (
    ".global disableInterrupts\n"
    "disableInterrupts:\n"
    "mfc0 $t0, $12\n"
   "addiu $t1, $zero, 0xfffe\n"
  "and $t1, $t0, $t1\n"
  "mtc0 $t1, $12\n"
  "or $v0, $t0, 1\n"
//  ::: "t0", "t1", "v0"
);

void restoreInterrupts(unsigned long long);
__asm__(
  ".global restoreInterrupts\n"
  "restoreInterrupts:\n"
  "mfc0 $t0, $12\n"
  "or $t1, $t0, $a0\n"
  "mtc0 $t1, $12\n"
//  ::: "t0", "t1"
);

/*
#define blockingGetCharFromModem(d) do {\
  int r;\
  while((r = getCharFromModem()) == -1); modemInterrupt();\
  *(d) = r;\
} while(0)
*/

int getCharFromModem(void) {
  unsigned char *pbVar1;
  unsigned long long uVar2;
  int ret;
  
  ret = -1;
  if (bytesRecv != 0) {
    //uVar2 = disableInterrupts();
    bytesRecv = bytesRecv + -1;
    pbVar1 = (unsigned char *)(modemData + modemReadOffset);
    modemReadOffset = modemReadOffset + 1;
    ret = *pbVar1;
    //restoreInterrupts(uVar2);
    if (modemReadOffset == 0x200) {
      modemReadOffset = 0;
    }
  }

  return ret;
}

int sendCharToModem(char uParm1) {
  //undefined8 interrupts;
  int iVar1;
  unsigned int uVar2;
  int iVar3;
  
  iVar1 = (int)uParm1;
  //interrupts = disableInterrupts();
  if (toSend == 0x200) {
    //restoreInterrupts(interrupts);
    iVar1 = -1;
  }
  else {
    iVar3 = piTransferCount + 1;
    sendBuffer[piTransferCount] = (unsigned char)iVar1;
    piTransferCount = iVar3;
    if (iVar3 == 0x200) {
      piTransferCount = 0;
    }
    if (toSend == 0) {
      uVar2 = read_volatile_4(0xa4600010);
      if ((uVar2 & 3) == 0) {
        write_volatile_4(0xaffd8064,0xe0);
      }
      else {
        uVar2 = read_volatile_4(0xa4600010);
        while ((uVar2 & 3) != 0) {
          uVar2 = read_volatile_4(0xa4600010);
        }
        write_volatile_4(0xaffd8064,0xe0);
      }
    }
    toSend = toSend + 1;
    //restoreInterrupts(interrupts);
  }
  return iVar1;
}

unsigned char __attribute__ ((aligned (16))) __attribute__((used)) interruptStack[65 * 1024];

// Save registers first thing, so we can just jump into C as soon as possible...
void interruptEntry(void);
/*asm("\
.global interruptEntry\n\
interruptEntry:\n\
.set noat\n\
la $k0, (interruptStack + 65 * 1024 - 0xd8)\n\
sd $v0, 0x0($k0)\n\
sd $v1, 0x8($k0)\n\
sd $a0, 0x10($k0)\n\
sd $a1, 0x18($k0)\n\
sd $a2, 0x20($k0)\n\
sd $a3, 0x28($k0)\n\
sd $t0, 0x30($k0)\n\
sd $t1, 0x38($k0)\n\
sd $t2, 0x40($k0)\n\
sd $t3, 0x48($k0)\n\
sd $t4, 0x50($k0)\n\
sd $t5, 0x58($k0)\n\
sd $t6, 0x60($k0)\n\
sd $t7, 0x68($k0)\n\
sd $t8, 0x70($k0)\n\
sd $t9, 0x78($k0)\n\
sd $s0, 0x80($k0)\n\
sd $s1, 0x88($k0)\n\
sd $s2, 0x90($k0)\n\
sd $s3, 0x98($k0)\n\
sd $s4, 0xa0($k0)\n\
sd $s5, 0xa8($k0)\n\
sd $s6, 0xb0($k0)\n\
sd $s7, 0xb8($k0)\n\
sd $ra, 0xc0($k0)\n\
sd $sp, 0xc8($k0)\n\
sd $at, 0xd0($k0)\n\
\n\
move $sp, $k0\n\
jal interruptHandler\n\
nop\
\n\
ld $v0, 0x0($k0)\n\
ld $v1, 0x8($k0)\n\
ld $a0, 0x10($k0)\n\
ld $a1, 0x18($k0)\n\
ld $a2, 0x20($k0)\n\
ld $a3, 0x28($k0)\n\
ld $t0, 0x30($k0)\n\
ld $t1, 0x38($k0)\n\
ld $t2, 0x40($k0)\n\
ld $t3, 0x48($k0)\n\
ld $t4, 0x50($k0)\n\
ld $t5, 0x58($k0)\n\
ld $t6, 0x60($k0)\n\
ld $t7, 0x68($k0)\n\
ld $t8, 0x70($k0)\n\
ld $t9, 0x78($k0)\n\
ld $s0, 0x80($k0)\n\
ld $s1, 0x88($k0)\n\
ld $s2, 0x90($k0)\n\
ld $s3, 0x98($k0)\n\
ld $s4, 0xa0($k0)\n\
ld $s5, 0xa8($k0)\n\
ld $s6, 0xb0($k0)\n\
ld $s7, 0xb8($k0)\n\
ld $ra, 0xc0($k0)\n\
ld $sp, 0xc8($k0)\n\
ld $at, 0xd0($k0)\n\
\n\
eret\n\
.set at\n\
");
*/

asm("\
.global interruptEntry\n\
interruptEntry:\n\
.set noat\n\
\n\
eret\n\
.set at\n\
");

__attribute__((used)) void interruptHandler(void) {
  //interruptExecuted = 1;

	asm volatile("mfc0 $k1, $13\n");
	register unsigned int cause asm("$k1");

	unsigned int i = (cause & 0xff00) >> 0xc;
	if(i == 0) ((cause & 0xff00) >> 0x8) + 0x10;

	if(i == 17) {
    interruptExecuted++;
		modemInterrupt();
	}
}

void enableModem(void) {
  /*
	*(volatile unsigned int *)0x80000000 = 0x3c1a0000 | ((((unsigned int)interruptEntry) & 0xffff0000) >> 16); // lui $k0, hi(interruptEntry)
	*(volatile unsigned int *)0x80000004 = 0x275a0000 | (((unsigned int)interruptEntry) & 0xffff); // addiu $k0, $k0, lo(interruptEntry)
	*(volatile unsigned int *)0x80000008 = 0x03400008; // jr $k0
	*(volatile unsigned int *)0x8000000c = 0x00000000; // nop

	*(volatile unsigned int *)0x80000080 = 0x3c1a0000 | ((((unsigned int)interruptEntry) & 0xffff0000) >> 16); // lui $k0, hi(interruptEntry)
	*(volatile unsigned int *)0x80000084 = 0x275a0000 | (((unsigned int)interruptEntry) & 0xffff); // addiu $k0, $k0, lo(interruptEntry)
	*(volatile unsigned int *)0x80000088 = 0x03400008; // jr $k0
	*(volatile unsigned int *)0x8000008c = 0x00000000; // nop

	*(volatile unsigned int *)0x80000100 = 0x3c1a0000 | ((((unsigned int)interruptEntry) & 0xffff0000) >> 16); // lui $k0, hi(interruptEntry)
	*(volatile unsigned int *)0x80000104 = 0x275a0000 | (((unsigned int)interruptEntry) & 0xffff); // addiu $k0, $k0, lo(interruptEntry)
	*(volatile unsigned int *)0x80000108 = 0x03400008; // jr $k0
	*(volatile unsigned int *)0x8000010c = 0x00000000; // nop
  */

	// Install interrupt hook
	*(volatile unsigned int *)0x80000180 = 0x3c1a0000 | ((((unsigned int)interruptEntry) & 0xffff0000) >> 16); // lui $k0, hi(interruptEntry)
	*(volatile unsigned int *)0x80000184 = 0x275a0000 | (((unsigned int)interruptEntry) & 0xffff); // addiu $k0, $k0, lo(interruptEntry)
	*(volatile unsigned int *)0x80000188 = 0x03400008; // jr $k0
	*(volatile unsigned int *)0x8000018c = 0x00000000; // nop

  //*(volatile unsigned int *)0x80000180 = 0x42000018; // eret

	void *addr;
	for(addr = (void *)0x80000180; addr < (void *)0x8000018c; addr += 4) {
	//for(addr = (void *)0x80000000; addr <= (void *)0x8000018c; addr += 4) {
		asm ("\tcache %0, (%1)\n"::"i" (0x15), "r" (addr)); // Hit Writeback Invalidate (primary data cache)
  		asm ("\tcache %0, (%1)\n"::"i" (0x10), "r" (addr)); // Hit Invalidate (primary instruction cache)
  	}


   // interrupts are hard - just call the function directly lol
	//restoreInterrupts(0x003fff01);
  //restoreInterrupts(0x2000FF01);

// zero the variables because loader doesn't zero memory
 offsetToWriteModemData = 0;
bytesRecv = 0;
modemReadOffset = 0;

toSend = 0;
sendOffset = 0;

sentBytes = 0;

//lengthToSendToCartOrFlags = 0;
lengthToSendToCartOrFlags = 0x20;

  interruptExecuted = 0;

  piTransferCount = 0;
}

char lastChar;
int charCount;

// Unstable for some reason, so just get the same char 2 times to be sure it's intended
// This is super hacky - and should be fixed by only polling for modem data on a cartridge interrupt so we don't get garbage in the first place
int stablizeGetCharFromModem(void) {
  int r;
  if((r = getCharFromModem()) != -1) {
    if(r == lastChar) {
      charCount++;
      if(charCount == 2) {
        charCount = 0;
        return r;
      }
    }
    else {
      charCount = 1;
      lastChar = r;
    }
  }

  return -1;
}

void initStablization(void) {
  lastChar = 0;
  charCount = -1;
}
