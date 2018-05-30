/********************************************************************************
 * Author         : z00225974
 * Date           : 2012-9-21
 * Description    : ���߳̽�����������
 * Function List  : 
 * History        : 
 ********************************************************************************/
#include "denoise_maincall.h"
#include "Pthread_Denoise_maincall.h"

#include <pthread.h>

#ifdef _MSC_VER
#pragma comment(lib, "pthreadvc2.lib")
#endif

#define ThreadNum 4      //Y���߳���,ThreadNum>1
#define extPixel  16     //��ȡ�����أ��ϲ�ʱ����

#define HW_MEM_ALIGN(x,a) (((x)+((a)-1))&~((a)-1))         //��������Ϊ8��������

extern pthread_mutex_t yReadedLock;
extern pthread_mutex_t uReadedLock;
extern pthread_mutex_t vReadedLock;
extern INT32 yReaded ;
extern INT32 uReaded ;
extern INT32 vReaded ;
#if 0
//���߳���������
E_ERROR Nr_MaincallMix2P(HANDLE_NR *phHandle)
{
    INT32 i, j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С
    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 Delta;           //����ǰ��ߵĲ�

    UINT8* pDataTemp;
    UINT8* SliceTemp;    
     
    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������
    HANDLE_NR   *handle_NR[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum]      = {NULL};   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    HANDLE_NR   handle_U_NR;
    HANDLE_NR   handle_V_NR;   
    pthread_t pid[ThreadNum + 2];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    handle_U_NR = *phHandle;
    handle_V_NR = *phHandle;
    
    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    if(alignHeightPer != HeightPer)
    {
        HeightFir  = alignHeightPer + extPixel;
        HeightMid  = alignHeightPer + 2 * extPixel;
        HeightLast = HeightFir;        
    }
    else
    {
        HeightFir  = HeightPer + extPixel;
        HeightMid  = HeightPer + 2 * extPixel;
        HeightLast = HeightFir;        
    }

    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
    SizeExt  = extPixel   * stride;
    SizePer  = HeightPer  * stride;

    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (UINT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));
        pBuf += 512 * 4 * sizeof(UINT16);
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/4;
    
    pDataSlice[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);    
    pBuf += SizeFir;
    
    for(i = 1; i < ThreadNum - 1; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/4;
        
        pDataSlice[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
    }    
    PpBuf[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/4;
    
    pDataSlice[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;


    handle_U_NR.pBuf = pBuf;
    pBuf += stride*height/8;
    handle_V_NR.pBuf = pBuf;
    pBuf += stride*height/8;
    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < ThreadNum; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];
    }

    //=====��һ�����=====    
    //�޸��߳�Slice��height
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    
    //�޸��߳�Slice������Y����    
    memcpy(pDataSlice[0], pSRC[0], SizeFir);
    handle_NR[0]->stParamIn.pstPicIn->pData[0] = pDataSlice[0];
        
    //=====�м伸��=====
    for(i = 1; i < ThreadNum - 1; i ++)
    {
        handle_NR[i]->stParamIn.pstPicIn->height = HeightMid;
            
        memcpy(pDataSlice[i],pSRC[0] + (i * HeightPer - extPixel)* stride, SizeMid);
        handle_NR[i]->stParamIn.pstPicIn->pData[0] = pDataSlice[i];
    }

    //=====���һ�����=====    
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->height = HeightLast;
    
    if(alignHeightPer != HeightPer)
    {
        Delta = alignHeightPer - HeightPer;
        memcpy(pDataSlice[ThreadNum - 1], pSRC[0] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride, SizeLast);
    }
    else
    {
        memcpy(pDataSlice[ThreadNum - 1], pSRC[0] + ((ThreadNum - 1) * HeightPer - extPixel) * stride, SizeLast);
    }
    
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[0] = pDataSlice[ThreadNum - 1];


    //*************************  ���ú���ȥ��  *************************
    //����ȥ��
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_create(&pid[i], NULL, &Nr_MaincallMix2_Y, handle_NR[i]);
        //pthread_create(&pid[i], NULL, &Nr_Maincall_Y, handle_NR[i]);
    }
    //ɫ��ȥ��
    pthread_create(&pid[ThreadNum], NULL, &Nr_MaincallMix2_U, &handle_U_NR);
    pthread_create(&pid[ThreadNum + 1], NULL, &Nr_MaincallMix2_V, &handle_V_NR);

    for(i = 0; i < ThreadNum + 2; i++)
    {
        pthread_join(pid[i],NULL);
    }

    //****************************** �ϲ� ******************************
    //��һ���ΪHeightPer������
    memcpy(pSRC[0], pDataSlice[0], SizePer);    

    //ѭ�������м������
    pDataTemp = pSRC[0] + SizePer;
    for(j = 1; j < ThreadNum - 1; j++)
    {
        SliceTemp = pDataSlice[j] + SizeExt;
        memcpy(pDataTemp, SliceTemp, SizePer);
        pDataTemp += SizePer;
    }

    //���һ���ΪHeightPer������
    if(alignHeightPer != HeightPer)
    {
        SliceTemp = pDataSlice[ThreadNum - 1] + Delta * stride + SizeExt;
        memcpy(pDataTemp, SliceTemp, SizePer);
    }
    else
    {
        SliceTemp = pDataSlice[ThreadNum - 1] + SizeExt;
        memcpy(pDataTemp, SliceTemp, SizePer);
    }

    //****************************** ���� ******************************
//    for(i = 0; i < ThreadNum; i++)
//    {
//        if(NULL != handle_NR[i])free(handle_NR[i]);
//        if(NULL != pDataSlice[i])free(pDataSlice[i]);
//        if(NULL != PpBuf[i])free(PpBuf[i]);
//        if(NULL != pDnCoef[i])free(pDnCoef[i]);
//    }
    return eRet;
}
#endif


#if 0
//���߳���������
E_ERROR Nr_MaincallMix2P(HANDLE_NR *phHandle)
{
    INT32 i, j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 width = phHandle->stParamIn.pstPicIn->width;    //Դͼ��
    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С
    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 Delta;           //����ǰ��ߵĲ�

    UINT8* pDataTemp;
    UINT8* SliceTemp;    
     
    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������
    HANDLE_NR   *handle_NR[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
    HANDLE_NR   handle_U_NR;
    HANDLE_NR   handle_V_NR;
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stPictureOut[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    
    pthread_t pid[ThreadNum + 2];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    //printf("==========pSRC = %x\n", pSRC[0]);

    handle_U_NR = *phHandle;
    handle_V_NR = *phHandle;

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    if(alignHeightPer != HeightPer)
    {
        HeightFir  = alignHeightPer + extPixel;
        HeightMid  = alignHeightPer + 2 * extPixel;
        HeightLast = HeightFir;        
    }
    else
    {
        HeightFir  = HeightPer + extPixel;
        HeightMid  = HeightPer + 2 * extPixel;
        HeightLast = HeightFir;        
    }

    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
    SizeExt  = extPixel   * stride;    
    SizePer  = HeightPer  * stride;

    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (UINT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));
        pBuf += 512 * 4 * sizeof(UINT16);
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/2;
    
   // pDataSlice[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);    
    //pBuf += SizeFir;
    
    for(i = 1; i < ThreadNum - 1; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/2;
        
        //pDataSlice[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        //pBuf += SizeMid;
    }    
    
    PpBuf[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/2;
    
    ///pDataSlice[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    //pBuf += SizeLast;

    handle_U_NR.pBuf = pBuf;
    pBuf += stride*height/8;
    handle_V_NR.pBuf = pBuf;
    pBuf += stride*height/8;
    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < ThreadNum; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];
        
        stPictureOut[i].height                   = HeightPer;
        stPictureOut[i].width  = width;
        stPictureOut[i].stride  = stride;
        stPictureOut[i].pData[0] = pSRC[0] + (i * HeightPer)* stride;//pDataSlice[i];//
        handle_NR[i]->stParamIn.pstPicOut = &stPictureOut[i];
    }

    //=====��һ�����=====    
    //�޸��߳�Slice��height
    //input
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    handle_NR[0]->stParamIn.pstPicIn->pData[0]   = pSRC[0]; ////�޸��߳�Slice������Y����    
    handle_NR[0]->threadOffset = 0;
    handle_NR[0]->threadIdx = 0;
        
    //=====�м伸��=====
    for(i = 1; i < ThreadNum - 1; i ++)
    {
        handle_NR[i]->stParamIn.pstPicIn->height = HeightMid;
        handle_NR[i]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (i * HeightPer - extPixel)* stride;
        handle_NR[i]->threadOffset = extPixel* stride;
        handle_NR[i]->threadIdx = i;
    }

    //=====���һ�����=====    
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->height = HeightLast;
    Delta = alignHeightPer - HeightPer;
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[0] = pSRC[0] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadOffset = (Delta + extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadIdx = ThreadNum - 1;

    //*************************  ���ú���ȥ��  *************************
    //����ȥ��
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_create(&pid[i], NULL, &Nr_MaincallMix2_Y_, handle_NR[i]);
    }
    //ɫ��ȥ��
    pthread_create(&pid[ThreadNum], NULL, &Nr_MaincallMix2_U, &handle_U_NR);
    pthread_create(&pid[ThreadNum + 1], NULL, &Nr_MaincallMix2_V, &handle_V_NR);

    for(i = 0; i < ThreadNum + 2; i++)
    {
        pthread_join(pid[i],NULL);
    }
    

    //****************************** ���� ******************************
//    for(i = 0; i < ThreadNum; i++)
//    {
//        if(NULL != handle_NR[i])free(handle_NR[i]);
//        if(NULL != pDataSlice[i])free(pDataSlice[i]);
//        if(NULL != PpBuf[i])free(PpBuf[i]);
//        if(NULL != pDnCoef[i])free(pDnCoef[i]);
//    }
    return eRet;
}
#else

