/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_DeNoise_V1.0
 * Date           : 2012-6-26
 * Description    : 算法主调，包含算法handle创建以及算法核心入口
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "gaussianblur.h"
#include "haarhfnr.h"
#include "edge.h"
#include "resize.h"
#include "haar.h"
#include "denoise_maincall.h"
#include "MemManage.h"
#include "bilateral.h"
#include "median.h"

#ifndef _MSC_VER
#include <arm_neon.h>
#include "gaussianblur_neon.h"
#include "haar_neon.h"
#include "haarhfnr_neon.h"
#include "edge_neon.h"
#include "MedianFilter_neon.h"
#include "DownScale_neon.h"
#include "resize_neon.h"
#define __TEST_ARM //ARM优化代码和C代码切换宏
//#define __TEST_YUV_G
#endif

#ifdef __GNUC__
#include <pthread.h>

pthread_mutex_t yReadedLock;
pthread_mutex_t uReadedLock;
pthread_mutex_t vReadedLock;

INT32 yReaded = 0;
INT32 uReaded = 0;
INT32 vReaded = 0;
#endif

//#define __DETAIL_LUMA

  /********************************************************************************
  * Function Description:   
      根据输入参数，创建句柄，返回创建是否成功的标志
      包括申请内存、分配内存
      
      申请的内存包括
      1、结构体所需内存: 句柄结构体
      2、句柄内数据内存(需要考虑字节对齐问题)
  * Parameters:          
       > pstParamIn            - [in]  模块输入参数
       > phHandle              - [out] 创建的句柄首地址
  ********************************************************************************/
 E_ERROR Nr_CreateHandle(ST_NR_IN *pstParamIn, HANDLE_NR **phHandle)
 {
    HANDLE_NR *phNr      = NULL;
    
    UINT8           *pMemBuf        = NULL;
    UINT8           *pMemAlign = NULL;
    
    ST_PICTURE  *pstPicInTmp    = NULL;

    UINT32          totalMemorySize = 1024;                 //初始化为1024，预留给对齐使用
    
    INT32            picSize;

    // ==================内存计算==============
    // handle结构体
    totalMemorySize += sizeof(HANDLE_NR);

    // 句柄内部数据所需的内存
    pstPicInTmp = pstParamIn->pstPicIn;
    picSize = pstPicInTmp->stride * pstPicInTmp->height;
    
    totalMemorySize +=  picSize; //for denoise buf
    totalMemorySize +=  picSize/4; //for denoise buf
    
    totalMemorySize += sizeof(ST_FASTDN2D);
    totalMemorySize +=  COEF_NUM*sizeof(INT16);//for coef
    totalMemorySize += MAX2(pstPicInTmp->stride>>1, pstPicInTmp->height>>1)* sizeof(UINT16);          //for line buf

    //============内存申请================
    pMemBuf = (UINT8 *)AlignMemMalloc(totalMemorySize);
    if(NULL == pMemBuf)
    {
        CE_LogMsg(MODULE_VERSION, __FILE__, __LINE__, LOG_ERROR, "can't malloc enough memory [size=%d]!", totalMemorySize);
        return ERROR_MEMORY;
    }

    // ============内存分配==================
    // handle结构体
    phNr = (HANDLE_NR *)pMemBuf;
    pMemBuf += sizeof(HANDLE_NR);

    // 句柄初始化+相关内存分配
    phNr->handleKey         = NR_HANDLE_KEY;
    phNr->stParamIn         = *(pstParamIn);
    phNr->bIsNrLuma          = (-100!= pstParamIn->nrRatio[0]) ||  (-100!= pstParamIn->nrLumaHFRatio) ;   //(1 == pstParamIn->nrSelect);
    phNr->bIsNrCha            = TRUE;
    //phNr->nrLumaRatio   = pstParamIn->nrLumaRatio;
    phNr->nrRatio[0]       = pstParamIn->nrRatio[0];
    phNr->nrRatio[1]       = pstParamIn->nrRatio[1];
    phNr->nrRatio[2]       = pstParamIn->nrRatio[2];
    phNr->nrLumaHFRatio= pstParamIn->nrLumaHFRatio;
    phNr->nrUVHFRatio    = pstParamIn->nrUVHFRatio;
    phNr->nrDetailRatio = pstParamIn->nrDetailRatio;
    
    pMemAlign = (UINT8 *)MEM_ALIGN((UINT32)pMemBuf,MEM_ALIGN_LEN);
    
    phNr->pDnCoef   = (INT16 *)pMemAlign;
    pMemAlign += COEF_NUM*sizeof(INT16);

    pMemAlign = (UINT8 *)MEM_ALIGN((UINT32)pMemAlign,MEM_ALIGN_LEN);
    phNr->pBuf     =  pMemAlign;
    pMemAlign += picSize + picSize/4;

    pMemAlign = (UINT8 *)MEM_ALIGN((UINT32)pMemAlign,MEM_ALIGN_LEN);
    FastDn2dInit((void**)&phNr->pstFastDn2d, &pMemAlign, pstPicInTmp->stride>>1, pstPicInTmp->height>>1);    
    
    // 已创建好的句柄传到外部
    *phHandle = phNr;

     return SUCCESS;
 }


 /********************************************************************************
 * Function Description:       
      1. colorenhance模块主调入口  
 * Parameters:          
      > hHandle      - [in]  算法句柄结构体 
 ********************************************************************************/
E_ERROR Nr_Maincall(HANDLE_NR *phHandle) 
{
    ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    //ST_PICTURE *pstPicOut = phHandle->stParamIn.pstPicOut;
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf  = phHandle->pBuf;
    
    UINT8 *pBlur;
    UINT8 *pEdge;
    UINT8 *pEdge2;
    UINT8 *pEdge3;
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?

    INT32  nrLumaRatio = phHandle->nrRatio[0];
    INT32  nrUVRatio = phHandle->nrRatio[1];
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
    INT32  nrUVHFRatio = phHandle->nrUVHFRatio;
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2         = height>> 1;
    INT32 chaW         = wDiv2;
    INT32 chaH          = hDiv2;
    INT32 uvStride     = stride >> 1; 
        
    INT32 planeIdx;     

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    INT32 strideDiv8 = stride >> 3;

    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    E_ERROR  eRet = SUCCESS;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    #if 0
    {
        FILE *pfYUV = fopen("yuv_in.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    //mrc_cg_imageNoiseReduction(pSrc[0], pSrc[1], pSrc[2], height,width,  nrLumaRatio,  nrUVRatio);

    //return;
    
    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2, pSrc[0]+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }
    
    //pSrc[0] 后面3/4大小的空间闲置
    pMemAlign = pSrc[0] + strideDiv2 * hDiv2; 
    pEdge        = pMemAlign;//wDiv4 * iHn这块空间一直到最后都不能动
    pMemAlign += wDiv4 * hDiv4;
    pBlur          = pMemAlign;

    //step2 : edge detect

    //检测LL的边缘信息
    #ifdef __TEST_ARM
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3_neon(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }

    CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
    #else
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }
    
    CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
    #endif  

    //检测HL\LH的边缘信息,将三个平面的边缘信息合并
    {
        UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
        UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
        UINT8 *pSrcV, *pSrcH;
        INT32 x,y;

        pSrcH  = pSrc[0] + hDiv4 * strideDiv4;// 水平
        pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4) << 1);// 垂直

        #ifdef __TEST_ARM
        {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);  
        #else
       {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);      
        #endif 

        pSrcV = pEdgebuf;
        pSrcH = pEdge;
        for(y = 0; y < hDiv4;y++)
        {
            for(x = 0; x < wDiv4; x++)
            {
                pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
            }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
        }
    }

    {
        INT32 nrYStrength        = 20;//20;
        INT32 nrYDetailRatio    = 4915;// (4.8/8)
        INT32 nrUVStrength     = 21;
        INT32 nrUVDetailRatio = 4915;//2765;//nrUVStrength/3;
    
        nrYStrength            =   ((nrYStrength*((105 + nrLumaRatio) <<10)/105)>>10);
        nrUVStrength         =   ((nrUVStrength*((105 + nrUVRatio) <<10)/105)>>10);

        FastDn2dControl(pstFastDn2d, nrYStrength ,  nrYDetailRatio,  nrUVStrength,  nrUVDetailRatio);
    }
    
    /*========================================================
                                                              亮度去噪
       ========================================================*/
    if(bIsNrLuma)
    { 
        INT32 nrEdgeTH = 60;
        INT32 nrTH;

        if(nrLumaRatio < -80)
        {
            nrEdgeTH = nrEdgeTH * (105 + nrLumaRatio)/25;
        }
        else if(nrLumaRatio > -60)
        {
            nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaRatio)/35,180); 
        }
        
        //-------------------LL 降噪--------------------
        //使用传导降噪渐变区域阶梯现象不明显
        #if 1
        FastDn2dEdge(pstFastDn2d,0,pSrc[0], pSrc[0],pEdge,wDiv4,  hDiv4,  strideDiv4,nrEdgeTH);
        FastDn2dInvertEdge(pstFastDn2d,0,pSrc[0], pSrc[0],pEdge,wDiv4,  hDiv4,  strideDiv4,nrEdgeTH);
        #else
        {
            UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
            UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
            
            INT32 nSigmaS = 1;//(nrEdgeTH*2 + 30) / 60;
            INT32 nSigmaR = 3;//(nrEdgeTH + 6) / 12;  //(nrEdgeTH/6)>>1
            
            //GaussianBlur3x3(pSrc[0], pBlur, pEdgebuf, wDiv4, hDiv4, strideDiv4, strideDiv4, strideDiv4);
            //BrightWareBilateralFilter(y, pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pTemp1);
            BilateralFilter(pSrc[0], hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
            
            //BrightWareBilateralFilter(pSrc[0],pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
        }
        #endif


        //-------------------HL LH HH 降噪--------------------
        {
            UINT8 * pSrcH,  * pSrcV, *pSrcHH;
            nrEdgeTH = 60;  //60
            nrTH = 15;

            nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
            }

            CalcDn2dCoefs(pDnCoef, nrTH);

            pSrcH  = pSrc[0] + hDiv4 * strideDiv4;
            pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4 )<< 1);
            pSrcHH =  pSrc[0] + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

            #ifdef __TEST_ARM
            HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);            
            #else
            HFNR_LUT_Edge_luma(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #endif

            // ------------第二层小波逆变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #endif
            
            // 16整数倍剩余像素的处理
            if (0 != mul16Rev2)
            {
                ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
            }
            
            //需要重新检测边缘
            {
                #ifdef __TEST_ARM
                {
                    UINT8 *pBufTemp = pSrc[0];//第一次小波逆变换后pucSrc[0]前1/4大小的空间闲置
                    GaussianBlur3x3_neon(pBuf, pBlur, pBufTemp, wDiv2, hDiv2, strideDiv2, wDiv2, wDiv2);
                }

                pEdge2 = pSrc[0];//继续利用
                CalcSobel_neon(pBlur,pEdge2,wDiv2  , hDiv2,wDiv2 ,wDiv2 );                   
                #else
                {
                    UINT8 *pBufTemp = pSrc[0];//第一次小波逆变换后pucSrc[0]前1/4大小的空间闲置
                    GaussianBlur3x3(pBuf, pBlur, pBufTemp, wDiv2, hDiv2, strideDiv2, wDiv2, wDiv2);
                }

                pEdge2 = pSrc[0];//继续利用
                CalcSobel(pBlur,pEdge2,wDiv2  , hDiv2,wDiv2 ,wDiv2 );
                #endif  

                {
                    UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                    UINT8 *pEdgebuf = (UINT8 *)malloc(wDiv2* hDiv2);
                    INT32 x,y;
                    
                    pSrcH  = pBuf + hDiv2 * strideDiv2;// 水平
                    pSrcV  = pBuf + ((hDiv2 * strideDiv2) << 1);// 垂直

                    #ifdef __TEST_ARM
                    {
                        UINT8 *pBufTemp = pEdgebuf;
                        GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                        GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                    }

                    CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
                    #else
                    {
                        UINT8 *pBufTemp = pEdgebuf;
                        GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                        GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                    }
                    
                    CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
                    #endif 

                    pSrcV = pEdgebuf;
                    pSrcH = pEdge2;
                    for(y = 0; y < hDiv2;y++)
                    {
                        for(x = 0; x < wDiv2; x++)
                        {
                            pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                        }
                        
                        pSrcV += wDiv2;
                        pSrcH += wDiv2;
                    }

                    free(pEdgebuf);
                }


                //-------------------LL 降噪--------------------
                //使用传导降噪渐变区域阶梯现象不明显
                //FastDn2dEdge(pstFastDn2d,0,pBuf, pBuf,pEdge2,wDiv2, hDiv2,  strideDiv2,nrEdgeTH);
                //FastDn2dInvertEdge(pstFastDn2d,0,pBuf, pBuf,pEdge2,wDiv2, hDiv2,  strideDiv2,nrEdgeTH);
                
#if 0
                {
                    UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                    UINT8 *pEdgebuf = (UINT8 *)malloc(wDiv2* hDiv2);
                            
                    INT32 nSigmaS = 2;//(nrEdgeTH*2 + 30) / 60;
                    INT32 nSigmaR = 8;//(nrEdgeTH + 6) / 12;  //(nrEdgeTH/6)>>1
                    
                    //GaussianBlur3x3(pSrc[0], pBlur, pEdgebuf, wDiv4, hDiv4, strideDiv4, strideDiv4, strideDiv4);
                    //BrightWareBilateralFilter(y, pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pTemp1);
                    //BilateralFilter(pBuf, hDiv2, wDiv2, nSigmaS, nSigmaR, pEdgebuf);
                    //EdgeWareBilateralFilter(pBuf,pEdge2, hDiv2, wDiv2, nSigmaS, nSigmaR,180, pEdgebuf);
                    
                    //BrightWareBilateralFilter(pSrc[0],pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
                }
#endif

                nrEdgeTH = 100;
                nrTH = 20;

                nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

                if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
                }  
                else if(nrLumaHFRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
                }

                pSrcH    = pBuf + hDiv2 * strideDiv2;
                pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
                pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

                #ifdef __TEST_ARM
                HFNR_Edge_luma_neon(pSrcH, pEdge2,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcV,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcHH,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #else
                HFNR_Edge_luma(pSrcH, pEdge2,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcV,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcHH,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #endif
            }
        }
        
    }
    else
    {
        // 不会进入，代码没改
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }
    }
     
    /*========================================================
                                                              色度去噪
       ========================================================*/
    if(bIsNrCha)
    {
        INT32 wDiv8 = wDiv4 >> 1;
        INT32 hDiv8 = hDiv4 >> 1;

        UINT8 *pUVBuf[3];
        
        pUVBuf[1] = pSrc[0]; //U  使用pucSrc[0]前面1/4大小
        pUVBuf[2] = pMemAlign; //V 使用pucSrc[0]中pucEdge后面的1/4大小

        pMemAlign += uvStride*chaH;
        
        // ------------第一层小波正变换------------
        mul16W     = chaW & 0xfffffff0;
        mul16Rev   = chaW - mul16W;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        
        #ifdef __TEST_YUV_G
        ApplyHaar_ChromaU_G_neon(pSrc[1], pUVBuf[1],mul16W<<1, chaH, stride);
        ApplyHaar_ChromaV_G_neon(pSrc[1], pUVBuf[2],mul16W<<1, chaH, stride); 
        #else //#ifdef __TEST_YUV_G
        ApplyHaar_new_neon(pSrc[1], pUVBuf[1], mul16W, chaH,  uvStride);
        ApplyHaar_new_neon(pSrc[2], pUVBuf[2], mul16W, chaH,  uvStride);
        #endif //#ifdef __TEST_YUV_G
        
        #else //#ifdef __TEST_ARM
        
        #ifdef __TEST_YUV_G
        ApplyHaar_Chroma_G(pSrc[1]+1, pUVBuf[1],mul16W<<1, chaH, stride);
        ApplyHaar_Chroma_G(pSrc[1], pUVBuf[2],mul16W<<1, chaH, stride); 
        #else
        ApplyHaar_new(pSrc[1], pUVBuf[1], mul16W, chaH,  uvStride);
        ApplyHaar_new(pSrc[2], pUVBuf[2], mul16W, chaH,  uvStride);
        #endif
        
        #endif//#ifdef __TEST_ARM  

        // 16整数倍剩余像素的处理
        if (0!= mul16Rev)
        {
            #ifdef __TEST_YUV_G
            ApplyHaar_Chroma_G(pSrc[1]+(mul16W<<1)+1, pUVBuf[1]+(mul16W>>1),mul16Rev<<1, chaH, stride);
            ApplyHaar_Chroma_G(pSrc[1]+(mul16W<<1), pUVBuf[2]+(mul16W>>1),mul16Rev<<1, chaH, stride); 
            #else
            ApplyHaar_new(pSrc[1]+mul16W, pUVBuf[1]+(mul16W>>1), mul16Rev, chaH,  uvStride);
            ApplyHaar_new(pSrc[2]+mul16W, pUVBuf[2]+(mul16W>>1), mul16Rev, chaH,  uvStride);
            #endif
        }

        
        // ------------第二层小波正变换------------
        mul16W2    = wDiv4 & 0xfffffff0;
        mul16Rev2  = wDiv4 - mul16W2;
        
        // 宽为16整数倍的处理
#ifdef __TEST_ARM
        ApplyHaar_new_neon(pUVBuf[1], pSrc[1], mul16W2, hDiv4,  strideDiv4);                
        ApplyHaar_new_neon(pUVBuf[2], pSrc[2], mul16W2, hDiv4,  strideDiv4);
#else
        ApplyHaar_new(pUVBuf[1], pSrc[1], mul16W2, hDiv4,  strideDiv4);
        ApplyHaar_new(pUVBuf[2], pSrc[2], mul16W2, hDiv4,  strideDiv4);
#endif
        
        // 16整数倍剩余像素的处理
        if (0!= mul16Rev2)
        {
            ApplyHaar_new(pUVBuf[1]+mul16W2, pSrc[1]+(mul16W2>>1), mul16Rev2, hDiv4,  strideDiv4);
            ApplyHaar_new(pUVBuf[2]+mul16W2, pSrc[2]+(mul16W2>>1), mul16Rev2, hDiv4,  strideDiv4);
        }
        
        pEdge2 = pMemAlign;
        pMemAlign += wDiv8 *hDiv8;
        pEdge3 = pMemAlign;
        pMemAlign += wDiv8 *hDiv8;

        //使用亮度的边缘
        MeanDown2x(pEdge, pEdge2, wDiv4 , hDiv4,wDiv4, wDiv8);
        
        {
            UINT8 *pTbuf = pMemAlign;
            UINT8 *pucEdgeUL = pTbuf,*pucEdgeVL = pEdge3;
            INT32 x,y;
            
            #ifdef __TEST_ARM
            CalcSobel_neon(pSrc[1],pTbuf,wDiv8 , hDiv8,strideDiv8,wDiv8);
            CalcSobel_neon(pSrc[2],pEdge3,wDiv8 , hDiv8,strideDiv8,wDiv8);
            #else
            CalcSobel(pSrc[1],pTbuf,wDiv8 , hDiv8,strideDiv8, wDiv8);
            CalcSobel(pSrc[2],pEdge3,wDiv8 , hDiv8,strideDiv8,wDiv8);  
            #endif 

            for(y = 0 ; y < hDiv8; y++)
            {
                for(x = 0 ; x < wDiv8; x++)
                {
                    pucEdgeVL[x] = MAX2(pucEdgeUL[x], pucEdgeVL[x]);
                }

                pucEdgeUL += wDiv8;
                pucEdgeVL += wDiv8;
            }
        }

        for(planeIdx = 1; planeIdx < 3 ; planeIdx++)
        {        
            INT32 nrEdgeTH = 70;
            INT32 nrUVEdgeTH = 128;
            INT32 nrTH         = 8;  
            INT32 protectLevel = 0;//高频处理如果把小值滤掉，渐变过程比较容易出现跳变形成阶梯
                
            if(nrUVRatio < -80)
            {
                nrEdgeTH = nrEdgeTH * (105 + nrUVRatio)/25;
            }
            else if(nrUVRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrUVRatio)/45,100); 
            }
            
            if(nrUVRatio < -50)
            {
                nrUVEdgeTH = MAX2(nrUVEdgeTH * (105 + nrUVRatio)/55,70);
            }

            if(nrUVRatio < -90)
            {
                protectLevel = 3;
            }
            else if(nrUVRatio < -80)
            {
                protectLevel = 2;
            }
            else if(nrUVRatio < -70)
            {
                protectLevel = 1;
            }
            
            FastDn2dUVEdge(pstFastDn2d,planeIdx,pSrc[planeIdx], pSrc[planeIdx],pEdge2,pEdge3,wDiv8,  hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH);
            FastDn2dInvertUVEdge(pstFastDn2d,planeIdx,pSrc[planeIdx], pSrc[planeIdx],pEdge2,pEdge3,wDiv8,  hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH);

            {
                UINT8 * pSrcH,  * pSrcV, *pSrcHH;
                nrEdgeTH = 40;//60;
                nrTH        = 10;

#if 0
                if(nrUVHFRatio < -80)
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrUVHFRatio)/25;
                    nrTH = nrTH * (105 + nrUVHFRatio)/25;
                }
                else if(nrUVHFRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrUVHFRatio)/35,128); 
                    nrTH = MIN2(nrTH * (105 + nrUVHFRatio)/45,25); 
                }
