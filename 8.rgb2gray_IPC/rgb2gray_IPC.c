/* 
 *  ======== rgb2gray_IPC.c ========
 *  ����һ��ͼ���ɫת�ҶȵĶ��DSP����
 *  ���ֳ���ͼ���Ϊ�˿飬C6678ÿ���˲�������һ��ͼ��
 *  ���ˣ���0����ͼ���ڴ���빲���ڴ棬���Ӻ˵ȴ����˽��ڴ����󣬿�ʼ��������
 *  �����Ӻ����ͼ���ת�������˽����ݴ洢��������ڴ��ͷ�
 */

#include<xdc/std.h>
#include<stdio.h>
#include<stdlib.h>

/*  -----------------------------------XDC.RUNTIME module Headers    */
#include <xdc/runtime/System.h>
#include <ti/sysbios/hal/Cache.h>

/*  ----------------------------------- IPC module Headers           */
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/Notify.h>
#include <xdc/runtime/IHeap.h>
#include <ti/ipc/Ipc.h>
/*  ----------------------------------- BIOS6 module Headers         */
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/BIOS.h>

/*  ----------------------------------- To get globals from .cfg Header */
#include <xdc/cfg/global.h>
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/HeapBufMP.h>
#include <xdc/runtime/Memory.h>
#include <ti/ipc/SharedRegion.h>

#define INTERRUPT_LINE  0

/* ������ɫ */
#define CV_BGR2GRAY 0

/* �������� */
#define masterProc 0
#define BufEVENT 10
#define heapId 0

#define coreNum 8
#define dateNum 16 //12900 16  //8 //
UInt16 recSlvNum = 0; // ��ʾ���˽��յ��ĴӺ˻ظ�����

UInt16 srcProc, dstProc;
unsigned char *inBuf=NULL;
unsigned char *outBuf=NULL;

String CoreName[8]={"MessageQ_Core0","MessageQ_Core1","MessageQ_Core2","MessageQ_Core3",
		"MessageQ_Core4","MessageQ_Core5","MessageQ_Core6","MessageQ_Core7"};


typedef struct MyMsg {
	MessageQ_MsgHeader header;
	SharedRegion_SRPtr inBuf_SRPtr;
	SharedRegion_SRPtr outBuf_SRPtr;
} MyMsg;

/*
 *  ======== AcCvtColor ========
 *  ����һ��ͼ��RGBת��Ϊ�Ҷ�ͼ��ĳ���
 *  Image1��RGBͼ��ÿ����ɫͨ��Ϊ8λ��������ɫ��������
 *  Image2�ǻҶ�ͼ��
 *  coder��ʾת��ģʽ����ҪΪ�Ժ���չ.
 */

void AcCvtColor(unsigned char* image1,unsigned char* image2,int coder)
{
	Int red,green,blue,gray;
	Int coreId=MultiProc_self();
	if(coder==CV_BGR2GRAY)
	{
		Int BlockSize=dateNum/coreNum;
		Int i;
		for(i=coreId*BlockSize;i<(coreId+1)*BlockSize;i++)
			{
				red=image1[i*3];
				green=image1[i*3+1];
				blue=image1[i*3+2];
				gray=(red*76+green*150+blue*30)>>8;
				image2[i]=(unsigned char)gray;
			}
	}
}

/*
 *  ======== cbFxn ========
 *  ����Notifyģ���ע�ắ��
 *  procId��ʾ����ע�ắ���ĺ�ID������˵���¼��Ǵ��ĸ�������
 */
static UInt16 recCoreNum=0;
Void cbFxn(UInt16 procId, UInt16 lineId,
           UInt32 eventId, UArg arg, UInt32 payload)
{
	if(procId!=0)
	{
		recCoreNum++;
		if(recCoreNum==7)
			Semaphore_post(semHandle);
	}else{
		Semaphore_post(semHandle);
	}
}

/*
 *  ======== tsk0_func ========
 *  Sends an event to the next processor then pends on a semaphore.
 *  The semaphore is posted by the callback function.
 */
