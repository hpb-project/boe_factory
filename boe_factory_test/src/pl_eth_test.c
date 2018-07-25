
/***************************** Include Files *********************************/
#include "xparameters.h"
#include "xil_exception.h"
#include "xstreamer.h"
#include "xil_cache.h"
#include "xllfifo.h"
#include "xstatus.h"

/***************** Macros (Inline Functions) Definitions *********************/

#define FIFO_DEV_ID	   	XPAR_AXI_FIFO_0_DEVICE_ID //XPAR_AXI_FIFO_1_DEVICE_ID

#define WORD_SIZE 4			/* Size of words in bytes */

#define MAX_PACKET_LEN 4

#define NO_OF_PACKETS 16

#define MAX_DATA_BUFFER_SIZE NO_OF_PACKETS*MAX_PACKET_LEN

#undef DEBUG

int fifotest(XLlFifo *InstancePtr, u16 DeviceId);
int TxSend(XLlFifo *InstancePtr, u8  *SourceAddr, int sendlen);
int RxReceive(XLlFifo *InstancePtr, u8 *DestinationAddr, int *getlen);

/************************** Variable Definitions *****************************/
/*
 * Device instance definitions
 */
XLlFifo FifoInstance;

u8 SourceBuffer[MAX_DATA_BUFFER_SIZE];
u8 DestinationBuffer[MAX_DATA_BUFFER_SIZE];

/*****************************************************************************/
/**
*
* Main function
*
* This function is the main entry of the Axi FIFO Polling test.
*
* @param	None
*
* @return
*		- XST_SUCCESS if tests pass
* 		- XST_FAILURE if fails.
*
* @note		None
*
******************************************************************************/
extern int pl_net_test()
{
	int Status;
	Status = fifotest(&FifoInstance, FIFO_DEV_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("pl eth fifo test failed\n\r");
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function demonstrates the usage AXI FIFO
* It does the following:
*       - Set up the output terminal if UART16550 is in the hardware build
*       - Initialize the Axi FIFO Device.
*	- Transmit the data
*	- Receive the data from fifo
*	- Compare the data
*	- Return the result
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo component.
* @param	DeviceId is Device ID of the Axi Fifo Deive instance,
*		typically XPAR_<AXI_FIFO_instance>_DEVICE_ID value from
*		xparameters.h.
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
******************************************************************************/
int fifotest(XLlFifo *InstancePtr, u16 DeviceId)
{
	XLlFifo_Config *Config;
	int Status;

	Status = XST_SUCCESS;
	xil_printf("Start XLlFifoPolling Example\r\n");

	/* Initialize the Device Configuration Interface driver */
	Config = XLlFfio_LookupConfig(DeviceId);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DeviceId);
		return XST_FAILURE;
	}

	/*
	 * This is where the virtual address would be used, this example
	 * uses physical address.
	 */
	Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed\n\r");
		return Status;
	}

	/* Check for the Reset value */
	Status = XLlFifo_Status(InstancePtr);
	XLlFifo_IntClear(InstancePtr,0xffffffff);
	Status = XLlFifo_Status(InstancePtr);
	if(Status != 0x0) {
		xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t"
			    "Expected : 0x0\n\r",
			    XLlFifo_Status(InstancePtr));
		return XST_FAILURE;
	}
	for(int i = 0; i < sizeof(SourceBuffer); i++){
		SourceBuffer[i] = i + 0x7;
	}
	int sendlen = sizeof(SourceBuffer);
	int getlen = 0;
	Status = TxSend(InstancePtr, DestinationBuffer, sendlen);
	if(Status != XST_SUCCESS){
		xil_printf("pl eth test txsend failed.\r\n");
		return Status;
	}
	Status = RxReceive(InstancePtr, DestinationBuffer, &getlen);
	if(Status != XST_SUCCESS){
		xil_printf("pl eth test receive failed.\r\n");
		return Status;
	}
	if(getlen != sendlen || (memcmp(DestinationBuffer, SourceBuffer, getlen) != 0)){
		xil_printf("pl eth test compare data failed.\r\n");
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* TxSend routine, It will send the requested amount of data at the
* specified addr.
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo component.
*
* @param	SourceAddr is the address where the FIFO stars writing
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
* @note		None
*
******************************************************************************/
int TxSend(XLlFifo *InstancePtr, u8  *SourceAddr, int sendlen)
{
	int i;
	u32 wBuf[512];
	u32 wlen;
	if(sendlen%WORD_SIZE == 0){
		wlen = sendlen/WORD_SIZE;
	}else{
		wlen = sendlen/WORD_SIZE + 1;
	}
	memset(wBuf, 0, sizeof(wBuf));
	memcpy((u8*)wBuf, SourceAddr, sendlen);

	for(i=0 ; i < wlen ; i++){
		/* Writing into the FIFO Transmit Port Buffer */
			if( XLlFifo_iTxVacancy(InstancePtr) ){
				XLlFifo_TxPutWord(InstancePtr,
					*(wBuf+i));
			}
	}

	/* Start Transmission by writing transmission length into the TLR */
	XLlFifo_iTxSetLen(InstancePtr, (wlen*WORD_SIZE));

	/* Check for Transmission completion */
	while( !(XLlFifo_IsTxDone(InstancePtr)) ){

	}

	/* Transmission Complete */
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* RxReceive routine.It will receive the data from the FIFO.
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo instance.
*
* @param	DestinationAddr is the address where to copy the received data.
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
* @note		None
*
******************************************************************************/
int RxReceive (XLlFifo *InstancePtr, u8* DestinationAddr, int *getlen)
{
	int i;
	u32 RxWord;
	u32 ReceiveLength;
	u32 *dest = (u32*)DestinationAddr;

	/* Read Recieve Length */
	if(XLlFifo_iRxOccupancy(InstancePtr) > 0){
		ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr));
		*getlen = ReceiveLength;
		ReceiveLength = (ReceiveLength%WORD_SIZE == 0) ? (ReceiveLength/WORD_SIZE) : (ReceiveLength/WORD_SIZE+1);

		/* Start Receiving */
		for ( i=0; i < ReceiveLength; i++){
			RxWord = XLlFifo_RxGetWord(InstancePtr);
			*(dest+i) = RxWord;
		}
	}

	return XST_SUCCESS;
}
