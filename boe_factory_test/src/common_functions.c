#include "defines.h"
#include "ddrc.h"
#include "ddr_phy.h"
#include "memtest.h"
#include <stdio.h>
#include <xil_printf.h>

#define REF_FREQ		33.33

#define IOPLL_CTRL	        0xFF5E0020
#define RPLL_CTRL		0xFF5E0030
#define APLL_CTRL		0xFD1A0020
#define	DPLL_CTRL		0xFD1A002C
#define	VPLL_CTRL		0xFD1A0038

#define	DDR_CTRL		0xFD1A0080

#define	PLL_FBDIV_SHIFT		8
#define	PLL_FBDIV_MASK		0x00007F00
#define	PLL_DIV2_SHIFT		16
#define PLL_DIV2_MASK		0x00010000

#define SOURCE_DIV0_SHIFT	8
#define SOURCE_DIV0_MASK	0x00003F00
#define SOURCE_SRCSEL_SHIFT	0
#define SOURCE_SRCSEL_MASK	0x00000007

#define LANE0MDLR0		DDR_PHY_DX0MDLR0
#define LANEOFFSET		0x100

#define PMCCNTR_EL0_EN	 	( 1 << 31 )
#define PMCCNTR_EL0_DIS	 	( 0 << 31 )
#define PMCR_EL0_EN		( 1 << 0 )

#define L1RADIS_SHIFT	25
#define RADIS_SHIFT	27

#define L1RADIS_4	0
#define L1RADIS_64	1
#define L1RADIS_128	2
#define L1RADIS_DISABLE	3
#define RADIS_16	0
#define RADIS_128	1
#define RADIS_512	2
#define RADIS_DISABLE	3

#define KBYTE		1024	
#define MBYTE		1024*1024
#define GBYTE		1024*1024*1024
#define MHZ		1000000.			

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))


unsigned int get_ddr_config_value(unsigned int ddr, unsigned int mask, unsigned int shift) {
	return ((*(volatile unsigned int*) (UINTPTR)ddr) & mask) >> shift;
}

void ddr_config_params() {
	unsigned int ddr_config__lanes__tmp = get_ddr_config_value(DDRC_MSTR, DDRC_MSTR_DATA_BUS_WIDTH_MASK, DDRC_MSTR_DATA_BUS_WIDTH_SHIFT);
	ddr_config__lanes = (ddr_config__lanes__tmp == 0) ? 8 : 4;
	ddr_config__device = get_ddr_config_value(DDRC_MSTR, DDRC_MSTR_DEVICE_CONFIG_MASK, DDRC_MSTR_DEVICE_CONFIG_SHIFT);	
	
	ddr_config__ranks = get_ddr_config_value(DDRC_MSTR, DDRC_MSTR_ACTIVE_RANKS_MASK, DDRC_MSTR_ACTIVE_RANKS_SHIFT);
	if(ddr_config__ranks == 1) {
		ddr_config__ranks = 1;
	}
	if(ddr_config__ranks == 3) {
		ddr_config__ranks = 2;
	}
	if(ddr_config__ranks == 8) {
		ddr_config__ranks = 4;
	}

}


double pll_freq(unsigned int ddr) {
	unsigned int val;
	unsigned int multiplier, div2;
	double freq;

	val = *(volatile unsigned int *) (UINTPTR)ddr;
	multiplier = (val & PLL_FBDIV_MASK) >> PLL_FBDIV_SHIFT;
	div2 = (val & PLL_DIV2_MASK) >> PLL_DIV2_SHIFT;

	if(div2) {
		freq = (REF_FREQ * multiplier) / (2.0);
	}
	else {
		freq = (REF_FREQ * multiplier);
	}
	return freq;
}

void read_ddrc_freq(void) {
	unsigned int ddr_ctrl_val, ddr_div0, ddr_srcsel;
	extern double ddr_freq;

	ddr_ctrl_val = *(volatile unsigned int *) DDR_CTRL;
	ddr_div0 = (ddr_ctrl_val & SOURCE_DIV0_MASK) >> SOURCE_DIV0_SHIFT;
	ddr_srcsel = (ddr_ctrl_val & SOURCE_SRCSEL_MASK) >> SOURCE_SRCSEL_SHIFT;

	if(ddr_srcsel == 0x00){
		ddr_freq = pll_freq(DPLL_CTRL) / (ddr_div0*1.0);
	}
	else if (ddr_srcsel == 0x01) {
		ddr_freq = pll_freq(VPLL_CTRL) / (ddr_div0*1.0);
	}
	else {
		xil_printf("Something wrong in ddr pll configuration.  Please reboot the system.\r\n");
	}

	//printf("DDR freq = %g\r\n", ddr_freq); 
	//return ddr_freq;
}

void printMemTestHeader(void)
{
      printf("---------+--------+------------------------------------------------+-----------\n");
      printf("  TEST   | ERROR  |          PER-BYTE-LANE ERROR COUNT             |  TIME\n");
      printf("         | COUNT  | LANES [ #0,  #1,  #2,  #3,  #4,  #5,  #6,  #7] |  (sec)\n");
      printf("---------+--------+------------------------------------------------+-----------\n");
}

void print_line(void) {
   printf("--------------------------------------------------------------------------------");
}


void print_line2(void) {
   printf("-------+--------+--------+--------+--------+--------+--------+--------+--------+\n");
}

void print_line3(void) {
   printf("---------+---------+---------+---------+---------+---------+---------+----------\n");
}

void print_line4(void) {
	printf("---------+--------+------------------------------------------------+-----------\n");
}


