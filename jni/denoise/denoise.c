/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_DeNoise_V1.0
 * Date           : 2012-6-26
 * Description    : ʵ��DeNoiseģ��Ĵ��������к�����
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "denoise.h"
#include "denoise_maincall.h" 
#include "MemManage.h"
#include "Pthread_Denoise_maincall.h"

/********************************************************************************
 * Function Description:       
      1.  У���������,����ֻУ���ļ��������������У������Run  
 * Parameters:          
      > pstParamIn        - [in]  ģ���������    
 ********************************************************************************/
static E_ERROR Nr_CheckParams(ST_NR_IN *pstParamIn)
{
    ST_PICTURE  *pstPicIn   = pstParamIn->pstPicIn;
    
    if(NULL == pstParamIn)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "input param = null!");
        return ERROR_PARAM;
    }

    // Ŀǰ��֧��YUV420���룬������׼YUV420�͸�ͨYUV420
    if((PIC_TYPE_YUV420_QUAL == pstPicIn->ePicType) || ((PIC_TYPE_YUV420 == pstPicIn->ePicType)))
    {
        if((PIC_TYPE_YUV420_QUAL == pstPicIn->ePicType) && ((NULL == pstPicIn->pData[0]) || (NULL == pstPicIn->pData[1])))
        {
            CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "No input image!");
            return ERROR_PARAM;
        }
        else if((PIC_TYPE_YUV420 == pstPicIn->ePicType) 
            && (NULL == pstPicIn->pData[0]) || (NULL == pstPicIn->pData[1]) || (NULL == pstPicIn->pData[2])) //PIC_TYPE_YUV420
        {
            CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "No input image!");
            return ERROR_PARAM;
        }
    }
    else
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "can't support this picture type");
        return ERROR_PARAM;
    }

    // YUV���У��
    if(pstPicIn->width < MIN_WIDTH || pstPicIn->width > MAX_WIDTH)
    {  
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, 
            "width[%d] out of range[%d~%d]!\n", pstPicIn->width,MIN_WIDTH,MAX_WIDTH);
        return ERROR_PARAM;
    }

    if(pstPicIn->height < MIN_HEIGHT || pstPicIn->height > MAX_HEIGHT)
    {  
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, 
            "height[%d] out of range[%d~%d]!\n", pstPicIn->height,MIN_HEIGHT,MAX_HEIGHT);
        return ERROR_PARAM;
    }
    
    
    if(pstPicIn->stride < MIN_WIDTH || pstPicIn->stride > MAX_STRIDE || pstPicIn->stride < pstPicIn->width)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, 
            "stride[%d] out of range[%d~%d]!\n", pstPicIn->stride,MIN_WIDTH,MAX_STRIDE);
        return ERROR_PARAM;
    }

    if(0 != pstPicIn->width % MULTIPLE_YUV_W || 0 != pstPicIn->stride % MULTIPLE_YUV_W)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, 
            "width[%d] or height[%d] or stride[%d] is not multiple of %d!", 
            pstPicIn->width, pstPicIn->height, pstPicIn->stride, MULTIPLE_YUV_W);
        return ERROR_PARAM;
    }
    
    if(0 != pstPicIn->height % MULTIPLE_YUV_H)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, 
            "width[%d] or height[%d] or stride[%d] is not multiple of %d!", 
            pstPicIn->width, pstPicIn->height, pstPicIn->stride, MULTIPLE_YUV_H);
        return ERROR_PARAM;
    }

    return SUCCESS;
    
}


/********************************************************************************
 * Function Description:       
      1.  �ж���������Ƿ��޸�  
 * Parameters:          
      > x                 - [in]  x    
 ********************************************************************************/
static BOOL IsEqual(void *pstParamOld, void *pstParamNew, INT32 structSize)
{
    if(0 == memcmp(pstParamOld, pstParamNew, structSize))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}



/********************************************************************************
 * Function Description:       
      1.DeNoiseģ�� run�����ľ������   
 * Parameters:          
      > x                 - [in]  x    
 ********************************************************************************/
