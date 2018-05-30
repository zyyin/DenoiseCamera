/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_resize_V1.0
 * Date           : 2012-7-3
 * Description    : 
 * Function List  : 
 * History        : 
 ********************************************************************************/


#include "resize.h"


/*******************************************************************************
    input is sigle plane
*******************************************************************************/
void MeanDown2x(UINT8 *pSrc, UINT8 *pDst, INT32 srcW, INT32 srcH, INT32 srcStride,  INT32  dstStride)
{
    UINT8 *pDstL;
    UINT8 *pSrcL;
    INT32 dstWDiv2 = srcW>>1;
    INT32 dstHDiv2  = srcH>>1;
    INT32 wPad     = dstStride - dstWDiv2;
    INT32 x,y;
    
    pSrcL = pSrc ;
    pDstL = pDst;

    for(y = dstHDiv2; y != 0; y--)
    {
        for(x = dstWDiv2; x != 0; x--)
        {
            *pDstL++ = (pSrcL[0] + pSrcL[1] + pSrcL[srcStride] + pSrcL[srcStride+1] + 2) >> 2;
            pSrcL += 2;
        }

        pDstL += wPad;
        pSrcL += srcStride;
    }

    //if(wPad)//pad 
    //{
    //    PadPlane(pDst, dstWDiv2, dstHDiv2, dstStride, dstHDiv2, dstStride);//将每一行都填充满
    //}
    
}

void MeanUp2x(UINT8 *pSrc, UINT8 *pDst, INT32 srcW, INT32 srcH, INT32 srcStride,  INT32  dstStride)
{
    UINT8 *pDstL;
    UINT8 *pSrcL;
    INT32 x,y;

    pSrcL = pSrc ;
    pDstL = pDst;
    
    for(y = srcH; y != 0; y--)
    {
        for(x = srcW-1; x != 0; x--)
        {
            *pDstL++ = pSrcL[0];
            *pDstL++ = (pSrcL[0] + pSrcL[1] + 1) >> 1;
            pSrcL++;
        }
        
        *pDstL++ = pSrcL[0];
        *pDstL++ = pSrcL[0];
        pSrcL++;
            
        pDstL += dstStride;
    }
    
    pDstL = pDst + dstStride;
    for(y = srcH -1; y != 0; y--)
    {
        for(x = srcW << 1; x != 0; x--)
        {
            *pDstL = (pDstL[-dstStride] + pDstL[dstStride] + 1) >> 1;
            pDstL++;
        }

        pDstL += dstStride;
    }

    memcpy(pDstL, pDstL-dstStride, dstStride);
    
}


/*===========================================================================

Function            : DownScale

Description         : 下采样
=========================================================================== */
void DownScale(UINT8 *pImageSrc, INT32 nHeight, INT32 nWidth,INT32 srcStride ,UINT8 *pImageDst)
{
    INT32 i = 0, j = 0;

    UINT8 *pSrc1 = pImageSrc, *pSrc2 = pImageSrc + srcStride;
    UINT8 *pDst = pImageDst;

    //隔行隔列遍历大图
    for(i = 0; i != nHeight; i += 2)
    {
        for(j = 0; j != nWidth; j += 2)
        {
            //biliner interpolation
            *pDst ++ = ( *pSrc1 + *(pSrc1+1) + *pSrc2 + *(pSrc2+1) + 2 ) >> 2;

            pSrc1 += 2;
            pSrc2 += 2;
        }

        pSrc1 += srcStride;
        pSrc2 += srcStride;
    }

}