#endif

                if(nrUVHFRatio < -60)
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrUVHFRatio)/45;
                    nrTH = nrTH * (105 + nrUVHFRatio)/45;
                }
                else
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrUVHFRatio)/45,128); 
                    nrTH = MIN2(nrTH * (105 + nrUVHFRatio)/45,25); 
                }
            
                #ifdef __DN_COEFS_TAB
                pDnCoef = DN2DCOEF_TH8;
                #else
                CalcDn2dCoefs(pDnCoef, nrTH);   
                #endif
                
                pSrcH = pSrc[planeIdx] + hDiv8*strideDiv8;
                pSrcV = pSrc[planeIdx] + ((hDiv8*strideDiv8)<<1);

                #ifdef __TEST_ARM
                HFNR_LUT_Edge_neon(pSrcH,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);
                HFNR_LUT_Edge_neon(pSrcV,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);                
                #else
                HFNR_LUT_Edge(pSrcH,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);
                HFNR_LUT_Edge(pSrcV,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);
                #endif
                pSrcHH = pSrc[planeIdx] + ((hDiv8*strideDiv8)<<1) + hDiv8*strideDiv8;
                memset(pSrcHH,128,hDiv8*strideDiv8);

            }

        }
        
        // ------------第二层小波逆变换------------
        mul16W2    = wDiv4 & 0xfffffff0;
        mul16Rev2  = wDiv4 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pSrc[1], pUVBuf[1],  mul16W2, hDiv4,  strideDiv4);
        ApplyInverseHaar_new_neon(pSrc[2], pUVBuf[2],  mul16W2, hDiv4,  strideDiv4);
        #else
        ApplyInverseHaar_new(pSrc[1], pUVBuf[1],  mul16W2, hDiv4,  strideDiv4);
        ApplyInverseHaar_new(pSrc[2], pUVBuf[2],  mul16W2, hDiv4,  strideDiv4); 
        #endif

        // 16整数倍剩余像素的处理
        if (0!= mul16Rev2)
        {
            ApplyInverseHaar_new(pSrc[1]+(mul16W2>>1), pUVBuf[1]+mul16W2, mul16Rev2, hDiv4,  strideDiv4); 
            ApplyInverseHaar_new(pSrc[2]+(mul16W2>>1), pUVBuf[2]+mul16W2, mul16Rev2, hDiv4,  strideDiv4);
        }

        {
            UINT8 *pucTUbuf = pMemAlign;
            UINT8 *pucTVbuf = pMemAlign+wDiv4 * hDiv4;
            UINT8 *pucEdgeUL = pucTUbuf,*pucEdgeVL = pucTVbuf;
            INT32 x,y;

            #ifdef __TEST_ARM
            CalcSobel_neon(pUVBuf[1],pucTUbuf,wDiv4 , hDiv4,strideDiv4, wDiv4);
            CalcSobel_neon(pUVBuf[2],pucTVbuf,wDiv4 , hDiv4,strideDiv4,wDiv4);
            #else
            CalcSobel(pUVBuf[1],pucTUbuf,wDiv4 , hDiv4,strideDiv4, wDiv4);
            CalcSobel(pUVBuf[2],pucTVbuf,wDiv4 , hDiv4,strideDiv4,wDiv4);  
            #endif

            for(y = 0 ; y < hDiv4; y++)
            {
                for(x = 0 ; x < wDiv4; x++)
                {
                    pucEdgeVL[x] = MAX2(pucEdgeUL[x], pucEdgeVL[x]);
                }

                pucEdgeUL += wDiv4;
                pucEdgeVL += wDiv4;
            }

            for(planeIdx = 1; planeIdx < 3 ; planeIdx++)
            {        
                INT32 nrEdgeTH = 60;
                INT32 nrUVEdgeTH = 128;
                INT32 nrTH         = 8;  
                INT32 protectLevel = 0;//高频处理如果把小值滤掉，渐变过程比较容易出现跳变形成阶梯


                if(nrUVRatio < -80)
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrUVRatio)/25;
                }
                else if(nrUVRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrUVRatio)/45,100); 
                }

                if(nrUVRatio < -50)
                {
                    nrUVEdgeTH = MAX2(nrUVEdgeTH * (105 + nrUVRatio)/55,70);
                }

                if(nrUVRatio < -90)
                {
                    protectLevel = 3;
                }
                else if(nrUVRatio < -80)
                {
                    protectLevel = 2;
                }
                else if(nrUVRatio < -70)
                {
                    protectLevel = 1;
                }

                /*==========================================================
                    最佳版本需要开启下面两个滤波步骤
                    但是为了提升速度可以只开启FastDn2dUVEdge只滤一遍
                    如果需要更进一步提升速度，则两个步骤均关闭
                ===========================================================*/
                //FastDn2dUVEdge(pstFastDn2d,planeIdx,pUVBuf[planeIdx], pUVBuf[planeIdx],pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrEdgeTH,nrUVEdgeTH);
                //FastDn2dInvertUVEdge(pstFastDn2d,planeIdx,pUVBuf[planeIdx], pUVBuf[planeIdx],pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrEdgeTH,nrUVEdgeTH);

#if 0
                {
                    UINT8 *pEdgebuf = (UINT8 *)malloc(wDiv2* hDiv2);
                            
                    INT32 nSigmaS = 2;//(nrEdgeTH*2 + 30) / 60;
                    INT32 nSigmaR = 15;//(nrEdgeTH + 6) / 12;  //(nrEdgeTH/6)>>1
                    
                    BilateralFilter(pUVBuf[planeIdx], hDiv4, wDiv4,nSigmaS, nSigmaR, pEdgebuf);

                    free(pEdgebuf);
                }
#endif

                
                {
                    UINT8 * pSrcH,  * pSrcV, *pSrcHH;
                    nrEdgeTH = 60;
                    nrTH = 15;
                    
                    if(nrUVHFRatio < -80)
                    {
                        nrEdgeTH = nrEdgeTH * (105 + nrUVHFRatio)/25;
                        nrTH = nrTH * (105 + nrUVHFRatio)/25;
                    }
                    else if(nrUVHFRatio > -60)
                    {
                        nrEdgeTH = MIN2(nrEdgeTH * (105 + nrUVHFRatio)/40,128); 
                        nrTH = MIN2(nrTH * (105 + nrUVHFRatio)/35,35); 
                    }  
                    
                    pSrcH = pUVBuf[planeIdx] + hDiv4* strideDiv4;
                    pSrcV = pUVBuf[planeIdx] + ((hDiv4* strideDiv4) <<1);

                    #ifdef __TEST_ARM
                    HFNR_Edge_neon(pSrcH, pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrTH,nrEdgeTH,nrUVEdgeTH,protectLevel);
                    HFNR_Edge_neon(pSrcV, pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrTH,nrEdgeTH,nrUVEdgeTH,protectLevel);
                    #else
                    HFNR_Edge(pSrcH, pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrTH,nrEdgeTH,nrUVEdgeTH,protectLevel);
                    HFNR_Edge(pSrcV, pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrTH,nrEdgeTH,nrUVEdgeTH,protectLevel);
                    #endif
                    
                    pSrcHH = pUVBuf[planeIdx] + ((hDiv4* strideDiv4) <<1) + hDiv4* strideDiv4;
                    memset(pSrcHH,128,hDiv4* strideDiv4);    
                }
            }    
        }
        
        // ------------第一层小波逆变换------------
        mul16W     = chaW & 0xfffffff0;
        mul16Rev   = chaW - mul16W;
        
        // 宽为16整数倍的处理
#ifdef __TEST_ARM
        #ifdef __TEST_YUV_G
        ApplyInverseHaar_ChromaU_G_neon(pUVBuf[1], pSrc[1], mul16W<<1, chaH, stride);
        ApplyInverseHaar_ChromaV_G_neon(pUVBuf[2], pSrc[1], mul16W<<1, chaH, stride);
        #else //#ifdef __TEST_YUV_G        
        ApplyInverseHaar_new_neon(pUVBuf[1], pSrc[1], mul16W, chaH,  uvStride);
        ApplyInverseHaar_new_neon(pUVBuf[2], pSrc[2], mul16W, chaH,  uvStride);
        #endif //#ifdef __TEST_YUV_G   
        
#else

#ifdef __TEST_YUV_G
        ApplyInverseHaar_Chroma_G(pUVBuf[1], pSrc[1]+1, mul16W<<1, chaH, stride);
        ApplyInverseHaar_Chroma_G(pUVBuf[2], pSrc[1], mul16W<<1, chaH, stride);
#else //#ifdef __TEST_YUV_G        
        ApplyInverseHaar_new(pUVBuf[1], pSrc[1], mul16W, chaH,  uvStride);
        ApplyInverseHaar_new(pUVBuf[2], pSrc[2], mul16W, chaH,  uvStride);
#endif

#endif

        // 16整数倍剩余像素的处理
        if (0 != mul16Rev)
        {
#ifdef __TEST_YUV_G
            ApplyInverseHaar_Chroma_G(pUVBuf[1]+(mul16W>>1), pSrc[1]+(mul16W<<1)+1, mul16Rev<<1, chaH, stride);
            ApplyInverseHaar_Chroma_G(pUVBuf[2]+(mul16W>>1), pSrc[1]+(mul16W<<1), mul16Rev<<1, chaH, stride);
#else
            ApplyInverseHaar_new(pUVBuf[1]+(mul16W>>1), pSrc[1]+mul16W, mul16Rev, chaH,  uvStride);
            ApplyInverseHaar_new(pUVBuf[2]+(mul16W>>1), pSrc[2]+mul16W, mul16Rev, chaH,  uvStride);
#endif
        }
    }

    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);
    #else
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    #endif
    
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_out.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    return eRet;
}    


void AlphaBlend(UINT8 *pSrcDst, UINT8 *pBlendBy, INT32 width, INT32 height, INT32 stride, UINT8 alpha)
{
    UINT8 *pSrcDstL,*pBlendByL;
    INT32 x,y;

    INT32 w8 = ((width>>3)<<3);

    UINT8 alpha1 = (128 - alpha);

    INT32 offset = stride - w8;
    
#ifdef __TEST_ARM
    uint8x8_t d8x8_0, d8x8_1,d8x8_alpha0,d8x8_alpha1;
    uint16x8_t d16x8_0;

    d8x8_alpha0      = vdup_n_u8(alpha);
    d8x8_alpha1      = vdup_n_u8(alpha1);
    


    pSrcDstL  = pSrcDst;
    pBlendByL = pBlendBy;
    
    for(y = height; y != 0; y--)
    {
        for(x = 0; x <= width-8;x += 8)
        {
            d8x8_0 = vld1_u8(pSrcDstL);
            d8x8_1 = vld1_u8(pBlendByL);             

            d16x8_0 = vmull_u8(d8x8_0,d8x8_alpha0);
            d16x8_0 = vmlal_u8(d16x8_0,d8x8_1,d8x8_alpha1);
            d8x8_0  = vrshrn_n_u16(d16x8_0,7);

            vst1_u8(pSrcDstL,d8x8_0);

            pSrcDstL += 8;
            pBlendByL += 8;
        }

        pSrcDstL  += offset;
        pBlendByL += offset;
    }

    //处理剩余列
    if(w8)
    {
        pSrcDstL  = pSrcDst  + w8;
        pBlendByL = pBlendBy + w8;
        for(y = height; y != 0; y--)
        {
            for(x = w8; x < width;x++)
            {
                pSrcDstL[x] = (pSrcDstL[x] * alpha + pBlendByL[x] * alpha1 + 64) >> 7;
            }

            pSrcDstL  += stride;
            pBlendByL += stride;
        }  
    }
    #else
    {
		pSrcDstL  = pSrcDst;
        pBlendByL = pBlendBy;

        for(y = 0; y < height;y++)
        {
            for(x = 0; x < width; x++)
            {
                pSrcDstL[x] = (pSrcDstL[x] * alpha + pBlendByL[x] * (128 - alpha) + 64) >> 7;
            }

            pSrcDstL  += stride;
            pBlendByL += stride;
        }
    }

    #endif
    
}


void LumaDetailRestore(UINT8 *pOrig, UINT8 *pFiltered,UINT8 *pLumaMask, INT32 width , INT32 height, INT32 stride, INT32 naxRatio)
{
    UINT8 *pOrigL,*pFilteredL,*pLumaMaskL;
    INT32 x,y;

    INT32 maxW = 32767;
    INT32 divider;
    INT32 maxValue = 255;

    INT32 strideMul3 = stride * 3;

    INT32 weight;
    
    divider = naxRatio * maxW / (maxValue * 128);

    pOrigL     = pOrig;
    pFilteredL = pFiltered;
    pLumaMaskL = pLumaMask;
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            weight        = pLumaMaskL[x] * divider;
            pFilteredL[x] = MIN2((pOrigL[x] *  weight + pFilteredL[x] * (maxW - weight) + 16384)>>15,255);
        }

        pFilteredL += stride;
        pOrigL     += stride;
        pLumaMaskL += stride;
    }    
}