Void tsk0_func(UArg arg0, UArg arg1)
{
    MyMsg     *msg;            // ��Ϣ
    MessageQ_Handle  messageQ;       // ��Ϣ���о��
    MessageQ_Params  messageQParams; // ��Ϣ���в���

    HeapBufMP_Handle heapHandle;  // �Ѿ��
    HeapBufMP_Params heapBufParams;  // �Ѳ���

    SharedRegion_SRPtr inBuf_srptr;  // ���뻺�����ڹ��������ָ��
    SharedRegion_SRPtr outBuf_srptr; // ����������ڹ��������ָ��


    int coreId=MultiProc_self();

    if (coreId != 0){  // �Ӻˣ�������Ϣ����
    	MessageQ_Params_init(&messageQParams);
    	messageQ = MessageQ_create(CoreName[coreId], &messageQParams);
    	if(messageQ==NULL)
    		System_printf("MessageQ core%d create is failed \n",coreId);
    	System_printf("MessageQ core%d create is finished \n",coreId);
    }else{
    	// �����ѣ�����Ϣ����ռ�
        HeapBufMP_Params_init(&heapBufParams);
        heapBufParams.regionId       = 0;
        heapBufParams.name           = "Msg_Heap";
        heapBufParams.numBlocks      = 1;             // 1����
        heapBufParams.blockSize      = sizeof(MyMsg); // ��Ĵ�С�պ���һ����Ϣ����
        heapHandle = HeapBufMP_create(&heapBufParams);
        if (heapHandle == NULL) {
            System_abort("HeapBufMP_create failed\n" );
        }

        int status = MessageQ_registerHeap(heapHandle,heapId);
        if (status == MessageQ_E_ALREADYEXISTS){
        	System_abort("heap already exists with heapId\n" );
        }
	    System_printf("MessageQ registerHeap finished\n");
        msg = (MyMsg*)MessageQ_alloc(heapId, sizeof(MyMsg));
        if (msg == NULL) {
        	System_abort("MessageQ_alloc failed\n");
        }
        System_printf("MessageQ alloc finished\n");
    }
    // �����ڴ�
    if (MultiProc_self() == 0) {

    	Int i;

    	inBuf = (unsigned char*)Memory_alloc(SharedRegion_getHeap(0), dateNum*3, 0, NULL);
    	outBuf = (unsigned char*)Memory_alloc(SharedRegion_getHeap(0), dateNum, 0, NULL);

    	if(inBuf==NULL||outBuf==NULL)
    	{
    		System_printf("malloc Buf failed\n");
    		BIOS_exit(0);
    	}


    	// ���ļ�
    	/*
    	FILE  *fp=NULL;
    	fp=fopen("E:\\Code\\CCS\\workspace_v5_2\\rgb2gray_IPC\\test.rgb","rb");
    	if(fp==NULL)
    		printf("open file failed\n");
    	fread(inBuf,dateNum*3,1,fp); // ��ͼ�����ݶ������뻺����
*/


    	for(i=0;i<dateNum;i++){  // ����dateNum=16
    		inBuf[i*3]=i*10+10;
    		inBuf[i*3+1]=i*10+30;
    		inBuf[i*3+2]=i*10+50;
    	}


    	inBuf_srptr = SharedRegion_getSRPtr(inBuf, 0);
    	outBuf_srptr = SharedRegion_getSRPtr(outBuf, 0);

    	msg->inBuf_SRPtr = inBuf_srptr;  // ��Ϣ���
    	msg->outBuf_SRPtr = outBuf_srptr;

    	Cache_wbAll();// Write back all caches();

    	//System_printf("finish msg config\n");

    	System_printf("core%d inBuf address 0x%x\n",coreId,inBuf);
    	System_printf("core%d outBuf address 0x%x\n",coreId,outBuf);

    	for(i=1;i<8;i++){  // 7����Ϣ���ж���Ҫ������Ϣ
    	    MessageQ_QueueId* sloverCoreQueueId;
    	    int status;
    	    do{ // �ȴ���ֱ����MessageQ
    	    	status= MessageQ_open(CoreName[i], sloverCoreQueueId);
    	    	if (status < 0)
    	    	    Task_sleep(1);
    	    }while (status < 0);
    	    status = MessageQ_put(*sloverCoreQueueId, (MessageQ_Msg)msg);
    	    if (status < 0)
    	    	System_abort("MessageQ_put was not successful\n");

    	    do{ // �ȴ���ֱ���ر�MessageQ
    	        status= MessageQ_close(sloverCoreQueueId);
    	        if (status < 0)
    	        	Task_sleep(1);
    	    }while (status < 0);
    	}

    	Cache_disable(Cache_Type_ALL);

    	AcCvtColor(inBuf,outBuf,CV_BGR2GRAY); // �ֺ����ͼ��Ҷ�ת������

    	Cache_wbAll(); // Write back all caches();

    	Semaphore_pend(semHandle, BIOS_WAIT_FOREVER); // �ȴ��Ӻ����ȫ������

    	Cache_inv(outBuf,sizeof(outBuf),Cache_Type_ALL,TRUE);
    	Cache_wait();

    	System_printf("outBuf date is ");
    	for(i=0;i<dateNum;i++)
    		System_printf("%d ",outBuf[i]);
    	System_printf("\n");

/* д��ͼ������
    	// �������ͼ��
    	FILE  *fq=NULL;
    	fq=fopen("E:\\Code\\CCS\\workspace_v5_2\\rgb2gray_IPC\\test.rgb","wb");
    	if(fq==NULL)
    		printf("write file failed\n");
    	fwrite(outBuf,sizeof(outBuf),1,fq);
    	*/

    	for(i=1;i<8;i++)
    		Notify_sendEvent(i, INTERRUPT_LINE, BufEVENT, 0, TRUE);


    }else{

    	int status = MessageQ_get(messageQ, (MessageQ_Msg *)&msg, MessageQ_FOREVER);
    	if (status < 0)
    		System_abort("This should not happen since timeout is forever\n");
    	inBuf_srptr = msg->inBuf_SRPtr;
    	outBuf_srptr = msg->outBuf_SRPtr;

    	System_printf("MessageQ get is finished \n");

    	Cache_disable(Cache_Type_ALL);

    	/*
    	status = MessageQ_free((MessageQ_Msg)msg);
    	if (status < 0)
    		System_abort("Message free failed\n");

    	System_printf("MessageQ free is finished \n");
    	*/

    	/*
    	status = MessageQ_delete(&messageQ);
    	if (status < 0)
    		System_abort("MessageQ delete failed\n");

    	System_printf("MessageQ delete is finished \n");
    	*/

    	inBuf = SharedRegion_getPtr(inBuf_srptr);
    	outBuf = SharedRegion_getPtr(outBuf_srptr);

    	//System_printf("core%d inBuf address 0x%x\n",coreId,inBuf);
    	//System_printf("core%d outBuf address 0x%x\n",coreId,outBuf);

    	AcCvtColor(inBuf,outBuf,CV_BGR2GRAY); // �ֺ����ͼ��Ҷ�ת������

    	Cache_wbAll(); // Write back all caches();

    	//System_printf("core%d : indate is %d,outdate is %d\n",coreId,inBuf[coreId*3],outBuf[coreId]);

    	// Write back all caches();
    	status = Notify_sendEvent(0, INTERRUPT_LINE, BufEVENT, 0, TRUE);

    	//System_printf("core%d sendEvent finished,status is %d\n",coreId,status);
        if (status < 0) {
            System_abort("sendEvent to MasterCore failed\n");
        }

        Semaphore_pend(semHandle, BIOS_WAIT_FOREVER); // �ȴ��Ӻ����ȫ������
    }
    // ����������������˳���
    System_printf("Rgb2Gray is finished\n");
    BIOS_exit(0);

}

