/*------------------------------------------------------------------
--  _____       ______  _____                                     --
-- |_   _|     |  ____|/ ____|                                    --
--   | |  _ __ | |__  | (___    Institute of Embedded Systems     --
--   | | | '_ \|  __|  \___ \   Zürcher Hochschule für Angewandte --
--  _| |_| | | | |____ ____) |  Wissenschaften                    --
-- |_____|_| |_|______|_____/   8401 Winterthur, Switzerland      --
--------------------------------------------------------------------
--
-- Project     : HSR/PRP-1
--
-- File Name   : lre_registers.h
-- Description : include file to access HSR/PRP-1 registers
--
--------------------------------------------------------------------*/

#ifndef ZHAW_HSR_REGISTERS_H
#define ZHAW_HSR_REGISTERS_H

//#include "lre_core_registers.h"
// lre_registers_base

/* CPU regs */
#define RCI_WCONFIG		0x0
#define RCI_RCONFIG		0x4
#define RCI_INT			0x8
#define RCI_TXRX_DATA(n)	(0xc + 4*(n))

/* Core regs */


#define R_MACADL       0
#define R_MACADH       1
#define R_TST1         2
#define R_TST2         3
#define R_PEN          4
#define R_PNT_AGT      5
#define R_DD_AGT       6

#define R_VER 0x23
#define R_RAM_STA 0x24
#define R_UFMC 0x25

#define R_FRA_ALL_ARX 0x26
#define R_FRA_TAG_ARX 0x27
#define R_FRA_NLL_ARX 0x28
#define R_FRA_ERR_ARX 0x29
#define R_FRA_WRO_ARX 0x2A

#define R_FRA_ALL_ATX 0x2B
#define R_FRA_TAG_ATX 0x2C
#define R_FRA_NLL_ATX 0x2D

#define R_FRA_ALL_BRX 0x2E
#define R_FRA_TAG_BRX 0x2F
#define R_FRA_NLL_BRX 0x30
#define R_FRA_ERR_BRX 0x31
#define R_FRA_WRO_BRX 0x32

#define R_FRA_ALL_BTX 0x33
#define R_FRA_TAG_BTX 0x34
#define R_FRA_NLL_BTX 0x35

#define R_FRA_ALL_CRX 0x36
#define R_FRA_TAG_CRX 0x37
#define R_FRA_NLL_CRX 0x38
#define R_FRA_ERR_CRX 0x39
#define R_FRA_WRO_CRX 0x3A

#define R_FRA_ALL_CTX 0x3B
#define R_FRA_TAG_CTX 0x3C
#define R_FRA_NLL_CTX 0x3D

#define R_PNT_S        0x41
#define R_PNT_D        0x42

const char *R_NAMES[]= {
    "R_MACADL",
    "R_MACADH",
    "R_TST1",
    "R_TST2",
    "R_PEN",
    "R_PNT_AGT",
    "R_DD_AGT",
    "R_MACFLT_I[1]L",
    "R_MACFLT_I[2]L",
    "R_MACFLT_I[3]L",
    "R_MACFLT_I[4]L",
    "R_MACFLT_I[5]L",
    "R_MACFLT_I[6]L",
    "R_MACFLT_I[7]L",
    "R_MACFLT_I[8]L",
    "R_MACFLT_I[1]H",
    "R_MACFLT_I[2]H",
    "R_MACFLT_I[3]H",
    "R_MACFLT_I[4]H",
    "R_MACFLT_I[5]H",
    "R_MACFLT_I[6]H",
    "R_MACFLT_I[7]H",
    "R_MACFLT_I[8]H",
    "R_MACFLT_C[1]L",
    "R_MACFLT_C[2]L",
    "R_MACFLT_C[3]L",
    "R_MACFLT_C[4]L",
    "R_MACFLT_C[5]L",
    "R_MACFLT_C[6]L",
    "R_MACFLT_C[1]H",
    "R_MACFLT_C[2]H",
    "R_MACFLT_C[3]H",
    "R_MACFLT_C[4]H",
    "R_MACFLT_C[5]H",
    "R_MACFLT_C[6]H",
    "R_VER",
    "R_RAM_STA",
    "R_UFMC",
    "R_FRA_ALL_ARX",
    "R_FRA_TAG_ARX",
    "R_FRA_NLL_ARX",
    "R_FRA_ERR_ARX",
    "R_FRA_AWRO_ARX",
    "R_FRA_ALL_ATX",
    "R_FRA_TAG_ATX",
    "R_FRA_NLL_ATX",
    "R_FRA_ALL_BRX",
    "R_FRA_TAG_BRX",
    "R_FRA_NLL_BRX",
    "R_FRA_ERR_BRX",
    "R_FRA_AWRO_BRX",
    "R_FRA_ALL_BTX",
    "R_FRA_TAG_BTX",
    "R_FRA_NLL_BTX",
    "R_FRA_ALL_CRX",
    "R_FRA_TAG_CRX",
    "R_FRA_NLL_CRX",
    "R_FRA_ERR_CRX",
    "R_FRA_AWRO_CRX",
    "R_FRA_ALL_CTX",
    "R_FRA_TAG_CTX",
    "R_FRA_NLL_CTX",
    "R_FREE_FRA_M",
    "R_DBG_RPT1",
    "R_DBG_RPT2",
    "R_PNT_S",
    "R_PNT_D",
};

#define R_COUNT (sizeof(R_NAMES) / sizeof(char*))


// core port numbers (eg. used in rx byte trailer)
#define PORT_B         3
#define PORT_A         2
#define PORT_INTERLINK 1
#define PORT_CPU       0
#define PORT_COUNT     4

// tx byte trailer
#define SEND_WITH_TAG          (1<<3)
#define SEND_TO_PORT_B         (1<<2)
#define SEND_TO_PORT_A         (1<<1)
#define SEND_TO_PORT_INTERLINK (1<<0)

#define REDBOX_MODE_SAN    0
#define REDBOX_MODE_PRP_A  1
#define REDBOX_MODE_PRP_B  2
#define REDBOX_MODE_HSR    3

#define OPERATION_MODE_H 0
#define OPERATION_MODE_N 1
#define OPERATION_MODE_T 2
#define OPERATION_MODE_M 3
#define OPERATION_MODE_U 4

#define NODE_TYPE_PRP0 0
#define NODE_TYPE_PRP1 1
#define NODE_TYPE_HSR  2

#endif