E_ERROR Nr_Maincall_Night(HANDLE_NR *phHandle) 
{
    ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn     = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf    = phHandle->pBuf;
    UINT8 *pHaar2Buf;
    
    UINT8 *pBlur;
    UINT8 *pEdge;
    UINT8 *pEdge2;
    UINT8 *pEdge3;
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
    BOOL  bIsNrCha   = phHandle->bIsNrCha;    //dechroma?

    INT32  nrLumaRatio   = phHandle->nrRatio[0];
    INT32  nrUVRatio     = phHandle->nrRatio[1];
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
    INT32  nrUVHFRatio   = phHandle->nrUVHFRatio;
    
    INT32 width          = pstPicIn->width;
    INT32 height         = pstPicIn->height;
    INT32 stride         = pstPicIn->stride;

    INT32 wDiv2          = width >> 1;
    INT32 hDiv2          = height>> 1;
    INT32 chaW           = wDiv2;
    INT32 chaH           = hDiv2;
    INT32 uvStride       = stride >> 1; 
        
    INT32 planeIdx;     

    INT32 wDiv4  = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    INT32 strideDiv8 = stride >> 3;

    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    E_ERROR  eRet = SUCCESS;

    UINT8 *pUpScale1 ;//= (UINT8 *)malloc(stride * height/4);
    UINT8 *pUpScale;// = (UINT8 *)malloc(stride * height);
        
    pMemAlign  = pBuf;
    pBuf       += strideDiv2* hDiv2;
    pUpScale1  = pBuf;
    pBuf       += stride * height/4;
    pUpScale   = pBuf;
    pBuf       += stride * height;
    pHaar2Buf  = pBuf; 
    pBuf       += strideDiv2* hDiv2;

    
    
    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];
    
    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }

    #ifdef __GNUC__
    //=================进行线程间同步，防止数据没有读就被写修改====
    pthread_mutex_lock(&yReadedLock);
    yReaded++;
    pthread_mutex_unlock(&yReadedLock);
    
    while(1)//循环里需要加语句，否则会形成死循环
    {
        //printf("==waiting\n");
        if(yReaded >=4)
        {
            break;
        }
        
        usleep(1);    //功能把进程挂起一段时间， 单位是微秒
    }
    //=======================================================================
    #endif
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pHaar2Buf,mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf, pHaar2Buf,mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2,pHaar2Buf+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }

    //pSrc[0] 后面3/4大小的空间闲置
    pEdge        = pMemAlign;//wDiv4 * iHn这块空间一直到最后都不能动
    pMemAlign += wDiv4 * hDiv4;
    pBlur          = pMemAlign;


    //如果没有开启亮度降噪则在此处检测LL的边缘信息，否则在LL降噪完成后进行边缘检测
    if(!bIsNrLuma)
    {
        #ifdef __TEST_ARM
        GaussianBlur3x3_neon(pHaar2Buf, pBlur, pEdge,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
        #else
        GaussianBlur3x3(pHaar2Buf, pBlur, pEdge,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
        #endif  
    }
    
    {
        INT32 nrYStrength     = 20;//20;
        INT32 nrYDetailRatio  = 8000;//6553;//4915;0.6// 
        INT32 nrUVStrength    = 21;
        INT32 nrUVDetailRatio = 4915;//2765;//nrUVStrength/3;
    
        nrYDetailRatio  =   MIN2(((nrLumaRatio + 70) * 3000/40 + 5000),8000);//-30 - 8000 (0.98); -60 - 5000(0.6)
        nrYStrength          =   ((nrYStrength*((105 + nrLumaRatio) <<10)/105)>>10);
        nrUVStrength         =   ((nrUVStrength*((105 + nrUVRatio) <<10)/105)>>10);

        FastDn2dControl(pstFastDn2d, nrYStrength ,  nrYDetailRatio,  nrUVStrength,  nrUVDetailRatio);
    }

    /*========================================================
           亮度去噪
     ========================================================*/
    if(bIsNrLuma)
    { 
        INT32 nrEdgeTH = 60;
        INT32 nrTH;

        #if 0
        {
            char name[200];
            FILE *pfYUV;

            sprintf(name, "/data/improc/%d_src.yuv", phHandle->threadIdx);

            printf("===%s ,%d %d\n",name,strideDiv4,hDiv4);

            pfYUV = fopen(name,"wb");
            if(NULL !=pfYUV)
            {
                fwrite(pHaar2Buf,1,(strideDiv4*hDiv4),pfYUV);
                fwrite(pHaar2Buf,1,(strideDiv4*hDiv4)/4,pfYUV);
                fwrite(pHaar2Buf,1,(strideDiv4*hDiv4)/4,pfYUV);
                fclose(pfYUV);
            }
            else
            {
                printf("=====NULL\n");
            }
        }
        #endif

        
        //-------------------LL 降噪--------------------
        if(-100 != nrLumaRatio)
        {
            #ifdef __DETAIL_LUMA
            if(nrLumaRatio> -60)
            {
                memcpy(pUpScale1,pHaar2Buf,hDiv4*strideDiv4);
            }
            #endif
            
            FastDn2d(pstFastDn2d,0,pHaar2Buf,pHaar2Buf,wDiv4,  hDiv4, strideDiv4);
            FastDn2dInvert(pstFastDn2d,0,pHaar2Buf, pHaar2Buf,wDiv4, hDiv4, strideDiv4);

            #ifdef __DETAIL_LUMA
            //根据亮度恢复图像细节
            if(nrLumaRatio> -60)
            {
                INT32 maxRatio = 128;//最好根据亮度来调节最大比率 

                if(nrLumaRatio < -30)
                {
                    maxRatio = (nrLumaRatio + 30)  * 5/6 + 110;
                }
                
                memcpy(pUpScale,pHaar2Buf,(height * stride));
                InplaceBlur(pUpScale, width, height, 15);
                LumaDetailRestore(pUpScale1, pHaar2Buf,pUpScale, wDiv4, hDiv4, strideDiv4, maxRatio);
            }
            #endif
        }
        

        #ifdef __TEST_ARM
        UpScale_neon(pHaar2Buf, hDiv4, wDiv4,strideDiv2,pUpScale1);
        UpScale_neon(pUpScale1, hDiv2, wDiv2,stride,pUpScale);
        #else
        UpScale(pHaar2Buf, hDiv4, wDiv4,strideDiv2,pUpScale1);
        UpScale(pUpScale1, hDiv2, wDiv2,stride,pUpScale);
        #endif

        
        //使用滤波后的图像进行边缘检测
        #ifdef __TEST_ARM
        GaussianBlur3x3_neon(pHaar2Buf, pBlur, pEdge,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
        #else
        GaussianBlur3x3(pHaar2Buf, pBlur, pEdge,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
        #endif  


        //-------------------HL LH HH 降噪--------------------
        {
            UINT8 * pSrcH,  * pSrcV, *pSrcHH;
            nrEdgeTH = 60;  //60
            nrTH     = 15;

            nrTH = nrTH * (100 + nrLumaHFRatio)/45;

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (100 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = nrEdgeTH * (100 + nrLumaHFRatio)/35;
            }

            if(nrTH)
            {
                CalcDn2dCoefs(pDnCoef, nrTH);

                pSrcH  = pHaar2Buf + hDiv4 * strideDiv4;
                pSrcV  = pHaar2Buf + ((hDiv4 * strideDiv4 )<< 1);
                pSrcHH =  pHaar2Buf + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

                #if 0//def __TEST_ARM
                //===========需要根据C代码修改neon!!!!!!!!!
                HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
                HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
                HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);            
                #else
                HFNR_LUT_Edge_luma_night(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4);
                HFNR_LUT_Edge_luma_night(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4);
                HFNR_LUT_Edge_luma_night(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4);
                #endif
            }

            // ------------第二层小波逆变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pHaar2Buf, pBuf, mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pHaar2Buf, pBuf, mul16W2, hDiv2,  strideDiv2);
            #endif
            
            // 16整数倍剩余像素的处理
            if (0 != mul16Rev2)
            {
                ApplyInverseHaar_new(pHaar2Buf +(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
            }

            //需要重新检测边缘
            {
                pBlur = pSrc[0] + wDiv2* hDiv2;
                pEdge2 = pHaar2Buf;//继续利用
                
                #ifdef __TEST_ARM
                GaussianBlur3x3_neon(pBuf, pBlur, pEdge2, wDiv2, hDiv2, strideDiv2, wDiv2, wDiv2);
                CalcSobel_neon(pBlur,pEdge2,wDiv2  , hDiv2,wDiv2 ,wDiv2 );                   
                #else
                GaussianBlur3x3(pBuf, pBlur, pEdge2, wDiv2, hDiv2, strideDiv2, wDiv2, wDiv2);
                CalcSobel(pBlur,pEdge2,wDiv2  , hDiv2,wDiv2 ,wDiv2 );
                #endif  
                

                nrEdgeTH = 100;
                nrTH     = 30;//20;

                nrTH = nrTH * (100 + nrLumaHFRatio)/45;

                if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
                {
                    nrEdgeTH = nrEdgeTH * (100 + nrLumaHFRatio)/25; // 25 = 105 - 80
                }  
                else if(nrLumaHFRatio > -60)
                {
                    nrEdgeTH = nrEdgeTH * (100 + nrLumaHFRatio)/35;
                }

                if(nrTH)
                {
                    pSrcH    = pBuf + hDiv2 * strideDiv2;
                    pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
                    pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

                    #if 0//def __TEST_ARM
                    //===========需要根据C代码修改neon!!!!!!!!!
                    HFNR_Edge_luma_neon(pSrcH, pEdge2,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                    HFNR_Edge_luma_neon(pSrcV,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                    HFNR_Edge_luma_neon(pSrcHH,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                    #else
                    HFNR_Edge_luma_night(pSrcH ,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH);
                    HFNR_Edge_luma_night(pSrcV ,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH);
                    HFNR_Edge_luma_night(pSrcHH,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH);
                    #endif
                }
               
            }
        }
    }
    else
    {
        // ------------第二层小波逆变换------------
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pHaar2Buf, pBuf, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pHaar2Buf, pBuf, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pHaar2Buf +(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }
    }

    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;

    #ifdef __TEST_ARM
    #ifdef __GNUC__
    {
        UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[0];
        INT32 dstH = phHandle->stParamIn.pstPicOut->height;
        INT32 srcH  = phHandle->stParamIn.pstPicIn->height;
        INT32 threadOffset = phHandle->threadOffset;
        
        ApplyInverseHaar_new_thread_neon(pBuf,pDst ,threadOffset,srcH,mul16W, dstH,  stride);
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev)
        {
            ApplyInverseHaar_thread_new(pBuf+(mul16W>>1),pDst+mul16W , threadOffset,srcH,mul16Rev, dstH,  stride);
        }
        
        //图像融合
        if(bIsNrLuma)
        {
            UINT8 *pSacleL = pUpScale + threadOffset;
            INT32 x,y;
            UINT8 alpha = 102;

            if(nrLumaHFRatio < -50)
            {
                alpha = MIN2(alpha - 128*(3*nrLumaHFRatio + 150)/500,128);
            }

            AlphaBlend(pDst, pSacleL,  width, dstH,  stride,  alpha);
        }
        
    }
    #else
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }
    
    if(bIsNrLuma)
    {
        UINT8 *pSacleL = pUpScale;
        INT32 x,y;
        UINT8 alpha = 102;

        if(nrLumaHFRatio < -50)
        {
            alpha = MIN2(alpha - 128*(3*nrLumaHFRatio + 150)/500,128);
        }

        AlphaBlend(pSrc[0], pSacleL,  width, height,  stride,  alpha);
    }
    #endif
    
    #else
    
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }

    if(bIsNrLuma)
    {
        UINT8 *pSacleL = pUpScale;
        INT32 x,y;
        UINT8 alpha = 102;

        if(nrLumaHFRatio < -50)
        {
            alpha = MIN2(alpha - 128*(3*nrLumaHFRatio + 150)/500,128);
        }

        AlphaBlend(pSrc[0], pSacleL,  width, height,  stride,  alpha);
    }
    #endif


    /*========================================================
           色度去噪
     ========================================================*/
    if(bIsNrCha)
    {
        INT32 wDiv8 = wDiv4 >> 1;
        INT32 hDiv8 = hDiv4 >> 1;

        UINT8 *pUVBuf[3];
        UINT8 *pUVHaar2Buf[3];

        UINT8 *puvscale[3];

        puvscale[1] = pUpScale;
        puvscale[2] = pUpScale + chaH*uvStride;
        
        pUVBuf[1] = pBuf; 
        pUVBuf[2] = pBuf + chaH*uvStride; 
        pUVHaar2Buf[1] = pBuf + chaH*uvStride + chaH*uvStride;  
        pUVHaar2Buf[2] = pBuf + chaH*uvStride + chaH*uvStride + hDiv4*strideDiv4;  
        
        // ------------第一层小波正变换------------
        mul16W     = chaW & 0xfffffff0;
        mul16Rev   = chaW - mul16W;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        
        #ifdef __TEST_YUV_G
        ApplyHaar_ChromaU_G_neon(pSrc[1], pUVBuf[1],mul16W<<1, chaH, stride);
        ApplyHaar_ChromaV_G_neon(pSrc[1], pUVBuf[2],mul16W<<1, chaH, stride); 
        #else //#ifdef __TEST_YUV_G
        ApplyHaar_new_neon(pSrc[1], pUVBuf[1], mul16W, chaH,  uvStride);
        ApplyHaar_new_neon(pSrc[2], pUVBuf[2], mul16W, chaH,  uvStride);
        #endif //#ifdef __TEST_YUV_G
        
        #else //#ifdef __TEST_ARM
        
        #ifdef __TEST_YUV_G
        ApplyHaar_Chroma_G(pSrc[1]+1, pUVBuf[1],mul16W<<1, chaH, stride);
        ApplyHaar_Chroma_G(pSrc[1], pUVBuf[2],mul16W<<1, chaH, stride); 
        #else
        ApplyHaar_new(pSrc[1], pUVBuf[1], mul16W, chaH,  uvStride);
        ApplyHaar_new(pSrc[2], pUVBuf[2], mul16W, chaH,  uvStride);
        #endif
        
        #endif//#ifdef __TEST_ARM  

        // 16整数倍剩余像素的处理
        if (0!= mul16Rev)
        {
            #ifdef __TEST_YUV_G
            ApplyHaar_Chroma_G(pSrc[1]+(mul16W<<1)+1, pUVBuf[1]+(mul16W>>1),mul16Rev<<1, chaH, stride);
            ApplyHaar_Chroma_G(pSrc[1]+(mul16W<<1), pUVBuf[2]+(mul16W>>1),mul16Rev<<1, chaH, stride); 
            #else
            ApplyHaar_new(pSrc[1]+mul16W, pUVBuf[1]+(mul16W>>1), mul16Rev, chaH,  uvStride);
            ApplyHaar_new(pSrc[2]+mul16W, pUVBuf[2]+(mul16W>>1), mul16Rev, chaH,  uvStride);
            #endif
        }

        #ifdef __GNUC__
        //=================进行线程间同步，防止数据没有读就被写修改====
        pthread_mutex_lock(&yReadedLock);
        yReaded++;
        pthread_mutex_unlock(&yReadedLock);
        
        while(1)//循环里需要加语句，否则会形成死循环
        {
            //printf("==waiting\n");
            if(yReaded >=8)
            {
                break;
            }
            
            usleep(1);    //功能把进程挂起一段时间， 单位是微秒
        }
        //=======================================================================
        #endif

        // ------------第二层小波正变换------------
        mul16W2    = wDiv4 & 0xfffffff0;
        mul16Rev2  = wDiv4 - mul16W2;
        
        // 宽为16整数倍的处理
#ifdef __TEST_ARM
        ApplyHaar_new_neon(pUVBuf[1], pUVHaar2Buf[1], mul16W2, hDiv4,  strideDiv4);                
        ApplyHaar_new_neon(pUVBuf[2], pUVHaar2Buf[2], mul16W2, hDiv4,  strideDiv4);
#else
        ApplyHaar_new(pUVBuf[1], pUVHaar2Buf[1], mul16W2, hDiv4,  strideDiv4);
        ApplyHaar_new(pUVBuf[2], pUVHaar2Buf[2], mul16W2, hDiv4,  strideDiv4);
#endif
        
        // 16整数倍剩余像素的处理
        if (0!= mul16Rev2)
        {
            ApplyHaar_new(pUVBuf[1]+mul16W2, pUVHaar2Buf[1]+(mul16W2>>1), mul16Rev2, hDiv4,  strideDiv4);
            ApplyHaar_new(pUVBuf[2]+mul16W2, pUVHaar2Buf[2]+(mul16W2>>1), mul16Rev2, hDiv4,  strideDiv4);
        }
        
        pEdge2    = pMemAlign;
        pMemAlign += wDiv8 *hDiv8;
        pEdge3    = pMemAlign;
        pMemAlign += wDiv8 *hDiv8;
        
        {
            UINT8 *pucEdgeUL = pEdge2,*pucEdgeVL = pEdge3;
            INT32 x,y;
            
            #ifdef __TEST_ARM
            CalcSobel_neon(pUVHaar2Buf[1],pEdge2,wDiv8 , hDiv8,strideDiv8,wDiv8);
            CalcSobel_neon(pUVHaar2Buf[2],pEdge3,wDiv8 , hDiv8,strideDiv8,wDiv8);
            #else
            CalcSobel(pUVHaar2Buf[1],pEdge2,wDiv8 , hDiv8,strideDiv8, wDiv8);
            CalcSobel(pUVHaar2Buf[2],pEdge3,wDiv8 , hDiv8,strideDiv8,wDiv8);  
            #endif 

            for(y = 0 ; y < hDiv8; y++)
            {
                for(x = 0 ; x < wDiv8; x++)
                {
                    pucEdgeVL[x] = MAX2(pucEdgeUL[x], pucEdgeVL[x]);
                }

                pucEdgeUL += wDiv8;
                pucEdgeVL += wDiv8;
            }
        }

        //使用亮度的边缘
        MeanDown2x(pEdge, pEdge2, wDiv4 , hDiv4,wDiv4, wDiv8);
        
        for(planeIdx = 1; planeIdx < 3 ; planeIdx++)
        {        
            INT32 nrEdgeTH = 70;
            INT32 nrUVEdgeTH = 128;
            INT32 nrTH         = 8;  
            INT32 protectLevel = 0;//高频处理如果把小值滤掉，渐变过程比较容易出现跳变形成阶梯
            
            if(nrUVRatio < -50)
            {
                nrUVEdgeTH = MAX2(nrUVEdgeTH * (105 + nrUVRatio)/55,70);
            }

            if(nrUVRatio < -90)
            {
                protectLevel = 3;
            }
            else if(nrUVRatio < -80)
            {
                protectLevel = 2;
            }
            else if(nrUVRatio < -70)
            {
                protectLevel = 1;
            }
            
            FastDn2d(pstFastDn2d,planeIdx,pUVHaar2Buf[planeIdx], pUVHaar2Buf[planeIdx], wDiv8,  hDiv8,  strideDiv8);
            FastDn2dInvert(pstFastDn2d,planeIdx,pUVHaar2Buf[planeIdx], pUVHaar2Buf[planeIdx],wDiv8,  hDiv8,  strideDiv8);

            
        #ifdef __TEST_ARM
        UpScale_neon(pUVHaar2Buf[planeIdx], hDiv8, wDiv8,strideDiv4,pUpScale1);
        UpScale_neon(pUpScale1, hDiv4, wDiv4,strideDiv2,puvscale[planeIdx]);
        #else
        UpScale(pUVHaar2Buf[planeIdx], hDiv8, wDiv8,strideDiv4,pUpScale1);
        UpScale(pUpScale1, hDiv4, wDiv4,strideDiv2,puvscale[planeIdx]);
        #endif
        
            {
                UINT8 * pSrcH,  * pSrcV, *pSrcHH;
                nrEdgeTH = 40;//60;
                nrTH     = 10;

                if(nrUVHFRatio < -60)
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrUVHFRatio)/45;
                    nrTH = nrTH * (105 + nrUVHFRatio)/45;
                }
                else
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrUVHFRatio)/45;
                    nrTH = nrTH * (105 + nrUVHFRatio)/45;
                }
            
                #ifdef __DN_COEFS_TAB
                pDnCoef = DN2DCOEF_TH8;
                #else
                CalcDn2dCoefs(pDnCoef, nrTH);   
                #endif
                
                pSrcH = pUVHaar2Buf[planeIdx] + hDiv8*strideDiv8;
                pSrcV = pUVHaar2Buf[planeIdx] + ((hDiv8*strideDiv8)<<1);

                #ifdef __TEST_ARM
                HFNR_LUT_Edge_neon(pSrcH,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);
                HFNR_LUT_Edge_neon(pSrcV,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);                
                #else
                HFNR_LUT_Edge(pSrcH,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);
                HFNR_LUT_Edge(pSrcV,pDnCoef,pEdge2,pEdge3, wDiv8, hDiv8,  strideDiv8,nrEdgeTH,nrUVEdgeTH,protectLevel);
                #endif
                
                pSrcHH = pUVHaar2Buf[planeIdx] + ((hDiv8*strideDiv8)<<1) + hDiv8*strideDiv8;
                memset(pSrcHH,128,hDiv8*strideDiv8);

            }

        }
        
        // ------------第二层小波逆变换------------
        mul16W2    = wDiv4 & 0xfffffff0;
        mul16Rev2  = wDiv4 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pUVHaar2Buf[1], pUVBuf[1],  mul16W2, hDiv4,  strideDiv4);
        ApplyInverseHaar_new_neon(pUVHaar2Buf[2], pUVBuf[2],  mul16W2, hDiv4,  strideDiv4);
        #else
        ApplyInverseHaar_new(pUVHaar2Buf[1], pUVBuf[1],  mul16W2, hDiv4,  strideDiv4);
        ApplyInverseHaar_new(pUVHaar2Buf[2], pUVBuf[2],  mul16W2, hDiv4,  strideDiv4); 
        #endif

        // 16整数倍剩余像素的处理
        if (0!= mul16Rev2)
        {
            ApplyInverseHaar_new(pUVHaar2Buf[1]+(mul16W2>>1), pUVBuf[1]+mul16W2, mul16Rev2, hDiv4,  strideDiv4); 
            ApplyInverseHaar_new(pUVHaar2Buf[2]+(mul16W2>>1), pUVBuf[2]+mul16W2, mul16Rev2, hDiv4,  strideDiv4);
        }

        {
            UINT8 *pucTUbuf = pUVHaar2Buf[1];
            UINT8 *pucTVbuf = pUVHaar2Buf[2];
            UINT8 *pucEdgeUL = pucTUbuf,*pucEdgeVL = pucTVbuf;
            INT32 x,y;

/*
            #ifdef __TEST_ARM
            CalcSobel_neon(pUVBuf[1],pucTUbuf,wDiv4 , hDiv4,strideDiv4, wDiv4);
            CalcSobel_neon(pUVBuf[2],pucTVbuf,wDiv4 , hDiv4,strideDiv4,wDiv4);
            #else
            CalcSobel(pUVBuf[1],pucTUbuf,wDiv4 , hDiv4,strideDiv4, wDiv4);
            CalcSobel(pUVBuf[2],pucTVbuf,wDiv4 , hDiv4,strideDiv4,wDiv4);  
            #endif

            for(y = 0 ; y < hDiv4; y++)
            {
                for(x = 0 ; x < wDiv4; x++)
                {
                    pucEdgeVL[x] = MAX2(pucEdgeUL[x], pucEdgeVL[x]);
                }

                pucEdgeUL += wDiv4;
                pucEdgeVL += wDiv4;
            }
*/
            for(planeIdx = 1; planeIdx < 3 ; planeIdx++)
            {        
                INT32 nrTH         = 8;  

                /*==========================================================
                    最佳版本需要开启下面两个滤波步骤
                    但是为了提升速度可以只开启FastDn2dUVEdge只滤一遍
                    如果需要更进一步提升速度，则两个步骤均关闭
                ===========================================================*/
                FastDn2d(pstFastDn2d,planeIdx,pUVBuf[planeIdx], pUVBuf[planeIdx],wDiv4, hDiv4,  strideDiv4);
                FastDn2dInvert(pstFastDn2d,planeIdx,pUVBuf[planeIdx], pUVBuf[planeIdx],wDiv4, hDiv4,  strideDiv4);

                {
                    UINT8 * pSrcH,  * pSrcV, *pSrcHH;
                    nrTH = 15;
                    
                    if(nrUVHFRatio < -80)
                    {
                        nrTH = nrTH * (105 + nrUVHFRatio)/25;
                    }
                    else if(nrUVHFRatio > -60)
                    {
                        nrTH = nrTH * (105 + nrUVHFRatio)/35;
                    }  
                    
                    pSrcH = pUVBuf[planeIdx] + hDiv4* strideDiv4;
                    pSrcV = pUVBuf[planeIdx] + ((hDiv4* strideDiv4) <<1);

                    #if 0//def __TEST_ARM
                    HFNR_Edge_neon(pSrcH, pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrTH,nrEdgeTH,nrUVEdgeTH,protectLevel);
                    HFNR_Edge_neon(pSrcV, pEdge,pucTVbuf,wDiv4, hDiv4,  strideDiv4,nrTH,nrEdgeTH,nrUVEdgeTH,protectLevel);
                    #else
                    HFNR_Edge_night(pSrcH,wDiv4, hDiv4, strideDiv4,nrTH);
                    HFNR_Edge_night(pSrcV,wDiv4, hDiv4, strideDiv4,nrTH);
                    #endif
                    
                    pSrcHH = pUVBuf[planeIdx] + ((hDiv4* strideDiv4) <<1) + hDiv4* strideDiv4;
                    memset(pSrcHH,128,hDiv4* strideDiv4);    
                }

                // ------------第一层小波逆变换------------
                mul16W     = chaW & 0xfffffff0;
                mul16Rev   = chaW - mul16W;
        
                #ifdef __TEST_ARM
                #ifdef __GNUC__
                {
                    UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[planeIdx];
                    INT32 dstH = phHandle->stParamIn.pstPicOut->height>>1;
                    INT32 srcH  = phHandle->stParamIn.pstPicIn->height>>1;
                    INT32 threadOffset = phHandle->threadOffset>>2;//色度交叉8个像素
                    
                    ApplyInverseHaar_new_thread_neon(pUVBuf[planeIdx],pDst ,threadOffset,srcH,mul16W, dstH,  uvStride);
                    
                    // 16整数倍剩余像素的处理
                    if (0 != mul16Rev)
                    {
                        ApplyInverseHaar_thread_new(pUVBuf[planeIdx]+(mul16W>>1),pDst+mul16W , threadOffset,srcH,mul16Rev, dstH,  uvStride);
                    }

                    //图像融合
                    {
                        UINT8 *pSacleL = puvscale[planeIdx] + threadOffset;
                        INT32 x,y;
                        UINT8 alpha = 75;

                        if(nrLumaHFRatio < -50)
                        {
                            alpha = MIN2(alpha - 128*(3*nrLumaHFRatio + 150)/500,128);
                        }

                        AlphaBlend(pDst, pSacleL,  chaW, dstH,  uvStride,  alpha);
                    }
                }
                #else
                ApplyInverseHaar_new_neon(pUVBuf[planeIdx], pSrc[planeIdx], mul16W, chaH,  uvStride);

                // 16整数倍剩余像素的处理
                if (0 != mul16Rev)
                {
                    ApplyInverseHaar_new(pUVBuf[planeIdx]+(mul16W>>1), pSrc[planeIdx]+mul16W, mul16Rev, chaH,  uvStride);
                }
                
                //图像融合
                {
                    UINT8 *pSacleL = puvscale[planeIdx];
                    INT32 x,y;
                    UINT8 alpha = 75;

                    if(nrLumaHFRatio < -50)
                    {
                        alpha = MIN2(alpha - 128*(3*nrLumaHFRatio + 150)/500,128);
                    }

                    AlphaBlend(pSrc[planeIdx], pSacleL,  chaW, chaH,  uvStride,  alpha);
                }                
                #endif
                
                #else
                
                ApplyInverseHaar_new(pUVBuf[planeIdx], pSrc[planeIdx], mul16W, chaH,  uvStride);
                // 16整数倍剩余像素的处理
                if (0 != mul16Rev)
                {
                    ApplyInverseHaar_new(pUVBuf[planeIdx]+(mul16W>>1), pSrc[planeIdx]+mul16W, mul16Rev, chaH,  uvStride);
                }
                
                //图像融合
                {
                    UINT8 *pSacleL = puvscale[planeIdx];
                    INT32 x,y;
                    UINT8 alpha = 75;

                    if(nrLumaHFRatio < -50)
                    {
                        alpha = MIN2(alpha - 128*(3*nrLumaHFRatio + 150)/500,128);
                    }

                    AlphaBlend(pSrc[planeIdx], pSacleL,  chaW, chaH,  uvStride,  alpha);
                }                
                #endif

            }    
        }
    }

    return eRet;
}    



