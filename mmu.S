// inspired by https://developer.arm.com/documentation/dai0527/a/
// Note that EL0 writable *must not* be EL1 executable, split at 0x200000
// Learn the architecture - AArch64 memory attributes and properties, Page 26
.section ".text"
.global setup_mmu
setup_mmu:
	// bits 30-31 0b10  Granule size 4K for ttbr1_el1
	// bits 16-21   32  upper range size 4G
	// bits 14-15 0b00  Granule size 4K for ttbr0_el1
	// bits  0- 5   32  lower range size 4G
	ldr x0, =0x80200020
	msr tcr_el1, x0
	ldr x1, =0xFF00 // idx 1: normal, idx 0: device
	msr mair_el1, x1
	adr x2, ttb0_base
	msr ttbr0_el1, x2
	msr ttbr1_el1, x2

	// enable mmu & caches
	mrs x3, sctlr_el1
	orr x3, x3, 1     // mmu
	orr x3, x3, 1<<2  // data cache
	orr x3, x3, 1<<12 // instruction cache
	msr sctlr_el1, x3
	dsb sy
	isb
	ret

// Put a 64-bit value with little endianness
.macro PUT_64B high, low
.word \low
.word \high
.endm

// Create pointer to a next-level table: set bits 0,1 for valid & table
.macro TABLE_ENTRY PA, ATTR
PUT_64B \ATTR, (\PA) + 0x3
.endm

// Create an entry for a 1GB block: set bit 0 for valid
.macro BLOCK_1GB PA, ATTR_HI, ATTR_LO
PUT_64B \ATTR_HI, ((\PA) & 0xC0000000) | \ATTR_LO | 0x1
.endm

// Create an entry for a 2MB block: set bit 0 for valid
.macro BLOCK_2MB PA, ATTR_HI, ATTR_LO
PUT_64B \ATTR_HI, ((\PA) & 0xFFE00000) | \ATTR_LO | 0x1
.endm

.align 12 // 12 for 4KB granule.
ttb0_base:
	TABLE_ENTRY level2_pagetable, 0
	BLOCK_1GB 0x40000000, 0, 0x440  // device: accessed(10), permissions(7-6):01
	BLOCK_1GB 0x80000000, 0, 0x440  // device: accessed(10), permissions(7-6):01
	BLOCK_1GB 0xC0000000, 0, 0x440  // device: accessed(10), permissions(7-6):01

.align 12 // 12 for 4KB granule.
level2_pagetable:
.set ADDR,  0x0                  // start at 0
BLOCK_2MB (ADDR << 21), 0, 0x404 // 2MB kernel/cached: accessed(10), mair(4-2):001
.rept 0x1ff                      // add 511 blocks
.set ADDR, ADDR+1
BLOCK_2MB (ADDR << 21), 0, 0x444 // 2MB user/cached: accessed(10), permissions(7-6):01, mair(4-2):001
.endr
