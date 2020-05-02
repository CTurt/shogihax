// Stripped down from: https://github.com/DragonMinded/libdragon/commit/2a6026ded7d2fe6570f8c281954ac2ad2423c7ee

//#include <stddef.h>

typedef unsigned long size_t;

#define UncachedAddr(_addr) ((void *)(((unsigned long)(_addr))|0x20000000))
#define MEMORY_BARRIER() asm volatile ("" : : : "memory")

#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
#define SI_STATUS_IO_BUSY   ( 1 << 1 )

#define TPAK_POWER_ON 0x84
#define TPAK_POWER_OFF 0xFE
#define TPAK_POWER_ADDRESS 0x8000
#define TPAK_BANK_ADDRESS 0xA000
#define TPAK_STATUS_ADDRESS 0xB000
#define TPAK_STATUS_CARTRIDGE_ABSENT 0x40 // bit 6

#define TPAK_DATA_ADDRESS 0xC000

#define BLOCK_SIZE 0x20
#define BANK_SIZE 0x4000

typedef unsigned char bool;
#define true 1
#define false 0

#define STATIC_ASSERT(COND, MSG) typedef char static_assertion_##MSG[(COND) ? 1 : -1]

typedef unsigned char uint8_t; STATIC_ASSERT(sizeof(uint8_t) == 1, s1);
typedef unsigned short uint16_t; STATIC_ASSERT(sizeof(uint16_t) == 2, s2);
typedef unsigned int uint32_t; STATIC_ASSERT(sizeof(uint32_t) == 4, s4);
typedef unsigned long long uint64_t; STATIC_ASSERT(sizeof(uint64_t) == 8, s8);

struct gameboy_cartridge_header {
    uint8_t entry_point[4];
    uint8_t logo[48];
    char title[16];
    uint16_t new_licensee_code;
    bool is_sgb_supported;
    uint8_t cartridge_type;
    uint8_t rom_size_code;
    uint8_t ram_size_code;
    uint8_t destination_code;
    uint8_t old_licensee_code;
    uint8_t version_number;
    uint8_t header_checksum;
    uint16_t global_checksum;
    uint8_t overflow[16];
} _gameboy_cartridge_header;

typedef struct SI_regs_s {
    /** @brief Uncached address in RAM where data should be found */
    volatile void * DRAM_addr;
    /** @brief Address to read when copying from PIF RAM */
    volatile void * PIF_addr_read;
    /** @brief Reserved word */
    uint32_t reserved1;
    /** @brief Reserved word */
    uint32_t reserved2;
    /** @brief Address to write when copying to PIF RAM */
    volatile void * PIF_addr_write;
    /** @brief Reserved word */
    uint32_t reserved3;
    /** @brief SI status, including DMA busy and IO busy */
    uint32_t status;
} SI_regs_t;

static volatile struct SI_regs_s *const SI_regs = (struct SI_regs_s *)0xa4800000;

#define cache_op(op) \
    addr=(void*)(((unsigned long)addr)&(~3));\
    for (;length>0;length-=4,addr+=4) \
	asm ("\tcache %0,(%1)\n"::"i" (op), "r" (addr))

void data_cache_hit_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x15);
}

/*
void *memset(void *ptr, int value, size_t num) {
	char *p = ptr;
	while(p < ptr + num) *(p++) = value;
	return ptr;
}
*/

// https://xs-labs.com/en/blog/2013/08/06/optimising-memset/

void *memset(void * p, int c, size_t n) {
    uint8_t  * sp;
    uint64_t * lp;
    uint64_t   u64;
    uint8_t    u8;
    
    u8  = ( uint8_t )c;
    u64 = ( uint64_t )c;
    u64 = ( u64 << 32 ) | u64;
    sp  = ( uint8_t * )p;
    
    while( n-- && ( ( ( uint32_t )sp & ( uint32_t )-8 ) < ( uint32_t )sp ) )
    {
        *( sp++ ) = u8;
    }
    
    lp  = ( uint32_t * )( ( void * )sp );
    
    while( ( n / 8 ) > 0 )
    {
        *( lp++ ) = u64;
        n        -= 8;
    }
    
    sp = ( uint8_t * )( ( void * )lp );
    
    while( n-- )
    {
        *( sp++ ) = u8;
    }
    
    return p;
}