/*===========================================================================

Function            : UpScale

Description         : 上采样
=========================================================================== */
void UpScale(const UINT8 *pImageSrc, INT32 nHeight, INT32 nWidth,INT32 dstStride, UINT8 *pImageDst)
{
/*
    INT32 i = 0, j = 0;
    INT32 nWidthX2 = nWidth + nWidth;

    const UINT8 *pSrc = pImageSrc;
    UINT8 *pDst1 = pImageDst, *pDst2 = pImageDst + dstStride ;
    INT32 offset = 2*dstStride - nWidthX2;

    //////////////////////////////////////////////////////////////////////////
    //the first nHeight-1 lines
    for(i = 1; i != nHeight; i ++)
    {
        //the first nWidth-1 pixels
        for(j = 1; j != nWidth; j ++)
        {
            *pDst1++ = *pSrc;
            *pDst1++ = (*pSrc + *(pSrc+1) + 1) >> 1;
            *pDst2++ = (*pSrc + *(pSrc+nWidth) + 1) >> 1;
            *pDst2++ = (*pSrc + *(pSrc+1) + *(pSrc+nWidth) + *(pSrc+nWidth+1) + 2) >> 2;

            pSrc++;
        }

        //the last pixel
        *pDst1++ = *pSrc;
        *pDst1++ = *pSrc;
        *pDst2++ = (*pSrc + *(pSrc+nWidth) + 1) >> 1;
        *pDst2++ = (*pSrc + *(pSrc+nWidth) + 1) >> 1;
        pSrc++;

        pDst1 += offset;
        pDst2 += offset;
    }

    //////////////////////////////////////////////////////////////////////////
    //the last line
    //the first nWidth-1 pixels
    for(j = 1; j != nWidth; j ++)
    {
        *pDst1++ = *pSrc;
        *pDst1++ = (*pSrc + *(pSrc+1) + 1) >> 1;
        *pDst2++ = *pSrc;
        *pDst2++ = (*pSrc + *(pSrc+1) + 1) >> 1;

        pSrc++;
    }

    //the last pixel
    *pDst1++ = *pSrc;
    *pDst1++ = *pSrc;
    *pDst2++ = *pSrc;
    *pDst2++ = *pSrc;
*/
    INT32 i = 0, j = 0;
    INT32 nWidthX2 = nWidth + nWidth;

    const UINT8 *pSrc = pImageSrc;
    UINT8 *pDst1 = pImageDst, *pDst2 = pImageDst + dstStride ;
    INT32 offset = 2*dstStride - nWidthX2;

    /////////////////////////////////////////////////////////////////////////
    //the first line
    *pDst1++ = *pSrc;
    for(j = 1; j != nWidth; j ++)
    {
        *pDst1++ = ( *pSrc*3 + *(pSrc+1) + 2) >> 2;
        *pDst1++ = ( *pSrc + *(pSrc+1)*3 + 2) >> 2;
        pSrc++;
    }
    *pDst1++ = *pSrc++;

    pSrc -= nWidth;
    pDst1 += offset;

    //////////////////////////////////////////////////////////////////////////
    //the mid nHeight-1 lines
    for(i = 1; i != nHeight; i ++)
    {
        *pDst2++ = (*pSrc*3 + *(pSrc+nWidth) + 2) >> 2;
        *pDst1++ = (*pSrc + *(pSrc+nWidth)*3 + 2) >> 2;

        for(j = 1; j != nWidth; j ++)
        {
            *pDst2++ = (*pSrc*9 + *(pSrc+1)*3 + *(pSrc+nWidth)*3 + *(pSrc+nWidth+1) + 8) >> 4;
            *pDst2++ = (*pSrc*3 + *(pSrc+1)*9 + *(pSrc+nWidth) + *(pSrc+nWidth+1)*3 + 8) >> 4;
            *pDst1++ = (*pSrc*3 + *(pSrc+1) + *(pSrc+nWidth)*9 + *(pSrc+nWidth+1)*3 + 8) >> 4;
            *pDst1++ = (*pSrc + *(pSrc+1)*3 + *(pSrc+nWidth)*3 + *(pSrc+nWidth+1)*9 + 8) >> 4;

            pSrc++;
        }

        *pDst2++ = (*pSrc*3 + *(pSrc+nWidth) + 2) >> 2;
        *pDst1++ = (*pSrc + *(pSrc+nWidth)*3 + 2) >> 2;
        pSrc++;

        pDst2 += offset;
        pDst1 += offset;
    }

    //////////////////////////////////////////////////////////////////////////
    //the last line
    *pDst2++ = *pSrc;
    for(j = 1; j != nWidth; j ++)
    {
        *pDst2++ = ( *pSrc*3 + *(pSrc+1) + 2) >> 2;
        *pDst2++ = ( *pSrc + *(pSrc+1)*3 + 2) >> 2;

        pSrc++;
    }
    *pDst2++ = *pSrc;
}

