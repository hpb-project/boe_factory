/******************************************************************************
 *
 * (c) Copyright 2010-2015 Xilinx, Inc. All rights reserved.
 *
 * This file contains confidential and proprietary information of Xilinx, Inc.
 * and is protected under U.S. and international copyright and other
 * intellectual property laws.
 *
 * DISCLAIMER
 * This disclaimer is not a license and does not grant any rights to the
 * materials distributed herewith. Except as otherwise provided in a valid
 * license issued to you by Xilinx, and to the maximum extent permitted by
 * applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL
 * FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS,
 * IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
 * MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
 * and (2) Xilinx shall not be liable (whether in contract or tort, including
 * negligence, or under any other theory of liability) for any loss or damage
 * of any kind or nature related to, arising under or in connection with these
 * materials, including for any direct, or any indirect, special, incidental,
 * or consequential loss or damage (including loss of data, profits, goodwill,
 * or any type of loss or damage suffered as a result of any action brought by
 * a third party) even if such damage or loss was reasonably foreseeable or
 * Xilinx had been advised of the possibility of the same.
 *
 * CRITICAL APPLICATIONS
 * Xilinx products are not designed or intended to be fail-safe, or for use in
 * any application requiring fail-safe performance, such as life-support or
 * safety devices or systems, Class III medical devices, nuclear facilities,
 * applications related to the deployment of airbags, or any other applications
 * that could lead to death, personal injury, or severe property or
 * environmental damage (individually and collectively, "Critical
 * Applications"). Customer assumes the sole risk and liability of any use of
 * Xilinx products in Critical Applications, subject only to applicable laws
 * and regulations governing limitations on product liability.
 *
 * THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE
 * AT ALL TIMES.
 *
 ******************************************************************************/

// -------------------------------------------------------------- INCLUDES ----
#include <xil_printf.h>
#include <stdlib.h>
#include <stdio.h>
#include <xil_cache.h>
#include "memtest.h"
#include "defines.h"
#include "freq_decoder.h"
#include "ddrc.h"


// --------------------------------------------------------- PREPROCESSORS ----
#define ERR_INFO					0xFF140024
#define TTC1_BASE					0xFF120000
#define TTC1_CNTR_VAL_1				0x18
#define TTC1_CNTR_CTRL_1  			0xC
#define TTC1_CLK_CTRL_1				0x0
#define min(a,b)     				(((a) < (b)) ? (a) : (b))
#define max(a,b)     				(((a) > (b)) ? (a) : (b))
#define lim(a,lo,hi) 				max(min(a,hi),lo)
#define YLFSR(a)    				((a << 1) + (((a >> 60) & 1) ^ ((a >> 54) & 1) ^ 1))
#define DLFSR23(a)					((a << 1) + (((a >> 22) & 1) ^ ((a >> 17) & 1) ^ 1))
#define DEFAULT_TEST_ADDR			0x0
#define DEFAULT_TEST_LEN			1024
#define DEFAULT_TEST_PATTERN		0


// ------------------------------------------------------------ PROTOTYPES ----
extern void outbyte(char c);
extern char inbyte();
extern void freq_decoder();
extern double read_cpu_freq();
extern void disable_caches(void);
extern void enable_caches(void);
extern void init_platform();
extern void measure_wr_eye(unsigned long int, unsigned int, unsigned int, unsigned int);
extern double measure_rd_eye(unsigned long int, unsigned int, unsigned int, unsigned int);
extern void ddr_config_params();
extern void read_ddrc_freq(void);
extern void print_help(void);
void printMemTestHeader(void);
extern void print_line(void);
extern void print_line2(void);
extern void print_line3(void);
extern void print_line4(void);

void error_info(long long int addr, long long int wrdat, long long int rddat);
int memtest(unsigned int _start, unsigned int _size, int mode, int loop);
int static inline reg_read_32(unsigned int addr){
				int retval;
				retval = *(volatile unsigned int *)(UINTPTR)addr;
				//xil_printf("Read 0x%08x at 0x%08x \r\n", addr, retval);
				return retval;
}

void static inline reg_write_32(unsigned int addr, int value) {
				*(volatile unsigned int *)(UINTPTR)addr = value;
				//xil_printf("Wrote 0x%08x at 0x%08x \r\n", addr, value);
}


long long int static inline reg_read_64(unsigned long long int addr) {
				unsigned int lower, upper;
				long long int retval = 0;
				lower = *(volatile unsigned int *) addr;
				upper = *(volatile unsigned int *) (addr+4);
				retval = ( ((long long int) upper) << 32) | (long long int) lower;
				//xil_printf("Read (UPPER) 0x%08x at 0x%08x (LOWER) 0x%08x at 0x%08x \r\n", upper, (addr+4), lower, addr);
				return retval;
}	

void static inline reg_write_64(unsigned long long int addr, long long int value) {
				unsigned int lower = (unsigned int) value;
				unsigned int upper = (unsigned int) (value >> 32);				
				*(volatile unsigned int *) addr = lower;
				*(volatile unsigned int *) (addr+4) = upper;
				//xil_printf("Written (UPPER) 0x%08x at 0x%08x (LOWER) 0x%08x at 0x%08x \r\n", upper, (addr+4), lower, addr);
}




// ----------------------------------------------------- GLOBAL VARIABLES ----
unsigned int bus_width = 64;      	// 64 or 32 bits
int verbose = 1;         			// 1=memtest, 2=writeeye, 4='after', 8=print errors, 0x10=qual, 0x20 eye info
long long int rseed = 0;           	// random seed
int memtest_stat = 1;    			// enables memtest status upload
int werr = 0;
long long int epp;                 	// epp = errors per pattern, 1 bit per subtest pattern
int wren = 1;            			// write enable, set to 0 to disble test writes
int delayed_reads = 0;
#define DELAY 10000
int response[4];         			// response to remote cmd
int errcnt[8];
int gresult[200];    				// enough space for 20 write eye results
char textmsg[256];   				// text msg to xregv
long long int err_buf[256];      	// 10/14/2013: capture the first ~80 errors: format is:  addr,wr,rd, addr,wr,rd,...
float test_time=0.0, total_test_time=0.0;
double pclk = 0.0;    				// pclk freq in mhz for cpu at 667
char cr[4] = "\r\n";

int ddr_init = 0;
int kbyte = 1024;
int mbyte = 1024*1024;
int gbyte = 1024*1024*1024;
long long int qmask = 0xffffffffffffffff;                 // default