void *memcpy(void *dest, void *src, size_t n) { 
   int i;
   for (i = 0; i < n; i++) ((char *)dest)[i] = ((char *)src)[i];
   return dest;
} 

static uint16_t __calc_address_crc( uint16_t address) {
    // CRC table
    uint16_t xor_table[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x15, 0x1F, 0x0B, 0x16, 0x19, 0x07, 0x0E, 0x1C, 0x0D, 0x1A, 0x01 };
    uint16_t crc = 0;

    // Make sure we have a valid address
    address &= ~0x1F;

    // Go through each bit in the address, and if set, xor the right value into the output
    for(int i = 15; i >= 5; i--) {
        // Is this bit set?
        if(((address >> i) & 0x1)) {
           crc ^= xor_table[i];
        }
    }

    // Just in case
    crc &= 0x1F;

    // Create a new address with the CRC appended
    return address | crc;
}

static void __SI_DMA_wait(void) {
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY));
}

static void __controller_exec_PIF( const void *inblock, void *outblock )
{
    volatile uint64_t inblock_temp[8];
    volatile uint64_t outblock_temp[8];

    data_cache_hit_writeback_invalidate(inblock_temp, 64);
    memcpy(UncachedAddr(inblock_temp), inblock, 64);

    /* Be sure another thread doesn't get into a resource fight */
    //disable_interrupts();

    __SI_DMA_wait();

    SI_regs->DRAM_addr = inblock_temp; // only cares about 23:0
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM; // is it really ever anything else?
    MEMORY_BARRIER();

    __SI_DMA_wait();

    data_cache_hit_writeback_invalidate(outblock_temp, 64);

    SI_regs->DRAM_addr = outblock_temp;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    MEMORY_BARRIER();

    __SI_DMA_wait();

    /* Now that we've copied, its safe to let other threads go */
    //enable_interrupts();

    memcpy(outblock, UncachedAddr(outblock_temp), 64);
}

static uint8_t __calc_data_crc( uint8_t *data )
{
    uint8_t ret = 0;

    for( int i = 0; i <= 32; i++ )
    {
        for( int j = 7; j >= 0; j-- )
        {
            int tmp = 0;

            if( ret & 0x80 )
            {
                tmp = 0x85;
            }

            ret <<= 1;

            if( i < 32 )
            {
                if( data[i] & (0x01 << j) )
                {
                    ret |= 0x1;
                }
            }

            ret ^= tmp;
        }
    }

    return ret;
}

int read_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    uint8_t output[64];
    uint8_t SI_read_mempak_block[64];
    int ret;

    /* Controller must be in range */
    if( controller < 0 || controller > 3 ) { return -1; }

    /* Last byte must be 0x01 to signal to the SI to process data */
    memset( SI_read_mempak_block, 0, 64 );
    SI_read_mempak_block[56] = 0xfe;
    SI_read_mempak_block[63] = 0x01;

    /* Start command at the correct channel to read from the right mempak */
    SI_read_mempak_block[controller]     = 0x03;
    SI_read_mempak_block[controller + 1] = 0x21;
    SI_read_mempak_block[controller + 2] = 0x02;

    /* Calculate CRC on address */
    uint16_t read_address = __calc_address_crc( address );
    SI_read_mempak_block[controller + 3] = (read_address >> 8) & 0xFF;
    SI_read_mempak_block[controller + 4] = read_address & 0xFF;

    /* Leave room for 33 bytes (32 bytes + CRC) to come back */
    memset( &SI_read_mempak_block[controller + 5], 0xFF, 33 );

    __controller_exec_PIF(SI_read_mempak_block,&output);

    /* Copy data correctly out of command */
    memcpy( data, &output[controller + 5], 32 );

    /* Validate CRC */
    uint8_t crc = __calc_data_crc( &output[controller + 5] );

    if( crc == output[controller + 5 + 32] )
    {
        /* Data was read successfully */
        ret = 0;
    }
    else
    {
        if( crc == (output[controller + 5 + 32] ^ 0xFF) )
        {
            /* Pak not present! */
            ret = 2;
        }
        else
        {
            /* Pak returned bad data */
            ret = 3;
        }
    }

    return ret;
}