E_ERROR Nr_Thread_Y_U(HANDLE_NR_THREAD *phHandle) 
{
    HANDLE_NR *phYHandle = phHandle->phHandle[0];
    HANDLE_NR *phUHandle = phHandle->phHandle[1];
    E_ERROR err = SUCCESS;
        
    err = Nr_MaincallMix2_Y_(phYHandle);
    if(err != SUCCESS)
    return err;    // added by wanghao because warning 534
    err = Nr_MaincallMix2_U_(phUHandle);

    return err;
}

E_ERROR Nr_Thread_Y_V(HANDLE_NR_THREAD *phHandle) 
{
    HANDLE_NR *phYHandle = phHandle->phHandle[0];
    HANDLE_NR *phVHandle = phHandle->phHandle[1];

    E_ERROR err = SUCCESS;
    err = Nr_MaincallMix2_Y_(phYHandle);
    if(err != SUCCESS)
        return err;    // added by wanghao because warning 534

    err = Nr_MaincallMix2_V_(phVHandle);
    return err;
}

E_ERROR Nr_Thread_Y_U_night(HANDLE_NR_THREAD *phHandle) 
{
    HANDLE_NR *phYHandle = phHandle->phHandle[0];
    HANDLE_NR *phUHandle = phHandle->phHandle[1];

    E_ERROR err = SUCCESS;
    err = Nr_MaincallMix2_Y_(phYHandle);
    if(err != SUCCESS)
        return err;    // added by wanghao because warning 534
    err = Nr_MaincallMix2_U_night(phUHandle);

    return err;
}

E_ERROR Nr_Thread_Y_V_night(HANDLE_NR_THREAD *phHandle) 
{
    HANDLE_NR *phYHandle = phHandle->phHandle[0];
    HANDLE_NR *phVHandle = phHandle->phHandle[1];

    E_ERROR err = SUCCESS;    
    err = Nr_MaincallMix2_Y_(phYHandle);
    if(err != SUCCESS)
        return err;
    err = Nr_MaincallMix2_V_night(phVHandle);
    return err;
}

