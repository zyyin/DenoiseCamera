/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_gaussianblur_V1.0
 * Date           : 2012-7-3
 * Description    : 高斯滤波
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "GaussianBlur.h"

#if 1
void InplaceBlur(UINT8 *data,int sizex, int sizey, float blur)
{
    float af = exp(log(0.5)/blur*sqrt((double)(2)));
    UINT32 a;
    INT32 pos;
    UINT32 old;
    
    INT32 x,y;
    
    if ((af<=0.0)||(af>=1.0)) 
    {
        return;
    }
    
    a=(int)(65536*af*af);
        
    if (a==0) 
    {
        return;
    }
    
    for (y=0;y<sizey;y++)
    {
        pos=y*sizex;
        old=data[pos]<<8;
        
        pos++;
        for (x=1;x<sizex;x++)
        {
            old=((data[pos]<<8)*(65535^a)+old*a)>>16;
            data[pos]=old>>8;
            pos++;
        }
        
        pos=y*sizex+sizex-1;
        for (x=1;x<sizex;x++)
        {
            old=((data[pos]<<8)*(65535^a)+old*a)>>16;
            data[pos]=old>>8;
            pos--;
        }

    }
    
    for (x=0;x<sizex;x++)
    {
        pos=x;
        old=data[pos]<<8;
        
        for (y=1;y<sizey;y++)
        {
            old=((data[pos]<<8)*(65535^a)+old*a)>>16;
            data[pos]=old>>8;
            pos+=sizex;
        }
        
        pos=x+sizex*(sizey-1);
        
        for (y=1;y<sizey;y++)
        {
            old=((data[pos]<<8)*(65535^a)+old*a)>>16;
            data[pos]=old>>8;
            pos-=sizex;
        }

    };
    
}
#endif

 /********************************************************************************
 * Function Description:       
      1.  生成高斯滤波参数
 * Parameters:          
      > pKernel   - [out]  高斯kernel
      > winSize   - [in] 高斯窗口直径
 ********************************************************************************/
void MakeGauss(double **pKernel, INT32 winSize)
{
    double  dDis  ;  // 数组的某一点到中心点的距离
    double  dValue; // 中间变量
    double  dSum  ;
    double  sigma;

    INT32 nCenter;// 数组的中心点
    INT32 i;

    dSum = 0 ; 

    // 数组长度，根据概率论的知识，选取[-3*sigma, 3*sigma]以内的数据。
    // 这些数据会覆盖绝大部分的滤波系数
    sigma = (double)(winSize - 1)/(2.0 * 3.0);

    // 中心
    nCenter = winSize / 2;

    // 分配内存
    *pKernel = (double *)malloc(sizeof(double)*winSize) ;

    for(i=0; i< winSize; i++)
    {
        dDis = (double)(i - nCenter);
        dValue = exp(-dDis*dDis/(2*sigma*sigma)) / (sqrt(2 * PI) * sigma );
        (*pKernel)[i] = dValue ;
        dSum += dValue;
    }

    // 归一化
    for(i=0; i < winSize ; i++)
    {
        (*pKernel)[i] /= dSum;
    }

}

 /********************************************************************************
 * Function Description:       
      1.  高斯滤波
 * Parameters:          
      > pSrc      - [in] 输入图像
      > pDst   - [out]  高斯滤波结果
      > winSize   - [in] 高斯窗口直径
 ********************************************************************************/