INT32 Nr_Run(API *pApi)
{
    HANDLE_NR *phNr   = NULL;
    BOOL            bIsParamEqual;
    E_ERROR      eRet  = SUCCESS;
    
    // ================����У��(1������������Ƿ���ȷ; 2����������Ƿ��в�����Ķ���3��)===============
    if(NULL == pApi)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "Nr_Run error: api=NULL!");
        return ERROR_PARAM;
    }

    phNr = (HANDLE_NR *)pApi->handle;
    if(NR_HANDLE_KEY != phNr->handleKey)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "Nr_Run error: wrong handleKey!");
        return ERROR_PARAM;
    }

    // У�鲢���²�����Ŀǰ�ݲ�֧��������������仯
    bIsParamEqual = IsEqual((void*)(&phNr->stParamIn), pApi->paramIn, sizeof(ST_NR_IN));
    if(FALSE == bIsParamEqual)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "Nr_Run error: input params is not as same as CreatModule!");
        return ERROR_PARAM;
    }
    
    // run
    //eRet = Nr_Maincall(phNr);
#ifndef __GNUC__ //���߳�
    if((phNr->nrDetailRatio < -100) && !phNr->bIsLittleImgNr)
    {
        eRet = Nr_Maincall_Night(phNr);
    }
    else
    {
        eRet = Nr_MaincallMix2_Y_(phNr);
        eRet = Nr_MaincallMix2_U(phNr);
        eRet = Nr_MaincallMix2_V(phNr);
    }
#else            //���߳�  
    //�����ڴ�����ԭ�����ͼ��߶�С��256������߳�֮����ڴ�����ص���������쳣
    if(phNr->stParamIn.pstPicIn->height > 256)
    {
    if ((phNr->nrDetailRatio < -100) && !phNr->bIsLittleImgNr)
    {
        eRet = Nr_MaincallMix_night2P(phNr);
    }
    else
    {
        eRet = Nr_MaincallMix2P(phNr);
        }
    }
#endif
    
    if(SUCCESS != eRet)
    {
        return eRet;
    }
    
    return SUCCESS;
}



/********************************************************************************
 * Function Description:       
      1. DeNoiseģ�� destory�����ľ������    
 * Parameters:          
      > x                 - [in]  x    
 ********************************************************************************/
void Nr_Destory(API *pApi)
{
    HANDLE_NR  *phNr = NULL;
    
    // ����У��(1������������Ƿ���ȷ)
    if(NULL == pApi)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "Nr_Destory error: api=NULL!");
        return;
    }

    phNr = (HANDLE_NR *)pApi->handle;
    if(NULL == phNr|| NR_HANDLE_KEY != phNr->handleKey)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "Nr_Destory error: wrong handle!");
        return;
    }

    // �ڴ�����
    AlignMemFree(phNr);

    // api�ṹ�����ݸ�null
    pApi->handle     = NULL;
    pApi->paramIn    = NULL;
    pApi->paramOut   = NULL;
    pApi->Run        = NULL;
    pApi->Destroy    = NULL;

    return;

}