// vars for multi-loop memory tests
int cum_errcnt[8];       // cumulative
int test_sizes[9] = { 16, 32, 64, 127,  128, 255, 511, 1023, 2047 };
int test_size_sel = 0;
int test_loop_cnt = 1;

// ------------------------------------------------------------------------------------------
// per bit test pattern, repeats every 16 words.
// __-__-_-   --_--_-_
// then every 16 words invert a different bit per byte.

long long int pattern_64bit[16] = { 				// for a 64-bit memory
	0x0000000000000000, 0x0000000000000000,
	0xFFFFFFFFFFFFFFFF, 0x0000000000000000,
	0x0000000000000000, 0xFFFFFFFFFFFFFFFF,
	0x0000000000000000, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0x0000000000000000, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF
};

long long int pattern_32bit[16] = { 				// for a 32-bit memory
	0x0000000000000000, 0xFFFFFFFF00000000,
	0x00000000FFFFFFFF, 0x00000000FFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF,
	0xFFFFFFFF00000000, 0xFFFFFFFF00000000,
	0x0000000000000000, 0xFFFFFFFF00000000,
	0x00000000FFFFFFFF, 0x00000000FFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF,
	0xFFFFFFFF00000000, 0xFFFFFFFF00000000
};

long long int invertmask[8] = {
	0x0101010101010101, 0x0202020202020202, 0x0404040404040404, 0x0808080808080808,
	0x1010101010101010, 0x2020202020202020, 0x4040404040404040, 0x8080808080808080
};

unsigned long int aggressor_pattern_64bit[] = {
	0x0101010101010101, 0x0101010101010101, 0xFEFEFEFEFEFEFEFE, 0x0101010101010101,
	0x0101010101010101, 0xFEFEFEFEFEFEFEFE, 0x0101010101010101, 0xFEFEFEFEFEFEFEFE,
	0xFEFEFEFEFEFEFEFE, 0xFEFEFEFEFEFEFEFE, 0x0101010101010101, 0xFEFEFEFEFEFEFEFE,
	0xFEFEFEFEFEFEFEFE, 0x0101010101010101, 0xFEFEFEFEFEFEFEFE, 0x0101010101010101,
	0x0202020202020202, 0x0202020202020202, 0xFDFDFDFDFDFDFDFD, 0x0202020202020202,
	0x0202020202020202, 0xFDFDFDFDFDFDFDFD, 0x0202020202020202, 0xFDFDFDFDFDFDFDFD,
	0xFDFDFDFDFDFDFDFD, 0xFDFDFDFDFDFDFDFD, 0x0202020202020202, 0xFDFDFDFDFDFDFDFD,
	0xFDFDFDFDFDFDFDFD, 0x0202020202020202, 0xFDFDFDFDFDFDFDFD, 0x0202020202020202,
	0x0404040404040404, 0x0404040404040404, 0xFBFBFBFBFBFBFBFB, 0x0404040404040404,
	0x0404040404040404, 0xFBFBFBFBFBFBFBFB, 0x0404040404040404, 0xFBFBFBFBFBFBFBFB,
	0xFBFBFBFBFBFBFBFB, 0xFBFBFBFBFBFBFBFB, 0x0404040404040404, 0xFBFBFBFBFBFBFBFB,
	0xFBFBFBFBFBFBFBFB, 0x0404040404040404, 0xFBFBFBFBFBFBFBFB, 0x0404040404040404,
	0x0808080808080808, 0x0808080808080808, 0xF7F7F7F7F7F7F7F7, 0x0808080808080808,
	0x0808080808080808, 0xF7F7F7F7F7F7F7F7, 0x0808080808080808, 0xF7F7F7F7F7F7F7F7,
	0xF7F7F7F7F7F7F7F7, 0xF7F7F7F7F7F7F7F7, 0x0808080808080808, 0xF7F7F7F7F7F7F7F7,
	0xF7F7F7F7F7F7F7F7, 0x0808080808080808, 0xF7F7F7F7F7F7F7F7, 0x0808080808080808,
	0x1010101010101010, 0x1010101010101010, 0xEFEFEFEFEFEFEFEF, 0x1010101010101010,
	0x1010101010101010, 0xEFEFEFEFEFEFEFEF, 0x1010101010101010, 0xEFEFEFEFEFEFEFEF,
	0xEFEFEFEFEFEFEFEF, 0xEFEFEFEFEFEFEFEF, 0x1010101010101010, 0xEFEFEFEFEFEFEFEF,
	0xEFEFEFEFEFEFEFEF, 0x1010101010101010, 0xEFEFEFEFEFEFEFEF, 0x1010101010101010,
	0x2020202020202020, 0x2020202020202020, 0xDFDFDFDFDFDFDFDF, 0x2020202020202020,
	0x2020202020202020, 0xDFDFDFDFDFDFDFDF, 0x2020202020202020, 0xDFDFDFDFDFDFDFDF,
	0xDFDFDFDFDFDFDFDF, 0xDFDFDFDFDFDFDFDF, 0x2020202020202020, 0xDFDFDFDFDFDFDFDF,
	0xDFDFDFDFDFDFDFDF, 0x2020202020202020, 0xDFDFDFDFDFDFDFDF, 0x2020202020202020,
	0x4040404040404040, 0x4040404040404040, 0xBFBFBFBFBFBFBFBF, 0x4040404040404040,
	0x4040404040404040, 0xBFBFBFBFBFBFBFBF, 0x4040404040404040, 0xBFBFBFBFBFBFBFBF,
	0xBFBFBFBFBFBFBFBF, 0xBFBFBFBFBFBFBFBF, 0x4040404040404040, 0xBFBFBFBFBFBFBFBF,
	0xBFBFBFBFBFBFBFBF, 0x4040404040404040, 0xBFBFBFBFBFBFBFBF, 0x4040404040404040,
	0x8080808080808080, 0x8080808080808080, 0x7F7F7F7F7F7F7F7F, 0x8080808080808080,
	0x8080808080808080, 0x7F7F7F7F7F7F7F7F, 0x8080808080808080, 0x7F7F7F7F7F7F7F7F,
	0x7F7F7F7F7F7F7F7F, 0x7F7F7F7F7F7F7F7F, 0x8080808080808080, 0x7F7F7F7F7F7F7F7F,
	0x7F7F7F7F7F7F7F7F, 0x8080808080808080, 0x7F7F7F7F7F7F7F7F, 0x8080808080808080
};