void UpScale_thread(const UINT8 *pImageSrc, INT32 threadOffset,INT32 srcH,INT32 nHeight, INT32 nWidth,INT32 dstStride, UINT8 *pImageDst)
{
    INT32 i = 0, j = 0;
    INT32 nWidthX2 = nWidth + nWidth;

    const UINT8 *pSrc = pImageSrc + (threadOffset>>2);
    UINT8 *pDst1 = pImageDst, *pDst2 = pImageDst + dstStride ;
    INT32 offset = 2*dstStride - nWidthX2;

    /////////////////////////////////////////////////////////////////////////
    //the first line
    if(0 == threadOffset)
    {
        *pDst1++ = *pSrc;
        for(j = 1; j != nWidth; j ++)
        {
            *pDst1++ = ( *pSrc*3 + *(pSrc+1) + 2) >> 2;
            *pDst1++ = ( *pSrc + *(pSrc+1)*3 + 2) >> 2;
            pSrc++;
        }
        *pDst1++ = *pSrc++;

        pSrc -= nWidth;
        pDst1 += offset;
    }
    else
    {
        *pDst1++ = (*(pSrc - nWidth) + *(pSrc)*3 + 2) >> 2;

        for(j = 1; j != nWidth; j ++)
        {
            *pDst1++ = (*(pSrc - nWidth)*3 + *(pSrc - nWidth + 1) + *(pSrc)*9 + *(pSrc+1)*3 + 8) >> 4;
            *pDst1++ = (*(pSrc - nWidth) + *(pSrc - nWidth + 1)*3 + *(pSrc)*3 + *(pSrc+1)*9 + 8) >> 4;

            pSrc++;
        }

        *pDst1++ = (*(pSrc - nWidth) + *(pSrc)*3 + 2) >> 2;
        pSrc++;
        
        pSrc -= nWidth;
        pDst1 += offset;        
    }

    //////////////////////////////////////////////////////////////////////////
    //the mid nHeight-1 lines
    for(i = 1; i != nHeight; i ++)
    {
        *pDst2++ = (*pSrc*3 + *(pSrc+nWidth) + 2) >> 2;
        *pDst1++ = (*pSrc + *(pSrc+nWidth)*3 + 2) >> 2;

        for(j = 1; j != nWidth; j ++)
        {
            *pDst2++ = (*pSrc*9 + *(pSrc+1)*3 + *(pSrc+nWidth)*3 + *(pSrc+nWidth+1) + 8) >> 4;
            *pDst2++ = (*pSrc*3 + *(pSrc+1)*9 + *(pSrc+nWidth) + *(pSrc+nWidth+1)*3 + 8) >> 4;
            *pDst1++ = (*pSrc*3 + *(pSrc+1) + *(pSrc+nWidth)*9 + *(pSrc+nWidth+1)*3 + 8) >> 4;
            *pDst1++ = (*pSrc + *(pSrc+1)*3 + *(pSrc+nWidth)*3 + *(pSrc+nWidth+1)*9 + 8) >> 4;

            pSrc++;
        }

        *pDst2++ = (*pSrc*3 + *(pSrc+nWidth) + 2) >> 2;
        *pDst1++ = (*pSrc + *(pSrc+nWidth)*3 + 2) >> 2;
        pSrc++;

        pDst2 += offset;
        pDst1 += offset;
    }

    //////////////////////////////////////////////////////////////////////////
    //the last line
    if(0 != threadOffset)//第二个分块下面没有参考点
    {
        *pDst2++ = *pSrc;
        for(j = 1; j != nWidth; j ++)
        {
            *pDst2++ = ( *pSrc*3 + *(pSrc+1) + 2) >> 2;
            *pDst2++ = ( *pSrc + *(pSrc+1)*3 + 2) >> 2;

            pSrc++;
        }
        *pDst2++ = *pSrc;
    }
    else
    {
        *pDst2++ = (*pSrc*3 + *(pSrc+nWidth) + 2) >> 2;

        for(j = 1; j != nWidth; j ++)
        {
            *pDst2++ = (*pSrc*9 + *(pSrc+1)*3 + *(pSrc+nWidth)*3 + *(pSrc+nWidth+1) + 8) >> 4;
            *pDst2++ = (*pSrc*3 + *(pSrc+1)*9 + *(pSrc+nWidth) + *(pSrc+nWidth+1)*3 + 8) >> 4;

            pSrc++;
        }

        *pDst2++ = (*pSrc*3 + *(pSrc+nWidth) + 2) >> 2;
        pSrc++;

        pDst1 += offset;
    }
}

