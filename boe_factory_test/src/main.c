#include <stdio.h>
#include <sleep.h>
#include "platform.h"
#include "xplatform_info.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xstatus.h"
#include "led.h"
#include "axu_connector.h"
#include "community.h"
#include "runtest.h"

static u8 tx[256];
static u8 result = 0;
static u8 bLoop  = 1;
static MsgPoolHandle gMsgIns;
static int package_check(A_Package *pack)
{
    return (pack->magic == MAGIC) ? 0 : 1;
}

static void ledSuccess()
{
	while(1){
		ledHigh(LED_ALL);
		sleep(1);
		ledLow(LED_ALL);
		usleep(500000);
	}
}

static void ledFailed()
{
	while(1){
		ledHigh(LED_ALL);
		usleep(200000);
		ledLow(LED_ALL);
		usleep(100000);
	}
}

int mainloop(void)
{
	A_Package *send = (A_Package *)tx;
    int ret = 0;
    while(bLoop){
        A_Package *rcv = NULL;
        u32 wlen = 0;
        u32 timeout_ms = 0;
        if(0 != msg_pool_fetch(gMsgIns, &rcv, timeout_ms)) {
            // have no msg.
        	usleep(1000);
        }else {
            ret = package_check(rcv);
            if(0 == ret)
            {
                u16 cmd = rcv->cmd;
                if(cmd == REQ_CONNECT){
                	ret = 0;
                	// 连接成功，亮两个灯
                	ledHigh(LED_1 | LED_2);
                }else if(cmd == REQ_SD_TEST){
                	ret = ffs_sd_test();
                }else if(cmd == REQ_FLASH_TEST){
                	ret = flashtest();
                }else if(cmd == REQ_DRAM_TEST){
                	ret = memtest();
                }else if(cmd == REQ_NET_TEST){
                	ret = pl_net_test();
                }else if(cmd == REQ_ECC_TEST){
                	ret = at508_test();
                }else if(cmd == REQ_RESULT){
                	result = rcv->data[0];
                	ret = 0;
                }else if(cmd == REQ_FINISH){
                	ret = 0;
                	bLoop = 0;
                }

                if(ret == 0)
					wlen = make_package(rcv, send, RET_SUCCES);
				else
					wlen = make_package(rcv, send, RET_FAILED);
                msg_pool_txsend(gMsgIns, send, wlen+60);
            }else{
            	xil_printf("package check failed\r\n");
            }
        }
    }
    if(result == 0){
    	ledSuccess();
    }else{
    	ledFailed();
    }

    return 0;
}

void selftest()
{
	int status = 0;
	status = flashtest();
	if(status != XST_SUCCESS){
		xil_printf("flash test failed.\r\n");
		goto failed;
	}
	status = memtest();
	if(status != XST_SUCCESS){
		xil_printf("memtest failed.\r\n");
		goto failed;
	}
	status = at508_test();
	if(status != XST_SUCCESS){
		xil_printf("at508 test failed.\r\n");
		goto failed;
	}
#if 0
	status = ffs_sd_test();
	if(status != XST_SUCCESS){
		xil_printf("ffs sd test failed.\r\n");
		goto failed;
	}
#endif

failed:
	if(status == 0){
		ledSuccess();
	}else{
		ledFailed();
	}
	return ;
}
int main()
{
    int status = 0;
    init_platform();

    xil_printf("-----------------  Enter factory test -------------- \r\n");
    ledInit();
    status = msg_pool_init(&gMsgIns);
    if(status != XST_SUCCESS){
        xil_printf("fifo init failed.\n\r");
        return -1;
    }
    // 系统启动，亮第一个灯
    ledHigh(LED_1);

    // self test
    //selftest();

    // 3. enter mainloop.
    mainloop();
    while(1){
    	sleep(1);
    	xil_printf("Hello wwwwww.\r\n");
    }

    cleanup_platform();
    return 0;
}