unsigned int aggressor_pattern_32bit[] = {
	0x01010101, 0x01010101, 0x01010101, 0x01010101, 0xFEFEFEFE, 0xFEFEFEFE, 0x01010101, 0x01010101,
	0x01010101, 0x01010101, 0xFEFEFEFE, 0xFEFEFEFE, 0x01010101, 0x01010101, 0xFEFEFEFE, 0xFEFEFEFE,
	0xFEFEFEFE, 0xFEFEFEFE, 0xFEFEFEFE, 0xFEFEFEFE, 0x01010101, 0x01010101, 0xFEFEFEFE, 0xFEFEFEFE,
	0xFEFEFEFE, 0xFEFEFEFE, 0x01010101, 0x01010101, 0xFEFEFEFE, 0xFEFEFEFE, 0x01010101, 0x01010101,
	0x02020202, 0x02020202, 0x02020202, 0x02020202, 0xFDFDFDFD, 0xFDFDFDFD, 0x02020202, 0x02020202,
	0x02020202, 0x02020202, 0xFDFDFDFD, 0xFDFDFDFD, 0x02020202, 0x02020202, 0xFDFDFDFD, 0xFDFDFDFD,
	0xFDFDFDFD, 0xFDFDFDFD, 0xFDFDFDFD, 0xFDFDFDFD, 0x02020202, 0x02020202, 0xFDFDFDFD, 0xFDFDFDFD,
	0xFDFDFDFD, 0xFDFDFDFD, 0x02020202, 0x02020202, 0xFDFDFDFD, 0xFDFDFDFD, 0x02020202, 0x02020202,
	0x04040404, 0x04040404, 0x04040404, 0x04040404, 0xFBFBFBFB, 0xFBFBFBFB, 0x04040404, 0x04040404,
	0x04040404, 0x04040404, 0xFBFBFBFB, 0xFBFBFBFB, 0x04040404, 0x04040404, 0xFBFBFBFB, 0xFBFBFBFB,
	0xFBFBFBFB, 0xFBFBFBFB, 0xFBFBFBFB, 0xFBFBFBFB, 0x04040404, 0x04040404, 0xFBFBFBFB, 0xFBFBFBFB,
	0xFBFBFBFB, 0xFBFBFBFB, 0x04040404, 0x04040404, 0xFBFBFBFB, 0xFBFBFBFB, 0x04040404, 0x04040404,
	0x08080808, 0x08080808, 0x08080808, 0x08080808, 0xF7F7F7F7, 0xF7F7F7F7, 0x08080808, 0x08080808,
	0x08080808, 0x08080808, 0xF7F7F7F7, 0xF7F7F7F7, 0x08080808, 0x08080808, 0xF7F7F7F7, 0xF7F7F7F7,
	0xF7F7F7F7, 0xF7F7F7F7, 0xF7F7F7F7, 0xF7F7F7F7, 0x08080808, 0x08080808, 0xF7F7F7F7, 0xF7F7F7F7,
	0xF7F7F7F7, 0xF7F7F7F7, 0x08080808, 0x08080808, 0xF7F7F7F7, 0xF7F7F7F7, 0x08080808, 0x08080808,
	0x10101010, 0x10101010, 0x10101010, 0x10101010, 0xEFEFEFEF, 0xEFEFEFEF, 0x10101010, 0x10101010,
	0x10101010, 0x10101010, 0xEFEFEFEF, 0xEFEFEFEF, 0x10101010, 0x10101010, 0xEFEFEFEF, 0xEFEFEFEF,
	0xEFEFEFEF, 0xEFEFEFEF, 0xEFEFEFEF, 0xEFEFEFEF, 0x10101010, 0x10101010, 0xEFEFEFEF, 0xEFEFEFEF,
	0xEFEFEFEF, 0xEFEFEFEF, 0x10101010, 0x10101010, 0xEFEFEFEF, 0xEFEFEFEF, 0x10101010, 0x10101010,
	0x20202020, 0x20202020, 0x20202020, 0x20202020, 0xDFDFDFDF, 0xDFDFDFDF, 0x20202020, 0x20202020,
	0x20202020, 0x20202020, 0xDFDFDFDF, 0xDFDFDFDF, 0x20202020, 0x20202020, 0xDFDFDFDF, 0xDFDFDFDF,
	0xDFDFDFDF, 0xDFDFDFDF, 0xDFDFDFDF, 0xDFDFDFDF, 0x20202020, 0x20202020, 0xDFDFDFDF, 0xDFDFDFDF,
	0xDFDFDFDF, 0xDFDFDFDF, 0x20202020, 0x20202020, 0xDFDFDFDF, 0xDFDFDFDF, 0x20202020, 0x20202020,
	0x40404040, 0x40404040, 0x40404040, 0x40404040, 0xBFBFBFBF, 0xBFBFBFBF, 0x40404040, 0x40404040,
	0x40404040, 0x40404040, 0xBFBFBFBF, 0xBFBFBFBF, 0x40404040, 0x40404040, 0xBFBFBFBF, 0xBFBFBFBF,
	0xBFBFBFBF, 0xBFBFBFBF, 0xBFBFBFBF, 0xBFBFBFBF, 0x40404040, 0x40404040, 0xBFBFBFBF, 0xBFBFBFBF,
	0xBFBFBFBF, 0xBFBFBFBF, 0x40404040, 0x40404040, 0xBFBFBFBF, 0xBFBFBFBF, 0x40404040, 0x40404040,
	0x80808080, 0x80808080, 0x80808080, 0x80808080, 0x7F7F7F7F, 0x7F7F7F7F, 0x80808080, 0x80808080,
	0x80808080, 0x80808080, 0x7F7F7F7F, 0x7F7F7F7F, 0x80808080, 0x80808080, 0x7F7F7F7F, 0x7F7F7F7F,
	0x7F7F7F7F, 0x7F7F7F7F, 0x7F7F7F7F, 0x7F7F7F7F, 0x80808080, 0x80808080, 0x7F7F7F7F, 0x7F7F7F7F,
	0x7F7F7F7F, 0x7F7F7F7F, 0x80808080, 0x80808080, 0x7F7F7F7F, 0x7F7F7F7F, 0x80808080, 0x80808080
};

unsigned long int mask_64bit[] = {
	0x00000000000000FF, 
	0x000000000000FF00, 
	0x0000000000FF0000, 
	0x00000000FF000000, 
	0x000000FF00000000, 
	0x0000FF0000000000, 
	0x00FF000000000000, 
	0xFF00000000000000
};

unsigned long int mask_32bit[] = {
	0x000000FF, 
	0x0000FF00, 
	0x00FF0000, 
	0xFF000000
};

// repeating 128-word patterns
long long int pat1[128], pat2[128];




/****************************************************************************
 * Function: Timer Init
 ****************************************************************************/