/*
 *  ======== main ========
 *  Synchronizes all processors (in Ipc_start), calls BIOS_start, and registers 
 *  for an incoming event
 */
Int main(Int argc, Char* argv[])
{
    Int status;
    
    /*  
     *  Ipc_start() calls Ipc_attach() to synchronize all remote processors
     *  because 'Ipc.procSync' is set to 'Ipc.ProcSync_ALL' in *.cfg
     */
    status = Ipc_start();
    if (status < 0) {
        System_abort("Ipc_start failed\n");
    }

    if (MultiProc_self() == 0) {
    	Int i;
    	for(i=1;i<8;i++)
    		status = Notify_registerEvent(i, INTERRUPT_LINE, BufEVENT,
    	    	    				(Notify_FnNotifyCbck)cbFxn, NULL);
    }else{
    	// �Ӻ�����¼�ע��
    	status = Notify_registerEvent(0, INTERRUPT_LINE, BufEVENT,
    	    				(Notify_FnNotifyCbck)cbFxn, NULL);

    	System_printf("registerEvent status is %d\n",status);

    }

    BIOS_start();

    return (0);
}

/*
 */
/*
 *  @(#) ti.sdo.ipc.examples.multicore.evm667x; 1, 0, 0, 0,1; 5-22-2012 16:36:06; /db/vtree/library/trees/ipc/ipc-h32/src/ xlibrary

 */
