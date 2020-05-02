# Force as start of binary
.section .text.startup

.set noreorder

.global _start
.extern main

_start:
	# Set stack to end of the memory
	la $sp, 0x80400000

	la $v0, main
	jalr $v0
	nop
