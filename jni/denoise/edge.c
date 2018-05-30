/*******************************************************************************
Copyright (C), 20010-2014, Huawei Tech. Co., Ltd.
File name: COMM.C
Author & ID: 杨小伟+ 00142940
Version: 1.00
Date:  2010-7-16
Description:  
Function List:

History:

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "basetype.h"
#include "define.h"
#include "CE_log.h"
#include "mathe.h"
#include "resize.h"
#include "gaussianblur.h"

/********************************************************************************
 * Function Description:     
 
    sobel 算子:
    水平   垂直
    -1 0 1     1   2  1
    -2 0 2     0   0  0
    -1 0 1    -1 -2 -1
    
 * Parameters:          
      > pSrc      - [in]  输入图像
      > pEdge    - [out]  输出边缘信息
      > width     - [in]  输入图像pSrc与输入图像边缘信息的宽度一样
      > height    - [in]  输入图像pSrc与输入图像边缘信息的高度一样
      > stride    - [in]  输入图像pSrc的步长
      > edgeStride    - [in]  输入图像边缘信息pEdge的步长
 ********************************************************************************/
void CalcSobel(UINT8 *pSrc ,UINT8 *pEdge,INT32 width ,INT32 height,INT32 stride,INT32 edgeStride) 
{
    UINT8 *pSrcL0,*pSrcL1,*pSrcL2/*,*pSrcL*/;
    UINT8 *pEdgeL;
    
    INT32 gx, gy;
    INT32 srcPad = stride - width + 2;
    INT32 dstPad = edgeStride - width;
    INT32 edgeV;
    INT32 x, y;

    memset(pEdge,0,width );//memset(pDst,0,width );
    
    pEdgeL = pEdge + (height-1) * edgeStride;
    
    memset(pEdgeL,0,width );//memset(pEdgeL,0,width );
    
    pSrcL0 = pSrc;
    pSrcL1 = pSrcL0 + stride;
    pSrcL2 = pSrcL1 + stride;
    pEdgeL = pEdge + edgeStride;
    for (y = 0 ; y < height-2; y++) 
    {
        *pEdgeL++ = 0;
        for (x = 0; x < width-2; x++) 
        {
            gx = (pSrcL0[2] + ((INT32)pSrcL1[2] << 1) + pSrcL2[2]) - (pSrcL0[0] + ((INT32)pSrcL1[0] << 1) + pSrcL2[0]);
            gy = (pSrcL0[0] + ((INT32)pSrcL0[1] << 1) + pSrcL0[2]) - (pSrcL2[0] + ((INT32)pSrcL2[1] << 1) + pSrcL2[2]);
            
            edgeV = (abs(gx) + abs(gy));
           // edgeV = sqrt((gx * gx) + (gy*gy));
            *pEdgeL++ = CLIP3(edgeV,0,255);//must use clip3 , do not use (UINT8)(abs(gx) + abs(gy))

            pSrcL0++;
            pSrcL1++;
            pSrcL2++;
        }
        
        *pEdgeL++ = 0;
        
        pSrcL0 += srcPad;
        pSrcL1 += srcPad;
        pSrcL2 += srcPad;
        pEdgeL += dstPad;
    }
}

/********************************************************************************
 * Function Description:     

    利用小波的两个高频分量进行边缘分析
 
    sobel 算子:
    水平   垂直
    -1 0 1     1   2  1
    -2 0 2     0   0  0
    -1 0 1    -1 -2 -1

 * Parameters:          
      > pSrcV                 - [in]  输入harr 小波的垂直方向高频图像
      > pSrcH                 - [in]  输入harr 小波的水平方向高频图像
      > pEdge              - [out]  输出边缘信息
      > width     - [in]  输入图像pSrcV、pSrcH与输入图像边缘信息的宽度一样
      > height    - [in]  输入图像pSrcV、pSrcH与输入图像边缘信息的高度一样
      > stride    - [in]  输入图像pSrcV、pSrcH的步长
      > edgeStride    - [in]  输入图像边缘信息pEdge的步长      
 ********************************************************************************/