void timer_init() {
	reg_write_32(TTC1_BASE + TTC1_CLK_CTRL_1,  1 + (15 << 1));  // 0x1f
	reg_write_32(TTC1_BASE + TTC1_CNTR_CTRL_1,  (1 << 4) + (1 << 5));  // 0x30
}

/****************************************************************************
 * Function: Timer Read
 ****************************************************************************/
int timer_read() {
	unsigned int value;
	value = *(volatile unsigned int *) (TTC1_BASE + TTC1_CNTR_VAL_1);
	return value;
}

/****************************************************************************
 * Function: Timer Value
 ****************************************************************************/
double timer_value(int t, double fpclk) {
	// t in timer units, fpclk in mhz; returns timer value in seconds
	return ((((double) t) * 65536.0) / (fpclk * 1000000.0));
	//xil_printf("Timer:=%g, %g, %d \r\n", x, fpclk, t);
}

/****************************************************************************
 * Function: Calculate Value PRBS31
 ****************************************************************************/
int calc_value_prbs31(int lfsr) {
	// "calc next lfsr value"
	// Based on XAPP052: for 32 bit: 32, 22, 2, 1
	//                   for 31 bit: 31, 28
	// do XNOR.
	// In verilog in would be:
	// lfsr = {lfsr[30:0] , lfsr_xnor = (lfsr[30] ^ lfsr[27]) ? 1'd0 : 1'd1;  }
	//  in one line:
	//      ((a << 1) + (((a >> 30) & 1) ^ ((a >> 27) & 1) ^ 1))
	//
	int lsb, rslt;
	lsb = ((lfsr >> 30) & 1) ^ ((lfsr >> 27) & 1);
	lsb = lsb ^ 1;
	rslt = ((lfsr << 1) + lsb);  // & 0x0ffffffff;
	return rslt;
}

/****************************************************************************
 * Function: Calculate Value PRBS23
 ****************************************************************************/
int calc_value_prbs23(int lfsr) {
	// "calc next lfsr value"
	// Based on XAPP052: for 32 bit: 32, 22, 2, 1
	//                   for 31 bit: 31, 28
	// do XNOR.
	// In verilog in would be:
	// lfsr = {lfsr[30:0] , lfsr_xnor = (lfsr[30] ^ lfsr[27]) ? 1'd0 : 1'd1;  }
	//  in one line:
	//      ((a << 1) + (((a >> 30) & 1) ^ ((a >> 27) & 1) ^ 1))
	//
	int lsb, rslt;
	lsb = ((lfsr >> 22) & 1) ^ ((lfsr >> 17) & 1);
	lsb = lsb ^ 1;
	rslt = ((lfsr << 1) + lsb);  // & 0x0ffffffff;
	return rslt;
}

/****************************************************************************
 * Function: Error Info
 ****************************************************************************/
void error_info(long long int addr, long long int wrdat, long long int rddat) {
	int n;
	// put in xregv mailbox regs
	if (werr <= 1) {  // init or 1st error
		reg_write_64(ERR_INFO,   addr);
		reg_write_64(ERR_INFO+8, wrdat);
		reg_write_64(ERR_INFO+16, rddat);
	}
	// also store first 80 errors in err_buf
	if (werr <= 80) {
		n = werr - 1;
		err_buf[3*n]   = addr;
		err_buf[3*n+1] = wrdat;
		err_buf[3*n+2] = rddat;
	}
}

/****************************************************************************
 * Function: Simple Memory Test
 ****************************************************************************/
int memtest_simple(unsigned int _start, unsigned int _size, int mode, int loop, long long int d0, long long int d1, long long int d2, long long int d3) {
	// Do a simple memory test
	// start and size are in bytes
	// if loop==-1, do write only
	unsigned long long int i, j, x=0;
	unsigned long long int size, start;
	unsigned long long int addr, data, ref;
	int merr = 0;   // per test word errors
	int lerrcnt[8];  // local errcnt per test
	long long int dat[4];

	start = ((unsigned long long int) _start) * mbyte;
	size = ((unsigned long long int) _size) * mbyte;
	//xil_printf("memset_simple: start=0x%016llx size=0x%016llx\r\n", start, size);

	dat[0] = d0 & qmask;
	dat[1] = d1 & qmask;
	dat[2] = d2 & qmask;
	dat[3] = d3 & qmask;

	for (i=0; i<8; i++) {
		lerrcnt[i] = 0;
	}

	if (memtest_stat) {
		reg_write_32(MAILBOX_STAT, 100+mode);
	}

	timer_init();

	addr = start;
	if (wren) {
		for (i=0; i<size; i+=32) {
			if(addr < (2048*mbyte)-32) {
				addr = start + i;
				//if(addr>0xffffff00)
					//				xil_printf("lower address: 0x%016llx, x=0x%016llx, i=0x%016llx \r\n", addr, x, i );
			}
			else {
				addr = 0x800000000 + x;
				//xil_printf("upper address: 0x%016llx, x=0x%016llx, i=0x%016llx \r\n", addr, x, i);
				x = x + 32;
			}

			reg_write_64(addr, dat[0]);
			reg_write_64(addr+8, dat[1]);
			reg_write_64(addr+16, dat[2]);
			reg_write_64(addr+24, dat[3]);
		}
	}

	Xil_DCacheInvalidate();

	if (loop == -1) {
		return 0;
	}

	x=0;
	addr = start;
	for (i=0; i<size; i+=32) {
		if(delayed_reads){
			int j = 123456;			// delay between loops
			for (i=0; i<5000; i++) {
				j = calc_value_prbs31(j);
			}
		}


		if(addr < (2048*mbyte)-32) {
			addr = start + i;
		}
		else {
			addr = 0x800000000 + x;
			x = x + 32;
		}

		for (j=0; j<4; j++) {
			data = reg_read_64(addr+8*j);
			ref = dat[j];
			//xil_printf("ADDR=0x%016llx DATA=0x%016llx REF=0x%016llx\r\n", addr+8*j, data, ref);
			if (data != ref) {
				werr++;
				merr++;
				if ((data & 0x00000000000000ff) != (ref & 0x00000000000000ff)) {
					lerrcnt[0]++;
				}
				if ((data & 0x000000000000ff00) != (ref & 0x000000000000ff00)) {
					lerrcnt[1]++;
				}
				if ((data & 0x0000000000ff0000) != (ref & 0x0000000000ff0000)) {
					lerrcnt[2]++;
				}
				if ((data & 0x00000000ff000000) != (ref & 0x00000000ff000000)) {
					lerrcnt[3]++;
				}
				if ((data & 0x000000ff00000000) != (ref & 0x000000ff00000000)) {
					lerrcnt[4]++;
				}
				if ((data & 0x0000ff0000000000) != (ref & 0x0000ff0000000000)) {
					lerrcnt[5]++;
				}
				if ((data & 0x00ff000000000000) != (ref & 0x00ff000000000000)) {
					lerrcnt[6]++;
				}
				if ((data & 0xff00000000000000) != (ref & 0xff00000000000000)) {
					lerrcnt[7]++;
				}

				if ((verbose & 8) && (merr <= 10)) {
					xil_printf("Memtest_s ERROR: addr=0x%X rd/ref/xor = 0x%016llx 0x%016llx 0x%016llx \r\n", addr+4*j, data, ref, data ^ ref);
				}
				error_info(addr+8*j, ref, data);
			}
		} 		// j
		if (merr != 0) {
			if ((lerrcnt[0] > 1000) && (lerrcnt[1] > 1000)	&& (lerrcnt[2] > 1000) && (lerrcnt[3] > 1000) && (lerrcnt[4] > 1000) && (lerrcnt[5] > 1000)	&& (lerrcnt[6] > 1000) && (lerrcnt[7] > 1000)) {
				break;  // to save time when there are lots of errors
			}
		}
	} // i

	i = timer_read();
	test_time = timer_value(i, pclk);
	total_test_time += test_time;
	if (verbose & 1) {
		/*printf("Memtest_s (%3d:%2d) Done %d MB starting at %d MB, %d errors (%d %d %d %d %d %d %d %d). %g sec \r\n",
				loop,mode, size/1024/1024, start/1024/1024, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time );*/
		printf("\rMTS(%d:%2d)| %6d | %4d, %4d, %4d, %4d, %4d, %4d, %4d, %4d | %g\r\n",
		            loop, mode, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time);
	}
	print_line4();
	// add local to global
	for (i=0; i<8; i++) {
		errcnt[i] += lerrcnt[i];
	}
	if (merr > 0){
		epp += 1 << mode;
	}

	return 0;
}

