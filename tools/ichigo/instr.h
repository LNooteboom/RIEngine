#ifndef ICHIGO_INSTR_H
#define ICHIGO_INSTR_H

#include <stdint.h>

#define INSTR_BASE	0xFF00
#define INSTR_NOP	(INSTR_BASE + 0x00)

#define INSTR_MOVI	(INSTR_BASE + 0x02)
#define INSTR_MOVF	(INSTR_BASE + 0x03)
#define INSTR_MOVSTR (INSTR_BASE + 0x04)
#define INSTR_MOVENT (INSTR_BASE + 0x05)

#define INSTR_CALL	(INSTR_BASE + 0x06)
#define INSTR_RET	(INSTR_BASE + 0x07)
#define INSTR_JMP	(INSTR_BASE + 0x08)
#define INSTR_JZ	(INSTR_BASE + 0x09)
#define INSTR_JNZ	(INSTR_BASE + 0x0A)
#define INSTR_SWITCH (INSTR_BASE + 0x0B)
#define INSTR_CALLA	(INSTR_BASE + 0x0C)
#define INSTR_KILL	(INSTR_BASE + 0x0D)
#define INSTR_KILLALL (INSTR_BASE + 0x0E)

#define INSTR_WAIT	(INSTR_BASE + 0x0F)

#define INSTR_ADDI	(INSTR_BASE + 0x10)
#define INSTR_ADDF	(INSTR_BASE + 0x11)
#define INSTR_SUBI	(INSTR_BASE + 0x12)
#define INSTR_SUBF	(INSTR_BASE + 0x13)
#define INSTR_MULI	(INSTR_BASE + 0x14)
#define INSTR_MULF	(INSTR_BASE + 0x15)
#define INSTR_DIVI	(INSTR_BASE + 0x16)
#define INSTR_DIVF	(INSTR_BASE + 0x17)

#define INSTR_EQI	(INSTR_BASE + 0x18)
#define INSTR_EQF	(INSTR_BASE + 0x19)
#define INSTR_NEQI	(INSTR_BASE + 0x1A)
#define INSTR_NEQF	(INSTR_BASE + 0x1B)
#define INSTR_LTI	(INSTR_BASE + 0x1C)
#define INSTR_LTF	(INSTR_BASE + 0x1D)
#define INSTR_LEI	(INSTR_BASE + 0x1E)
#define INSTR_LEF	(INSTR_BASE + 0x1F)
#define INSTR_GTI	(INSTR_BASE + 0x20)
#define INSTR_GTF	(INSTR_BASE + 0x21)
#define INSTR_GEI	(INSTR_BASE + 0x22)
#define INSTR_GEF	(INSTR_BASE + 0x23)

#define INSTR_MOD	(INSTR_BASE + 0x24)
#define INSTR_AND	(INSTR_BASE + 0x25)
#define INSTR_OR	(INSTR_BASE + 0x26)
#define INSTR_XOR	(INSTR_BASE + 0x27)
#define INSTR_SHL	(INSTR_BASE + 0x28)
#define INSTR_SHR	(INSTR_BASE + 0x29)
#define INSTR_NOT	(INSTR_BASE + 0x2A)
#define INSTR_INV	(INSTR_BASE + 0x2B)

#define INSTR_SQRT	(INSTR_BASE + 0x2C)
#define INSTR_SIN	(INSTR_BASE + 0x2D)
#define INSTR_COS	(INSTR_BASE + 0x2E)
#define INSTR_ATAN2	(INSTR_BASE + 0x2F)
#define INSTR_ABS	(INSTR_BASE + 0x30)
#define INSTR_FLOOR	(INSTR_BASE + 0x31)
#define INSTR_CEIL  (INSTR_BASE + 0x32)
#define INSTR_ROUND	(INSTR_BASE + 0x33)
#define INSTR_LERP	(INSTR_BASE + 0x34)
#define INSTR_MINF	(INSTR_BASE + 0x35)
#define INSTR_MAXF	(INSTR_BASE + 0x36)

#define INSTR_MOVIARR	(INSTR_BASE + 0x37)
#define INSTR_MOVFARR	(INSTR_BASE + 0x38)
#define INSTR_MOVEARR	(INSTR_BASE + 0x39)
#define INSTR_LDIARR	(INSTR_BASE + 0x3A)
#define INSTR_LDFARR	(INSTR_BASE + 0x3B)
#define INSTR_LDEARR	(INSTR_BASE + 0x3C)
#define INSTR_STIARR	(INSTR_BASE + 0x3D)
#define INSTR_STFARR	(INSTR_BASE + 0x3E)
#define INSTR_STEARR	(INSTR_BASE + 0x3F)
#define INSTR_REFIARR	(INSTR_BASE + 0x40)
#define INSTR_REFFARR	(INSTR_BASE + 0x41)
#define INSTR_REFEARR	(INSTR_BASE + 0x42)

/* Parameter type */
#define PARAM_REG		0 /* 2 bytes; register */
#define PARAM_4			1 /* 4 bytes; int or float */
#define PARAM_REG_IND	2 /* 2 bytes; reg indirect */
#define PARAM_STR		3 /* variable bytes, uses 16 bit length header */

struct IchigoFile {
	char signature[8]; /* "ICHIGO\0\0" */
	int version;
};

struct IchigoChunk {
	char sig[4];
	int len;
};

struct IchigoInstr {
	uint16_t instr;
	uint16_t params;
};

struct IchigoFn {
	int instrCount;
	int instrLen;

	int paramCount;
	int nameLen;
};

struct IchigoImport {
	int nameLen;
};

struct IchigoGlobal {
	int nameLen;
};

#endif