void CalcSobel_HF(UINT8 *pSrcV ,UINT8 *pSrcH,UINT8 *pEdge,INT32 width ,INT32 height,INT32 stride,INT32 edgeStride) 
{
    UINT8 *pSrcVL0,*pSrcVL1,*pSrcVL2;
    UINT8 *pSrcHL0,*pSrcHL1,*pSrcHL2;
    UINT8 *pEdgeL;
    
    INT32 x, y;
    INT32 gx, gy;
    INT32 srcPad = stride - width + 2;
    INT32 dstPad = edgeStride - width;
    INT32 edgeV;

    memset(pEdge,0,width );//memset(pDst,0,width );
    
    pEdgeL = pEdge + (height-1) * edgeStride;
    
    memset(pEdgeL,0,width );//memset(pEdgeL,0,width );
    
    pSrcVL0 = pSrcV;
    pSrcVL1 = pSrcVL0 + stride;
    pSrcVL2 = pSrcVL1 + stride;

    pSrcHL0 = pSrcH;
    pSrcHL1 = pSrcHL0 + stride;
    pSrcHL2 = pSrcHL1 + stride;
    
    pEdgeL = pEdge + edgeStride;
    for (y = 0 ; y < height-2; y++) 
    {
        *pEdgeL++ = 0;
        for (x = 0; x < width-2; x++) 
        {
            gx = (pSrcHL0[2] + ((INT32)pSrcHL1[2] << 1) + pSrcHL2[2]) - (pSrcHL0[0] + ((INT32)pSrcHL1[0] << 1) + pSrcHL2[0]);
            gy = (pSrcVL0[0] + ((INT32)pSrcVL0[1] << 1) + pSrcVL0[2]) - (pSrcVL2[0] + ((INT32)pSrcVL2[1] << 1) + pSrcVL2[2]);
            
            edgeV = (abs(gx) + abs(gy));
           // edgeV = sqrt((gx * gx) + (gy*gy));
            *pEdgeL++ = CLIP3(edgeV,0,255);//must use clip3 , do not use (UINT8)(abs(gx) + abs(gy))

            pSrcVL0++;
            pSrcVL1++;
            pSrcVL2++;
            pSrcHL0++;
            pSrcHL1++;
            pSrcHL2++;            
        }
        
        *pEdgeL++ = 0;
        
        pSrcVL0 += srcPad;
        pSrcVL1 += srcPad;
        pSrcVL2 += srcPad;
        
        pSrcHL0 += srcPad;
        pSrcHL1 += srcPad;
        pSrcHL2 += srcPad;
        
        pEdgeL += dstPad;
    }
}


/*
输入:yuv420p三个平面
输出:
pEdgeY - Y平面边缘图像，大小为1/4图像大小(与UV大小相同)
pEdge   - yuv三个平面边缘合并成的边缘图像，大小为1/4图像大小(与UV大小相同)
*/
void EdgeAnalyse(UINT8 *pSrc[3], UINT8 *pEdgeY,UINT8 *pEdgeUV,INT32 width, INT32 height, INT32 stride )
{
    //UINT8 *pSrcL;
    UINT8 *pEdgeL,*pEdgeUL, *pEdgeVL;

    UINT8 *pBuf0 = (UINT8 *)malloc(stride* height);
    UINT8 *pBuf1 = (UINT8 *)malloc(stride* height/4);
    UINT8 *pBuf2 = (UINT8 *)malloc(stride* height/4);
    UINT8 *pBuf = (UINT8 *)malloc(stride* height/4);

    INT32 wDiv2 = width >> 1;
    INT32 hDiv2 = height >> 1;
    INT32 strideDiv2 = stride >> 1;

    INT32 x,y;

    MeanDown2x(pSrc[0],pBuf0, width, height, stride,wDiv2);

    GaussianBlur3x3(pBuf0,pBuf1,pBuf,wDiv2,hDiv2,wDiv2,wDiv2,wDiv2);
    CalcSobel(pBuf1,pEdgeY,wDiv2,hDiv2,wDiv2,wDiv2);
    //CalcSobel(pBuf0,pEdgeY,wDiv2,hDiv2,wDiv2,wDiv2);

    pEdgeUL = pBuf0;
    pEdgeVL = pBuf0 + wDiv2*hDiv2;

    pEdgeL  = pEdgeUV;

    GaussianBlur3x3(pSrc[1],pBuf1,pBuf,wDiv2,hDiv2,strideDiv2,wDiv2,wDiv2);
    GaussianBlur3x3(pSrc[2],pBuf2,pBuf,wDiv2,hDiv2,strideDiv2,wDiv2,wDiv2);
    CalcSobel(pBuf1,pEdgeUL,wDiv2,hDiv2,wDiv2,wDiv2);
    CalcSobel(pBuf2,pEdgeVL,wDiv2,hDiv2,wDiv2,wDiv2);

    for(y = 0; y < hDiv2; y++)
    {
        for(x = 0; x < wDiv2; x++)
        {
            pEdgeL[x] = MAX2(pEdgeUL[x],pEdgeVL[x]);
        }

        pEdgeUL += wDiv2;
        pEdgeVL += wDiv2;
        pEdgeL  += wDiv2;
    }
    

#if 0
    for(y = 0; y < hDiv2; y++)
    {
        for(x = 0; x < wDiv2; x++)
        {
            pEdgeL[x] = MAX2(pEdgeUL[x],pEdgeVL[x]);
            if(pEdgeL[x] < edgeTH)
            {
                pEdgeL[x] = (pEdgeL[x] * pEdgeL[x]) /edgeTH;
            }

            pEdgeL[x] = MAX2(pEdgeL[x],pEdgeYL[x]);
        }

        pEdgeUL += wDiv2;
        pEdgeVL += wDiv2;
        pEdgeYL += wDiv2;
        pEdgeL  += wDiv2;
    }

#endif

    free(pBuf0);
    free(pBuf1);
    free(pBuf2);
    free(pBuf);
    
}