/****************************************************************************
 * Function: LFSR Memory Test
 ****************************************************************************/
int memtest_lfsr(unsigned int _start, unsigned int _size, int mode, int loop) {
	// Do a simple memory test
	// start and size are in bytes
	unsigned long long int i, start, size, x=0;
	unsigned long long int addr, data, ref;
	int merr = 0;   // per test word errors
	long long int randval;
	int lerrcnt[8];  // local errcnt per test

	start = ((unsigned long long int ) _start) * mbyte;
	size = ((unsigned long long int) _size) * mbyte;
	//xil_printf("memset_lfsr: start=0x%016llx size=0x%016llx\r\n", start, size);

	// change random seed
	rseed += 0x017c1e2313567c9b;

	for (i=0; i<8; i++) {
		lerrcnt[i] = 0;
	}
	if (memtest_stat) {
		reg_write_32(MAILBOX_STAT, 100+mode);
	}
	timer_init();

	randval = 0x12345678 + loop + 19*mode + rseed;
	addr = start;
	if (wren) {
		for (i=0; i<size; i+=8) {
			if(addr < (2048*mbyte)-8) {
				addr = start + i;
			}
			else {
				addr = 0x800000000 + x;
				x = x + 8;
			}

			randval = YLFSR(randval);
			ref = randval & qmask;
			reg_write_64(addr, ref);
		}
	}

	x = 0;
	addr = start;

	Xil_DCacheInvalidate();
	randval = 0x12345678 + loop + 19*mode + rseed;
	for (i=0; i<size; i+=8) {
		if(delayed_reads){
			int j = 123456;			// delay between loops
			for (i=0; i<DELAY; i++) {
				j = calc_value_prbs31(j);
			}
		}

		if(addr < (2048*mbyte)-8) {
			addr = start + i;
		}
		else {
			addr = 0x800000000 + x;
			x = x + 8;
		}
		randval = YLFSR(randval);
		ref = randval & qmask;
		data = reg_read_64(addr);
		if (data != ref) {
			werr++;
			merr++;
			if ((data & 0x00000000000000ff) != (ref & 0x00000000000000ff)) {
				lerrcnt[0]++;
			}
			if ((data & 0x000000000000ff00) != (ref & 0x000000000000ff00)) {
				lerrcnt[1]++;
			}
			if ((data & 0x0000000000ff0000) != (ref & 0x0000000000ff0000)) {
				lerrcnt[2]++;
			}
			if ((data & 0x00000000ff000000) != (ref & 0x00000000ff000000)) {
				lerrcnt[3]++;
			}
			if ((data & 0x000000ff00000000) != (ref & 0x000000ff00000000)) {
				lerrcnt[4]++;
			}
			if ((data & 0x0000ff0000000000) != (ref & 0x0000ff0000000000)) {
				lerrcnt[5]++;
			}
			if ((data & 0x00ff000000000000) != (ref & 0x00ff000000000000)) {
				lerrcnt[6]++;
			}
			if ((data & 0xff00000000000000) != (ref & 0xff00000000000000)) {
				lerrcnt[7]++;
			}
			if ((verbose & 8) && (merr <= 10)) {
				xil_printf("Memtest_l ERROR: addr=0x%X rd/ref/xor = 0x%016llx 0x%016llx 0x%016llx \r\n", addr, data, ref, data ^ ref);
			}
			error_info(addr, ref, data);
		}
		if (merr != 0) {
			if ((lerrcnt[0] > 1000) && (lerrcnt[1] > 1000)	&& (lerrcnt[2] > 1000) && (lerrcnt[3] > 1000) && (lerrcnt[4] > 1000) && (lerrcnt[5] > 1000)	&& (lerrcnt[6] > 1000) && (lerrcnt[7] > 1000)) {
				break;  // to save time when there are lots of errors
			}
		}
	} // i

	i = timer_read();
	test_time = timer_value(i, pclk);
	total_test_time += test_time;
	if (verbose & 1) {
		/*printf("Memtest_l (%3d:%2d) Done %d MB starting at %d MB, %d errors (%d %d %d %d %d %d %d %d). %g sec \r\n",
				loop,mode, size/1024/1024, start/1024/1024, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time );*/
		printf("\rMTL(%d:%2d)| %6d | %4d, %4d, %4d, %4d, %4d, %4d, %4d, %4d | %g\r\n",
		            loop, mode, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time);
	}
	print_line4();
	// add local to global
	for (i=0; i<8; i++) {
		errcnt[i] += lerrcnt[i];
	}
	if (merr > 0) {
		epp += 1 << mode;
	}
	return 0;
}