int write_mempak_address( int controller, uint16_t address, uint8_t *data )
{
    uint8_t output[64];
    uint8_t SI_write_mempak_block[64];
    int ret;

    /* Controller must be in range */
    if( controller < 0 || controller > 3 ) { return -1; }

    /* Last byte must be 0x01 to signal to the SI to process data */
    memset( SI_write_mempak_block, 0, 64 );
    SI_write_mempak_block[56] = 0xfe;
    SI_write_mempak_block[63] = 0x01;

    /* Start command at the correct channel to write from the right mempak */
    SI_write_mempak_block[controller]     = 0x23;
    SI_write_mempak_block[controller + 1] = 0x01;
    SI_write_mempak_block[controller + 2] = 0x03;

    /* Calculate CRC on address */
    uint16_t write_address = __calc_address_crc( address );
    SI_write_mempak_block[controller + 3] = (write_address >> 8) & 0xFF;
    SI_write_mempak_block[controller + 4] = write_address & 0xFF;

    /* Place the data to be written */
    memcpy( &SI_write_mempak_block[controller + 5], data, 32 );

    /* Leave room for CRC to come back */
    SI_write_mempak_block[controller + 5 + 32] = 0xFF;

    __controller_exec_PIF(SI_write_mempak_block,&output);

    /* Calculate CRC on output */
    uint8_t crc = __calc_data_crc( &output[controller + 5] );

    if( crc == output[controller + 5 + 32] )
    {
        /* Data was written successfully */
        ret = 0;
    }
    else
    {
        if( crc == (output[controller + 5 + 32] ^ 0xFF) )
        {
            /* Pak not present! */
            ret = 2;
        }
        else
        {
            /* Pak returned bad data */
            ret = 3;
        }
    }

    return ret;
}

int tpak_set_value(int controller, uint16_t address, uint8_t value) {
    uint8_t block[BLOCK_SIZE];
    memset(block, value, BLOCK_SIZE);
    return write_mempak_address(controller, address, block);
}

int tpak_set_power(int controller, bool power_state) {
    uint8_t value = power_state ? TPAK_POWER_ON : TPAK_POWER_OFF;
    return tpak_set_value(controller, TPAK_POWER_ADDRESS, value);
}

int tpak_set_access(int controller, bool access_state) {
    uint8_t value = access_state ? 1 : 0;
    return tpak_set_value(controller, TPAK_STATUS_ADDRESS, value);
}

int tpak_set_bank(int controller, int bank) {
    return tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);
}

uint8_t tpak_get_status(int controller)
{
    uint8_t block[BLOCK_SIZE];
    read_mempak_address(controller, TPAK_STATUS_ADDRESS, block);

    return block[0];
}

static bool __is_transfer_pak( int controller )
{
    uint8_t data[32];
    memset( data, 0x84, 32 );
    write_mempak_address(controller, 0x8000, data);
    read_mempak_address(controller, 0x8000, data);

    bool result = (data[0] == 0x84);

    memset( data, 0xFE, 32 );
    write_mempak_address(controller, 0x8000, data);
    read_mempak_address(controller, 0x8000, data);

    return result & (data[0] == 0x00);
}