//���߳���������
/*
�ڴ�����:
[1]for ����
4*sizeof(HANDLE_NR) 
512 * 4 * sizeof(UINT16)
[2]for Y
SizeFir*3/2 + 2*SizeMid*3/2 + SizeLast*3/2
[3]fro uv
uvBufSize * 4 // (hDiv2 + extPixel) * strideDiv2 /2 or  (hDiv2 + extPixel) * strideDiv2 
*/
#if 0
E_ERROR Nr_MaincallMix2P(HANDLE_NR *phHandle)
{
    INT32 i;    //comment by wanghao because 529    , j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 width = phHandle->stParamIn.pstPicIn->width;    //Դͼ��
    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
//    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С    //comment by wanghao because 550:not accessed
//    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С    //comment by wanghao because 550:not accessed

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 alignHUVPer;
    INT32 Delta;           //����ǰ��ߵĲ�

    INT32 wDiv2 = width>>1;
    INT32 hDiv2   = height>>1;
    INT32 strideDiv2 = stride>>1;

    HANDLE_NR_THREAD handleThread[4];

//    UINT8* pDataTemp;    //comment by wanghao because 529
//    UINT8* SliceTemp;        //comment by wanghao because 529
     
//    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������    //comment by wanghao because 529
    HANDLE_NR   *handle_NR[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
    HANDLE_NR   handle_U_NR[2];
    HANDLE_NR   handle_V_NR[2];
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stCHaPictureIn[4];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stPictureOut[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stChaPictureOut[4];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    INT32  uvBufSize;
    
    pthread_t pid[ThreadNum];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    #if 1
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_in.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif
    

    //printf("==========pSRC = %x\n", pSRC[0]);

    handle_U_NR[0] = *phHandle;
    handle_U_NR[1] = *phHandle;
    handle_V_NR[0] = *phHandle;
    handle_V_NR[1] = *phHandle;

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    HeightFir  = alignHeightPer + extPixel;
    HeightMid  = alignHeightPer + 2 * extPixel;
    HeightLast = HeightFir;        

    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
//    SizeExt  = extPixel   * stride;    //comment by wanghao because 550:not accessed
//    SizePer  = HeightPer  * stride;    //comment by wanghao because 550:not accessed

    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (INT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));    //modified by wanghao because warning 64
        pBuf += 512 * 4 * sizeof(INT16);
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/2;
    
   // pDataSlice[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);    
    //pBuf += SizeFir;
    
    for(i = 1; i < ThreadNum - 1; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/2;
        
        //pDataSlice[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        //pBuf += SizeMid;
    }    
    
    PpBuf[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/2;
    
    ///pDataSlice[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    //pBuf += SizeLast;

    alignHUVPer = HW_MEM_ALIGN(hDiv2/2,4); // ��֤UV�ĸ߶�Ϊ4�ı���
    uvBufSize = (alignHUVPer + extPixel) * strideDiv2 /2;
    if(phHandle->bIsLittleImgNr)
    {
        uvBufSize = (alignHUVPer + extPixel) * strideDiv2; //С�ߴ�ͼ��ʹ���²�������;��neon�汾��˫�߲���Ҫ����
    }
    
    handle_U_NR[0].threadOffset = 0;
    stCHaPictureIn[0].pData[1]   = pSRC[1];
    stCHaPictureIn[0].height        = alignHUVPer + extPixel;
    stCHaPictureIn[0].stride         = strideDiv2;
    stCHaPictureIn[0].width         = wDiv2;
    handle_U_NR[0].stParamIn.pstPicIn = &stCHaPictureIn[0];
    stChaPictureOut[0].pData[1] = pSRC[1];
    stChaPictureOut[0].height     = hDiv2/2;
    handle_U_NR[0].stParamIn.pstPicOut= &stChaPictureOut[0];
    handle_U_NR[0].pBuf = pBuf;
    pBuf += uvBufSize ;

    handle_U_NR[1].threadOffset = (alignHUVPer -hDiv2/2 +extPixel)  * strideDiv2;
    stCHaPictureIn[1].pData[1]   = pSRC[1] + hDiv2 * strideDiv2/2 - handle_U_NR[1].threadOffset;
    stCHaPictureIn[1].height        = alignHUVPer + extPixel;
    stCHaPictureIn[1].stride         = strideDiv2;
    stCHaPictureIn[1].width         = wDiv2;
    handle_U_NR[1].stParamIn.pstPicIn = &stCHaPictureIn[1];
    stChaPictureOut[1].pData[1] = pSRC[1] +  hDiv2 * strideDiv2/2;
    stChaPictureOut[1].height     = hDiv2/2;
    handle_U_NR[1].stParamIn.pstPicOut= &stChaPictureOut[1];
    handle_U_NR[1].pBuf = pBuf;
    pBuf += uvBufSize;
    
    handle_V_NR[0].threadOffset = 0;
    stCHaPictureIn[2].pData[2]   = pSRC[2];
    stCHaPictureIn[2].height        = alignHUVPer + extPixel;
    stCHaPictureIn[2].stride         = strideDiv2;
    stCHaPictureIn[2].width         = wDiv2;
    handle_V_NR[0].stParamIn.pstPicIn = &stCHaPictureIn[2];
    stChaPictureOut[2].pData[2] = pSRC[2];
    stChaPictureOut[2].height     = hDiv2/2;
    handle_V_NR[0].stParamIn.pstPicOut= &stChaPictureOut[2];
    handle_V_NR[0].pBuf = pBuf;
    pBuf += uvBufSize;

    handle_V_NR[1].threadOffset = (alignHUVPer -hDiv2/2 +extPixel)  * strideDiv2;
    stCHaPictureIn[3].pData[2]   = pSRC[2] + hDiv2 * strideDiv2/2 - handle_V_NR[1].threadOffset;
    stCHaPictureIn[3].height        = alignHUVPer + extPixel;
    stCHaPictureIn[3].stride         = strideDiv2;
    stCHaPictureIn[3].width         = wDiv2;
    handle_V_NR[1].stParamIn.pstPicIn = &stCHaPictureIn[3];
    stChaPictureOut[3].pData[2] = pSRC[2] +  hDiv2 * strideDiv2/2;
    stChaPictureOut[3].height     = hDiv2/2;
    handle_V_NR[1].stParamIn.pstPicOut= &stChaPictureOut[3];
    handle_V_NR[1].pBuf = pBuf;
    pBuf += uvBufSize;
    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < ThreadNum; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];
        
        stPictureOut[i].height                   = HeightPer;
        stPictureOut[i].width  = width;
        stPictureOut[i].stride  = stride;
        stPictureOut[i].pData[0] = pSRC[0] + (i * HeightPer)* stride;//pDataSlice[i];//
        handle_NR[i]->stParamIn.pstPicOut = &stPictureOut[i];
    }

    //=====��һ�����=====    
    //�޸��߳�Slice��height
    //input
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    handle_NR[0]->stParamIn.pstPicIn->pData[0]   = pSRC[0]; ////�޸��߳�Slice������Y����    
    handle_NR[0]->threadOffset = 0;
    handle_NR[0]->threadIdx = 0;
        
    //=====�м伸��=====
    for(i = 1; i < ThreadNum - 1; i ++)
    {
        handle_NR[i]->stParamIn.pstPicIn->height = HeightMid;
        handle_NR[i]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (i * HeightPer - extPixel)* stride;
        handle_NR[i]->threadOffset = extPixel* stride;
        handle_NR[i]->threadIdx = i;
    }

    //=====���һ�����=====    
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->height = HeightLast;
    Delta = alignHeightPer - HeightPer;
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[0] = pSRC[0] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadOffset = (Delta + extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadIdx = ThreadNum - 1;

    //*************************  ���ú���ȥ��  *************************
    uReaded = 0;
    vReaded = 0;

    handleThread[0].phHandle[0] = handle_NR[0];
    handleThread[0].phHandle[1] = &handle_U_NR[0];
    
    handleThread[1].phHandle[0] = handle_NR[1];
    handleThread[1].phHandle[1] = &handle_U_NR[1];
    
    handleThread[2].phHandle[0] = handle_NR[2];
    handleThread[2].phHandle[1] = &handle_V_NR[0];    
    
    handleThread[3].phHandle[0] = handle_NR[3];
    handleThread[3].phHandle[1] = &handle_V_NR[1];
    
    pthread_mutex_init(&yReadedLock, NULL);
    pthread_mutex_init(&uReadedLock, NULL);
    pthread_mutex_init(&vReadedLock, NULL);
    
    pthread_create(&pid[0], NULL, &Nr_Thread_Y_U, &handleThread[0]);
    pthread_create(&pid[1], NULL, &Nr_Thread_Y_U, &handleThread[1]);
    pthread_create(&pid[2], NULL, &Nr_Thread_Y_V, &handleThread[2]);
    pthread_create(&pid[3], NULL, &Nr_Thread_Y_V, &handleThread[3]);
    
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_join(pid[i],NULL);
    }
    
    pthread_mutex_destroy(&yReadedLock);
    pthread_mutex_destroy(&uReadedLock);
    pthread_mutex_destroy(&vReadedLock);
    
    #if 1
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_out.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    return eRet;
}
#endif

