#ifndef MESSAGES_H
#define MESSAGES_H

#define BLK_EMPTY               0x00    // 00000000
#define BLK_BLOCK               0x01    // 00000001
#define BLK_BLOCK_ACK           0x81    // 10000001
#define BLK_ACK                 0x02    // 00000010

#define NEED_ACK                0x80    // 10000000

#define COOKIE_MSG              0x08    // 00001000

#define BW_MSG                  0x40    // 01000000

#define MASK_BLOCK_ACK          0x7E    // 01111110

#define BLOCK_CONSECUTIVE       0x04    // 00000100
#define BLOCK_MASK_CONSECUTIVE  0xFB    // 11111011

#endif