//将与原图加权的算法放到1次小波时做
E_ERROR Nr_MaincallMix2(HANDLE_NR *phHandle) 
{
    //ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    //ST_PICTURE *pstPicOut = phHandle->stParamIn.pstPicOut;
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf  = phHandle->pBuf;
    
    UINT8 *pBlur;
    UINT8 *pEdge;
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?

    INT32  nrLumaRatio = phHandle->nrRatio[0];
    //INT32  nrUVRatio = phHandle->nrUVRatio;
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
    //INT32  nrUVHFRatio = phHandle->nrUVHFRatio;
    INT32  nrDetailRatio = phHandle->nrDetailRatio;
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2         = height>> 1;
    INT32 chaW         = wDiv2;
    INT32 chaH          = hDiv2;
    INT32 uvStride     = stride >> 1; 
        
    INT32 planeIdx;     

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    //INT32 strideDiv8 = stride >> 3;

    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    UINT8 *pCopY;//(UINT8 *)malloc(strideDiv2* hDiv2);

    E_ERROR  eRet = SUCCESS;
    pCopY = pBuf;
    pBuf    += strideDiv2* hDiv2;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    #if 0
    {
        FILE *pfYUV = fopen("yuv_in.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }

    memcpy(pCopY,pBuf,(strideDiv2* hDiv2));
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2, pSrc[0]+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }
    
    //pSrc[0] 后面3/4大小的空间闲置
    pMemAlign = pSrc[0] + strideDiv2 * hDiv2; 
    pEdge        = pMemAlign;//wDiv4 * iHn这块空间一直到最后都不能动
    pMemAlign += wDiv2 * hDiv2;
    pBlur          = pMemAlign;

    //step2 : edge detect

    //检测LL的边缘信息
    #ifdef __TEST_ARM
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3_neon(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }

    CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
    #else
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }
    
    CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
    #endif  

    //检测HL\LH的边缘信息,将三个平面的边缘信息合并
    {
        UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
        UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
        UINT8 *pSrcV, *pSrcH;
        INT32 x,y;
        
#ifdef __TEST_ARM
        UINT32 wDiv4_neon = wDiv4 >> 4;    
        UINT32 n = wDiv4 - (wDiv4_neon << 4);
        uint8x16_t pSrcH_neon, pSrcV_neon, temp;
#endif


        pSrcH  = pSrc[0] + hDiv4 * strideDiv4;// 水平
        pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4) << 1);// 垂直

        #ifdef __TEST_ARM
        {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4); 
        
        pSrcV = pEdgebuf;
        pSrcH = pEdge;        
        
        for(y = 0; y < hDiv4; y++)
        {
            for(x = 0; x < wDiv4_neon; x++)
            {
                pSrcH_neon = vld1q_u8(pSrcH);
                pSrcV_neon = vld1q_u8(pSrcV);
                temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
                vst1q_u8(pSrcH, temp);

                pSrcH += 16;
                pSrcV += 16;
            }
            pSrcH += n;
            pSrcV += n;
        }
        
        if(0 != n)
        {
            pSrcV = pEdgebuf + (wDiv4_neon << 4);
            pSrcH = pEdge + (wDiv4_neon << 4);
            for(y = 0; y < hDiv4;y++)
            {
                for(x = 0; x < n; x++)
                {
                    pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
            }
        }
        
       #else
       {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);      
        

        pSrcV = pEdgebuf;
        pSrcH = pEdge;
        for(y = 0; y < hDiv4;y++)
        {
            for(x = 0; x < wDiv4; x++)
            {
                pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
            }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
        }
       #endif 

    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_edgeLL2.yuv","wb");
        
        fwrite(pEdge,1,wDiv4* hDiv4,pfYUV);
        fwrite(pEdge,1,wDiv4* hDiv4/4,pfYUV);
        fwrite(pEdge,1,wDiv4* hDiv4/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    /*========================================================
                                                              亮度去噪
       ========================================================*/
    if(bIsNrLuma) 
    { 
        INT32 nrEdgeTH = 60;
        INT32 nrTH;

        nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaRatio)/45,180); 
    
        //-------------------LL 降噪--------------------
        //使用传导降噪渐变区域阶梯现象不明显
        if(-100 != nrLumaRatio)
        {
            UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
            UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
            
            INT32  nSigmaS = (nrEdgeTH + 30) / 60;
            INT32  nSigmaR = ((nrEdgeTH +8) >>3) >>1;
            
            BilateralFilter(pSrc[0], hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
        }
        
        //-------------------HL LH HH 降噪--------------------
        {
            UINT8 * pSrcH,  * pSrcV, *pSrcHH;
            nrEdgeTH = 60;  //60
            nrTH = 15;

            nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
            }

            CalcDn2dCoefs(pDnCoef, nrTH);

            pSrcH  = pSrc[0] + hDiv4 * strideDiv4;
            pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4 )<< 1);
            pSrcHH =  pSrc[0] + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

            #ifdef __TEST_ARM
            HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #else
            HFNR_LUT_Edge_luma(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #endif

            // ------------第二层小波逆变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #endif
            
            // 16整数倍剩余像素的处理
            if (0 != mul16Rev2)
            {
                ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
            }

            //边缘检测
            memcpy(pSrc[0],pBuf,hDiv2*strideDiv2);

            #ifdef __TEST_ARM
            MedianFilter_neon(pSrc[0], hDiv2,  wDiv2, pEdge, 3);
            CalcSobel_neon(pSrc[0],pEdge,wDiv2,hDiv2,strideDiv2, wDiv2);
            #else
            MedianFilter(pSrc[0], hDiv2,  wDiv2, pEdge, 3);
            CalcSobel(pSrc[0],pEdge,wDiv2,hDiv2,strideDiv2, wDiv2);
            #endif

            {
                //UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                UINT8 *pEdgebuf = pSrc[0];
                INT32 x,y;
                
                #ifdef __TEST_ARM
                UINT32 wDiv2_neon = wDiv2 >> 4;    
                UINT32 n = wDiv2 - (wDiv2_neon << 4);
                uint8x16_t pSrcH_neon, pSrcV_neon, temp;
                #endif
                
                pSrcH  = pBuf + hDiv2 * strideDiv2;// 水平
                pSrcV  = pBuf + ((hDiv2 * strideDiv2) << 1);// 垂直

                CalcSobel_HF( pSrcV,  pSrcH,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);

                pSrcV = pEdgebuf;
                pSrcH = pEdge;
                
                #ifdef __TEST_ARM
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2_neon; x++)
                    {
                        pSrcH_neon = vld1q_u8(pSrcH);
                        pSrcV_neon = vld1q_u8(pSrcV);
                        temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
                        vst1q_u8(pSrcH, temp);

                        pSrcH += 16;
                        pSrcV += 16;
                    }
                    pSrcH += n;
                    pSrcV += n;
                }
                
                if(0 != n)
                {
                    pSrcV = pEdgebuf + (wDiv2_neon << 4);
                    pSrcH = pEdge + (wDiv2_neon << 4);
                    for(y = 0; y < hDiv2; y++)
                    {
                        for(x = 0; x < n; x++)
                        {
                            pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                        }
                        pSrcV += wDiv2;
                        pSrcH += wDiv2;
                    }
                } 
                #else
                for(y = 0; y < hDiv2;y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                    }
                    
                    pSrcV += wDiv2;
                    pSrcH += wDiv2;
                }
                #endif

                #if 0
                {
                    FILE *pfYUV = fopen("yuv_yedge_merge.yuv","wb");
                    
                    fwrite(pEdge,1,(wDiv2* hDiv2),pfYUV);
                    fwrite(pEdge,1,(wDiv2* hDiv2)/4,pfYUV);
                    fwrite(pEdge,1,(wDiv2* hDiv2)/4,pfYUV);
                    fclose(pfYUV);
                }
                #endif

            }
                
            if(nrDetailRatio > -100)
            {
                INT32 x,y;
                UINT8 *pTSrcL,*pTCopyL,*pEdgeL;
                INT32 weight;
                INT32  detailRecover = 40 * (100 - nrDetailRatio)/100;
                INT16   maxWeight   = 32767; //(1L<<15);
                INT32   edgeWeight  = maxWeight/detailRecover; //

                pTSrcL    = pBuf;
                pTCopyL = pCopY;
                pEdgeL   = pEdge;
                
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        if(pEdgeL[x] < detailRecover)
                        {
                            weight = MAX2(pEdgeL[x]*edgeWeight,6553);
                            //weight = MAX2(pEdgeL[x]*edgeWeight,0);
                            pTSrcL[x] = MIN2(((pTSrcL[x] * (maxWeight - weight) + pTCopyL[x] * weight + 32768) >> 15),255);
                        }
                        else
                        {
                            pTSrcL[x] = pTCopyL[x];
                        }
                    }

                    pTSrcL    += strideDiv2;
                    pTCopyL += strideDiv2;
                    pEdgeL    += wDiv2;
                }

            }
            /*
            else
            {
                INT32 x,y;
                UINT8 *pTSrcL,*pTCopyL;
                INT32 weight;
                INT16   maxWeight   = 32767;

                pTSrcL    = pBuf;
                pTCopyL = pCopY;
                
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        weight = 6553;
                        //weight = MAX2(pEdgeL[x]*edgeWeight,0);
                        pTSrcL[x] = MIN2((pTSrcL[x] * (maxWeight - weight) + pTCopyL[x] * weight + 32768) >> 15,255);
                    }

                    pTSrcL    += strideDiv2;
                    pTCopyL += strideDiv2;
                }

            }
            */
    
            //需要重新检测边缘
            {
                nrEdgeTH = 100;
                nrTH = 20;

                nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

                if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
                }  
                else if(nrLumaHFRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
                }

                pSrcH    = pBuf + hDiv2 * strideDiv2;
                pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
                pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

                #ifdef __TEST_ARM
                HFNR_Edge_luma_neon(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #else
                HFNR_Edge_luma(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #endif
            }
        }
    }
    else
    {
        // 不会进入，代码没改
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }
    }
     
    /*========================================================
                                                              色度去噪
       ========================================================*/
    if(bIsNrCha)
    {
        UINT8 *pDownBuf = pSrc[0];  //chroma down2x2
        UINT8 *pTBuf =  pSrc[0] + wDiv4* hDiv4; //chroma down2x2 bilateral
        INT32 nSigmaS ,nSigmaR;
        
        for(planeIdx = 1; planeIdx < 3 ; planeIdx++)
        {
            INT32 nrUVEdgeTH = 128;

            nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[planeIdx]) / 55;

            nSigmaS = (nrUVEdgeTH*3 + 64) / 128;
            nSigmaR = ((nrUVEdgeTH+8) >>3) >>1;

            //printf("%d %d\n",nSigmaS,nSigmaR);

#ifdef __TEST_ARM
            DownScale_neon(pSrc[planeIdx], chaH, chaW,uvStride, pDownBuf);
#else
            DownScale(pSrc[planeIdx], chaH, chaW,uvStride, pDownBuf);
#endif
        
            BilateralFilter(pDownBuf, chaH>>1, chaW>>1, nSigmaS, nSigmaR,pTBuf);
            /*
            //Luma down4x4
            UINT8 *pLuma =  pTBuf + wDiv4* hDiv4;   //luma down4x4, size of "chroma down2x2"
            DownScale(pBuf, chaH, chaW, uvStride, pLuma);
            
            printf("chaH = %d, chaW = %d, hDiv4 = %d, wDiv4 = %d, uvStride = %d.\n", 
                chaH, chaW, hDiv4, wDiv4, uvStride);

            FILE *pfYUV = fopen("/data/yuv_out.yuv","wb");
            if(pfYUV)
            {
                fwrite(pLuma,1,hDiv4*wDiv4,pfYUV);
                fwrite(pDownBuf,1,hDiv4*wDiv4,pfYUV);
                fclose(pfYUV);
            }
            
            //Cross Bilateral Filter
            CrossBilateralFilter(pDownBuf, chaH>>1, chaW>>1, nSigmaS, nSigmaR, pTBuf, pLuma);
            */
#ifdef __TEST_ARM
            UpScale_neon(pDownBuf, chaH>>1, chaW>>1, uvStride, pSrc[planeIdx]);
#else
            UpScale(pDownBuf, chaH>>1, chaW>>1, uvStride, pSrc[planeIdx]);
#endif
        }

    }
    
    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);
    #else
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    #endif
    
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_out.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    
   // free(pCopY);
    
    return eRet;
    
}