/********************************************************************************
 * Function Description:       
      1.  ����ģ��DeNoise�������Ѵ�����ģ��api�ṹ��ָ�룬
          �����ص�api�е�handleΪnull�����ʾ����ʧ��
 * Parameters:          
      > x                 - [in]  ģ����������ṹ��    
 ********************************************************************************/
 API Nr_Create(ST_NR_IN *pstParamIn)
 {
    E_ERROR               eRet;
    API                 apiDeNoise  = {NULL,NULL,NULL,NULL,NULL};
    HANDLE_NR     *phNr   = NULL;
    UINT8           *pMemAlign = NULL;
//    INT32            picSize;    //comment by wanghao because 550:not accessed
    ST_PICTURE  *pstPicInTmp    = NULL;
    UINT8 *pMemBuf;
    
    phNr   = (HANDLE_NR *)pstParamIn->pMemBuf;
    pstParamIn->pMemBuf += sizeof(HANDLE_NR);
    pMemBuf = pstParamIn->pMemBuf;
    
    pstPicInTmp = pstParamIn->pstPicIn;
//    picSize = pstPicInTmp->stride * pstPicInTmp->height;    //comment by wanghao because 550:not accessed
 
    // �������У��
    eRet = Nr_CheckParams(pstParamIn);
    if(SUCCESS != eRet)
    {
        return apiDeNoise;
    }

//    // �������
//    eRet = Nr_CreateHandle(pstParamIn, &phNr);
//    if(SUCCESS != eRet)
//    {
//        apiDeNoise.handle   = (void *)phNr;  
//        Nr_Destory(&apiDeNoise);
//        return apiDeNoise;
//    }

    // �����ʼ��+����ڴ����
    phNr->handleKey         = NR_HANDLE_KEY;
    phNr->stParamIn         = *(pstParamIn);
    phNr->bIsNrLuma          = (-100!= pstParamIn->nrRatio[0]) ||  (-100< pstParamIn->nrLumaHFRatio) ;
    phNr->bIsNrCha            = TRUE;
    //phNr->nrLumaRatio   = pstParamIn->nrLumaRatio;
    phNr->nrRatio[0]       = pstParamIn->nrRatio[0];
    phNr->nrRatio[1]       = pstParamIn->nrRatio[1];
    phNr->nrRatio[2]       = pstParamIn->nrRatio[2];
    phNr->nrLumaHFRatio= pstParamIn->nrLumaHFRatio;
    phNr->nrUVHFRatio    = pstParamIn->nrUVHFRatio;
    phNr->nrDetailRatio = pstParamIn->nrDetailRatio;
    //====С�ߴ�ͼƬ���⴦��
    phNr->bIsLittleImgNr = (pstPicInTmp->width * pstPicInTmp->height) < ((3264*2448)>>1); //С��4M
    if(phNr->bIsLittleImgNr)
    {
        INT32 distance;
        float    scale;

        scale = 1.0 - ((float)pstPicInTmp->width * pstPicInTmp->height) / 4000000.0;
        
        distance = 100 + phNr->nrRatio[0];
        phNr->nrRatio[0] = (INT32)MAX2(phNr->nrRatio[0] - distance * scale,-100);    //(INT32)modified by wanghao because:524

        distance = 100 + phNr->nrLumaHFRatio;
        phNr->nrLumaHFRatio = (INT32)MAX2(phNr->nrLumaHFRatio - distance * scale,-100);

        scale = MIN2(0.8, scale);//��ɫ������󽵵�һ���ǿ��
        distance = 100 + phNr->nrRatio[1];
        phNr->nrRatio[1] = (INT32)MAX2((phNr->nrRatio[1] - distance * scale),-100);    //(INT32)modified by wanghao because:524
        distance = 100 + phNr->nrRatio[2];
        phNr->nrRatio[2] = (INT32)MAX2((phNr->nrRatio[2] - distance * scale),-100);    //(INT32)modified by wanghao because:524

        distance = 100 + phNr->nrUVHFRatio;
        phNr->nrUVHFRatio = (INT32)MAX2(phNr->nrUVHFRatio - distance * scale,-100);
    }
    
    pMemAlign = (UINT8 *)MEM_ALIGN((UINT32)pMemBuf,MEM_ALIGN_LEN);
    FastDn2dInit((void**)&phNr->pstFastDn2d, &pMemAlign, pstPicInTmp->stride>>1, pstPicInTmp->height>>1);    
    
    pMemAlign = (UINT8 *)MEM_ALIGN((UINT32)pMemAlign,MEM_ALIGN_LEN);
    phNr->pDnCoef   = (INT16 *)pMemAlign;
    pMemAlign += COEF_NUM*sizeof(INT16);
    
    pMemAlign = (UINT8 *)MEM_ALIGN((UINT32)pMemAlign,MEM_ALIGN_LEN);
    phNr->pBuf     =  pMemAlign;
    
    // ����ӿڵĽ�����ֵ
    apiDeNoise.handle     = (void *)phNr;   
    apiDeNoise.paramIn    = (void *)pstParamIn;
    apiDeNoise.paramOut   = NULL;
    apiDeNoise.Run        = Nr_Run;
//    apiDeNoise.Destroy    = Nr_Destory;

    return apiDeNoise;
 }