void GaussianSmooth(UINT8 *pSrc, UINT8 * pDst, INT32 width, INT32 height, INT32 stride, INT32 winSize)
{
    INT32 y;
    INT32 x;
    INT32 i;

    //  窗口长度的1/2
    INT32    nHalfLen;   

    // 一维高斯数据滤波器
    double *pKernel ;

    // 高斯系数与图象数据的点乘
    double  dDotMul     ;

    // 高斯滤波系数的总和
    double  dWeightSum     ;          

    // 中间变量
    double * pdTmp ;

    // 分配内存
    pdTmp = (double *)malloc(sizeof(double)*width*height);

    // 产生一维高斯数据滤波器
    MakeGauss(&pKernel, winSize) ;

    // MakeGauss返回窗口的长度，利用此变量计算窗口的半长
    nHalfLen = winSize / 2;

    // x方向进行滤波
    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            dDotMul = 0;
            dWeightSum = 0;
            for(i=(-nHalfLen); i<=nHalfLen; i++)
            {
                // 判断是否在图象内部
                if( (i+x) >= 0  && (i+x) < width )
                {
                    dDotMul += (double)pSrc[y*stride + (i+x)] * pKernel[nHalfLen+i];
                    dWeightSum += pKernel[nHalfLen+i];
                }
            }
            pdTmp[y*width + x] = dDotMul/dWeightSum ;
        }
    }

    // y方向进行滤波
    for(x=0; x<width; x++)
    {
        for(y=0; y<height; y++)
        {
            dDotMul = 0;
            dWeightSum = 0;
            for(i=(-nHalfLen); i<=nHalfLen; i++)
            {
                // 判断是否在图象内部
                if( (i+y) >= 0  && (i+y) < height )
                {
                    dDotMul += (double)pdTmp[(y+i)*width + x] * pKernel[nHalfLen+i];
                    dWeightSum += pKernel[nHalfLen+i];
                }
            }
            
            pDst[y*width + x] = (UINT8)CLIP_UINT8(dDotMul/dWeightSum) ;
        }
    }

    // 释放内存
    free(pKernel);
    free(pdTmp);
}



 /********************************************************************************
 * Function Description:       
      1.  3x3高斯滤波
 * Parameters:          
      > pSrc      - [in] 输入图像
      > pDst   - [out]  高斯滤波结果
      > pBuf      - [in] 临时缓冲区，大小是图像大小
      > width      - [in] 图像宽度(pSrc、pDst、pBuf)
      > height      - [in]图像高度(pSrc、pDst、pBuf)
      > srcStride      - [in] 输入图像步长
      > dstStride      - [in] 输出图像步长
      > bufStride      - [in] 临时缓冲区步长
 ********************************************************************************/
void GaussianBlur3x3 (UINT8 *  pSrc, UINT8 *  pDst,UINT8 *  pBuf,INT32 width, INT32 height,INT32 srcStride,INT32 dstStride,INT32 bufStride)
{
    UINT8 *pSrcL;
    UINT8 *pLine0,*pLine1,*pLine2;
    UINT8 *pDstL;
    UINT8 *pBufL;
    
    INT32 x,y;
    INT32 padBuf       = bufStride - width;
    INT32 padSrc       = srcStride - width + 1;
    INT32 padDst       = dstStride - width;

    pSrcL = pSrc;
    pDstL = pDst;
    pBufL = pBuf;

    /*******************************************************************************
          Horizontal blur
     *******************************************************************************/
    for(y = height; y != 0 ; y--)
    {
        *pBufL++ = ((pSrcL[0] * 3 + pSrcL[1]) >> 2);
 
        for(x = 1 ; x < width - 1 ; x++)
        {
            *pBufL++ = ((pSrcL[0] + pSrcL[1] * 2 + pSrcL[2]) >> 2);
            pSrcL++;
        }

        *pBufL++ = ((pSrcL[0] + pSrcL[1] * 3) >> 2);
        pSrcL++;

        pBufL += padBuf;
        pSrcL += padSrc;
        
    }

    /*******************************************************************************
              Vertical blur
    *******************************************************************************/
    pLine0 = pBuf;
    pLine1 = pBuf + bufStride;
    pDstL = pDst;

    //the first line
    for(x = width; x!=0 ; x--)
    {
        *pDstL++ = ((*pLine0)*3 + (*pLine1))>>2;
        pLine0++;
        pLine1++;        
    }
    
    pLine0 = pBuf;
    pLine1 = pBuf + bufStride;
    pLine2 = pBuf + 2*bufStride;
    pDstL += padDst;

    for(y = height - 2; y != 0 ; y--)
    {
        for(x = width; x!=0 ; x--)
        {
            *pDstL++ = (((*pLine0) + (*pLine1)*2 + (*pLine2)) >> 2);
            pLine0++;
            pLine1++;
            pLine2++;
        }
        
        pDstL += padDst;
        pBufL += padBuf;
        pLine0 += padBuf;
        pLine1 += padBuf;
        pLine2 += padBuf;
    }

    //the last line
    for(x = width; x!=0 ; x--)
    {
        *pDstL++ = ((*pLine0) + (*pLine1)*3)>>2;
        pLine0++;
        pLine1++;  
    }
    
}