//ͼ��������8�ı���
E_ERROR Nr_MaincallMix2P(HANDLE_NR *phHandle)
{
    INT32 i;    //comment by wanghao because 529    , j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 width = phHandle->stParamIn.pstPicIn->width;    //Դͼ��
    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    INT32 dstYH[4];

    INT32 uvHFir,uvHSec;
    INT32 dstUVH[2];
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
//    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С    //comment by wanghao because 550:not accessed
//    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С    //comment by wanghao because 550:not accessed

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 alignHUVPer;
    INT32 Delta;           //����ǰ��ߵĲ�

    INT32 wDiv2 = width>>1;
    INT32 hDiv2   = height>>1;
    INT32 strideDiv2 = stride>>1;

    HANDLE_NR_THREAD handleThread[4];

//    UINT8* pDataTemp;    //comment by wanghao because 529
//    UINT8* SliceTemp;        //comment by wanghao because 529
     
//    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������    //comment by wanghao because 529
    HANDLE_NR   *handle_NR[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
    HANDLE_NR   handle_U_NR[2];
    HANDLE_NR   handle_V_NR[2];
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stCHaPictureIn[4];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stPictureOut[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stChaPictureOut[4];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    INT32  uvBufSize;
    
    pthread_t pid[ThreadNum];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    #if 0
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_in.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif
    

    //printf("==========pSRC = %x\n", pSRC[0]);

    handle_U_NR[0] = *phHandle;
    handle_U_NR[1] = *phHandle;
    handle_V_NR[0] = *phHandle;
    handle_V_NR[1] = *phHandle;

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    dstYH[1] = alignHeightPer;
    dstYH[2] = alignHeightPer;
    dstYH[3] = alignHeightPer;
    
    HeightMid  = alignHeightPer + 2 * extPixel;
    HeightLast  = alignHeightPer + extPixel;

    dstYH[0] = (height - 3*alignHeightPer);
    HeightFir = HW_MEM_ALIGN(dstYH[0],8) + extPixel;

    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
    
    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (INT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));    //modified by wanghao because warning 64
        pBuf += 512 * 4 * sizeof(INT16);
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/2;
    
    for(i = 1; i < 3; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/2;
    }    
    
    PpBuf[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/2;

    alignHUVPer = HW_MEM_ALIGN(hDiv2/2,4); // ��֤UV�ĸ߶�Ϊ4�ı���

    dstUVH[0] = hDiv2 - alignHUVPer;//hDiv2 , alignHUVPer����4�ı��������Ա�֤dstUVH[0]Ҳ��4�ı���
    dstUVH[1] = alignHUVPer;

    uvHFir  = dstUVH[0] + extPixel;
    uvHSec = dstUVH[1] + extPixel;

    //�����ߴ����ڴ��С
    uvBufSize = uvHSec * strideDiv2 /2;
    if(phHandle->bIsLittleImgNr)
    {
        uvBufSize = uvHSec * strideDiv2; //С�ߴ�ͼ��ʹ���²�������;��neon�汾��˫�߲���Ҫ����
    }
    
    handle_U_NR[0].threadOffset = 0;
    stCHaPictureIn[0].pData[1]   = pSRC[1];
    stCHaPictureIn[0].height        = uvHFir;
    stCHaPictureIn[0].stride         = strideDiv2;
    stCHaPictureIn[0].width         = wDiv2;
    handle_U_NR[0].stParamIn.pstPicIn = &stCHaPictureIn[0];
    stChaPictureOut[0].pData[1] = pSRC[1];
    stChaPictureOut[0].height     = dstUVH[0];
    handle_U_NR[0].stParamIn.pstPicOut= &stChaPictureOut[0];
    handle_U_NR[0].pBuf = pBuf;
    pBuf += uvBufSize ;

    handle_U_NR[1].threadOffset = extPixel  * strideDiv2;
    stCHaPictureIn[1].pData[1]   = pSRC[1] + (dstUVH[0] - extPixel) * strideDiv2;
    stCHaPictureIn[1].height        = uvHSec;
    stCHaPictureIn[1].stride         = strideDiv2;
    stCHaPictureIn[1].width         = wDiv2;
    handle_U_NR[1].stParamIn.pstPicIn = &stCHaPictureIn[1];
    stChaPictureOut[1].pData[1] = pSRC[1] +  dstUVH[0] * strideDiv2;
    stChaPictureOut[1].height     = dstUVH[1];
    handle_U_NR[1].stParamIn.pstPicOut= &stChaPictureOut[1];
    handle_U_NR[1].pBuf = pBuf;
    pBuf += uvBufSize;
    
    
    stCHaPictureIn[2].pData[2]   = pSRC[2];
    stCHaPictureIn[2].height        = uvHFir;
    stCHaPictureIn[2].stride         = strideDiv2;
    stCHaPictureIn[2].width         = wDiv2;
    
    handle_V_NR[0].stParamIn.pstPicIn = &stCHaPictureIn[2];
    
    stChaPictureOut[2].pData[2] = pSRC[2];
    stChaPictureOut[2].height     = dstUVH[0];
    
    handle_V_NR[0].stParamIn.pstPicOut= &stChaPictureOut[2];
    handle_V_NR[0].threadOffset = 0;
    handle_V_NR[0].pBuf = pBuf;
    pBuf += uvBufSize;

    
    stCHaPictureIn[3].pData[2]   = pSRC[2] + (dstUVH[0] - extPixel) * strideDiv2;
    stCHaPictureIn[3].height        = uvHSec;
    stCHaPictureIn[3].stride         = strideDiv2;
    stCHaPictureIn[3].width         = wDiv2;
    
    handle_V_NR[1].stParamIn.pstPicIn = &stCHaPictureIn[3];
    
    stChaPictureOut[3].pData[2] = pSRC[2] +  dstUVH[0] * strideDiv2;
    stChaPictureOut[3].height     = dstUVH[1];

    handle_V_NR[1].stParamIn.pstPicOut= &stChaPictureOut[3];

    handle_V_NR[1].threadOffset = extPixel * strideDiv2;
    handle_V_NR[1].pBuf = pBuf;
    pBuf += uvBufSize;
    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < ThreadNum; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];
    }

    stPictureOut[0].height = dstYH[0];
    stPictureOut[0].width  = width;
    stPictureOut[0].stride  = stride;
    stPictureOut[0].pData[0] = pSRC[0];
    handle_NR[0]->stParamIn.pstPicOut = &stPictureOut[0];
    
    stPictureOut[1].height = dstYH[1];
    stPictureOut[1].width  = width;
    stPictureOut[1].stride  = stride;
    stPictureOut[1].pData[0] = pSRC[0] + dstYH[0] * stride;
    handle_NR[1]->stParamIn.pstPicOut = &stPictureOut[1];    

    stPictureOut[2].height = dstYH[2];
    stPictureOut[2].width  = width;
    stPictureOut[2].stride  = stride;
    stPictureOut[2].pData[0] = stPictureOut[1].pData[0] + dstYH[1] * stride;
    handle_NR[2]->stParamIn.pstPicOut = &stPictureOut[2];    

    stPictureOut[3].height = dstYH[3];
    stPictureOut[3].width  = width;
    stPictureOut[3].stride  = stride;
    stPictureOut[3].pData[0] = stPictureOut[2].pData[0] + dstYH[2] * stride;
    handle_NR[3]->stParamIn.pstPicOut = &stPictureOut[3];    

    
    //=====��һ�����=====    
    //�޸��߳�Slice��height
    //input
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    handle_NR[0]->stParamIn.pstPicIn->pData[0]   = pSRC[0]; ////�޸��߳�Slice������Y����    
    handle_NR[0]->threadOffset = 0;
    handle_NR[0]->threadIdx = 0;
        
    handle_NR[1]->stParamIn.pstPicIn->height = HeightMid;
    handle_NR[1]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (dstYH[0]- extPixel)* stride;
    handle_NR[1]->threadOffset = extPixel* stride;
    handle_NR[1]->threadIdx = 1;

    handle_NR[2]->stParamIn.pstPicIn->height = HeightMid;
    handle_NR[2]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (dstYH[0] + dstYH[1] - extPixel)* stride;
    handle_NR[2]->threadOffset = extPixel* stride;
    handle_NR[2]->threadIdx = 2;
    
    //=====���һ�����=====    
    handle_NR[3]->stParamIn.pstPicIn->height = HeightLast;
    handle_NR[3]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (dstYH[0] + dstYH[1]  + dstYH[2]  - extPixel) * stride;
    handle_NR[3]->threadOffset = extPixel * stride;
    handle_NR[3]->threadIdx = 3;

    //*************************  ���ú���ȥ��  *************************
    uReaded = 0;
    vReaded = 0;

    handleThread[0].phHandle[0] = handle_NR[0];
    handleThread[0].phHandle[1] = &handle_U_NR[0];
    
    handleThread[1].phHandle[0] = handle_NR[1];
    handleThread[1].phHandle[1] = &handle_U_NR[1];
    
    handleThread[2].phHandle[0] = handle_NR[2];
    handleThread[2].phHandle[1] = &handle_V_NR[0];    
    
    handleThread[3].phHandle[0] = handle_NR[3];
    handleThread[3].phHandle[1] = &handle_V_NR[1];
    
    pthread_mutex_init(&yReadedLock, NULL);
    pthread_mutex_init(&uReadedLock, NULL);
    pthread_mutex_init(&vReadedLock, NULL);
    
    pthread_create(&pid[0], NULL, &Nr_Thread_Y_U, &handleThread[0]);
    pthread_create(&pid[1], NULL, &Nr_Thread_Y_U, &handleThread[1]);
    pthread_create(&pid[2], NULL, &Nr_Thread_Y_V, &handleThread[2]);
    pthread_create(&pid[3], NULL, &Nr_Thread_Y_V, &handleThread[3]);
    
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_join(pid[i],NULL);
    }
    
    pthread_mutex_destroy(&yReadedLock);
    pthread_mutex_destroy(&uReadedLock);
    pthread_mutex_destroy(&vReadedLock);
    
    #if 0
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_out.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    return eRet;
}