/*===========================================================================

Function            : DownScaleWithError

Description         : 保留误差的下采样

Restriction         : nWidth代表行方向像素个数，必须为16的整数倍
=========================================================================== */
INT32 DownScaleWithError(UINT8 *pImageSrc, INT32 nHeight, INT32 nWidth, UINT8 *pImageDst, INT16 *pError)
{
#ifdef _ARM_NEON_OPTIMIZE
    INT32 i = 0, j = 0;

    UINT8 *pSrc1 = pImageSrc, *pSrc2 = pImageSrc + nWidth;
    INT16 *pError1 = pError, *pError2 = pError + nWidth;

    UINT8 *pDst = pImageDst;

    uint8x8_t uint8x8;
    int16x8_t int16x8, int16x8_1, int16x8_2, int16x8_3, int16x8_4;  
    int16x8x2_t int16x8x2_1;

    //隔行遍历大图
    for(i = 0; i != nHeight; i += 2)
    {
        for(j = 0; j != nWidth; j += 16)
        {
            //src
            //1234 5678 abcd efgh
            //lmno pqrs ABCD EFGH 
            uint8x8 = vld1_u8(pSrc1);
            int16x8_1 = vreinterpretq_s16_u16(vmovl_u8(uint8x8));
            pSrc1 += 8;

            uint8x8 = vld1_u8(pSrc2);
            int16x8_2 = vreinterpretq_s16_u16(vmovl_u8(uint8x8));
            pSrc2 += 8;

            uint8x8 = vld1_u8(pSrc1);
            int16x8_3 = vreinterpretq_s16_u16(vmovl_u8(uint8x8));
            pSrc1 += 8;

            uint8x8 = vld1_u8(pSrc2);
            int16x8_4 = vreinterpretq_s16_u16(vmovl_u8(uint8x8));
            pSrc2 += 8;

            //sum of top and bottom
            //abcdefgh = 1234 5678 + lmno pqrs
            //ABCDEFGH = abcd efgh + ABCD EFGH
            int16x8x2_1.val[0] = vaddq_s16(int16x8_1, int16x8_2);
            int16x8x2_1.val[1] = vaddq_s16(int16x8_3, int16x8_4);

            //sum of left and right
            //1234 5678 = a+b c+d ... A+B C+D ...
            int16x8x2_1 = vuzpq_s16(int16x8x2_1.val[0], int16x8x2_1.val[1]);
            int16x8 = vaddq_s16(int16x8x2_1.val[0], int16x8x2_1.val[1]);

            //average
            int16x8 = vshrq_n_s16(int16x8, 2);

            //copy to twice length
            //11223344 55667788
            int16x8x2_1 = vzipq_s16(int16x8, int16x8);

            //error = src -dst
            int16x8_1 = vsubq_s16(int16x8_1, int16x8x2_1.val[0]);
            vst1q_s16(pError1, int16x8_1);
            pError1 += 8;

            int16x8_2 = vsubq_s16(int16x8_2, int16x8x2_1.val[0]);
            vst1q_s16(pError2, int16x8_2);
            pError2 += 8;

            int16x8_3 = vsubq_s16(int16x8_3, int16x8x2_1.val[1]);
            vst1q_s16(pError1, int16x8_3);
            pError1 += 8;

            int16x8_4 = vsubq_s16(int16x8_4, int16x8x2_1.val[1]);
            vst1q_s16(pError2, int16x8_4);
            pError2 += 8;

            //dst
            uint8x8 = vqmovun_s16(int16x8);
            vst1_u8(pDst, uint8x8);
            pDst += 8;
        }

        pSrc1 += nWidth;
        pSrc2 += nWidth;

        pError1 += nWidth;
        pError2 += nWidth;
    }
#else
    INT32 i = 0, j = 0;

    UINT8 *pSrc1 = pImageSrc, *pSrc2 = pImageSrc + nWidth;
    INT16 *pError1 = pError, *pError2 = pError + nWidth;

    UINT8 *pDst = pImageDst;

    //隔行隔列遍历大图
    for(i = 0; i != nHeight; i += 2)
    {
        for(j = 0; j != nWidth; j += 2)
        {
            //biliner interpolation
            *pDst = ( *pSrc1 + *(pSrc1+1) + *pSrc2 + *(pSrc2+1) + 2 ) >> 2;

            //error reservation
            *pError1++ = *pSrc1++ - *pDst;
            *pError1++ = *pSrc1++ - *pDst;

            *pError2++ = *pSrc2++ - *pDst;
            *pError2++ = *pSrc2++ - *pDst;
 
            pDst ++;
        }

        pSrc1 += nWidth;
        pSrc2 += nWidth;

        pError1 += nWidth;
        pError2 += nWidth;
    }
#endif
    return 0;
}