//将与原图加权的算法放到1次小波时做
E_ERROR Nr_MaincallMix2_m(HANDLE_NR *phHandle) 
{
    //ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    //ST_PICTURE *pstPicOut = phHandle->stParamIn.pstPicOut;
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf  = phHandle->pBuf;
    
    UINT8 *pBlur;
    UINT8 *pEdge;
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?

    INT32  nrLumaRatio = phHandle->nrRatio[0];
    //INT32  nrUVRatio = phHandle->nrUVRatio;
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
    //INT32  nrUVHFRatio = phHandle->nrUVHFRatio;
    INT32  nrDetailRatio = phHandle->nrDetailRatio;
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2         = height>> 1;
    INT32 chaW         = wDiv2;
    INT32 chaH          = hDiv2;
    INT32 uvStride     = stride >> 1; 
        
    INT32 planeIdx;     

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    //INT32 strideDiv8 = stride >> 3;

    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    UINT8 *pCopY;//(UINT8 *)malloc(strideDiv2* hDiv2);

    E_ERROR  eRet = SUCCESS;
    pCopY = pBuf;
    pBuf    += strideDiv2* hDiv2;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    #if 0
    {
        FILE *pfYUV = fopen("yuv_in.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }

    memcpy(pCopY,pBuf,(strideDiv2* hDiv2));
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2, pSrc[0]+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }
    
    //pSrc[0] 后面3/4大小的空间闲置
    pMemAlign = pSrc[0] + strideDiv2 * hDiv2; 
    pEdge        = pMemAlign;//wDiv4 * iHn这块空间一直到最后都不能动
    pMemAlign += wDiv2 * hDiv2;
    pBlur          = pMemAlign;

    //step2 : edge detect

    //检测LL的边缘信息
    #ifdef __TEST_ARM
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3_neon(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }

    CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
    #else
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }
    
    CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
    #endif  

    //检测HL\LH的边缘信息,将三个平面的边缘信息合并
    {
        UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
        UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
        UINT8 *pSrcV, *pSrcH;
        INT32 x,y;
        
#ifdef __TEST_ARM
        UINT32 wDiv4_neon = wDiv4 >> 4;    
        UINT32 n = wDiv4 - (wDiv4_neon << 4);
        uint8x16_t pSrcH_neon, pSrcV_neon, temp;
#endif


        pSrcH  = pSrc[0] + hDiv4 * strideDiv4;// 水平
        pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4) << 1);// 垂直

        #ifdef __TEST_ARM
        {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4); 
        
        pSrcV = pEdgebuf;
        pSrcH = pEdge;        
        
        for(y = 0; y < hDiv4; y++)
        {
            for(x = 0; x < wDiv4_neon; x++)
            {
                pSrcH_neon = vld1q_u8(pSrcH);
                pSrcV_neon = vld1q_u8(pSrcV);
                temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
                vst1q_u8(pSrcH, temp);

                pSrcH += 16;
                pSrcV += 16;
            }
            pSrcH += n;
            pSrcV += n;
        }
        
        if(0 != n)
        {
            pSrcV = pEdgebuf + (wDiv4_neon << 4);
            pSrcH = pEdge + (wDiv4_neon << 4);
            for(y = 0; y < hDiv4;y++)
            {
                for(x = 0; x < n; x++)
                {
                    pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
            }
        }
        
       #else
       {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);      
        

        pSrcV = pEdgebuf;
        pSrcH = pEdge;
        for(y = 0; y < hDiv4;y++)
        {
            for(x = 0; x < wDiv4; x++)
            {
                pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
            }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
        }
       #endif 

    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_edgeLL2.yuv","wb");
        
        fwrite(pEdge,1,wDiv4* hDiv4,pfYUV);
        fwrite(pEdge,1,wDiv4* hDiv4/4,pfYUV);
        fwrite(pEdge,1,wDiv4* hDiv4/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    /*========================================================
                                                              亮度去噪
       ========================================================*/
    if(bIsNrLuma) 
    { 
        INT32 nrEdgeTH = 60;
        INT32 nrTH;

        nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaRatio)/45,180); 
    
        //-------------------LL 降噪--------------------
        //使用传导降噪渐变区域阶梯现象不明显
        if(-100 != nrLumaRatio)
        {
            UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
            UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
            
            INT32  nSigmaS = (nrEdgeTH + 30) / 60;
            INT32  nSigmaR = ((nrEdgeTH +8) >>3) >>1;
            
            BilateralFilter(pSrc[0], hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
        }
        
        //-------------------HL LH HH 降噪--------------------
        {
            UINT8 * pSrcH,  * pSrcV, *pSrcHH;
            nrEdgeTH = 60;  //60
            nrTH = 15;

            nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
            }

            CalcDn2dCoefs(pDnCoef, nrTH);

            pSrcH  = pSrc[0] + hDiv4 * strideDiv4;
            pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4 )<< 1);
            pSrcHH =  pSrc[0] + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

            #ifdef __TEST_ARM
            HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #else
            HFNR_LUT_Edge_luma(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #endif

            // ------------第二层小波逆变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #endif
            
            // 16整数倍剩余像素的处理
            if (0 != mul16Rev2)
            {
                ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
            }

            //边缘检测
            memcpy(pSrc[0],pBuf,hDiv2*strideDiv2);

            #ifdef __TEST_ARM
            MedianFilter_neon(pSrc[0], hDiv2,  wDiv2, pEdge, 3);
            CalcSobel_neon(pSrc[0],pEdge,wDiv2,hDiv2,strideDiv2, wDiv2);
            #else
            MedianFilter(pSrc[0], hDiv2,  wDiv2, pEdge, 3);
            CalcSobel(pSrc[0],pEdge,wDiv2,hDiv2,strideDiv2, wDiv2);
            #endif

            {
                //UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                UINT8 *pEdgebuf = pSrc[0];
                INT32 x,y;
                
                #ifdef __TEST_ARM
                UINT32 wDiv2_neon = wDiv2 >> 4;    
                UINT32 n = wDiv2 - (wDiv2_neon << 4);
                uint8x16_t pSrcH_neon, pSrcV_neon, temp;
                #endif
                
                pSrcH  = pBuf + hDiv2 * strideDiv2;// 水平
                pSrcV  = pBuf + ((hDiv2 * strideDiv2) << 1);// 垂直

                CalcSobel_HF( pSrcV,  pSrcH,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);

                pSrcV = pEdgebuf;
                pSrcH = pEdge;
                
                #ifdef __TEST_ARM
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2_neon; x++)
                    {
                        pSrcH_neon = vld1q_u8(pSrcH);
                        pSrcV_neon = vld1q_u8(pSrcV);
                        temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
                        vst1q_u8(pSrcH, temp);

                        pSrcH += 16;
                        pSrcV += 16;
                    }
                    pSrcH += n;
                    pSrcV += n;
                }
                
                if(0 != n)
                {
                    pSrcV = pEdgebuf + (wDiv2_neon << 4);
                    pSrcH = pEdge + (wDiv2_neon << 4);
                    for(y = 0; y < hDiv2; y++)
                    {
                        for(x = 0; x < n; x++)
                        {
                            pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                        }
                        pSrcV += wDiv2;
                        pSrcH += wDiv2;
                    }
                } 
                #else
                for(y = 0; y < hDiv2;y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                    }
                    
                    pSrcV += wDiv2;
                    pSrcH += wDiv2;
                }
                #endif

                #if 0
                {
                    FILE *pfYUV = fopen("yuv_yedge_merge.yuv","wb");
                    
                    fwrite(pEdge,1,(wDiv2* hDiv2),pfYUV);
                    fwrite(pEdge,1,(wDiv2* hDiv2)/4,pfYUV);
                    fwrite(pEdge,1,(wDiv2* hDiv2)/4,pfYUV);
                    fclose(pfYUV);
                }
                #endif

            }
                
            if(-100 != nrDetailRatio)
            {
                INT32 x,y;
                UINT8 *pTSrcL,*pTCopyL,*pEdgeL;
                INT32 weight;
                INT32  detailRecover = 40 * (100 - nrDetailRatio)/100;
                INT16   maxWeight   = 32767; //(1L<<15);
                INT32   edgeWeight  = maxWeight/detailRecover; //

                pTSrcL    = pBuf;
                pTCopyL = pCopY;
                pEdgeL   = pEdge;
                
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        if(pEdgeL[x] < detailRecover)
                        {
                            weight = MAX2(pEdgeL[x]*edgeWeight,6553);
                            //weight = MAX2(pEdgeL[x]*edgeWeight,0);
                            pTSrcL[x] = MIN2(((pTSrcL[x] * (maxWeight - weight) + pTCopyL[x] * weight + 32768) >> 15),255);
                        }
                        else
                        {
                            pTSrcL[x] = pTCopyL[x];
                        }
                    }

                    pTSrcL    += strideDiv2;
                    pTCopyL += strideDiv2;
                    pEdgeL    += wDiv2;
                }

            }
            else
            {
                INT32 x,y;
                UINT8 *pTSrcL,*pTCopyL;
                INT32 weight;
                INT16   maxWeight   = 32767;

                pTSrcL    = pBuf;
                pTCopyL = pCopY;
                
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        weight = 6553;
                        //weight = MAX2(pEdgeL[x]*edgeWeight,0);
                        pTSrcL[x] = MIN2((pTSrcL[x] * (maxWeight - weight) + pTCopyL[x] * weight + 32768) >> 15,255);
                    }

                    pTSrcL    += strideDiv2;
                    pTCopyL += strideDiv2;
                }

            }
    
            //需要重新检测边缘
            {
                nrEdgeTH = 100;
                nrTH = 20;

                nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

                if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
                }  
                else if(nrLumaHFRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
                }

                pSrcH    = pBuf + hDiv2 * strideDiv2;
                pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
                pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

                #ifdef __TEST_ARM
                HFNR_Edge_luma_neon(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #else
                HFNR_Edge_luma(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #endif
            }
        }
    }
    else
    {
        // 不会进入，代码没改
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }
    }
     
    /*========================================================
                                                              色度去噪
       ========================================================*/
    if(bIsNrCha)
    {
        UINT8 *pDownBuf = pSrc[0];
        UINT8 *pTBuf        =  pSrc[0] + wDiv4* hDiv4;
        INT32 nSigmaS ,nSigmaR;
        
        for(planeIdx = 1; planeIdx < 3 ; planeIdx++)
        {
            INT32 nrUVEdgeTH = 128;

            //if(phHandle->nrRatio[planeIdx] < -50)
            nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[planeIdx]) / 55;

            nSigmaS = (nrUVEdgeTH*3 + 64) / 128;
            nSigmaR = ((nrUVEdgeTH+8) >>3) >>1;

            //printf("%d %d\n",nSigmaS,nSigmaR);

#ifdef __TEST_ARM
            DownScale_neon(pSrc[planeIdx], chaH, chaW,uvStride, pDownBuf);
#else
            DownScale(pSrc[planeIdx], chaH, chaW,uvStride, pDownBuf);
#endif
        
            BilateralFilter(pDownBuf, chaH>>1, chaW>>1, nSigmaS, nSigmaR,pTBuf);

#ifdef __TEST_ARM
            UpScale_neon(pDownBuf, chaH>>1, chaW>>1, uvStride, pSrc[planeIdx]);
#else
            UpScale(pDownBuf, chaH>>1, chaW>>1, uvStride, pSrc[planeIdx]);
#endif
        }

    }
    
    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);
    #else
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    #endif
    
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_out.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    
   // free(pCopY);
    
    return eRet;
    
}


E_ERROR Nr_Maincall_Y(HANDLE_NR *phHandle) 
{
    ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    //ST_PICTURE *pstPicOut = phHandle->stParamIn.pstPicOut;
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf  = phHandle->pBuf;
    
    UINT8 *pBlur;
    UINT8 *pEdge;
    UINT8 *pEdge2;
//    UINT8 *pEdge3;    //comment by wanghao
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
//    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?    //comment by wanghao because 529

    INT32  nrLumaRatio = phHandle->nrRatio[0];
    INT32  nrUVRatio = phHandle->nrRatio[1];
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
//    INT32  nrUVHFRatio = phHandle->nrUVHFRatio;    //comment by wanghao because 529
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2         = height>> 1;
//    INT32 chaW         = wDiv2;    //comment by wanghao because 529
//    INT32 chaH          = hDiv2;    //comment by wanghao because 529
//    INT32 uvStride     = stride >> 1;    //comment by wanghao because 529
        
//    INT32 planeIdx;    //comment by wanghao because 529

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
//    INT32 strideDiv8 = stride >> 3;    //comment by wanghao because 529

    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    E_ERROR  eRet = SUCCESS;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    #if 0
    {
        FILE *pfYUV = fopen("yuv_in.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    //mrc_cg_imageNoiseReduction(pSrc[0], pSrc[1], pSrc[2], height,width,  nrLumaRatio,  nrUVRatio);

    //return;
    
    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2, pSrc[0]+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }
    
    //pSrc[0] 后面3/4大小的空间闲置
    pMemAlign = pSrc[0] + strideDiv2 * hDiv2; 
    pEdge        = pMemAlign;//wDiv4 * iHn这块空间一直到最后都不能动
    pMemAlign += wDiv4 * hDiv4;
    pBlur          = pMemAlign;

    //step2 : edge detect

    //检测LL的边缘信息
    #ifdef __TEST_ARM
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3_neon(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }

    CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
    #else
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }
    
    CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
    #endif  

    //检测HL\LH的边缘信息,将三个平面的边缘信息合并
    {
        UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
        UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
        UINT8 *pSrcV, *pSrcH;
        INT32 x,y;

        pSrcH  = pSrc[0] + hDiv4 * strideDiv4;// 水平
        pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4) << 1);// 垂直

        #ifdef __TEST_ARM
        {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);  
        #else
       {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);      
        #endif 

        pSrcV = pEdgebuf;
        pSrcH = pEdge;
        for(y = 0; y < hDiv4;y++)
        {
            for(x = 0; x < wDiv4; x++)
            {
                pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
            }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
        }
    }

    {
        INT32 nrYStrength        = 20;//20;
        INT32 nrYDetailRatio    = 4915;// (4.8/8)
        INT32 nrUVStrength     = 21;
        INT32 nrUVDetailRatio = 4915;//2765;//nrUVStrength/3;
    
        nrYStrength            =   ((nrYStrength*((105 + nrLumaRatio) <<10)/105)>>10);
        nrUVStrength         =   ((nrUVStrength*((105 + nrUVRatio) <<10)/105)>>10);

        FastDn2dControl(pstFastDn2d, nrYStrength ,  nrYDetailRatio,  nrUVStrength,  nrUVDetailRatio);
    }
    
    /*========================================================
                                                              亮度去噪
       ========================================================*/
    if(bIsNrLuma)
    { 
        INT32 nrEdgeTH = 60;
        INT32 nrTH;

        if(nrLumaRatio < -80)
        {
            nrEdgeTH = nrEdgeTH * (105 + nrLumaRatio)/25;
        }
        else if(nrLumaRatio > -60)
        {
            nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaRatio)/35,180); 
        }
        
        //-------------------LL 降噪--------------------
        //使用传导降噪渐变区域阶梯现象不明显
        #if 1
        FastDn2dEdge(pstFastDn2d,0,pSrc[0], pSrc[0],pEdge,wDiv4,  hDiv4,  strideDiv4,nrEdgeTH);
        FastDn2dInvertEdge(pstFastDn2d,0,pSrc[0], pSrc[0],pEdge,wDiv4,  hDiv4,  strideDiv4,nrEdgeTH);
        #else
        {
            UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
            UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
            
            INT32 nSigmaS = 1;//(nrEdgeTH*2 + 30) / 60;
            INT32 nSigmaR = 3;//(nrEdgeTH + 6) / 12;  //(nrEdgeTH/6)>>1
            
            //GaussianBlur3x3(pSrc[0], pBlur, pEdgebuf, wDiv4, hDiv4, strideDiv4, strideDiv4, strideDiv4);
            //BrightWareBilateralFilter(y, pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pTemp1);
            BilateralFilter(pSrc[0], hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
            
            //BrightWareBilateralFilter(pSrc[0],pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
        }
        #endif


        //-------------------HL LH HH 降噪--------------------
        {
            UINT8 * pSrcH,  * pSrcV, *pSrcHH;
            nrEdgeTH = 60;  //60
            nrTH = 15;

            nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
            }

            CalcDn2dCoefs(pDnCoef, nrTH);

            pSrcH  = pSrc[0] + hDiv4 * strideDiv4;
            pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4 )<< 1);
            pSrcHH =  pSrc[0] + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

            #ifdef __TEST_ARM
            HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);            
            #else
            HFNR_LUT_Edge_luma(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #endif

            // ------------第二层小波逆变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #endif
            
            // 16整数倍剩余像素的处理
            if (0 != mul16Rev2)
            {
                ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
            }
            
            //需要重新检测边缘
            {
                #ifdef __TEST_ARM
                {
                    UINT8 *pBufTemp = pSrc[0];//第一次小波逆变换后pucSrc[0]前1/4大小的空间闲置
                    GaussianBlur3x3_neon(pBuf, pBlur, pBufTemp, wDiv2, hDiv2, strideDiv2, wDiv2, wDiv2);
                }

                pEdge2 = pSrc[0];//继续利用
                CalcSobel_neon(pBlur,pEdge2,wDiv2  , hDiv2,wDiv2 ,wDiv2 );                   
                #else
                {
                    UINT8 *pBufTemp = pSrc[0];//第一次小波逆变换后pucSrc[0]前1/4大小的空间闲置
                    GaussianBlur3x3(pBuf, pBlur, pBufTemp, wDiv2, hDiv2, strideDiv2, wDiv2, wDiv2);
                }

                pEdge2 = pSrc[0];//继续利用
                CalcSobel(pBlur,pEdge2,wDiv2  , hDiv2,wDiv2 ,wDiv2 );
                #endif  

                {
                    UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                    UINT8 *pEdgebuf = (UINT8 *)malloc(wDiv2* hDiv2);
                    INT32 x,y;
                    
                    pSrcH  = pBuf + hDiv2 * strideDiv2;// 水平
                    pSrcV  = pBuf + ((hDiv2 * strideDiv2) << 1);// 垂直

                    #ifdef __TEST_ARM
                    {
                        UINT8 *pBufTemp = pEdgebuf;
                        GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                        GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                    }

                    CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
                    #else
                    {
                        UINT8 *pBufTemp = pEdgebuf;
                        GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                        GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                    }
                    
                    CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
                    #endif 

                    pSrcV = pEdgebuf;
                    pSrcH = pEdge2;
                    for(y = 0; y < hDiv2;y++)
                    {
                        for(x = 0; x < wDiv2; x++)
                        {
                            pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                        }
                        
                        pSrcV += wDiv2;
                        pSrcH += wDiv2;
                    }

                    free(pEdgebuf);
                }


                //-------------------LL 降噪--------------------
                //使用传导降噪渐变区域阶梯现象不明显
                //FastDn2dEdge(pstFastDn2d,0,pBuf, pBuf,pEdge2,wDiv2, hDiv2,  strideDiv2,nrEdgeTH);
                //FastDn2dInvertEdge(pstFastDn2d,0,pBuf, pBuf,pEdge2,wDiv2, hDiv2,  strideDiv2,nrEdgeTH);
                
#if 0
                {
                    UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                    UINT8 *pEdgebuf = (UINT8 *)malloc(wDiv2* hDiv2);
                            
                    INT32 nSigmaS = 2;//(nrEdgeTH*2 + 30) / 60;
                    INT32 nSigmaR = 8;//(nrEdgeTH + 6) / 12;  //(nrEdgeTH/6)>>1
                    
                    //GaussianBlur3x3(pSrc[0], pBlur, pEdgebuf, wDiv4, hDiv4, strideDiv4, strideDiv4, strideDiv4);
                    //BrightWareBilateralFilter(y, pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pTemp1);
                    //BilateralFilter(pBuf, hDiv2, wDiv2, nSigmaS, nSigmaR, pEdgebuf);
                    //EdgeWareBilateralFilter(pBuf,pEdge2, hDiv2, wDiv2, nSigmaS, nSigmaR,180, pEdgebuf);
                    
                    //BrightWareBilateralFilter(pSrc[0],pBlur, hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
                }
#endif

                nrEdgeTH = 100;
                nrTH = 20;

                nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

                if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
                }  
                else if(nrLumaHFRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
                }

                pSrcH    = pBuf + hDiv2 * strideDiv2;
                pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
                pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

                #ifdef __TEST_ARM
                HFNR_Edge_luma_neon(pSrcH, pEdge2,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcV,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcHH,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #else
                HFNR_Edge_luma(pSrcH, pEdge2,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcV,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcHH,pEdge2, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #endif
            }
        }
        
    }
    else
    {
        // 不会进入，代码没改
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }
    }
     
    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);
    #else
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    #endif
    
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_out.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    return eRet;
}