E_ERROR Nr_MaincallMix_lowLight(HANDLE_NR *phHandle)
{
    INT32 i;    //comment by wanghao because 529, j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 width = phHandle->stParamIn.pstPicIn->width;    //Դͼ��
    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
//    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С    //comment by wanghao because 550:not accessed
//    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С    //comment by wanghao because 550:not accessed

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 Delta;           //����ǰ��ߵĲ�

    INT32 wDiv2 = width>>1;
    INT32 hDiv2   = height>>1;
    INT32 strideDiv2 = stride>>1;

    HANDLE_NR_THREAD handleThread[4];

//    UINT8* pDataTemp;    //comment by wanghao because 529
//    UINT8* SliceTemp;        //comment by wanghao because 529
     
//    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������    //comment by wanghao because 529
    HANDLE_NR   *handle_NR[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
    HANDLE_NR   handle_U_NR[2];
    HANDLE_NR   handle_V_NR[2];
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stCHaPictureIn[4];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stPictureOut[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stChaPictureOut[4];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_FASTDN2D *pstFastDn2d[ThreadNum];
    INT32  uvBufSize;
    
    pthread_t pid[ThreadNum];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    //printf("==========pSRC = %x\n", pSRC[0]);

    handle_U_NR[0] = *phHandle;
    handle_U_NR[1] = *phHandle;
    handle_V_NR[0] = *phHandle;
    handle_V_NR[1] = *phHandle;

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    if(alignHeightPer != HeightPer)
    {
        HeightFir  = alignHeightPer + extPixel;
        HeightMid  = alignHeightPer + 2 * extPixel;
        HeightLast = HeightFir;        
    }
    else
    {
        HeightFir  = HeightPer + extPixel;
        HeightMid  = HeightPer + 2 * extPixel;
        HeightLast = HeightFir;        
    }

    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
//    SizeExt  = extPixel   * stride;    //comment by wanghao because 550:not accessed
//    SizePer  = HeightPer  * stride;    //comment by wanghao because 550:not accessed

    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (INT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));
        pBuf += 512 * 4 * sizeof(INT16);
        
        pstFastDn2d[i] = (ST_FASTDN2D*)pBuf; //for UV
        pBuf +=  sizeof(ST_FASTDN2D);
        pstFastDn2d[i]->pPrevLine = (UINT16 *)pBuf;
        pBuf += MAX2(stride>>1, hDiv2/2 + extPixel) * sizeof(UINT16);//�����߶�HeightMid
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/2;
    
   // pDataSlice[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);    
    //pBuf += SizeFir;
    
    for(i = 1; i < ThreadNum - 1; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/2;
        
        //pDataSlice[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        //pBuf += SizeMid;
    }    
    
    PpBuf[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/2;
    
    ///pDataSlice[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    //pBuf += SizeLast;


    uvBufSize = (hDiv2 + extPixel) * strideDiv2 /2;
    if(phHandle->bIsLittleImgNr)
    {
        uvBufSize = (hDiv2 + extPixel) * strideDiv2; //С�ߴ�ͼ��ʹ���²�������;��neon�汾��˫�߲���Ҫ����
    }

    handle_U_NR[0].pstFastDn2d = pstFastDn2d[0];
    handle_U_NR[0].threadOffset = 0;
    stCHaPictureIn[0].pData[1]   = pSRC[1];
    stCHaPictureIn[0].height        = hDiv2/2 + extPixel;
    stCHaPictureIn[0].stride         = strideDiv2;
    stCHaPictureIn[0].width         = wDiv2;
    handle_U_NR[0].stParamIn.pstPicIn = &stCHaPictureIn[0];
    stChaPictureOut[0].pData[1] = pSRC[1];
    stChaPictureOut[0].height     = hDiv2/2;
    handle_U_NR[0].stParamIn.pstPicOut= &stChaPictureOut[0];
    handle_U_NR[0].pBuf = pBuf;
    pBuf += uvBufSize ;

    handle_U_NR[1].pstFastDn2d = pstFastDn2d[1];
    handle_U_NR[1].threadOffset = extPixel  * strideDiv2;
    stCHaPictureIn[1].pData[1]   = pSRC[1] + hDiv2 * strideDiv2/2 - handle_U_NR[1].threadOffset;
    stCHaPictureIn[1].height        = hDiv2/2 + extPixel;
    stCHaPictureIn[1].stride         = strideDiv2;
    stCHaPictureIn[1].width         = wDiv2;
    handle_U_NR[1].stParamIn.pstPicIn = &stCHaPictureIn[1];
    stChaPictureOut[1].pData[1] = pSRC[1] +  hDiv2 * strideDiv2/2;
    stChaPictureOut[1].height     = hDiv2/2;
    handle_U_NR[1].stParamIn.pstPicOut= &stChaPictureOut[1];
    handle_U_NR[1].pBuf = pBuf;
    pBuf += uvBufSize;

    handle_V_NR[0].pstFastDn2d = pstFastDn2d[2];
    handle_V_NR[0].threadOffset = 0;
    stCHaPictureIn[2].pData[2]   = pSRC[2];
    stCHaPictureIn[2].height        = hDiv2/2 + extPixel;
    stCHaPictureIn[2].stride         = strideDiv2;
    stCHaPictureIn[2].width         = wDiv2;
    handle_V_NR[0].stParamIn.pstPicIn = &stCHaPictureIn[2];
    stChaPictureOut[2].pData[2] = pSRC[2];
    stChaPictureOut[2].height     = hDiv2/2;
    handle_V_NR[0].stParamIn.pstPicOut= &stChaPictureOut[2];
    handle_V_NR[0].pBuf = pBuf;
    pBuf += uvBufSize;

    handle_V_NR[1].pstFastDn2d = pstFastDn2d[3];
    handle_V_NR[1].threadOffset = extPixel  * strideDiv2;
    stCHaPictureIn[3].pData[2]   = pSRC[2] + hDiv2 * strideDiv2/2 - handle_V_NR[1].threadOffset;
    stCHaPictureIn[3].height        = hDiv2/2 + extPixel;
    stCHaPictureIn[3].stride         = strideDiv2;
    stCHaPictureIn[3].width         = wDiv2;
    handle_V_NR[1].stParamIn.pstPicIn = &stCHaPictureIn[3];
    stChaPictureOut[3].pData[2] = pSRC[2] +  hDiv2 * strideDiv2/2;
    stChaPictureOut[3].height     = hDiv2/2;
    handle_V_NR[1].stParamIn.pstPicOut= &stChaPictureOut[3];
    handle_V_NR[1].pBuf = pBuf;
    pBuf += uvBufSize;

    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < ThreadNum; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];
        
        stPictureOut[i].height                   = HeightPer;
        stPictureOut[i].width  = width;
        stPictureOut[i].stride  = stride;
        stPictureOut[i].pData[0] = pSRC[0] + (i * HeightPer)* stride;//pDataSlice[i];//
        handle_NR[i]->stParamIn.pstPicOut = &stPictureOut[i];
    }

    //=====��һ�����=====    
    //�޸��߳�Slice��height
    //input
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    handle_NR[0]->stParamIn.pstPicIn->pData[0]   = pSRC[0]; ////�޸��߳�Slice������Y����    
    handle_NR[0]->threadOffset = 0;
    handle_NR[0]->threadIdx = 0;
        
    //=====�м伸��=====
    for(i = 1; i < ThreadNum - 1; i ++)
    {
        handle_NR[i]->stParamIn.pstPicIn->height = HeightMid;
        handle_NR[i]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (i * HeightPer - extPixel)* stride;
        handle_NR[i]->threadOffset = extPixel* stride;
        handle_NR[i]->threadIdx = i;
    }

    //=====���һ�����=====    
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->height = HeightLast;
    Delta = alignHeightPer - HeightPer;
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[0] = pSRC[0] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadOffset = (Delta + extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadIdx = ThreadNum - 1;

    //*************************  ���ú���ȥ��  *************************
    uReaded = 0;
    vReaded = 0;

    handleThread[0].phHandle[0] = handle_NR[0];
    handleThread[0].phHandle[1] = &handle_U_NR[0];
    
    handleThread[1].phHandle[0] = handle_NR[1];
    handleThread[1].phHandle[1] = &handle_U_NR[1];
    
    handleThread[2].phHandle[0] = handle_NR[2];
    handleThread[2].phHandle[1] = &handle_V_NR[0];    
    
    handleThread[3].phHandle[0] = handle_NR[3];
    handleThread[3].phHandle[1] = &handle_V_NR[1];
    
    pthread_mutex_init(&yReadedLock, NULL);
    pthread_mutex_init(&uReadedLock, NULL);
    pthread_mutex_init(&vReadedLock, NULL);
    
    pthread_create(&pid[0], NULL, &Nr_Thread_Y_U_night, &handleThread[0]);
    pthread_create(&pid[1], NULL, &Nr_Thread_Y_U_night, &handleThread[1]);
    pthread_create(&pid[2], NULL, &Nr_Thread_Y_V_night, &handleThread[2]);
    pthread_create(&pid[3], NULL, &Nr_Thread_Y_V_night, &handleThread[3]);
    
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_join(pid[i],NULL);
    }
    
    pthread_mutex_destroy(&yReadedLock);
    pthread_mutex_destroy(&uReadedLock);
    pthread_mutex_destroy(&vReadedLock);

    //****************************** ���� ******************************
//    for(i = 0; i < ThreadNum; i++)
//    {
//        if(NULL != handle_NR[i])free(handle_NR[i]);
//        if(NULL != pDataSlice[i])free(pDataSlice[i]);
//        if(NULL != PpBuf[i])free(PpBuf[i]);
//        if(NULL != pDnCoef[i])free(pDnCoef[i]);
//    }
    return eRet;
}