/*===========================================================================

Function            : UpScaleWithError

Description         : 保留误差的上采样

Restriction         : nWidth代表行方向像素个数，必须为8的整数倍
=========================================================================== */
INT32 UpScaleWithError(UINT8 *pImageSrc, INT32 nHeight, INT32 nWidth, UINT8 *pImageDst, INT16 *pError)
{
#ifdef _ARM_NEON_OPTIMIZE
    uint8x8_t uint8x8;
    uint8x8x2_t uint8x8x2;
    uint16x8_t uint16x8;
    int16x8_t int16x8, int16x8_1;

    INT32 i = 0, j = 0;

    INT32 nWidthX2 = nWidth + nWidth;

    UINT8 *pSrc = pImageSrc;
    UINT8 *pDst1 = pImageDst, *pDst2 = pImageDst + nWidthX2 ;
    INT16 *pError1 = pError, *pError2 = pError + nWidthX2;

    //逐行逐列遍历小图
    for(i = 0; i != nHeight; i ++)
    {
        for(j = 0; j != nWidth; j +=8)
        {
            //读8个下采样像素
            //1234 5678
            uint8x8 = vld1_u8(pSrc);
            pSrc += 8;

            //对应到原始的16个像素
            //11223344 55667788
            uint8x8x2 = vzip_u8(uint8x8, uint8x8);

            //左8个
            //11223344
            uint16x8 = vmovl_u8(uint8x8x2.val[0]);
            int16x8 = vreinterpretq_s16_u16(uint16x8);

            int16x8_1 = vld1q_s16(pError1);
            int16x8_1 = vaddq_s16(int16x8, int16x8_1);
            uint8x8 = vqmovun_s16(int16x8_1);
            vst1_u8(pDst1, uint8x8);
            pError1 += 8;
            pDst1 += 8;

            int16x8_1 = vld1q_s16(pError2);
            int16x8_1 = vaddq_s16(int16x8, int16x8_1);
            uint8x8 = vqmovun_s16(int16x8_1);
            vst1_u8(pDst2, uint8x8);
            pError2 += 8;
            pDst2 += 8;

            //右8个
            //55667788
            uint16x8 = vmovl_u8(uint8x8x2.val[1]);
            int16x8 = vreinterpretq_s16_u16(uint16x8);

            int16x8_1 = vld1q_s16(pError1);
            int16x8_1 = vaddq_s16(int16x8, int16x8_1);
            uint8x8 = vqmovun_s16(int16x8_1);
            vst1_u8(pDst1, uint8x8);
            pError1 += 8;
            pDst1 += 8;

            int16x8_1 = vld1q_s16(pError2);
            int16x8_1 = vaddq_s16(int16x8, int16x8_1);
            uint8x8 = vqmovun_s16(int16x8_1);
            vst1_u8(pDst2, uint8x8);
            pError2 += 8;
            pDst2 += 8;
        }

        pDst1 += nWidthX2;
        pDst2 += nWidthX2;

        pError1 += nWidthX2;
        pError2 += nWidthX2;
    }
#else
    INT32 i = 0, j = 0;

    INT32 nWidthX2 = nWidth + nWidth;

    UINT8 *pSrc = pImageSrc;
    UINT8 *pDst1 = pImageDst, *pDst2 = pImageDst + nWidthX2 ;
    INT16 *pError1 = pError, *pError2 = pError + nWidthX2;

    INT16 result = 0;

    //逐行逐列遍历小图
    for(i = 0; i != nHeight; i ++)
    {
        for(j = 0; j != nWidth; j ++)
        {
            result = *pSrc + *pError1++;
            *pDst1++ = MAX2(0, MIN2(255, result));

            result = *pSrc + *pError1++;
            *pDst1++ = MAX2(0, MIN2(255, result));

            result = *pSrc + *pError2++;
            *pDst2++ = MAX2(0, MIN2(255, result));
            
            result = *pSrc + *pError2++;
            *pDst2++ = MAX2(0, MIN2(255, result));

            pSrc++;
        }

        pDst1 += nWidthX2;
        pDst2 += nWidthX2;

        pError1 += nWidthX2;
        pError2 += nWidthX2;
    }
#endif

    return 0;
}

