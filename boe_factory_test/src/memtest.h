/*
 * mailbox.h
 *
 *  Created on: Sep 16, 2015
 *      Author: dharmes
 */

#ifndef MAILBOX_H_
#define MAILBOX_H_

// for communication with xregv use 12 sequential 16-bit regs in ttc0 starting at 0xf8001024
#define MAILBOX_XREGV    0xFF110000       /* xregv mode here, a 7-bit reg */
#define MAILBOX          0xFF110024
#define MAILBOX_GO       (MAILBOX+0x0)    /* incr to start a test */
#define MAILBOX_DONE     (MAILBOX+0x04)     /* incr to indicate done */
#define MAILBOX_STAT     (MAILBOX+0x08)     /* status */
#define MAILBOX_START    (MAILBOX+0x0C)     /* test start addr in MB units */
#define MAILBOX_SIZE     (MAILBOX+0x10)     /* [9:0]=test size in MB, [15:10]=loop_cnt */
#define MAILBOX_RESULT   (0xFF130024+0x00)  /* 4 error counts per byte lane, + total */
#define MAILBOX_MODE     (MAILBOX+0x28)     /* mode, 1 bit per test, if msb=0 */
#define MAILBOX_LAST     (MAILBOX+0x2C)     /* last of the 12 words */

#define DDRC_MSTR_OFFSET  					0XFD070000
#define DDRC_MSTR_DATA_BUS_WIDTH_SHIFT      12

void print_line2(void);
void print_line3(void);

#endif /* MAILBOX_H_ */