/****************************************************************************
 * Function: Memory Test - 128 Word Pattern
 ****************************************************************************/
int memtest_pat128(unsigned int _start, unsigned int _size, int mode, int loop, long long int *pat) {
	// Do a simple memory test using a 128-word pattern
	// start and size are in bytes
	unsigned long long int i, mod128, start, size, x=0;
	long long int addr, data, ref;
	int merr = 0;   				// per test word errors
	int lerrcnt[8];  				// local errcnt per test

	start = ((unsigned long long int ) _start) * mbyte;
	size = ((unsigned long long int) _size) * mbyte;
	//xil_printf("memset_pat128: start=0x%016llx size=0x%016llx\r\n", start, size);

	for (i=0; i<8; i++) {
		lerrcnt[i] = 0;
	}

	if (memtest_stat) {
		reg_write_32(MAILBOX_STAT, 100+mode);
	}
	timer_init();

	addr = start;
	if (wren) {
		for (i=0; i<size; i+=8) {
			mod128 = (i >> 2) & 0x07f;
			if(addr < (2048*mbyte)-8) {
				addr = start + i;
			}
			else {
				addr = 0x800000000 + x;
				x = x+8;
			}
			ref = pat[mod128] & qmask;
			reg_write_64(addr, ref);
		}
	}
	x = 0;
	addr = start;

	Xil_DCacheInvalidate();

	for (i=0; i<size; i+=8) {
		if(delayed_reads){
			int j = 123456;			// delay between loops
			for (i=0; i<DELAY; i++) {
				j = calc_value_prbs31(j);
			}
		}


		mod128 = (i >> 2) & 0x07f;
		if(addr < (2048*mbyte)-8) {
			addr = start + i;
		}
		else {
			addr = 0x800000000 + x;
			x = x+8;
		}
		ref = pat[mod128] & qmask;
		data = reg_read_64(addr);
		if (data != ref) {
			werr++;
			merr++;
			if ((data & 0x00000000000000ff) != (ref & 0x00000000000000ff)) {
				lerrcnt[0]++;
			}
			if ((data & 0x000000000000ff00) != (ref & 0x000000000000ff00)) {
				lerrcnt[1]++;
			}
			if ((data & 0x0000000000ff0000) != (ref & 0x0000000000ff0000)) {
				lerrcnt[2]++;
			}
			if ((data & 0x00000000ff000000) != (ref & 0x00000000ff000000)) {
				lerrcnt[3]++;
			}
			if ((data & 0x000000ff00000000) != (ref & 0x000000ff00000000)) {
				lerrcnt[4]++;
			}
			if ((data & 0x0000ff0000000000) != (ref & 0x0000ff0000000000)) {
				lerrcnt[5]++;
			}
			if ((data & 0x00ff000000000000) != (ref & 0x00ff000000000000)) {
				lerrcnt[6]++;
			}
			if ((data & 0xff00000000000000) != (ref & 0xff00000000000000)) {
				lerrcnt[7]++;
			}
			if ((verbose & 8) && (merr <= 10)) {
				xil_printf("Memtest_p ERROR: addr=0x%X rd/ref/xor = 0x%016llx 0x%016llx 0x%016llx \r\n", addr, data, ref, data ^ ref);
			}
			error_info(addr, ref, data);
		}
		if (merr != 0) {
			if ((lerrcnt[0] > 1000) && (lerrcnt[1] > 1000)	&& (lerrcnt[2] > 1000) && (lerrcnt[3] > 1000) && (lerrcnt[4] > 1000) && (lerrcnt[5] > 1000)	&& (lerrcnt[6] > 1000) && (lerrcnt[7] > 1000)) {
				break;  // to save time when there are lots of errors
			}
		}
	} 	// i

	i = timer_read();
	test_time = timer_value(i, pclk);
	total_test_time += test_time;
	if (verbose & 1) {
		/*xil_printf("Memtest_l (%3d:%2d) Done %d MB starting at %d MB, %d errors (%d %d %d %d %d %d %d %d). %g sec \r\n",
		            loop,mode, size/1024/1024, start/1024/1024, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time );*/
		printf("\rMTP(%d:%2d)| %6d | %4d, %4d, %4d, %4d, %4d, %4d, %4d, %4d | %g\r\n",
		            loop, mode, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time);
	}

	print_line4();
	// add local to global
	for (i=0; i<8; i++) {
		errcnt[i] += lerrcnt[i];
	}
	if (merr > 0) {
		epp += 1 << mode;
	}
	return 0;
}


// Test Modes
//  0 - data = addr
//  1 - data = 00
//  2 - data = FF
//  3 - data = AA
//  4 - data = 55
//  5 - data = 00 FF
//  6 - data = FF 00
//  7 - data = 55 aa
//  8 - data = aa 55
//  9 - data = pattern without bit inversion
// 10 - data = pattern WITH    bit inversion
// 11 - data = p-random lfsr

/****************************************************************************
 * Function: Memory Test
 ****************************************************************************/
