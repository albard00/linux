/** ***************************************************************************
 **  _____       ______  _____                                               **
 ** |_   _|     |  ____|/ ____|                                              **
 **   | |  _ __ | |__  | (___       Institute of Embedded Systems            **
 **   | | | '_ \|  __|  \___ \      Zürcher Hochschule für Angewandte        **
 **  _| |_| | | | |____ ____) |     Wissenschaften                           **
 ** |_____|_| |_|______|_____/      8401 Winterthur, Switzerland             **
 ******************************************************************************
 **
 ** Project     HSR / PRP with Linux
 **
 ** @file       sysfs_lre.c
 ** @brief      Sysfs kernel module for the HSR/PRP registers
 **
 ** Features
 **
 **
 ******************************************************************************
 ** @date       2011-11-24
 ** @version    1.0
 ** @author     walh, reld
 ******************************************************************************/
#ifndef HSR_H
#define HSR_H


/* bit masking */
#define HVM_BIT                 (0xff <<  8)
#define HVS_BIT                 (0xff <<  0)
#define PNTS_BIT                (0xffff0000)    /* Proxy Node Table Size */
#define PNTP_BIT                (0x0000ffff)    /* Proxy Node Table Pointer */
#define VALID_BIT               (0x01 << 16)    /* valid bit of a proxynode table entry */
#define BIT2                    (0x01 <<  2)
#define BIT3                    (0x01 <<  3)
#define BIT6                    (0x01 <<  6)
#define BIT7                    (0x01 <<  7)
#define BIT18                   (0x01 << 18)
#define BIT19                   (0x01 << 19)
#define BIT20                   (0x01 << 20)
#define BIT21                   (0x01 << 21)
#define BIT22                   (0x01 << 22)
#define BIT23                   (0x01 << 23)
#define BIT24                   (0x01 << 24)
#define BIT25                   (0x01 << 25)
#define BIT26                   (0x01 << 26)
#define BIT27                   (0x01 << 27)
#define BIT28                   (0x01 << 28)
#define BIT29                   (0x01 << 29)
#define BIT30                   (0x01 << 30)
#define BIT31                   (0x01 << 31)

union REG_RCI_RCONFIG                                            /* Offset=0x0004 : RCI_RCONFIG */
{
    unsigned long     LONG;
    struct
    {
        unsigned long   RFS:12;                                     /* [11:0] */
        unsigned long   :4;                                         /* [15:12] */
        unsigned long   RPT:2;                                      /* [17:16] */
        unsigned long   TAG:2;                                      /* [19:18] */
        unsigned long   :9;                                         /* [28:20] */
        unsigned long   END:1;                                      /* [29] */
        unsigned long   FP:1;                                       /* [30] */
        unsigned long   RFD:1;                                      /* [31] */
    } BIT;
};

union REG_RCI_WCONFIG                                            /* Offset=0x0000 : RCI_WCONFIG */
{
    unsigned long     LONG;
    struct
    {
        unsigned long   WFS:12;                                     /* [11:0] */
        unsigned long   :4;                                         /* [15:12] */
        unsigned long   I:1;                                        /* [16] */
        unsigned long   A:1;                                        /* [17] */
        unsigned long   B:1;                                        /* [18] */
        unsigned long   TAG:1;                                      /* [19] */
        unsigned long   PTH:4;                                      /* [23:20] */
        unsigned long   :5;                                         /* [28:24] */
        unsigned long   END:1;                                      /* [29] */
        unsigned long   :1;                                         /* [30] */
        unsigned long   SB:1;                                       /* [31] */
    } BIT;
};

union hsr_sup_frame
{
    uint8_t ARRAY[64];
    struct{
        uint8_t dest_mac[6];
        uint8_t src_mac[6];
        uint16_t ethertype;
        uint16_t pathId : 4;
        uint16_t LSDUsize : 12;
        uint16_t seqnbr;
        uint16_t supEtherType;
        uint16_t supPath : 4;
        uint16_t supVersion : 12;
        uint16_t supSequenceNumber;
        uint8_t tlv1_type;
        uint8_t tlv1_len;
        uint8_t mac_addr[6];
        uint8_t tlv2_type;
        uint8_t tlv2_len;
        uint8_t redbox_mac[6];
        uint8_t tlv0_type;
        uint8_t tlv0_len;
    };
};


typedef enum{
  DUPLICATE_DISCARD_OPMODE = 20,
  DUPLICATE_ACCEPT_OPMODE = 21
}operation_mode_t;

typedef struct {
    operation_mode_t opmode;
    enum {
        node_type_danp,
        node_type_redboxp,
        node_type_vdanp,
        node_type_danh,
        node_type_redboxh,
        node_type_vdanh,
    } node_type;
    uint8_t addr[6];
    int age_a_s;
    int age_b_s;
    bool valid;
} nodes_table_node_t;

#endif /* HSR_H */