static int __is_valid_accessory( uint32_t data )
{
    if( ((data >> 8) & 0xFFFF) == 0x0001 )
    {
        /* This is a rumble pak, mem pak or transfer pak */
        return 1;
    }
    else if( ((data >> 8) & 0xFFFF) == 0x0100 )
    {
        /* This is a VRU */
        return 1;
    }

    return 0;
}


/** @brief SI Controller Data */
typedef struct SI_condat
{
    /** @brief Unused padding bits */
    unsigned : 16;
    /** @brief Status of the last command */
    unsigned err : 2;
    /** @brief Unused padding bits */
    unsigned : 14;

    union
    {
        struct
        {
            /** @brief 32-bit data sent to or returned from SI */
            unsigned int data : 32;
        };
        struct
        {
            /** @brief State of the A button */
            unsigned A : 1;
            /** @brief State of the B button */
            unsigned B : 1;
            /** @brief State of the Z button */
            unsigned Z : 1;
            /** @brief State of the start button */
            unsigned start : 1;
            /** @brief State of the up button */
            unsigned up : 1;
            /** @brief State of the down button */
            unsigned down : 1;
            /** @brief State of the left button */
            unsigned left : 1;
            /** @brief State of the right button */
            unsigned right : 1;
            /** @brief Unused padding bits */
            unsigned : 2;
            /** @brief State of the L button */
            unsigned L : 1;
            /** @brief State of the R button */
            unsigned R : 1;
            /** @brief State of the C up button */
            unsigned C_up : 1;
            /** @brief State of the C down button */
            unsigned C_down : 1;
            /** @brief State of the C left button */
            unsigned C_left : 1;
            /** @brief State of the C right button */
            unsigned C_right : 1;
            /** @brief State of the X button */
            signed x : 8;
            /** @brief State of the Y button */
            signed y : 8;
        };
    };
} _SI_condat;

/** @brief SI Controller Data, GC */
typedef struct SI_condat_gc
{
    union
    {
        struct
        {
            /** @brief 64-bit data sent to or returned from SI */
            uint64_t data;
        };
        struct
        {
            unsigned err : 2;
            unsigned origin_unchecked : 1;
            unsigned start : 1;
            unsigned y : 1;
            unsigned x : 1;
            unsigned b : 1;
            unsigned a : 1;
            unsigned unused2 : 1;
            unsigned l : 1;
            unsigned r : 1;
            unsigned z : 1;
            unsigned up : 1;
            unsigned down : 1;
            unsigned right : 1;
            unsigned left : 1;
            unsigned stick_x : 8;
            unsigned stick_y : 8;

            unsigned cstick_x : 8;
            unsigned cstick_y : 8;
            unsigned analog_l : 8;
            unsigned analog_r : 8;
        };
    };
} _SI_condat_gc;

struct SI_origdat_gc {
    struct SI_condat_gc data;
    uint8_t deadzone0;
    uint8_t deadzone1;
};

typedef struct controller_data
{
    /** @brief Controller Data */
    struct SI_condat c[4];
    /** @brief Padding or GC data to allow mapping directly to a PIF block */
    struct SI_condat_gc gc[4];
} _controller_data;


#define ACCESSORY_NONE          0
/** @brief Mempak present */
#define ACCESSORY_MEMPAK        1
/** @brief Rumblepak present */
#define ACCESSORY_RUMBLEPAK     2
/** @brief VRU present */
#define ACCESSORY_VRU           3
/** @brief Transferpak present */
#define ACCESSORY_TRANSFERPAK    4

static void __get_accessories_present( struct controller_data *output )
{
    static const unsigned long long SI_read_status_block[8] =
    {
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xff010300ffffffff,
        0xfe00000000000000,
        0,
        0,
        1
    };

    __controller_exec_PIF(SI_read_status_block,output);
}