int memtest(unsigned int _start, unsigned int _size, int mode, int loop) {
	// Do a simple memory test
	// start and size are in bytes
	int rd, mod16, imaski;
	unsigned long long i, start, size, x=0;
	unsigned long long addr, ref, data;
	int merr = 0;   // per test word errors
	int randval;
	int lerrcnt[8];  // local errcnt per test

	start = ((unsigned long long int ) _start) * mbyte;
	size = ((unsigned long long int) _size) * mbyte;
	//xil_printf("memset_test: start=0x%016llx size=0x%016llx\r\n", start, size);

	for (i=0; i<8; i++) {
		lerrcnt[i] = 0;
	}
	if (memtest_stat) {
		reg_write_32(MAILBOX_STAT, 100+mode);
	}

	timer_init();
	// write then read memory
	for (rd=0; rd<2; rd++) {  // 0=wr, 1=rd
		x = 0;
		addr = start;
		randval = 0x12345678 + loop + 19*mode;
		for (i=0; i<size; i+=8) {
			mod16 = (i >> 2) & 0x0f;
			imaski = (i >> 6) & 0x07;  // invert mask index

			if(addr <(2048*mbyte-8)) {
				addr = start + i;
			}
			else {
				addr = 0x800000000 + x;
				x = x+8;
			}

			if (mode == 0)       {
				ref = (( ((addr+4)<<32) | addr ) & qmask);
			}
			else if (mode == 1)  {
				ref = 0;
			}
			else if (mode == 2)  {
				ref = 0xffffffffffffffff;
			}
			else if (mode == 3)  {
				ref = 0xAAAAAAAAAAAAAAAA;
			}
			else if (mode == 4)  {
				ref = 0x5555555555555555;
			}
			else if (mode == 5)  {
				ref = (i & 8) ? 0xffffffffffffffff : 0;
			}
			else if (mode == 6)  {
				ref = (i & 8) ? 0 : 0xffffffffffffffff;
			}
			else if (mode == 7)  {
				ref = (i & 8) ? 0xaaaaaaaaaaaaaaaa : 0x5555555555555555;
			}
			else if (mode == 8)  {
				ref = (i & 8) ? 0x5555555555555555 : 0xaaaaaaaaaaaaaaaa;
			}
			else if (mode == 9)  {
				ref = pattern_64bit[mod16];
			}
			else if (mode == 10) {
				ref = pattern_64bit[mod16] ^ invertmask[imaski];
			}
			else {
				randval = calc_value_prbs31(randval);
				ref = randval;
			}


			if (rd == 0) {
				if (wren || ddr_init) {
					reg_write_64(addr, ref);
					//xil_printf("DDR INIT 0x%016llx 0x%016llx \r\n", addr, ref);
				}
			}
			if (rd==1 && !ddr_init) { 															// read and compare
				if(delayed_reads){
					int j = 123456;			// delay between loops
					for (i=0; i<DELAY; i++) {
						j = calc_value_prbs31(j);
					}
				}

			data = reg_read_64(addr);
			//xil_printf("Memtest_0 : addr=0x%X rd/ref/xor = 0x%016llx 0x%016llx 0x%016llx \r\n", addr, data, ref, data ^ ref);
			if (data != ref) {
				werr++;
				merr++;
				if ((data & 0x00000000000000ff) != (ref & 0x00000000000000ff)) {
					lerrcnt[0]++;
				}
				if ((data & 0x000000000000ff00) != (ref & 0x000000000000ff00)) {
					lerrcnt[1]++;
				}
				if ((data & 0x0000000000ff0000) != (ref & 0x0000000000ff0000)) {
					lerrcnt[2]++;
				}
				if ((data & 0x00000000ff000000) != (ref & 0x00000000ff000000)) {
					lerrcnt[3]++;
				}
				if ((data & 0x000000ff00000000) != (ref & 0x000000ff00000000)) {
					lerrcnt[4]++;
				}
				if ((data & 0x0000ff0000000000) != (ref & 0x0000ff0000000000)) {
					lerrcnt[5]++;
				}
				if ((data & 0x00ff000000000000) != (ref & 0x00ff000000000000)) {
					lerrcnt[6]++;
				}
				if ((data & 0xff00000000000000) != (ref & 0xff00000000000000)) {
					lerrcnt[7]++;
				}
				if ((verbose & 8) && (merr <= 10)) {
					xil_printf("Memtest_0 ERROR: addr=0x%X rd/ref/xor = 0x%016llx 0x%016llx 0x%016llx \r\n", addr, data, ref, data ^ ref);
				}
				error_info(addr, ref, data);			// this has to change - dharmesh
			}

			if ((lerrcnt[0] > 1000) && (lerrcnt[1] > 1000)	&& (lerrcnt[2] > 1000) && (lerrcnt[3] > 1000) && (lerrcnt[4] > 1000) && (lerrcnt[5] > 1000)	&& (lerrcnt[6] > 1000) && (lerrcnt[7] > 1000)) {
				break;  // to save time when there are lots of errors
			}
			} 				// read and compare
		} 								// i
		if(!ddr_init) {
			Xil_DCacheInvalidate();
		}
	}  // rd

	i = timer_read();
	test_time = timer_value(i, pclk);
	total_test_time += test_time;

	if (verbose & 1) {
		/*printf("Memtest_0 (%3d:%2d) Done %d MB starting at %d MB, %d errors (%d %d %d %d %d %d %d %d). %g sec \r\n",
				loop,mode, size/1024/1024, start/1024/1024, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time );*/
		printf("\rMT0(%d:%2d)| %6d | %4d, %4d, %4d, %4d, %4d, %4d, %4d, %4d | %g\r\n",
		               loop, mode, merr, lerrcnt[0], lerrcnt[1], lerrcnt[2], lerrcnt[3], lerrcnt[4], lerrcnt[5], lerrcnt[6], lerrcnt[7], test_time);
	}

	print_line4();
	for (i=0; i<8; i++) {
		errcnt[i] += lerrcnt[i];
	}

	if (merr > 0) {
		epp += 1 << mode;
	}
	return 0;
}


/****************************************************************************
 * Function: Memory Test - All
 ****************************************************************************/
