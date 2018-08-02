/*
 * atca_test.c
 *
 *  Created on: 2018年7月30日
 *      Author: luxq
 */
#include "xil_types.h"
#include "atca_iface.h"
#include "atca_cfgs.h"
ATCAIfaceCfg *gcfg = &cfg_ateccx08a_i2c_default;
uint8_t SerialNum[ATCA_SERIAL_NUM_SIZE];
uint8_t SRandom[32];

extern void at508_get_sernum(u8 *sernum)
{
	ATCA_STATUS status;
	atcab_init( gcfg );
	//status = atcab_read_serial_number( sernum );
	status = atcab_random(SRandom);
	atcab_release();
	char displayStr[ATCA_SERIAL_NUM_SIZE*3];
	if(status != ATCA_SUCCESS){
		xil_printf("atcab read serial number failed.\r\n");
		return ;
	}
	int displen = sizeof(displayStr);
	atcab_bin2hex(sernum,ATCA_SERIAL_NUM_SIZE,displayStr,&displen);
	atcab_printbin_label((const uint8_t*)"\r\n Serial Number from atcab is: ",sernum, ATCA_SERIAL_NUM_SIZE);

}
extern void at508_test(void)
{
	at508_get_sernum(SerialNum);
}