/************************************** 多线程速度优化 *******************************************/


 //多线程主调函数调用，Y去噪
 E_ERROR Nr_MaincallMix2_Y(HANDLE_NR *phHandle) 
 {
    //ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    //ST_PICTURE *pstPicOut = phHandle->stParamIn.pstPicOut;
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf  = phHandle->pBuf;
    
    UINT8 *pBlur;
    UINT8 *pEdge;
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
//    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?    //comment by wanghao because 529

    INT32  nrLumaRatio = phHandle->nrRatio[0];
    //INT32  nrUVRatio = phHandle->nrUVRatio;
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
    //INT32  nrUVHFRatio = phHandle->nrUVHFRatio;
    INT32  nrDetailRatio = phHandle->nrDetailRatio;
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2         = height>> 1;
//    INT32 chaW         = wDiv2;    //comment by wanghao because 529
//    INT32 chaH          = hDiv2;    //comment by wanghao because 529
//    INT32 uvStride     = stride >> 1;    //comment by wanghao because 529

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    //INT32 strideDiv8 = stride >> 3;

    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    UINT8 *pCopY;// = (UINT8 *)malloc(strideDiv2* hDiv2);

    E_ERROR  eRet = SUCCESS;
    pCopY = pBuf; 
    pBuf   += strideDiv2* hDiv2;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    #if 0
    {
        FILE *pfYUV = fopen("yuv_in.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }

    memcpy(pCopY,pBuf,(strideDiv2* hDiv2));
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf, pSrc[0],mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2, pSrc[0]+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }
    
    //pSrc[0] 后面3/4大小的空间闲置
    pMemAlign = pSrc[0] + strideDiv2 * hDiv2; 
    pEdge        = pMemAlign;//wDiv4 * iHn这块空间一直到最后都不能动
    pMemAlign += wDiv2 * hDiv2;
    pBlur          = pMemAlign;

    //step2 : edge detect

    //检测LL的边缘信息
    #ifdef __TEST_ARM
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3_neon(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }

    CalcSobel_neon(pBlur,pEdge,wDiv4  , hDiv4,wDiv4 ,wDiv4 );  
    #else
    {
        UINT8 *pBufTemp = pMemAlign + strideDiv4 * hDiv4;
        GaussianBlur3x3(pSrc[0], pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
    }
    
    CalcSobel(pBlur,pEdge,wDiv4 , hDiv4,wDiv4, wDiv4);
    #endif  

    //检测HL\LH的边缘信息,将三个平面的边缘信息合并
    {
        UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
        UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
        UINT8 *pSrcV, *pSrcH;
        INT32 x,y;
               
#ifdef __TEST_ARM
        UINT32 wDiv4_neon = wDiv4 >> 4;    
        UINT32 n = wDiv4 - (wDiv4_neon << 4);
        uint8x16_t pSrcH_neon, pSrcV_neon, temp;
#endif
        
        pSrcH  = pSrc[0] + hDiv4 * strideDiv4;// 水平
        pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4) << 1);// 垂直

        #ifdef __TEST_ARM
        {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4); 
        
        pSrcV = pEdgebuf;
        pSrcH = pEdge;        
        
        for(y = 0; y < hDiv4; y++)
        {
            for(x = 0; x < wDiv4_neon; x++)
            {
                pSrcH_neon = vld1q_u8(pSrcH);
                pSrcV_neon = vld1q_u8(pSrcV);
                temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
                vst1q_u8(pSrcH, temp);

                pSrcH += 16;
                pSrcV += 16;
            }
            pSrcH += n;
            pSrcV += n;
        }
        if(0 != n)
        {
            pSrcV = pEdgebuf + (wDiv4_neon << 4);
            pSrcH = pEdge + (wDiv4_neon << 4);
            for(y = 0; y < hDiv4;y++)
            {
                for(x = 0; x < n; x++)
                {
                    pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
            }
        }
        
       #else
       {
            UINT8 *pBufTemp = pEdgebuf;
            GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
            GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        }
        
        CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv4 , hDiv4,wDiv4, wDiv4);      
        

        pSrcV = pEdgebuf;
        pSrcH = pEdge;
        for(y = 0; y < hDiv4;y++)
        {
            for(x = 0; x < wDiv4; x++)
            {
                pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
            }
            pSrcV += wDiv4;
            pSrcH += wDiv4;
        }
       #endif 

    }
    
    #if 0
    {
        FILE *pfYUV = fopen("yuv_edgeLL2.yuv","wb");
        
        fwrite(pEdge,1,wDiv4* hDiv4,pfYUV);
        fwrite(pEdge,1,wDiv4* hDiv4/4,pfYUV);
        fwrite(pEdge,1,wDiv4* hDiv4/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    /*========================================================
                                                              亮度去噪
       ========================================================*/
    if(bIsNrLuma) 
    { 
        INT32 nrEdgeTH = 60;
        INT32 nrTH;

        nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaRatio)/45,180); 
    
        //-------------------LL 降噪--------------------
        //使用传导降噪渐变区域阶梯现象不明显
        if(-100 != nrLumaRatio)
        {
            UINT8 *pTbuf       = pBlur + wDiv4 * hDiv4;
            UINT8 *pEdgebuf = pTbuf + wDiv4 * hDiv4 ;
            
            INT32  nSigmaS = (nrEdgeTH + 30) / 60;
            INT32  nSigmaR = ((nrEdgeTH +8) >>3) >>1;
            
            BilateralFilter(pSrc[0], hDiv4, wDiv4, nSigmaS, nSigmaR, pEdgebuf);
        }
        
        //-------------------HL LH HH 降噪--------------------
        {
            UINT8 * pSrcH,  * pSrcV, *pSrcHH;
            nrEdgeTH = 60;  //60
            nrTH = 15;

            nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
            }

            CalcDn2dCoefs(pDnCoef, nrTH);

            pSrcH  = pSrc[0] + hDiv4 * strideDiv4;
            pSrcV  = pSrc[0] + ((hDiv4 * strideDiv4) << 1);
            pSrcHH =  pSrc[0] + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

            #ifdef __TEST_ARM
            HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #else
            HFNR_LUT_Edge_luma(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            HFNR_LUT_Edge_luma(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
            #endif

            // ------------第二层小波逆变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
            #endif
            
            // 16整数倍剩余像素的处理
            if (0 != mul16Rev2)
            {
                ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
            }

            //边缘检测
            memcpy(pSrc[0],pBuf,hDiv2*strideDiv2);

            #ifdef __TEST_ARM
            //MedianFilter_neon(pSrc[0], hDiv2,  wDiv2, pEdge, 3);
            GaussianBlur3x3_neon(pSrc[0], pBlur, pEdge, wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
            CalcSobel_neon(pBlur,pEdge,wDiv2,hDiv2,wDiv2, wDiv2);
            #else
            GaussianBlur3x3(pSrc[0], pBlur, pEdge, wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
            CalcSobel(pBlur,pEdge,wDiv2,hDiv2,wDiv2, wDiv2);
            #endif

            {
                //UINT8 *pTbuf = pBlur + wDiv2* hDiv2;
                UINT8 *pEdgebuf = pSrc[0];
                INT32 x,y;
                              
                #ifdef __TEST_ARM
                UINT32 wDiv2_neon = wDiv2 >> 4;    
                UINT32 n = wDiv2 - (wDiv2_neon << 4);
                uint8x16_t pSrcH_neon, pSrcV_neon, temp;
                #endif
                
                pSrcH  = pBuf + hDiv2 * strideDiv2;// 水平
                pSrcV  = pBuf + ((hDiv2 * strideDiv2) << 1);// 垂直

#if 0
{
                #ifdef __TEST_ARM
                {
                    UINT8 *pBufTemp = pEdgebuf;
                    GaussianBlur3x3_neon(pSrcH, pBlur, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                    GaussianBlur3x3_neon(pSrcV, pTbuf, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                }

                CalcSobel_HF_neon( pTbuf,  pBlur,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
                #else
                {
                    UINT8 *pBufTemp = pEdgebuf;
                    GaussianBlur3x3(pSrcH, pBlur, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                    GaussianBlur3x3(pSrcV, pTbuf, pBufTemp,wDiv2 , hDiv2,strideDiv2, wDiv2, wDiv2);
                }
                
                CalcSobel_HF( pTbuf,  pBlur,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
                #endif 

}
#else
                CalcSobel_HF( pSrcV,  pSrcH,  pEdgebuf, wDiv2 , hDiv2,wDiv2, wDiv2);
#endif

                pSrcV = pEdgebuf;
                pSrcH = pEdge;

                #ifdef __TEST_ARM                                            
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2_neon; x++)
                    {
                        pSrcH_neon = vld1q_u8(pSrcH);
                        pSrcV_neon = vld1q_u8(pSrcV);
                        temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
                        vst1q_u8(pSrcH, temp);

                        pSrcH += 16;
                        pSrcV += 16;
                    }
                    pSrcH += n;
                    pSrcV += n;
                }
                if(0 != n)
                {
                    pSrcV = pEdgebuf + (wDiv2_neon << 4);
                    pSrcH = pEdge + (wDiv2_neon << 4);
                    for(y = 0; y < hDiv2; y++)
                    {
                        for(x = 0; x < n; x++)
                        {
                            pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                        }
                        pSrcV += wDiv2;
                        pSrcH += wDiv2;
                    }
                } 
                #else
                for(y = 0; y < hDiv2;y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
                    }
                    
                    pSrcV += wDiv2;
                    pSrcH += wDiv2;
                }
                #endif
                
                #if 0
                {
                    FILE *pfYUV = fopen("yuv_yedge_merge.yuv","wb");
                    
                    fwrite(pEdge,1,(wDiv2* hDiv2),pfYUV);
                    fwrite(pEdge,1,(wDiv2* hDiv2)/4,pfYUV);
                    fwrite(pEdge,1,(wDiv2* hDiv2)/4,pfYUV);
                    fclose(pfYUV);
                }
                #endif

            }
    
            if(-100 != nrDetailRatio)
            {
                INT32 x,y;
                UINT8 *pTSrcL,*pTCopyL,*pEdgeL;
                INT32 weight;
                INT32  detailRecover = 40 * (100 - nrDetailRatio)/100;
                INT16   maxWeight   = 32767; //(1L<<15);
                INT32   edgeWeight  = maxWeight/detailRecover; //

                pTSrcL    = pBuf;
                pTCopyL = pCopY;
                pEdgeL   = pEdge;
                
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        if(pEdgeL[x] < detailRecover)
                        {
                            weight = MAX2(pEdgeL[x]*edgeWeight,6553);
                            //weight = MAX2(pEdgeL[x]*edgeWeight,0);
                            pTSrcL[x] = MIN2(((pTSrcL[x] * (maxWeight - weight) + pTCopyL[x] * weight + 32768) >> 15),255);
                        }
                        else
                        {
                            pTSrcL[x] = pTCopyL[x];
                        }
                    }

                    pTSrcL    += strideDiv2;
                    pTCopyL += strideDiv2;
                    pEdgeL    += wDiv2;
                }

            }
            /*
            else
            {
                INT32 x,y;
                UINT8 *pTSrcL,*pTCopyL;
                INT32 weight;
                INT16   maxWeight   = 32767;

                pTSrcL    = pBuf;
                pTCopyL = pCopY;
                
                for(y = 0; y < hDiv2; y++)
                {
                    for(x = 0; x < wDiv2; x++)
                    {
                        weight = 6553;
                        //weight = MAX2(pEdgeL[x]*edgeWeight,0);
                        pTSrcL[x] = MIN2((pTSrcL[x] * (maxWeight - weight) + pTCopyL[x] * weight + 32768) >> 15,255);
                    }

                    pTSrcL    += strideDiv2;
                    pTCopyL += strideDiv2;
                }

            }
            */
    
            //需要重新检测边缘
            {
                nrEdgeTH = 100;
                nrTH = 20;

                nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

                if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
                {
                    nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
                }  
                else if(nrLumaHFRatio > -60)
                {
                    nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
                }

                pSrcH    = pBuf + hDiv2 * strideDiv2;
                pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
                pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

                #ifdef __TEST_ARM
                HFNR_Edge_luma_neon(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma_neon(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #else
                HFNR_Edge_luma(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                HFNR_Edge_luma(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
                #endif
            }
        }
    }
    else
    {
        // 不会进入，代码没改
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pSrc[0], pBuf, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pSrc[0]+(mul16W2>>1), pBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }
    }
    
    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);
    #else
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    #endif
    
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }

    #if 0
    {
        FILE *pfYUV = fopen("yuv_out.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif


    
    //free(pCopY);
    
    return eRet;
    
}

void CalEdgeUseHaarCoef(UINT8 *pEdge,UINT8 *pEdgeHF, UINT8 *pLL, UINT8 *pSrcV , UINT8 *pSrcH, UINT8 *pBlur,INT32 width, INT32 height, INT32 stride)
{
    INT32 x,y;
                  
    #ifdef __TEST_ARM
    UINT32 wDiv2_neon = width >> 4;    
    UINT32 n = width - (wDiv2_neon << 4);
    uint8x16_t pSrcH_neon, pSrcV_neon, temp;
    #endif
   
    #ifdef __TEST_ARM      
    
    CalcSobel_HF_neon( pSrcV,  pSrcH,  pEdgeHF, width , height,stride, width);

    //LL
    GaussianBlur3x3_neon(pLL, pBlur, pEdge, width , height,stride, width, width);
    CalcSobel_neon(pBlur,pEdge,width,height,width, width);

    pSrcV = pEdgeHF;
    pSrcH = pEdge;
    
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < wDiv2_neon; x++)
        {
            pSrcH_neon = vld1q_u8(pSrcH);
            pSrcV_neon = vld1q_u8(pSrcV);
            temp = vmaxq_u8(pSrcH_neon, pSrcV_neon);
            vst1q_u8(pSrcH, temp);

            pSrcH += 16;
            pSrcV += 16;
        }
        pSrcH += n;
        pSrcV += n;
    }
    
    if(0 != n)
    {
        pSrcV = pEdgeHF + (wDiv2_neon << 4);
        pSrcH = pEdge + (wDiv2_neon << 4);
        for(y = 0; y < height; y++)
        {
            for(x = 0; x < n; x++)
            {
                pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
            }
            pSrcV += width;
            pSrcH += width;
        }
    } 

    #else
    CalcSobel_HF( pSrcV,  pSrcH,  pEdgeHF, width , height,width, width);

    //LL
    GaussianBlur3x3(pLL, pBlur, pEdge, width , height,stride, width, width);
    CalcSobel(pBlur,pEdge,width,height,width, width);

    pSrcV = pEdgeHF;
    pSrcH = pEdge;
    
    for(y = 0; y < height;y++)
    {
        for(x = 0; x < width; x++)
        {
            pSrcH[x] = MAX2(pSrcH[x], pSrcV[x]);
        }
        
        pSrcV += width;
        pSrcH += width;
    }
    #endif
}

void RecoverDetail(UINT8 *pEdge,UINT8 *pFiltered, UINT8 *pDst , INT32 nrDetailRatio,INT32 width, INT32 height, INT32 stride)
{
    INT32 x,y;
    UINT8 *pTSrcL,*pOrigL,*pEdgeL;    //comment by wanghao because 529 ,*pDstL;
    INT32 weight;
    INT32  detailRecover = 40 * (100 - nrDetailRatio)/100;
    INT16   maxWeight   = 32767; //(1L<<15);
    INT32   edgeWeight  = maxWeight/detailRecover; //
    

    pTSrcL    = pFiltered;
    pOrigL     = pDst;
    pEdgeL   = pEdge;

    #ifdef __TEST_ARM
    {
        UINT32 wDiv8 = width >> 3;                
        
        uint8x8_t pOrigL_8;                
        uint8x8_t pTSrcL_8;                
        uint8x8_t pEdgeL_8;
        
        uint16x8_t pOrigL_16;                
        uint16x8_t pTSrcL_16;                
        uint16x8_t pEdgeL_16;
                
        uint32x4_t pOrigL_h, pOrigL_l;    //pOrigL_16高、低64位                
        uint32x4_t pTSrcL_h, pTSrcL_l;    //pTSrcL_16高、低64位
        uint16x4_t pEdgeL_h, pEdgeL_l;    //pEdgeL_16高、低64位                
        
        uint32x4_t weight_32;                
        uint8x8_t  detailRecover_8 = vdup_n_u8(detailRecover);                
        uint16x8_t edgeWeight_16   = vdupq_n_u16(edgeWeight);                
        uint32x4_t maxWeight_32    = vdupq_n_u32(maxWeight);                
        
        uint16x4_t edgeWeight_h, edgeWeight_l; //edgeWeight_16高、低64位
            
        uint16x8_t Num255   = vdupq_n_u16(255);                
        uint32x4_t Num6553  = vdupq_n_u32(6553);             
                
        uint8x8_t  Flag, dst;
        uint16x8_t temp_16;
        uint16x4_t Temp1_16, Temp2_16;
        uint32x4_t Temp1_32, Temp2_32;

        for(y = 0; y < height; y++)
        {
            for(x = 0; x < wDiv8; x++)
            {
                pOrigL_8  = vld1_u8(pOrigL);
                pTSrcL_8  = vld1_u8(pTSrcL);
                pEdgeL_8  = vld1_u8(pEdgeL);
                
                pOrigL_16 = vmovl_u8(pOrigL_8);
                pTSrcL_16 = vmovl_u8(pTSrcL_8);
                pEdgeL_16 = vmovl_u8(pEdgeL_8);
                
                //处理高64位
                Temp1_16     = vget_high_u16(pOrigL_16);
                pOrigL_h     = vmovl_u16(Temp1_16);                        
                        
                Temp1_16     = vget_high_u16(pTSrcL_16);
                pTSrcL_h     = vmovl_u16(Temp1_16);                        
                        
                pEdgeL_h     = vget_high_u16(pEdgeL_16);
                edgeWeight_h = vget_high_u16(edgeWeight_16);
                        
                weight_32    = vmull_u16(pEdgeL_h, edgeWeight_h);
                weight_32    = vmaxq_u32(weight_32, Num6553);

                Temp1_32 = vsubq_u32(maxWeight_32, weight_32);                        
                Temp1_32 = vmulq_u32(pTSrcL_h, Temp1_32);

                Temp2_32 = vmulq_u32(pOrigL_h, weight_32);
                Temp1_32 = vaddq_u32(Temp1_32, Temp2_32);
                Temp1_32 = vrshrq_n_u32(Temp1_32, 15);
                Temp1_16 = vmovn_u32(Temp1_32);

                //处理低64位
                Temp2_16     = vget_low_u16(pOrigL_16);
                pOrigL_l     = vmovl_u16(Temp2_16);                        
                        
                Temp2_16      = vget_low_u16(pTSrcL_16);
                pTSrcL_l     = vmovl_u16(Temp2_16);                        
                        
                pEdgeL_l     = vget_low_u16(pEdgeL_16);
                edgeWeight_l = vget_low_u16(edgeWeight_16);
                        
                weight_32    = vmull_u16(pEdgeL_l, edgeWeight_l);
                weight_32    = vmaxq_u32(weight_32, Num6553);

                Temp1_32 = vsubq_u32(maxWeight_32, weight_32);                        
                Temp1_32 = vmulq_u32(pTSrcL_l, Temp1_32);

                Temp2_32 = vmulq_u32(pOrigL_l, weight_32);
                Temp1_32 = vaddq_u32(Temp1_32, Temp2_32);
                Temp1_32 = vrshrq_n_u32(Temp1_32, 15);
                Temp2_16 = vmovn_u32(Temp1_32);

                //合并
                temp_16  = vcombine_u16(Temp2_16, Temp1_16);
                temp_16  = vminq_u16(temp_16, Num255);
                dst      = vmovn_u16(temp_16);
                        
                Flag     = vclt_u8(pEdgeL_8, detailRecover_8);
                dst      = vand_u8(dst, Flag);
                Flag     = vmvn_u8(Flag);
                pOrigL_8 = vand_u8(pOrigL_8, Flag);
                dst      = vadd_u8(pOrigL_8, dst);

                vst1_u8(pOrigL, dst);

                pOrigL += 8;
                pTSrcL += 8;
                pEdgeL += 8;
            }
        }
   
    }
    #else
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            if(pEdgeL[x] < detailRecover)
            {
                weight = MAX2(pEdgeL[x]*edgeWeight,6553);
                pOrigL[x] = MIN2(((pTSrcL[x] * (maxWeight - weight) + pOrigL[x] * weight + 16384) >> 15),255);
            }
        }

        pTSrcL    += stride;
        pOrigL    += stride;
        pEdgeL    += width;
    }
    #endif

}


//多线程主调函数调用，Y去噪
 E_ERROR Nr_MaincallMix2_Y_(HANDLE_NR *phHandle) 
 {
    //ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个
    //ST_PICTURE *pstPicOut = phHandle->stParamIn.pstPicOut;
    
    INT16 *pDnCoef = phHandle->pDnCoef;
    UINT8 *pBuf    = phHandle->pBuf;
    
    UINT8 *pEdge;
    UINT8 *pMemAlign;

    UINT8 *pSrc[3];
    UINT8 *pHaar2Buf;
    
    BOOL  bIsNrLuma  = phHandle->bIsNrLuma; //deluma?
    //BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?

    INT32  nrLumaRatio = phHandle->nrRatio[0];
    //INT32  nrUVRatio = phHandle->nrUVRatio;
    INT32  nrLumaHFRatio = phHandle->nrLumaHFRatio;
    //INT32  nrUVHFRatio = phHandle->nrUVHFRatio;
    INT32  nrDetailRatio = phHandle->nrDetailRatio;
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2         = height>> 1;
//    INT32 chaW         = wDiv2;    //comment by wanghao because 529
//    INT32 chaH          = hDiv2;    //comment by wanghao because 529
//    INT32 uvStride     = stride >> 1;    //comment by wanghao because 529

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    //INT32 strideDiv8 = stride >> 3;
    INT32 nrEdgeTH = 60;
    INT32 nrTH;
    
    INT32 mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev, mul16Rev2; // 剩余的部分

    UINT8 *pCopY;// = (UINT8 *)malloc(strideDiv2* hDiv2);

    E_ERROR  eRet = SUCCESS;

    if(!bIsNrLuma)
    {
        return eRet;
    }
    
    pCopY = pBuf; 
    pBuf   += strideDiv2* hDiv2;
    
    pHaar2Buf  = pBuf; 
    pBuf   += strideDiv2* hDiv2;

    pSrc[0] = pstPicIn->pData[0];

    #if 0
    {
        FILE *pfYUV = fopen("yuv_in.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    //step1 : wavelet transform
    //------------第一层小波正变换---------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    ApplyHaar_new_neon(pSrc[0], pBuf,mul16W, height,  stride);    
    #else
    ApplyHaar_new(pSrc[0], pBuf,mul16W, height,  stride); 
    #endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyHaar_new(pSrc[0]+mul16W, pBuf+(mul16W>>1), mul16Rev, height,  stride); 
    }

    #ifdef __GNUC__
    //=================进行线程间同步，防止数据没有读就被写修改====
    pthread_mutex_lock(&yReadedLock);
    yReaded++;
    pthread_mutex_unlock(&yReadedLock);
    
    while(1)//循环里需要加语句，否则会形成死循环
    {
        //printf("==waiting\n");
        if(yReaded >=4)
        {
            break;
        }
        
        usleep(1);    //功能把进程挂起一段时间， 单位是微秒
    }
    //=======================================================================
    #endif
    
    //memcpy(pCopY,pBuf,(strideDiv2* hDiv2));
    
    //------------第二层小波正变换---------------
    mul16W2    = wDiv2 & 0xfffffff0;
    mul16Rev2  = wDiv2 - mul16W2;
    
    // 宽为16整数倍的处理
#ifdef __TEST_ARM
    ApplyHaar_new_neon(pBuf, pHaar2Buf,mul16W2, hDiv2,  strideDiv2);
#else
    ApplyHaar_new(pBuf,pHaar2Buf,mul16W2, hDiv2,  strideDiv2);
#endif

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev2)
    {
        ApplyHaar_new(pBuf+mul16W2, pHaar2Buf+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
    }
    
    //pSrc[0] 后面3/4大小的空间闲置
    pEdge        = pCopY;
    pMemAlign = pCopY + wDiv4 * hDiv4;

    //step2 : edge detect
    //检测HH\HL\LH的边缘信息,将三个平面的边缘信息合并
    {
        UINT8 *pBuf1 = pCopY;
        UINT8 *pBuf2 = pBuf1 + wDiv4 * hDiv4;
        UINT8 *pBuf3 = pBuf2 + wDiv4 * hDiv4 ;
        UINT8 *pSrcV, *pSrcH;
//        INT32 x,y;    //comment by wanghao because 529
        
        pSrcH  = pHaar2Buf + hDiv4 * strideDiv4;// 水平
        pSrcV  = pHaar2Buf + ((hDiv4 * strideDiv4) << 1);// 垂直

        #ifdef __TEST_ARM
        GaussianBlur3x3_neon(pSrcH, pBuf1, pBuf3,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        GaussianBlur3x3_neon(pSrcV, pBuf2, pBuf3,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
       #else
        GaussianBlur3x3(pSrcH, pBuf1, pBuf3,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
        GaussianBlur3x3(pSrcV, pBuf2, pBuf3,wDiv4 , hDiv4,strideDiv4, wDiv4, wDiv4);
       #endif 

       CalEdgeUseHaarCoef(pEdge, pBuf3, pHaar2Buf, pBuf2, pBuf1,pBuf2, wDiv4 , hDiv4,strideDiv4);
    }


    //-------------------LL 降噪--------------------
    if(-100 != nrLumaRatio)
    {
        UINT8 *pDst = pMemAlign + wDiv4 * hDiv4;
        INT32  nSigmaS,nSigmaR;
            
        nrEdgeTH = 60;
        nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaRatio)/45,180); 
        
        //=====这两个参数不能为0，否则会出现锯齿
        nSigmaS = MAX2((nrEdgeTH + 30) / 60,1);
        nSigmaR = MAX2(((nrEdgeTH +8) >>3) >>1,1);
        
        BilateralFilter_new(pHaar2Buf,pDst, hDiv4, wDiv4, nSigmaS, nSigmaR);
        memcpy(pHaar2Buf,pDst, hDiv4*wDiv4);
    }
    
    //-------------------HL LH HH 降噪--------------------
    {
        UINT8 * pHaar2Dst;
        UINT8 * pSrcH,  * pSrcV, *pSrcHH;

        if(-100 < nrDetailRatio)
        {
            pHaar2Dst = pCopY;
        }
        else
        {
            pHaar2Dst = pBuf;
        }
        
        nrEdgeTH = 60;  //60
        nrTH = 15;

        nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

        if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
        {
            nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
        }  
        else if(nrLumaHFRatio > -60)
        {
            nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
        }

        CalcDn2dCoefs(pDnCoef, nrTH);

        pSrcH  = pHaar2Buf + hDiv4 * strideDiv4;
        pSrcV  = pHaar2Buf + ((hDiv4 * strideDiv4) << 1);
        pSrcHH =  pHaar2Buf + ((hDiv4 * strideDiv4) << 1)+ hDiv4 * strideDiv4;

        #ifdef __TEST_ARM
        HFNR_LUT_Edge_luma_neon(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
        HFNR_LUT_Edge_luma_neon(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
        HFNR_LUT_Edge_luma_neon(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
        #else
        HFNR_LUT_Edge_luma(pSrcH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
        HFNR_LUT_Edge_luma(pSrcV,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
        HFNR_LUT_Edge_luma(pSrcHH,pDnCoef,pEdge,wDiv4, hDiv4,  strideDiv4,nrEdgeTH);
        #endif

        // ------------第二层小波逆变换------------
        mul16W2    = wDiv2 & 0xfffffff0;
        mul16Rev2  = wDiv2 - mul16W2;
        
        // 宽为16整数倍的处理
        #ifdef __TEST_ARM
        ApplyInverseHaar_new_neon(pHaar2Buf, pHaar2Dst, mul16W2, hDiv2,  strideDiv2);
        #else
        ApplyInverseHaar_new(pHaar2Buf, pHaar2Dst, mul16W2, hDiv2,  strideDiv2);
        #endif
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev2)
        {
            ApplyInverseHaar_new(pHaar2Buf+(mul16W2>>1), pHaar2Dst+mul16W2, mul16Rev2, hDiv2,  strideDiv2);
        }

        //如果pstPicIn->height小于64，则此处使用pSrc[0]作为缓冲会与其他线程的有效数据冲突
        //边缘检测
        pEdge = pSrc[0] + strideDiv2 * hDiv2; //不要使用前面与后面的空间，防止冲突

        {
            UINT8 *pEdgeHF = pHaar2Buf;
            UINT8 *pBlur       = pSrc[0] + strideDiv2 * hDiv2 * 2;
                         
            pSrcH  = pBuf + hDiv2 * strideDiv2;// 水平
            pSrcV  = pBuf + ((hDiv2 * strideDiv2) << 1);// 垂直

            CalEdgeUseHaarCoef(pEdge, pEdgeHF,pHaar2Dst,  pSrcV,  pSrcH,  pBlur, wDiv2 , hDiv2,strideDiv2);

        }
            
        if(-100 < nrDetailRatio)
        {
            RecoverDetail(pEdge, pHaar2Dst, pBuf,  nrDetailRatio, wDiv2, hDiv2, strideDiv2);
        }

        //需要重新检测边缘
        {
            nrEdgeTH = 100;
            nrTH = 20;

            nrTH = MIN2(nrTH * (105 + nrLumaHFRatio)/45,35); // 45 = 105 - 60

            if(nrLumaHFRatio < -80) //小于80则进行更强的降噪强度衰减
            {
                nrEdgeTH = nrEdgeTH * (105 + nrLumaHFRatio)/25; // 25 = 105 - 80
            }  
            else if(nrLumaHFRatio > -60)
            {
                nrEdgeTH = MIN2(nrEdgeTH * (105 + nrLumaHFRatio)/35,180); // 25 = 105 - 80
            }

            pSrcH    = pBuf + hDiv2 * strideDiv2;
            pSrcV    = pBuf + ((hDiv2 * strideDiv2) << 1);
            pSrcHH = pBuf + ((hDiv2 * strideDiv2) << 1) + hDiv2 * strideDiv2;

            #ifdef __TEST_ARM
            HFNR_Edge_luma_neon(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
            HFNR_Edge_luma_neon(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
            HFNR_Edge_luma_neon(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
            #else
            HFNR_Edge_luma(pSrcH, pEdge,wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
            HFNR_Edge_luma(pSrcV,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
            HFNR_Edge_luma(pSrcHH,pEdge, wDiv2, hDiv2,  strideDiv2,nrTH,nrEdgeTH);
            #endif
        }
    }
    
    // ------------第一层小波逆变换------------
    mul16W     = width & 0xfffffff0;
    mul16Rev   = width - mul16W;
    
    // 宽为16整数倍的处理
    #ifdef __TEST_ARM
    
    #ifdef __GNUC__
    {
        UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[0];
        INT32 dstH = phHandle->stParamIn.pstPicOut->height;
        INT32 srcH  = phHandle->stParamIn.pstPicIn->height;
        INT32 threadOffset = phHandle->threadOffset;
        
        ApplyInverseHaar_new_thread_neon(pBuf,pDst ,threadOffset,srcH,mul16W, dstH,  stride);
        
        // 16整数倍剩余像素的处理
        if (0 != mul16Rev)
        {
            ApplyInverseHaar_thread_new(pBuf+(mul16W>>1),pDst+mul16W , threadOffset,srcH,mul16Rev, dstH,  stride);
        }
    }
    #else
    ApplyInverseHaar_new_neon(pBuf,pSrc[0],mul16W, height,  stride);

    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }
    #endif
    
    #else
    
    ApplyInverseHaar_new(pBuf,pSrc[0],mul16W, height,  stride);
    // 16整数倍剩余像素的处理
    if (0 != mul16Rev)
    {
        ApplyInverseHaar_new(pBuf+(mul16W>>1),pSrc[0]+mul16W,mul16Rev, height,  stride);
    }
    
    #endif
    
    #if 0
    {
        FILE *pfYUV = fopen("yuv_out.yuv","wb");
        
        fwrite(pSrc[0],1,width*height,pfYUV);
        fwrite(pSrc[1],1,width*height/4,pfYUV);
        fwrite(pSrc[2],1,width*height/4,pfYUV);
        fclose(pfYUV);
    }
    #endif

    
    return eRet;
    
}


//多线程主调函数调用，U去噪
 E_ERROR Nr_MaincallMix2_U(HANDLE_NR *phHandle)
 {
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个

    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?
    
    UINT8 *pSrc[3];
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2        = height>> 1;
    INT32 chaW         = wDiv2;
    INT32 chaH         = hDiv2;
    INT32 uvStride     = stride >> 1;     

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1;
    
    E_ERROR  eRet = SUCCESS;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    if(bIsNrCha)
    {
        UINT8 *pDownBuf ;//pSrc[0];
        UINT8 *pTBuf;
        INT32 nSigmaS ,nSigmaR;
        
        INT32 nrUVEdgeTH = 128;

        //if(phHandle->nrRatio[1] < -50)
        nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[1]) / 55;

        nSigmaS = MAX2((nrUVEdgeTH*3 + 64) / 128,1);
        nSigmaR = MAX2(((nrUVEdgeTH+8) >>3) >>1,1);

        if(!phHandle->bIsLittleImgNr)
        {
            pDownBuf = phHandle->pBuf;//pSrc[0];
            pTBuf        =  phHandle->pBuf + wDiv4* hDiv4;
        
            #ifdef __TEST_ARM
            DownScale_neon(pSrc[1], chaH, chaW,uvStride, pDownBuf);
            #else
            DownScale(pSrc[1], chaH, chaW,uvStride, pDownBuf);
            #endif
            
            BilateralFilter_new(pDownBuf,pTBuf, chaH>>1, chaW>>1, nSigmaS, nSigmaR);

            #ifdef __TEST_ARM
            UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            #else
            UpScale(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            #endif
        }
        else
        {
            pTBuf = phHandle->pBuf;
            
            BilateralFilter_new(pSrc[1],pTBuf, chaH, chaW, nSigmaS, nSigmaR);
            memcpy(pSrc[1],pTBuf,chaH*chaW);
        }
    }
    return  eRet;
 }


 //多线程主调函数调用，V去噪
 E_ERROR Nr_MaincallMix2_V(HANDLE_NR *phHandle)
{
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个

    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?
    
    UINT8 *pSrc[3];
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2        = height>> 1;
    INT32 chaW         = wDiv2;
    INT32 chaH         = hDiv2;
    INT32 uvStride     = stride >> 1; 

    INT32 wDiv4 = wDiv2 >> 1;
    INT32 hDiv4  = hDiv2 >> 1; 

    E_ERROR  eRet = SUCCESS;

    pSrc[0] = pstPicIn->pData[0];
    pSrc[1] = pstPicIn->pData[1];
    pSrc[2] = pstPicIn->pData[2];

    if(bIsNrCha)
    {
        UINT8 *pDownBuf = phHandle->pBuf;
        UINT8 *pTBuf    =  phHandle->pBuf + wDiv4* hDiv4;
        INT32 nSigmaS ,nSigmaR;
        
        INT32 nrUVEdgeTH = 128;

        //if(phHandle->nrRatio[2] < -50)
        nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[2]) / 55;

        nSigmaS = MAX2((nrUVEdgeTH*3 + 64) / 128,1);
        nSigmaR = MAX2(((nrUVEdgeTH+8) >>3) >>1,1);

        //printf("%d %d\n",nSigmaS,nSigmaR);

        if(!phHandle->bIsLittleImgNr)
        {
            pDownBuf = phHandle->pBuf;
            pTBuf        =  phHandle->pBuf + wDiv4* hDiv4;
        
            #ifdef __TEST_ARM
            DownScale_neon(pSrc[2], chaH, chaW,uvStride, pDownBuf);
            #else
            DownScale(pSrc[2], chaH, chaW,uvStride, pDownBuf);
            #endif
            
            BilateralFilter_new(pDownBuf,pTBuf, chaH>>1, chaW>>1, nSigmaS, nSigmaR);

            #ifdef __TEST_ARM
            UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[2]);
            #else
            UpScale(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[2]);
            #endif
        }
        else
        {
            pTBuf = phHandle->pBuf;
            
            BilateralFilter_new(pSrc[2],pTBuf, chaH, chaW, nSigmaS, nSigmaR);
            memcpy(pSrc[2],pTBuf,chaH*chaW);
        }
    }
    return  eRet;
 }

 //多线程主调函数调用，U去噪,参数中pstPicIn的宽高为色度的宽高
 //只能多线程调用是因为pstPicIn的宽高是色度的宽高
E_ERROR Nr_MaincallMix2_U_(HANDLE_NR *phHandle)
{
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个

    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?
    
    UINT8 *pSrc[3];
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2        = height>> 1;
    INT32 chaW         = width;
    INT32 chaH         = height;
    INT32 uvStride     = stride;     
    
    E_ERROR  eRet = SUCCESS;

    pSrc[1] = pstPicIn->pData[1];

    if(bIsNrCha)
    {
        UINT8 *pDownBuf ;//pSrc[0];
        UINT8 *pTBuf;
//        UINT8 *pBuf ;    //comment by wanghao because 550:not accessed
        INT32 nSigmaS ,nSigmaR;
        
        INT32 nrUVEdgeTH = 128;

        //if(phHandle->nrRatio[1] < -50)
        nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[1]) / 55;

        nSigmaS = MAX2((nrUVEdgeTH*3 + 64) / 128,1);
        nSigmaR = MAX2(((nrUVEdgeTH+8) >>3) >>1,1);
       
        
        //printf("%d %d\n",nSigmaS,nSigmaR);

        if(!phHandle->bIsLittleImgNr)
        {
            pDownBuf = phHandle->pBuf;//pSrc[0];
            pTBuf        =  phHandle->pBuf + wDiv2* hDiv2;
            //pBuf           = NULL;//pSrc[1] + wDiv2* hDiv2;    //comment by wanghao because 550:not accessed
            
            #ifdef __TEST_ARM
            DownScale_neon(pSrc[1], chaH, chaW,uvStride, pDownBuf);
            #else
            DownScale(pSrc[1], chaH, chaW,uvStride, pDownBuf);
            #endif

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&uReadedLock);
            uReaded++;
            pthread_mutex_unlock(&uReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(uReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

            //uReaded++;

            BilateralFilter_new(pDownBuf,pTBuf, hDiv2, wDiv2, nSigmaS, nSigmaR);

            #ifdef __TEST_ARM
            #ifdef __GNUC__
            //UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height>>1;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[1];

                //printf("=========%d %d %d %x %x\n",hDiv2,offset,dstH,pDst,pSrc[1]);

                UpScale_thread_neon(pTBuf,offset,hDiv2,dstH, wDiv2, uvStride, pDst);
            }
            #else
            UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            #endif
            #else
            UpScale(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            #endif
        }
        else
        {
            pTBuf        =  phHandle->pBuf ;
//            pBuf          = NULL;//neon版本的双边不需要临时缓存    //comment by wanghao because 550:not accessed
            
            BilateralFilter_new(pSrc[1],pTBuf, height, width, nSigmaS, nSigmaR);

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&uReadedLock);
            uReaded++;
            pthread_mutex_unlock(&uReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(uReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

           {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[1];

                UINT8 *pSrcL,*pDstL;
                INT32 y;    //comment by wanghao because 529    x,y;

                pSrcL = pTBuf + offset;
                pDstL = pDst;

                for(y = 0; y < dstH; y++)
                {
                    memcpy(pDstL,pSrcL,width);
                    pSrcL += width;
                    pDstL += uvStride;
                }
            }

        }
    }
    
    return  eRet;
}


 //多线程主调函数调用，V去噪,参数中pstPicIn的宽高为色度的宽高
 //只能多线程调用是因为pstPicIn的宽高是色度的宽高
E_ERROR Nr_MaincallMix2_V_(HANDLE_NR *phHandle)
{
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个

    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?
    
    UINT8 *pSrc[3];
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;

    INT32 wDiv2        = width >> 1;
    INT32 hDiv2        = height>> 1;
    INT32 chaW         = width;
    INT32 chaH         = height;
    INT32 uvStride     = stride;     
    
    E_ERROR  eRet = SUCCESS;

    pSrc[2] = pstPicIn->pData[2];

    if(bIsNrCha)
    {
        UINT8 *pDownBuf = phHandle->pBuf;//pSrc[0];
        UINT8 *pTBuf        =  phHandle->pBuf + wDiv2* hDiv2;
//        UINT8 *pBuf           = pSrc[2] + wDiv2* hDiv2;    //comment by wanghao because 550:not accessed
        INT32 nSigmaS ,nSigmaR;
        
        INT32 nrUVEdgeTH = 128;

        //if(phHandle->nrRatio[1] < -50)
        nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[1]) / 55;

        nSigmaS = MAX2((nrUVEdgeTH*3 + 64) / 128,1);
        nSigmaR = MAX2(((nrUVEdgeTH+8) >>3) >>1,1);

        if(!phHandle->bIsLittleImgNr)
        {
            pDownBuf = phHandle->pBuf;//pSrc[0];
            pTBuf        =  phHandle->pBuf + wDiv2* hDiv2;
//            pBuf           = NULL;//pSrc[1] + wDiv2* hDiv2;    //comment by wanghao because 550:not accessed
            
            #ifdef __TEST_ARM
            DownScale_neon(pSrc[2], chaH, chaW,uvStride, pDownBuf);
            #else
            DownScale(pSrc[2], chaH, chaW,uvStride, pDownBuf);
            #endif

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&vReadedLock);
            vReaded++;
            pthread_mutex_unlock(&vReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(vReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

            //uReaded++;

            BilateralFilter_new(pDownBuf,pTBuf, hDiv2, wDiv2, nSigmaS, nSigmaR);

            #ifdef __TEST_ARM
            #ifdef __GNUC__
            //UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height>>1;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[2];

                //printf("=========%d %d %d %x %x\n",hDiv2,offset,dstH,pDst,pSrc[1]);

                UpScale_thread_neon(pTBuf,offset,hDiv2,dstH, wDiv2, uvStride, pDst);
            }
            #else
            UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[2]);
            #endif
            #else
            UpScale(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[2]);
            #endif
        }
        else
        {
            pTBuf        =  phHandle->pBuf ;
            
            BilateralFilter_new(pSrc[2],pTBuf, height, width, nSigmaS, nSigmaR);

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&vReadedLock);
            vReaded++;
            pthread_mutex_unlock(&vReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(vReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

            //========= wait for modify
           {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[2];

                UINT8 *pSrcL,*pDstL;

                INT32 y;    //comment by wanghao because 529 x,y;

                pSrcL = pTBuf + offset;
                pDstL = pDst;

                for(y = 0; y < dstH; y++)
                {
                    memcpy(pDstL,pSrcL,width);
                    pSrcL += width;
                    pDstL += uvStride;
                }
            }
        }
    }
    
    return  eRet;
}

 //多线程主调函数调用，U去噪,参数中pstPicIn的宽高为色度的宽高
 //只能多线程调用是因为pstPicIn的宽高是色度的宽高
E_ERROR Nr_MaincallMix2_U_night(HANDLE_NR *phHandle)
{
    ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个

    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?
    
    UINT8 *pSrc[3];
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;
    INT32 wDiv4        = width >> 2;
    INT32 hDiv4        = height>> 2;
    INT32 wDiv2        = width >> 1;
    INT32 hDiv2        = height>> 1;
    
    E_ERROR  eRet = SUCCESS;
    
    INT32 mul16W2;    //comment by wanghao because 529    mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev2;    //comment by wanghao because 529    mul16Rev, mul16Rev2; // 剩余的部分

    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    
    pSrc[1] = pstPicIn->pData[1];

    if(bIsNrCha)
    {
        UINT8 *pDownBuf ;//pSrc[0];
        UINT8 *pTBuf;
//        UINT8 *pBuf ;
        INT32 nSigmaS = 0 , nSigmaR = 0;    //modified by wanghao because 530:not initialized
        
        INT32 nrUVEdgeTH = 128;

        {
            INT32 nrUVStrength     = 16;//21;
            INT32 nrUVDetailRatio = 4915;//2765;//nrUVStrength/3;
            
            nrUVStrength         =   ((nrUVStrength*((105 + phHandle->nrRatio[1]) <<10)/105)>>10);

            FastDn2dUVControl(pstFastDn2d,  nrUVStrength,  nrUVDetailRatio);
        }

        //printf("%d %d\n",nSigmaS,nSigmaR);

        if(!phHandle->bIsLittleImgNr)
        {
            pDownBuf = phHandle->pBuf;//pSrc[0];
            pTBuf        =  phHandle->pBuf + wDiv2* hDiv2;
//            pBuf           = NULL;//pSrc[1] + wDiv2* hDiv2;    //comment by wanghao because 550:not accessed
            
            #ifdef __TEST_ARM
            DownScale_neon(pSrc[1], height, width,stride, pDownBuf);
            #else
            DownScale(pSrc[1], height, width,stride, pDownBuf);
            #endif

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&uReadedLock);
            uReaded++;
            pthread_mutex_unlock(&uReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(uReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

                //uReaded++;
            // ------------小波正变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
#ifdef __TEST_ARM
            ApplyHaar_new_neon(pDownBuf, pTBuf, mul16W2, hDiv2,  strideDiv2);                
#else
            ApplyHaar_new(pDownBuf, pTBuf, mul16W2, hDiv2,  strideDiv2);
#endif
            
            // 16整数倍剩余像素的处理
            if (0!= mul16Rev2)
            {
                ApplyHaar_new(pDownBuf+mul16W2, pTBuf+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
            }

            FastDn2d(pstFastDn2d,1,pTBuf, pTBuf, wDiv4,  hDiv4,  strideDiv4);
            FastDn2dInvert(pstFastDn2d,1,pTBuf, pTBuf,wDiv4,  hDiv4,  strideDiv4);
        
            // ------------小波逆变换------------
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pTBuf, pDownBuf,  mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pTBuf, pDownBuf,  mul16W2, hDiv2,  strideDiv2);
            #endif

            // 16整数倍剩余像素的处理
            if (0!= mul16Rev2)
            {
                ApplyInverseHaar_new(pTBuf+(mul16W2>>1), pDownBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2); 
            }

            //if(phHandle->nrRatio[1] < -50)
            nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[1]) / 55;

            nSigmaS = MAX2((nrUVEdgeTH*3 + 64) / 128,1);
            nSigmaR = MAX2(((nrUVEdgeTH+8) >>3) >>1,1);
            
            BilateralFilter_new(pDownBuf,pTBuf, hDiv2, wDiv2, nSigmaS, nSigmaR);

            #ifdef __TEST_ARM
            #ifdef __GNUC__
            //UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height>>1;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[1];

                //printf("=========%d %d %d %x %x\n",hDiv2,offset,dstH,pDst,pSrc[1]);

                UpScale_thread_neon(pTBuf,offset,hDiv2,dstH, wDiv2, stride, pDst);
            }
            #else
            UpScale_neon(pTBuf, height>>1, width>>1, stride, pSrc[1]);
            #endif
            #else
            UpScale(pTBuf, height>>1, width>>1, stride, pSrc[1]);
            #endif
        }
        else
        {
            pTBuf        =  phHandle->pBuf ;
//            pBuf          = NULL;//neon版本的双边不需要临时缓存    //comment by wanghao because 550:not accessed
            
            BilateralFilter_new(pSrc[1],pTBuf, height, width, nSigmaS, nSigmaR);

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&uReadedLock);
            uReaded++;
            pthread_mutex_unlock(&uReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(uReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

           {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[1];

                UINT8 *pSrcL,*pDstL;
                INT32 y;    //comment by wanghao because 529x,y;

                pSrcL = pTBuf + offset;
                pDstL = pDst;

                for(y = 0; y < dstH; y++)
                {
                    memcpy(pDstL,pSrcL,width);
                    pSrcL += width;
                    pDstL += stride;
                }
            }

        }
    }
    
    return  eRet;
}


 //多线程主调函数调用，V去噪,参数中pstPicIn的宽高为色度的宽高
 //只能多线程调用是因为pstPicIn的宽高是色度的宽高
E_ERROR Nr_MaincallMix2_V_night(HANDLE_NR *phHandle)
{
    ST_FASTDN2D *pstFastDn2d = phHandle->pstFastDn2d;
    ST_PICTURE *pstPicIn = phHandle->stParamIn.pstPicIn; //本函数的输入输出是同一个

    BOOL  bIsNrCha     = phHandle->bIsNrCha;    //dechroma?
    
    UINT8 *pSrc[3];
    
    INT32 width        = pstPicIn->width;
    INT32 height       = pstPicIn->height;
    INT32 stride       = pstPicIn->stride;
    INT32 wDiv4        = width >> 2;
    INT32 hDiv4        = height>> 2;
    INT32 wDiv2        = width >> 1;
    INT32 hDiv2        = height>> 1;
    
    E_ERROR  eRet = SUCCESS;
    
    INT32 mul16W2;    //comment by wanghao because 529    mul16W, mul16W2; // 宽被16整除的部分
    INT32 mul16Rev2;    //comment by wanghao because 529    mul16Rev, mul16Rev2; // 剩余的部分

    INT32 strideDiv2 = stride >> 1;
    INT32 strideDiv4 = stride >> 2;
    
    pSrc[2] = pstPicIn->pData[2];

    if(bIsNrCha)
    {
        UINT8 *pDownBuf ;//pSrc[0];
        UINT8 *pTBuf;
//        UINT8 *pBuf ;    //comment by wanghao because 550:not accessed
        INT32 nSigmaS = 0 , nSigmaR = 0;    //modified by wanghao because 530:not initialized
        
        INT32 nrUVEdgeTH = 128;


       
        {
            INT32 nrUVStrength     = 16;//21;
            INT32 nrUVDetailRatio = 4915;//2765;//nrUVStrength/3;
        
            nrUVStrength         =   ((nrUVStrength*((105 + phHandle->nrRatio[1]) <<10)/105)>>10);

            FastDn2dUVControl(pstFastDn2d,  nrUVStrength,  nrUVDetailRatio);
        }

        //printf("%d %d\n",nSigmaS,nSigmaR);

        if(!phHandle->bIsLittleImgNr)
        {
            pDownBuf = phHandle->pBuf;//pSrc[0];
            pTBuf        =  phHandle->pBuf + wDiv2* hDiv2;
//            pBuf           = NULL;//pSrc[1] + wDiv2* hDiv2;    //comment by wanghao because 550:not accessed
            
            #ifdef __TEST_ARM
            DownScale_neon(pSrc[2], height, width,stride, pDownBuf);
            #else
            DownScale(pSrc[2], height, width,stride, pDownBuf);
            #endif

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&vReadedLock);
            vReaded++;
            pthread_mutex_unlock(&vReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(vReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

                //uReaded++;
            // ------------小波正变换------------
            mul16W2    = wDiv2 & 0xfffffff0;
            mul16Rev2  = wDiv2 - mul16W2;
            
            // 宽为16整数倍的处理
#ifdef __TEST_ARM
            ApplyHaar_new_neon(pDownBuf, pTBuf, mul16W2, hDiv2,  strideDiv2);                
#else
            ApplyHaar_new(pDownBuf, pTBuf, mul16W2, hDiv2,  strideDiv2);
#endif
            
            // 16整数倍剩余像素的处理
            if (0!= mul16Rev2)
            {
                ApplyHaar_new(pDownBuf+mul16W2, pTBuf+(mul16W2>>1), mul16Rev2, hDiv2,  strideDiv2);
            }

            FastDn2d(pstFastDn2d,2,pTBuf, pTBuf, wDiv4,  hDiv4,  strideDiv4);
            FastDn2dInvert(pstFastDn2d,2,pTBuf, pTBuf,wDiv4,  hDiv4,  strideDiv4);
        
            // ------------小波逆变换------------
            // 宽为16整数倍的处理
            #ifdef __TEST_ARM
            ApplyInverseHaar_new_neon(pTBuf, pDownBuf,  mul16W2, hDiv2,  strideDiv2);
            #else
            ApplyInverseHaar_new(pTBuf, pDownBuf,  mul16W2, hDiv2,  strideDiv2);
            #endif

            // 16整数倍剩余像素的处理
            if (0!= mul16Rev2)
            {
                ApplyInverseHaar_new(pTBuf+(mul16W2>>1), pDownBuf+mul16W2, mul16Rev2, hDiv2,  strideDiv2); 
            }

            //if(phHandle->nrRatio[1] < -50)
            nrUVEdgeTH = nrUVEdgeTH * (105 + phHandle->nrRatio[2]) / 55;

            nSigmaS = MAX2((nrUVEdgeTH*3 + 64) / 128,1);
            nSigmaR = MAX2(((nrUVEdgeTH+8) >>3) >>1,1);
        
            BilateralFilter_new(pDownBuf,pTBuf, hDiv2, wDiv2, nSigmaS, nSigmaR);

            #ifdef __TEST_ARM
            #ifdef __GNUC__
            //UpScale_neon(pTBuf, chaH>>1, chaW>>1, uvStride, pSrc[1]);
            {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height>>1;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[2];

                //printf("=========%d %d %d %x %x\n",hDiv2,offset,dstH,pDst,pSrc[1]);

                UpScale_thread_neon(pTBuf,offset,hDiv2,dstH, wDiv2, stride, pDst);
            }
            #else
            UpScale_neon(pTBuf, height>>1, width>>1, stride, pSrc[2]);
            #endif
            #else
            UpScale(pTBuf, height>>1, width>>1, stride, pSrc[2]);
            #endif
        }
        else
        {
            pTBuf        =  phHandle->pBuf ;
//            pBuf          = NULL;//neon版本的双边不需要临时缓存    //comment by wanghao because 550:not accessed
            
            BilateralFilter_new(pSrc[2],pTBuf, height, width, nSigmaS, nSigmaR);

            #ifdef __GNUC__
            //=================进行线程间同步，防止数据没有读就被写修改====
            pthread_mutex_lock(&uReadedLock);
            uReaded++;
            pthread_mutex_unlock(&uReadedLock);
            
            while(1)//循环里需要加语句，否则会形成死循环
            {
                //printf("==waiting\n");
                if(uReaded >=2)
                {
                    break;
                }
                
                usleep(1);    //功能把进程挂起一段时间， 单位是微秒
            }
            //=======================================================================
            #endif

           {
                INT32 offset = phHandle->threadOffset;
                INT32 dstH = phHandle->stParamIn.pstPicOut->height;
                UINT8 *pDst = phHandle->stParamIn.pstPicOut->pData[2];

                UINT8 *pSrcL,*pDstL;
                INT32 y;    //comment by wanghao because 529x,y;

                pSrcL = pTBuf + offset;
                pDstL = pDst;

                for(y = 0; y < dstH; y++)
                {
                    memcpy(pDstL,pSrcL,width);
                    pSrcL += width;
                    pDstL += stride;
                }
            }

        }
    }
    
    return  eRet;
}