int memtest_all(unsigned int test_start, unsigned int test_size, int sel, int lp) {
	int j, rc=0;
	long long int err_buf_addr = (long long int) &err_buf[0];
	werr = 0;
	epp  = 0;
	error_info(0, 0, 0);  // init to 0
	printMemTestHeader();

	//xil_printf("memtest_all: start=0x%08x size=0x%08x\r\n", test_start, test_size);

	*(volatile int *) (ERR_INFO+24) = err_buf_addr;

	for (j=0; j<250; j++) {
		err_buf[j] = 0;
	}
	for (j=0; j<8; j++) {
		errcnt[j] = 0;
	}

	if (sel & 0x0001) {
		rc = memtest(test_start, test_size, 0, lp);
	}
	if (sel & 0x0002) {
		rc = memtest_simple(test_start, test_size, 1, lp, 0,0,0,0);
	}
	if (sel & 0x0004) {
		rc = memtest_simple(test_start, test_size, 2, lp, 0xFFFFFFFFFFFFFFFF,0xFFFFFFFFFFFFFFFF,0xFFFFFFFFFFFFFFFF,0xFFFFFFFFFFFFFFFF);
	}
	if (sel & 0x0008) {
		rc = memtest_simple(test_start, test_size, 3, lp, 0xAAAAAAAAAAAAAAAA,0xAAAAAAAAAAAAAAAA,0xAAAAAAAAAAAAAAAA,0xAAAAAAAAAAAAAAAA);
	}
	if (sel & 0x0010) {
		rc = memtest_simple(test_start, test_size, 4, lp, 0x5555555555555555,0x5555555555555555,0x5555555555555555,0x5555555555555555);
	}
	if (bus_width == 64) {
		if (sel & 0x0020) {
			rc = memtest_simple(test_start, test_size, 5, lp, 0xFFFF0000FFFF0000,0xFFFF0000FFFF0000,0xFFFF0000FFFF0000,0xFFFF0000FFFF0000);
		}
		if (sel & 0x0040) {
			rc = memtest_simple(test_start, test_size, 6, lp, 0x0000FFFF0000FFFF,0x0000FFFF0000FFFF,0x0000FFFF0000FFFF,0x0000FFFF0000FFFF);
		}
		if (sel & 0x0080) {
			rc = memtest_simple(test_start, test_size, 7, lp, 0xAAAA5555AAAA5555,0xAAAA5555AAAA5555,0xAAAA5555AAAA5555,0xAAAA5555AAAA5555);
		}
		if (sel & 0x0100) {
			rc = memtest_simple(test_start, test_size, 8, lp, 0x5555AAAA5555AAAA,0x5555AAAA5555AAAA,0x5555AAAA5555AAAA,0x5555AAAA5555AAAA);
		}
	}
	else {
		if (sel & 0x0020) {
			rc = memtest_simple(test_start, test_size, 5, lp, 0, 0xFFFFFFFFFFFFFFFF, 0, 0xFFFFFFFFFFFFFFFF);
		}
		if (sel & 0x0040) {
			rc = memtest_simple(test_start, test_size, 6, lp, 0xFFFFFFFFFFFFFFFF, 0, 0xFFFFFFFFFFFFFFFF, 0);
		}
		if (sel & 0x0080) {
			rc = memtest_simple(test_start, test_size, 7, lp, 0x5555555555555555,0xAAAAAAAAAAAAAAAA,0x5555555555555555,0xAAAAAAAAAAAAAAAA);
		}
		if (sel & 0x0100) {
			rc = memtest_simple(test_start, test_size, 8, lp, 0xAAAAAAAAAAAAAAAA,0x5555555555555555,0xAAAAAAAAAAAAAAAA,0x5555555555555555);
		}
	}
	if (sel & 0x0200) {
		rc = memtest_pat128(test_start, test_size,  9, lp, &pat1[0]);
	}
	if (sel & 0x0400) {
		rc = memtest_pat128(test_start, test_size, 10, lp, &pat2[0]);
	}
	if (sel & 0x0800) {
		rc = memtest_lfsr(test_start, test_size, 11, lp);

	}
	if (sel & 0x1000) {
		rc = memtest_lfsr(test_start, test_size, 12, lp);
	}
	if (sel & 0x2000) {
		rc = memtest_lfsr(test_start, test_size, 13, lp);
	}
	if (sel & 0x4000) {
		rc = memtest_lfsr(test_start, test_size, 14, lp);
	}
	reg_write_64(ERR_INFO+32, epp);   // put epp in err_info+24
	return rc;
}




/****************************************************************************
 * Function: Upload Memory Test Results
 ****************************************************************************/
void upload_memtest_results() {
	int i, a;
	// save results to mailbox, limit to 16-bits
	for (i=0; i<8; i++) {
		a = errcnt[i];
		if (a > 0x0ffff) a = 0x0ffff;
		reg_write_32(MAILBOX_RESULT + 4*i, a);
	}
	a = werr;
	if (a > 0x0ffff) {
		a = 0x0ffff;
	}
	reg_write_32(MAILBOX_RESULT + 4*8, a);
}


/****************************************************************************
 * Function: Update Patterns
 ****************************************************************************/
void update_patterns() {
	int i, imaski;
	if (bus_width == 64) {
		for (i=0; i<128; i++) {
			imaski = (i >> 4) & 0x07;  // invert mask index
			pat1[i] = pattern_64bit[i & 15];
			pat2[i] = pattern_64bit[i & 15] ^ invertmask[imaski];
		}
	}
	else {
		for (i=0; i<128; i++) {
			imaski = (i >> 4) & 0x07;  // invert mask index
			pat1[i] = pattern_32bit[i & 15];
			pat2[i] = pattern_32bit[i & 15] ^ invertmask[imaski];
		}
	}
}

/****************************************************************************
 * Function: Check Bus Width
 ****************************************************************************/
int check_bus_width() {
	int val = 	reg_read_32(DDRC_MSTR_OFFSET);
	val = (val & DDRC_MSTR_DATA_BUS_WIDTH_MASK) >> DDRC_MSTR_DATA_BUS_WIDTH_SHIFT;
	if (val == 0) {
		bus_width = 64;
		//xil_printf("Bus width set to 64-bits\r\n");
	}
	else if (val == 1) {
		bus_width = 32;
		//xil_printf("Bus width set to 32-bits\r\n");
	}
	else {
		xil_printf("Error!  Bus width set to %d\r\n", val);
		return -1;
	}
	return 0;
}

/****************************************************************************
 * Function: Clear mail box regs
 ****************************************************************************/
void clear_mailbox_regs(void) {
	*(volatile unsigned int *) MAILBOX_GO = 0;
	*(volatile unsigned int *) MAILBOX_DONE = 0;
	*(volatile unsigned int *) MAILBOX_START = 1;
	*(volatile unsigned int *) MAILBOX_SIZE = 4;
	*(volatile unsigned int *) MAILBOX_MODE = 0x07FFF;
	*(volatile unsigned int *) MAILBOX_LAST = 0x08000;
}

/****************************************************************************
 * Function: TTC out of reset
 ****************************************************************************/
void ttc_out_of_reset(){
	unsigned int val = *(unsigned int *)0xFF5E0238;
	val = val & 0xFFFF87FF;
	*(unsigned int *)0xFF5E0238 = val;
};

/****************************************************************************
 * Function: Main
 ****************************************************************************/
extern int dramtest(void)
{
	int i, rc, imaski;
	int printerr=0;
	
	init_platform();
	ttc_out_of_reset();

	clear_mailbox_regs();

	check_bus_width();										// before doing anything, check buswidth

	pclk = read_lpd_lsbus_freq();

	
	read_ddrc_freq();
	ddr_config_params();


	err_buf[255] = (long long int) &textmsg[0];       // pointer to the text message

	// Create the 128-word patterns
	for (i=0; i<128; i++) {
		imaski = (i >> 4) & 0x07;  // invert mask index
		pat1[i] = pattern_64bit[i & 15];
		pat2[i] = pattern_64bit[i & 15] ^ invertmask[imaski];
	}

	while (1) {
		printf("\r\nStarting Memory Test - Testing 4GB length from address 0x0...\r\n");
		verbose = 1 + printerr*8;
		if(ddr_init) {
			disable_caches();
			memtest_all(0, 4096, 0x0001, test_loop_cnt);
			enable_caches();
		}
		else {
			memtest_all(0, 4096, 0xffff, test_loop_cnt);
		}
	}
	return rc;
}