void UpScaleX16(unsigned char* srcImage, int height, int width, unsigned char *dstImage)
{    
    int h, w;
    int dstWidth = width << 2;
    int dstHeight = height << 2;
    unsigned char* p1 = srcImage;
    unsigned char* p2 = srcImage + width;

    unsigned char* pDst1 = dstImage + dstWidth*2 + 2;
    unsigned char* pDst2 = pDst1 + dstWidth;
    unsigned char* pDst3 = pDst2 + dstWidth;
    unsigned char* pDst4 = pDst3 + dstWidth;

    /************************************************************************/
    /*  填0法                                                                     */
    /************************************************************************/
    /*    for (int h = 0; h  <dstHeight; h++)
    {
    dstImage[h*dstWidth] = 0;
    dstImage[h*dstWidth+dstWidth-1] = 0;

    dstImage[h*dstWidth+1] = 0;
    dstImage[h*dstWidth+dstWidth-2] = 0;
    }
    for (int w = 0; w < dstWidth; w++) 
    {
    dstImage[w] = 0;
    dstImage[dstWidth*(dstHeight-1)+w] = 0;

    dstImage[dstWidth+w] = 0;
    dstImage[dstWidth*(dstHeight-2)+w] = 0;
    }
    */    
    /************************************************************************/
    /*    近邻法                                                                     */
    /************************************************************************/
    unsigned char* pTmp1 = dstImage;
    unsigned char* pTmp2 = dstImage + dstWidth - 1;
    // 前两列和后两列
    /*    for (int h = 0; h < height; h++)
    {
    *pTmp1 = srcImage[h*width];        *(pTmp1+1) = *pTmp1;
    *(pTmp1+dstWidth) = *pTmp1;    *(pTmp1+dstWidth+1) = *pTmp1;
    *(pTmp1+2*dstWidth) = *pTmp1;    *(pTmp1+2*dstWidth+1) = *pTmp1;
    *(pTmp1+3*dstWidth) = *pTmp1;    *(pTmp1+3*dstWidth+1) = *pTmp1;

    *pTmp2 = srcImage[h*width+width-1];        *(pTmp2-1) = *pTmp2; 
    *(pTmp2+dstWidth) = *pTmp2;    *(pTmp2+dstWidth-1) = *pTmp2;
    *(pTmp2+2*dstWidth) = *pTmp2;    *(pTmp2+2*dstWidth-1) = *pTmp2;
    *(pTmp2+3*dstWidth) = *pTmp2;    *(pTmp2+3*dstWidth-1) = *pTmp2;

    pTmp1 += (dstWidth*4);
    pTmp2 += (dstWidth*4);
    }

    // 前两行和后两行
    pTmp1 = dstImage;
    pTmp2 = dstImage + dstWidth;
    unsigned char* pTmp3 = dstImage + (dstHeight-2)*dstWidth;
    unsigned char* pTmp4 = dstImage + (dstHeight-1)*dstWidth;
    for (int w = 0; w < width; w++)
    {
    *pTmp1++ = srcImage[w];
    *pTmp1++ = srcImage[w];
    *pTmp1++ = srcImage[w];
    *pTmp1++ = srcImage[w];

    *pTmp2++ = srcImage[w];
    *pTmp2++ = srcImage[w];
    *pTmp2++ = srcImage[w];
    *pTmp2++ = srcImage[w];

    *pTmp3++ = srcImage[(height-1)*width+w];
    *pTmp3++ = srcImage[(height-1)*width+w];
    *pTmp3++ = srcImage[(height-1)*width+w];
    *pTmp3++ = srcImage[(height-1)*width+w];

    *pTmp4++ = srcImage[(height-1)*width+w];
    *pTmp4++ = srcImage[(height-1)*width+w];
    *pTmp4++ = srcImage[(height-1)*width+w];
    *pTmp4++ = srcImage[(height-1)*width+w];
    }


    /************************************************************************/
    /*   插值法                                                                     */
    /************************************************************************/
    pTmp1 = dstImage;
    // 第一列
    for ( h = 0; h < height; h++)
    {
        *pTmp1 = srcImage[h*width];        
        pTmp1 += (dstWidth*4);
    }
    pTmp1 = dstImage;
    pTmp2 = dstImage + dstWidth * 4;
    for ( h = 0; h < height-1; h++)
    {
        *(pTmp1+dstWidth) = (*pTmp1) * 3/4.0 + (*pTmp2) * 1/4.0;
        *(pTmp1+2*dstWidth) = (*pTmp1) * 2/4.0 + (*pTmp2) * 2/4.0;
        *(pTmp1+3*dstWidth) = (*pTmp1) * 1/4.0 + (*pTmp2) * 3/4.0;

        pTmp1 += (dstWidth*4);
        pTmp2 += (dstWidth*4);
    }
    *(pTmp1+dstWidth) = *pTmp1;

    // 第二列
    pTmp1 = dstImage;
    for ( h = 0; h < dstHeight; h++)
    {
        *(pTmp1+1) = *pTmp1;
        pTmp1 += dstWidth;
    }

    // 第一行
    pTmp1 = dstImage;
    for ( w = 0; w < width; w++)
    {
        *pTmp1 = srcImage[w];        
        pTmp1 += 4;
    }
    pTmp1 = dstImage;
    pTmp2 = dstImage + 4;
    for ( w = 0; w < width-1; w++)
    {
        *(pTmp1+1) = (*pTmp1) * 3/4.0 + (*pTmp2) * 1/4.0;
        *(pTmp1+2) = (*pTmp1) * 2/4.0 + (*pTmp2) * 2/4.0;
        *(pTmp1+3) = (*pTmp1) * 1/4.0 + (*pTmp2) * 3/4.0;

        pTmp1 += 4;
        pTmp2 += 4;
    }
    *(pTmp1+1) = *pTmp1;
    // 第二行
    pTmp1 = dstImage;
    pTmp2 = dstImage + dstWidth;
    for ( w = 0; w < dstWidth; w++)
    {
        *pTmp2++ = *pTmp1++;
    }

    // 倒数第二行
    pTmp1 = dstImage + (dstHeight-2)*dstWidth;
    for ( w = 0; w < width; w++)
    {
        *pTmp1 = srcImage[(height-1)*width+w];        
        pTmp1 += 4;
    }
    pTmp1 = dstImage + (dstHeight-2)*dstWidth;
    pTmp2 = pTmp1 + 4;
    for ( w = 0; w < width-1; w++)
    {
        *(pTmp1+1) = (*pTmp1) * 3/4.0 + (*pTmp2) * 1/4.0;
        *(pTmp1+2) = (*pTmp1) * 2/4.0 + (*pTmp2) * 2/4.0;
        *(pTmp1+3) = (*pTmp1) * 1/4.0 + (*pTmp2) * 3/4.0;

        pTmp1 += 4;
        pTmp2 += 4;
    }
    *(pTmp1+1) = *pTmp1;

    // 倒数第一行
    pTmp1 = dstImage + (dstHeight-2)*dstWidth;
    pTmp2 = dstImage + (dstHeight-1)*dstWidth;
    for ( w = 0; w < dstWidth; w++)
    {
        *pTmp2++ = *pTmp1++;
    }

    // 倒数第二列
    pTmp1 = dstImage + dstWidth-2;
    for ( h = 0; h < height; h++)
    {
        *pTmp1 = srcImage[h*width+width-1];        
        pTmp1 += (dstWidth*4);
    }
    pTmp1 = dstImage + dstWidth-2;
    pTmp2 = pTmp1 + dstWidth * 4;
    for ( h = 0; h < height-1; h++)
    {
        *(pTmp1+dstWidth) = (*pTmp1) * 3/4.0 + (*pTmp2) * 1/4.0;
        *(pTmp1+2*dstWidth) = (*pTmp1) * 2/4.0 + (*pTmp2) * 2/4.0;
        *(pTmp1+3*dstWidth) = (*pTmp1) * 1/4.0 + (*pTmp2) * 3/4.0;

        pTmp1 += (dstWidth*4);
        pTmp2 += (dstWidth*4);
    }
    *(pTmp1+dstWidth) = *pTmp1;
    *(pTmp1+2*dstWidth) = *pTmp1;
    *(pTmp1+3*dstWidth) = *pTmp1;

    // 倒数第一列
    pTmp1 = dstImage + dstWidth-2;
    for ( h = 0; h < dstHeight; h++)
    {
        *(pTmp1+1) = *pTmp1;
        pTmp1 += dstWidth;
    }

    // 主体
    for (h = 0; h < height-1; h++)
    {
        for (w = 0; w < width-1; w++)
        {            
            *pDst1 = ((*p1) *(7/8.0) + (*(p1+1))*(1/8.0)) * (7/8.0) + ((*p2)*(7/8.0) + (*(p2+1))*(1/8.0)) * (1/8.0);
            *(pDst1+1) = ((*p1) *(5/8.0) + (*(p1+1))*(3/8.0)) * (7/8.0) + ((*p2)*(5/8.0) + (*(p2+1))*(3/8.0)) * (1/8.0);
            *(pDst1+2) = ((*p1) *(3/8.0) + (*(p1+1))*(5/8.0)) * (7/8.0) + ((*p2)*(3/8.0) + (*(p2+1))*(5/8.0)) * (1/8.0);
            *(pDst1+3) = ((*p1) *(1/8.0) + (*(p1+1))*(7/8.0)) * (7/8.0) + ((*p2)*(1/8.0) + (*(p2+1))*(7/8.0)) * (1/8.0);

            *pDst2 = ((*p1) *(7/8.0) + (*(p1+1))*(1/8.0)) * (5/8.0) + ((*p2)*(7/8.0) + (*(p2+1))*(1/8.0)) * (3/8.0);
            *(pDst2+1) = ((*p1) *(5/8.0) + (*(p1+1))*(3/8.0)) * (5/8.0) + ((*p2)*(5/8.0) + (*(p2+1))*(3/8.0)) * (3/8.0);
            *(pDst2+2) = ((*p1) *(3/8.0) + (*(p1+1))*(5/8.0)) * (5/8.0) + ((*p2)*(3/8.0) + (*(p2+1))*(5/8.0)) * (3/8.0);
            *(pDst2+3) = ((*p1) *(1/8.0) + (*(p1+1))*(7/8.0)) * (5/8.0) + ((*p2)*(1/8.0) + (*(p2+1))*(7/8.0)) * (3/8.0);

            *pDst3 = ((*p1) *(7/8.0) + (*(p1+1))*(1/8.0)) * (3/8.0) + ((*p2)*(7/8.0) + (*(p2+1))*(1/8.0)) * (5/8.0);
            *(pDst3+1) = ((*p1) *(5/8.0) + (*(p1+1))*(3/8.0)) * (3/8.0) + ((*p2)*(5/8.0) + (*(p2+1))*(3/8.0)) * (5/8.0);
            *(pDst3+2) = ((*p1) *(3/8.0) + (*(p1+1))*(5/8.0)) * (3/8.0) + ((*p2)*(3/8.0) + (*(p2+1))*(5/8.0)) * (5/8.0);
            *(pDst3+3) = ((*p1) *(1/8.0) + (*(p1+1))*(7/8.0)) * (3/8.0) + ((*p2)*(1/8.0) + (*(p2+1))*(7/8.0)) * (5/8.0);

            *pDst4 = ((*p1) *(7/8.0) + (*(p1+1))*(1/8.0)) * (1/8.0) + ((*p2)*(7/8.0) + (*(p2+1))*(1/8.0)) * (7/8.0);
            *(pDst4+1) = ((*p1) *(5/8.0) + (*(p1+1))*(3/8.0)) * (1/8.0) + ((*p2)*(5/8.0) + (*(p2+1))*(3/8.0)) * (7/8.0);
            *(pDst4+2) = ((*p1) *(3/8.0) + (*(p1+1))*(5/8.0)) * (1/8.0) + ((*p2)*(3/8.0) + (*(p2+1))*(5/8.0)) * (7/8.0);
            *(pDst4+3) = ((*p1) *(1/8.0) + (*(p1+1))*(7/8.0)) * (1/8.0) + ((*p2)*(1/8.0) + (*(p2+1))*(7/8.0)) * (7/8.0);

            p1++;
            p2++;

            pDst1 += 4;
            pDst2 += 4;
            pDst3 += 4;
            pDst4 += 4;
        }
        p1++;
        p2++;

        pDst1 = pDst1 + 3*dstWidth + 4;
        pDst2 = pDst2 + 3*dstWidth + 4;
        pDst3 = pDst3 + 3*dstWidth + 4;
        pDst4 = pDst4 + 3*dstWidth + 4;

    }

    return 0;
}