/**********************************************************************
    GaussianBlur5x5 1D kernel:
    1 4 6 4 1
**********************************************************************/
void GaussianBlur5x5(UINT8 *pSrc,UINT8 *pDst,UINT8 *pBuf,INT32 width, INT32 height, INT32 srcStride, INT32 dstStride)
{
    UINT8 *pSrcL,*pDstL;
    UINT8 *pBufL,*pBufL0,*pBufL1,*pBufL2,*pBufL3,*pBufL4;

    INT32 srcPad = srcStride - width;
    //INT32 dstPad = dstStride - width;
    INT32 x,y;

    //============================水平滤波==========================
    pSrcL = pSrc;
    pBufL = pBuf;
    for(y = 0; y < height; y++)
    {
        *pBufL++ = ((pSrcL[0] + pSrcL[1] + 1) >> 1); //使用均值
        *pBufL++ = ((pSrcL[0] + (pSrcL[1]<<1) + pSrcL[1] + 2) >> 2); // 1 2 1
        
        for(x = 2; x < width-2; x++)
        {
            *pBufL++ = ((pSrcL[0] + pSrcL[1]*4 + pSrcL[2]*6 + pSrcL[3]*4 + pSrcL[4] + 8) >> 4); // 1 4 6 4 1
            pSrcL++;
        }

        pSrcL++;
        *pBufL++ = ((pSrcL[0] + (pSrcL[1]<<1) + pSrcL[1] + 2) >> 2); // 1 2 1        
        pSrcL++;
        *pBufL++ = ((pSrcL[0] + pSrcL[1] + 1) >> 1); //使用均值

        pSrcL += 2; //偏移到当前行末尾
        
        pSrcL += srcPad;
    }

    //============================垂直滤波==========================
    pBufL0 = pBuf;
    pBufL1 = pBufL0 + width;
    pBufL2 = pBufL1 + width;
    pBufL3 = pBufL2 + width;
    pBufL4 = pBufL3 + width;
    pDstL = pDst;

    for(x = 0; x < width; x++)
    {
        pDstL[x] = ((pBufL0[x] + pBufL1[x] + 1) >> 1); //使用均值
    }

    pDstL += dstStride;

    for(x = 0; x < width; x++)
    {
        pDstL[x] = ((pBufL0[x] + (pBufL1[x]<<1) + pBufL2[x] + 2) >> 2); // 1 2 1
    }

    pDstL += dstStride;
    
    for(y = 2; y < height-2; y++)
    {        
        for(x = 0; x < width; x++)
        {
            pDstL[x] = ((pBufL0[x] + pBufL1[x]*4 + pBufL2[x]*6 + pBufL3[x]*4 + pBufL4[x] + 8) >> 4); // 1 2 1
        }
        
        pBufL0 += width;
        pBufL1 += width;
        pBufL2 += width;       
        pBufL3 += width;
        pBufL4 += width;         
        pDstL  += dstStride;
    }    

    for(x = 0; x < width; x++)
    {
        pDstL[x] = ((pBufL1[x] + (pBufL2[x]<<1) + pBufL3[x] + 2) >> 2); // 1 2 1
    }
    
    
    for(x = 0; x < width; x++)
    {
        pDstL[x] = ((pBufL2[x] + pBufL3[x] + 1) >> 1); //使用均值
    }
    
}


/************************************************************************
* $Log: GaussianBlur.c,v $
* Revision 1.1  2011/11/02 02:50:35  huangyining
* 项目组转测试版本上传
*
************************************************************************/