/*
�ڴ�����:
[1]for ����
4*sizeof(HANDLE_NR) 
4*512 * 4 * sizeof(UINT16)
4*sizeof(ST_FASTDN2D)
4*MAX2(stride>>1, HeightMid>>1) * sizeof(UINT16)
[2]for Y U V
SizeFir*3/2 + 2*SizeMid*3/2 + SizeLast*3/2
*/
#if 0
//ͼ����һ��Ҫ��16�ı���
E_ERROR Nr_MaincallMix_night2P(HANDLE_NR *phHandle)
{
    INT32 i;    //comment by wanghao because 529, j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 width = phHandle->stParamIn.pstPicIn->width;    //Դͼ��
    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
//    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С    //comment by wanghao because 550:not accessed
//    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С    //comment by wanghao because 550:not accessed

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 Delta;           //����ǰ��ߵĲ�

//    UINT8* pDataTemp;    //comment by wanghao because 529
//    UINT8* SliceTemp;        //comment by wanghao because 529
     
//    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������    //comment by wanghao because 529
    HANDLE_NR   *handle_NR[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
//    HANDLE_NR   handle_U_NR;    //comment by wanghao because 550:not accessed
//    HANDLE_NR   handle_V_NR;    //comment by wanghao because 550:not accessed
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stPictureOut[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_FASTDN2D *pstFastDn2d[ThreadNum];
    
    pthread_t pid[ThreadNum + 2];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    printf(">>=== pSRC[1] = %x pSRC[2] = %x\n",pSRC[1],pSRC[2]);

    #if 1
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_in.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


   // printf("==========pSRC = %x %x %x\n", pSRC[0], pSRC[1], pSRC[2]);

//    handle_U_NR = *phHandle;    //comment by wanghao because 550:not accessed
//    handle_V_NR = *phHandle;    //comment by wanghao because 550:not accessed

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    //Delta = alignHeightPer - HeightPer;
    //Delta = HW_MEM_ALIGN(Delta,4); 
    //alignHeightPer = HeightPer + Delta;
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    HeightFir  = alignHeightPer + extPixel;
    HeightMid  = alignHeightPer + 2 * extPixel;
    HeightLast = HeightFir;        


    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
//    SizeExt  = extPixel   * stride;    //comment by wanghao because 550:not accessed
//    SizePer  = HeightPer  * stride;    //comment by wanghao because 550:not accessed

    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (UINT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));
        pBuf += 512 * 4 * sizeof(UINT16);

        pstFastDn2d[i] = (ST_FASTDN2D*)pBuf;
        pBuf +=  sizeof(ST_FASTDN2D);
        pstFastDn2d[i]->pPrevLine = pBuf;
        pBuf += MAX2(stride>>1, HeightMid>>1) * sizeof(UINT16);//�����߶�HeightMid
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/2;
    
   // pDataSlice[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);    
    //pBuf += SizeFir;
    
    for(i = 1; i < ThreadNum - 1; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/2;
        
        //pDataSlice[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        //pBuf += SizeMid;
    }    
    
    PpBuf[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/2;
    
    ///pDataSlice[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    //pBuf += SizeLast;
    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < ThreadNum; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];

        handle_NR[i]->pstFastDn2d = pstFastDn2d[i];
        
        stPictureOut[i].height = HeightPer;
        stPictureOut[i].width  = width;
        stPictureOut[i].stride  = stride;
        stPictureOut[i].pData[0] = pSRC[0] + (i * HeightPer)* stride;//pDataSlice[i];//
        stPictureOut[i].pData[1] = pSRC[1] + (i * HeightPer)* stride/4;//pDataSlice[i];//
        stPictureOut[i].pData[2] = pSRC[2] + (i * HeightPer)* stride/4;//pDataSlice[i];//
        handle_NR[i]->stParamIn.pstPicOut = &stPictureOut[i];
    }

    //=====��һ�����=====    
    //�޸��߳�Slice��height
    //input
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    handle_NR[0]->stParamIn.pstPicIn->pData[0]   = pSRC[0]; ////�޸��߳�Slice������Y����    
    handle_NR[0]->stParamIn.pstPicIn->pData[1]   = pSRC[1]; //
    handle_NR[0]->stParamIn.pstPicIn->pData[2]   = pSRC[2]; //
    handle_NR[0]->threadOffset = 0;
    handle_NR[0]->threadIdx = 0;
        
    //=====�м伸��=====
    for(i = 1; i < ThreadNum - 1; i ++)
    {
        handle_NR[i]->stParamIn.pstPicIn->height = HeightMid;
        handle_NR[i]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (i * HeightPer - extPixel)* stride;
        handle_NR[i]->stParamIn.pstPicIn->pData[1] = pSRC[1] + (i * HeightPer - extPixel)* stride/4;
        handle_NR[i]->stParamIn.pstPicIn->pData[2] = pSRC[2] + (i * HeightPer - extPixel)* stride/4;
        handle_NR[i]->threadOffset = extPixel* stride;
        handle_NR[i]->threadIdx = i;
    }

    //=====���һ�����=====    
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->height = HeightLast;
    Delta = alignHeightPer - HeightPer;
    printf("===alignHeightPer = %d  , delta = %d\n",alignHeightPer,Delta);
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[0] = pSRC[0] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride;
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[1] = pSRC[1] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride/4;
    handle_NR[ThreadNum - 1]->stParamIn.pstPicIn->pData[2] = pSRC[2] + ((ThreadNum - 1) * HeightPer - Delta - extPixel) * stride/4;
    handle_NR[ThreadNum - 1]->threadOffset = (Delta + extPixel) * stride;
    handle_NR[ThreadNum - 1]->threadIdx = ThreadNum - 1;

    //*************************  ���ú���ȥ��  *************************
    //����ֿ鵥�߳�����̵߳Ľ����һ�£�����Ϊ���̵߳Ŀ�ʯ����ִ�У���һ����
    //���漸�вο��ĵ����������Ѿ�������ĵ㣻�����߳����ǲο�ԭʼ��;
#if 0
    yReaded = 4;
    Nr_Maincall_Night(handle_NR[0]);
    Nr_Maincall_Night(handle_NR[1]);
    Nr_Maincall_Night(handle_NR[2]);
    Nr_Maincall_Night(handle_NR[3]);    
#else
    yReaded = 0;
    uReaded = 0;
    vReaded = 0;
    
    pthread_mutex_init(&yReadedLock, NULL);
    pthread_mutex_init(&uReadedLock, NULL);
    pthread_mutex_init(&vReadedLock, NULL);
    
    for(i = 0; i < 4; i++)
    {
        pthread_create(&pid[i], NULL, &Nr_Maincall_Night, handle_NR[i]);
    }      

    for(i = 0; i < 4; i++)
    {
        pthread_join(pid[i],NULL);
    }
    
    pthread_mutex_destroy(&yReadedLock);
    pthread_mutex_destroy(&uReadedLock);
    pthread_mutex_destroy(&vReadedLock);
#endif

    #if 1
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_out.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    
    /*
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_create(&pid[i], NULL, &Nr_Maincall_Night, handle_NR[i]);
    }    */

    //****************************** ���� ******************************
//    for(i = 0; i < ThreadNum; i++)
//    {
//        if(NULL != handle_NR[i])free(handle_NR[i]);
//        if(NULL != pDataSlice[i])free(pDataSlice[i]);
//        if(NULL != PpBuf[i])free(PpBuf[i]);
//        if(NULL != pDnCoef[i])free(pDnCoef[i]);
//    }
    return eRet;
}
#endif