int identify_accessory( int controller )
{
    uint8_t data[32];
    struct controller_data output;

    /* Grab the actual accessory data */
    __get_accessories_present( &output );

    if( __is_valid_accessory( output.c[controller].data ) )
    {
        switch( ( output.c[controller].data >> 8 ) & 0xFFFF )
        {
            case 0x0001: /* Mempak/rumblepak/transferpak */
            {
                /* Init string one */
                memset( data, 0xfe, 32 );
                write_mempak_address( controller, 0x8000, data );

                /* Init string two */
                memset( data, 0x80, 32 );
                write_mempak_address( controller, 0x8000, data );

                /* Get register contents */
                if( read_mempak_address( controller, 0x8000, data ) == 0 )
                {
                    /* Should really check all bytes, but this should suffice */
                    if( data[0] == 0x80 )
                    {
                        return ACCESSORY_RUMBLEPAK;
                    }
                    else
                    {
                        return __is_transfer_pak( controller ) ? ACCESSORY_TRANSFERPAK : ACCESSORY_MEMPAK;
                    }
                }

                /* For good measure */
                break;
            }
            case 0x0100: /* VRU! */
            {
                return ACCESSORY_VRU;
            }
        }
    }

    /* Couldn't identify */
    return ACCESSORY_NONE;
}

#define TPAK_ERROR_INVALID_ARGUMENT 1
#define TPAK_ERROR_NO_TPAK  2
#define TPAK_ERROR_NO_CONTROLLER 3
#define TPAK_ERROR_UNKNOWN_BEHAVIOUR 4
#define TPAK_ERROR_NO_CARTRIDGE 5
#define TPAK_ERROR_ADDRESS_OVERFLOW 6

#define TPAK_STATUS_READY           0x01
#define TPAK_STATUS_WAS_RESET       0x04
#define TPAK_STATUS_IS_RESETTING    0x08
#define TPAK_STATUS_REMOVED         0x40
#define TPAK_STATUS_POWERED         0x80

int tpak_init(int controller)
{
    int accessory = identify_accessory(controller);

    if (accessory != ACCESSORY_TRANSFERPAK)
    {
	//if(!__is_transfer_pak(controller))
        return TPAK_ERROR_NO_TPAK;
    }

    int result = tpak_set_power(controller, true);
    if (result)
    {
        return result;
    }

    tpak_set_access(controller, true);

    uint8_t status = tpak_get_status(controller);
    if (status & TPAK_STATUS_CARTRIDGE_ABSENT)
    {
        return TPAK_ERROR_NO_CARTRIDGE;
    }

    if (!(status & TPAK_STATUS_READY))
    {
        return TPAK_ERROR_UNKNOWN_BEHAVIOUR;
    }

    return 0;
}

int tpak_read(int controller, uint16_t address, uint8_t* buffer, uint16_t size) {
    //if (controller < 0 || controller > 3 || size % BLOCK_SIZE || address % BLOCK_SIZE) {
    //    return TPAK_ERROR_INVALID_ARGUMENT;
    //}

    uint16_t adjusted_address = (address % BANK_SIZE) + TPAK_DATA_ADDRESS;
    uint16_t end_address = address + size;
    //if (end_address < address) {
    //    return TPAK_ERROR_ADDRESS_OVERFLOW;
    //}

    uint8_t *cursor = buffer;

    int bank = address / BANK_SIZE;
    tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);

    while(address < end_address) {
        // Check if we need to change the bank.
        if (address / BANK_SIZE > bank) {
            bank = address / BANK_SIZE;
            tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);
            adjusted_address = TPAK_DATA_ADDRESS;
        }

        int r = read_mempak_address(controller, adjusted_address, cursor);
        if(r) return r;
        
        address += BLOCK_SIZE;
        cursor += BLOCK_SIZE;
        adjusted_address += BLOCK_SIZE;
    }

    return 0;
}

int tpak_get_cartridge_header(int controller, struct gameboy_cartridge_header *header) {
    // We're interested in 0x0000 - 0x3FFF of gb space.
    tpak_set_bank(controller, 0);
    // Header starts at 0x0100
    const uint16_t address = 0x0100;

    return tpak_read(controller, address, (uint8_t *)header, sizeof(*header));
}