//ͼ��������8�ı���
E_ERROR Nr_MaincallMix_night2P(HANDLE_NR *phHandle)
{
    INT32 i;    //comment by wanghao because 529, j;
    
    UINT8 *pBuf  = phHandle->pBuf;

    INT32 width = phHandle->stParamIn.pstPicIn->width;    //Դͼ��
    INT32 height = phHandle->stParamIn.pstPicIn->height;    //Դͼ��
    INT32 stride = phHandle->stParamIn.pstPicIn->stride;    //Դͼ���

    UINT8* pSRC[3];
     
    INT32 HeightFir;       //��һ��Slice��    
    INT32 HeightMid;       //�м伸��Slice��
    INT32 HeightLast;      //���һ��Slice��
    INT32 HeightPer;       //ԭͼ��ΪThreadNum�飬ÿһ���
    INT32 dstHeight[4];
    
    INT32 SizeFir;         //��һ��Slice��С
    INT32 SizeMid;         //�м伸��Slice��С 
    INT32 SizeLast;        //���һ��Slice��С
//    INT32 SizePer;         //ԭͼ��ΪThreadNum�飬ÿһ��Ĵ�С    //comment by wanghao because 550:not accessed
//    INT32 SizeExt;         //��ΪextPixel�����ص��ص������С    //comment by wanghao because 550:not accessed

    INT32 alignHeightPer;  //ԭͼ��ΪThreadNum�飬��ÿһ�������Ϊ8������
    INT32 Delta;           //����ǰ��ߵĲ�

//    UINT8* pDataTemp;    //comment by wanghao because 529
//    UINT8* SliceTemp;        //comment by wanghao because 529
     
//    UINT8*      pDataSlice[ThreadNum]     = {NULL};   //���̷ֿ߳飬ָ��Y������    //comment by wanghao because 529
    HANDLE_NR   *handle_NR[4]     = {NULL};   //���̷ֿ߳飬ָ��Yȥ�����
//    HANDLE_NR   handle_U_NR;    //comment by wanghao because 550:not accessed
//    HANDLE_NR   handle_V_NR;    //comment by wanghao because 550:not accessed
    UINT8       *PpBuf[ThreadNum]         = {NULL};   //���̷ֿ߳�ʹ��
    INT16       *pDnCoef[ThreadNum]       = {NULL};   //���̷ֿ߳�ʹ��
    ST_PICTURE  stPicture[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_PICTURE  stPictureOut[ThreadNum];   //���̷ֿ߳�ʹ��,ָ��ͼ����� 
    ST_FASTDN2D *pstFastDn2d[ThreadNum];
    
    pthread_t pid[ThreadNum + 2];                     //�߳�ID
    
    E_ERROR  eRet = SUCCESS;

    pSRC[0] =  phHandle->stParamIn.pstPicIn->pData[0];
    pSRC[1] =  phHandle->stParamIn.pstPicIn->pData[1];
    pSRC[2] =  phHandle->stParamIn.pstPicIn->pData[2];

    //printf(">>=== pSRC[1] = %x pSRC[2] = %x\n",pSRC[1],pSRC[2]);

    #if 0
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_in.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


   // printf("==========pSRC = %x %x %x\n", pSRC[0], pSRC[1], pSRC[2]);

//    handle_U_NR = *phHandle;    //comment by wanghao because 550:not accessed
//    handle_V_NR = *phHandle;    //comment by wanghao because 550:not accessed

    //**************************** ��������  *******************************
    HeightPer      = height / ThreadNum;
    alignHeightPer = HW_MEM_ALIGN(HeightPer,8); 
    //Delta = alignHeightPer - HeightPer;
    //Delta = HW_MEM_ALIGN(Delta,4); 
    //alignHeightPer = HeightPer + Delta;
    
    //ԭͼ��ΪThreadNum�飬��ÿ��߶Ȳ�Ϊ8��������������
    //HeightFir  = alignHeightPer + extPixel;
    dstHeight[1] = alignHeightPer;
    dstHeight[2] = alignHeightPer;
    dstHeight[3] = alignHeightPer;
    
    HeightMid  = alignHeightPer + 2 * extPixel;
    HeightLast = alignHeightPer + extPixel;        

    dstHeight[0] = (height - 3*alignHeightPer);//��Ϊheight��alignHeightPer����ż��������  dstHeight[0]�ܹ���֤Ϊ2�ı���
    HeightFir    = HW_MEM_ALIGN(dstHeight[0],8) + extPixel;
    
    SizeFir  = HeightFir  * stride;
    SizeMid  = HeightMid  * stride;
    SizeLast = HeightLast * stride;    
//    SizeExt  = extPixel   * stride;    //comment by wanghao because 550:not accessed
//    SizePer  = HeightPer  * stride;    //comment by wanghao because 550:not accessed

    //****************************  �����ڴ�  ******************************
    for(i = 0; i < ThreadNum; i++)
    {
        handle_NR[i] = (HANDLE_NR *)pBuf;//(HANDLE_NR *)malloc(sizeof(HANDLE_NR));
        pBuf += sizeof(HANDLE_NR);
        
        pDnCoef[i]   = (INT16*)pBuf;//(UINT16*)malloc(512 * 4 * sizeof(UINT16));
        pBuf += 512 * 4 * sizeof(INT16);

        pstFastDn2d[i] = (ST_FASTDN2D*)pBuf;
        pBuf +=  sizeof(ST_FASTDN2D);
        pstFastDn2d[i]->pPrevLine = (UINT16 *)pBuf;
        pBuf += MAX2(stride>>1, HeightMid>>1) * sizeof(UINT16);//�����߶�HeightMid
    }

    PpBuf[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);
    pBuf += SizeFir;
    pBuf += SizeFir/2;
    pBuf += SizeFir + SizeFir/4;
    
   // pDataSlice[0] = (UINT8*)pBuf;//(UINT8*)malloc(SizeFir);    
    //pBuf += SizeFir;
    
    for(i = 1; i < 3; i++)
    {
        PpBuf[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        pBuf += SizeMid;
        pBuf += SizeMid/2;
        pBuf += SizeMid + SizeMid/4;
        
        //pDataSlice[i] = (UINT8*)pBuf;//(UINT8*)malloc(SizeMid);
        //pBuf += SizeMid;
    }    
    
    PpBuf[3] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    pBuf += SizeLast;
    pBuf += SizeLast/2;
    pBuf += SizeLast + SizeLast/4;
    
    ///pDataSlice[ThreadNum - 1] = (UINT8*)pBuf;//(UINT8*)malloc(SizeLast);
    //pBuf += SizeLast;
    
    //**************************** �޸Ĳ��� ******************************
    for(i = 0; i < 4; i ++)
    {
        //�������߳�Slice����    
        *handle_NR[i] = *phHandle;
        
        //�������߳�ͼ������ṹ��
        stPicture[i] = *(phHandle->stParamIn.pstPicIn);
        handle_NR[i]->stParamIn.pstPicIn = &stPicture[i];

        //�������߳�pBuf
        handle_NR[i]->pBuf = PpBuf[i];

        //�������߳�pDnCoef
        handle_NR[i]->pDnCoef = pDnCoef[i];
        handle_NR[i]->pstFastDn2d = pstFastDn2d[i];
    }

    //�������ͼ����Ϣ
    stPictureOut[0].height = dstHeight[0];
    stPictureOut[0].width  = width;
    stPictureOut[0].stride  = stride;
    stPictureOut[0].pData[0] = pSRC[0];
    stPictureOut[0].pData[1] = pSRC[1];
    stPictureOut[0].pData[2] = pSRC[2];
    handle_NR[0]->stParamIn.pstPicOut = &stPictureOut[0];    

    stPictureOut[1].height = dstHeight[1];
    stPictureOut[1].width  = width;
    stPictureOut[1].stride  = stride;
    stPictureOut[1].pData[0] = pSRC[0] + dstHeight[0] * stride;
    stPictureOut[1].pData[1] = pSRC[1] + dstHeight[0] * stride/4;
    stPictureOut[1].pData[2] = pSRC[2] + dstHeight[0] * stride/4;
    handle_NR[1]->stParamIn.pstPicOut = &stPictureOut[1];    

    stPictureOut[2].height = dstHeight[2];
    stPictureOut[2].width  = width;
    stPictureOut[2].stride  = stride;
    stPictureOut[2].pData[0] = stPictureOut[1].pData[0] + dstHeight[1] * stride;
    stPictureOut[2].pData[1] = stPictureOut[1].pData[1] + dstHeight[1] * stride/4;
    stPictureOut[2].pData[2] = stPictureOut[1].pData[2] + dstHeight[1] * stride/4;
    handle_NR[2]->stParamIn.pstPicOut = &stPictureOut[2];    

    stPictureOut[3].height = dstHeight[3];
    stPictureOut[3].width  = width;
    stPictureOut[3].stride  = stride;
    stPictureOut[3].pData[0] = stPictureOut[2].pData[0] + dstHeight[2] * stride;
    stPictureOut[3].pData[1] = stPictureOut[2].pData[1] + dstHeight[2] * stride/4;
    stPictureOut[3].pData[2] = stPictureOut[2].pData[2] + dstHeight[2] * stride/4;
    handle_NR[3]->stParamIn.pstPicOut = &stPictureOut[3];    
    
    //=====��һ�����=====    
    //�޸��߳�Slice��height
    //input
    handle_NR[0]->stParamIn.pstPicIn->height = HeightFir;
    handle_NR[0]->stParamIn.pstPicIn->pData[0]   = pSRC[0]; ////�޸��߳�Slice������Y����    
    handle_NR[0]->stParamIn.pstPicIn->pData[1]   = pSRC[1]; //
    handle_NR[0]->stParamIn.pstPicIn->pData[2]   = pSRC[2]; //
    handle_NR[0]->threadOffset = 0;
    handle_NR[0]->threadIdx = 0;

    handle_NR[1]->stParamIn.pstPicIn->height = HeightMid;
    handle_NR[1]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (dstHeight[0] - extPixel)* stride;
    handle_NR[1]->stParamIn.pstPicIn->pData[1] = pSRC[1] + (dstHeight[0] - extPixel)* stride/4;
    handle_NR[1]->stParamIn.pstPicIn->pData[2] = pSRC[2] + (dstHeight[0] - extPixel)* stride/4;
    handle_NR[1]->threadOffset = extPixel* stride;//ƫ�����е�������(extPixel�� stride)��������4�ı���
    handle_NR[1]->threadIdx = 1;

    handle_NR[2]->stParamIn.pstPicIn->height = HeightMid;
    handle_NR[2]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (dstHeight[0] + dstHeight[1] - extPixel)* stride;
    handle_NR[2]->stParamIn.pstPicIn->pData[1] = pSRC[1] + (dstHeight[0] + dstHeight[1] - extPixel)* stride/4;
    handle_NR[2]->stParamIn.pstPicIn->pData[2] = pSRC[2] + (dstHeight[0] + dstHeight[1] - extPixel)* stride/4;
    handle_NR[2]->threadOffset = extPixel* stride;//ƫ�����е�������(extPixel�� stride)��������4�ı���
    handle_NR[2]->threadIdx = 2;        

    handle_NR[3]->stParamIn.pstPicIn->height = HeightLast;
    handle_NR[3]->stParamIn.pstPicIn->pData[0] = pSRC[0] + (dstHeight[0] + dstHeight[1] + dstHeight[2] - extPixel)* stride;
    handle_NR[3]->stParamIn.pstPicIn->pData[1] = pSRC[1] + (dstHeight[0] + dstHeight[1] + dstHeight[2] - extPixel)* stride/4;
    handle_NR[3]->stParamIn.pstPicIn->pData[2] = pSRC[2] + (dstHeight[0] + dstHeight[1] + dstHeight[2] - extPixel)* stride/4;
    handle_NR[3]->threadOffset = extPixel* stride;//ƫ�����е�������(extPixel�� stride)��������4�ı���
    handle_NR[3]->threadIdx = 3;        

    //*************************  ���ú���ȥ��  *************************
    //����ֿ鵥�߳�����̵߳Ľ����һ�£�����Ϊ���̵߳Ŀ�ʯ����ִ�У���һ����
    //���漸�вο��ĵ����������Ѿ�������ĵ㣻�����߳����ǲο�ԭʼ��;
#if 0
    yReaded = 4;
    Nr_Maincall_Night(handle_NR[0]);
    Nr_Maincall_Night(handle_NR[1]);
    Nr_Maincall_Night(handle_NR[2]);
    Nr_Maincall_Night(handle_NR[3]);    
#else
    yReaded = 0;
    uReaded = 0;
    vReaded = 0;
    
    pthread_mutex_init(&yReadedLock, NULL);
    pthread_mutex_init(&uReadedLock, NULL);
    pthread_mutex_init(&vReadedLock, NULL);
    
    for(i = 0; i < 4; i++)
    {
        pthread_create(&pid[i], NULL, &Nr_Maincall_Night, handle_NR[i]);
    }      

    for(i = 0; i < 4; i++)
    {
        pthread_join(pid[i],NULL);
    }
    
    pthread_mutex_destroy(&yReadedLock);
    pthread_mutex_destroy(&uReadedLock);
    pthread_mutex_destroy(&vReadedLock);
#endif

    #if 0
    {
        FILE *pfYUV = fopen("/data/improc/new/yuv_out.yuv","wb");
        
        fwrite(pSRC[0],1,width*height,pfYUV);
        fwrite(pSRC[1],1,width*height/4,pfYUV);
        fwrite(pSRC[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    
    /*
    for(i = 0; i < ThreadNum; i++)
    {
        pthread_create(&pid[i], NULL, &Nr_Maincall_Night, handle_NR[i]);
    }    */

    //****************************** ���� ******************************
//    for(i = 0; i < ThreadNum; i++)
//    {
//        if(NULL != handle_NR[i])free(handle_NR[i]);
//        if(NULL != pDataSlice[i])free(pDataSlice[i]);
//        if(NULL != PpBuf[i])free(PpBuf[i]);
//        if(NULL != pDnCoef[i])free(pDnCoef[i]);
//    }
    return eRet;
}


#endif

