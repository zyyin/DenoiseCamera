/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_harr_V1.0
 * Date           : 2012-7-3
 * Description    : harr小波变换
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "haar.h"


  /********************************************************************************
 * Function Description:       
      1.   将小波参数交叉存放
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pBuf   - [in] 临时缓冲区大小至少是宽高最大值
      > rx      - [in] =2表示对水平方向进行系数重排=1则不做
      > ry      - [in] =2表示对垂直方向进行系数重排=1则不做
      > width      - [in] 图像宽度(pSrc)
      > height      - [in]图像高度(pSrc)
      > Stride      - [in] 图像步长(pSrc)
 ********************************************************************************/
void ApplyInterleave(UINT8 *pSrc,UINT8 *pBuf, INT32 rx, INT32 ry,INT32 width, INT32 height, INT32 stride) 
{
    UINT8 *pSrcL;
    INT32 wDiv2 = width >> 1;
    INT32 hDiv2  = height >> 1;
    INT32 strideMul2 = stride << 1;
    INT32 x,y;

    // interleave in x
    if (rx != 1) 
    {
        INT32 iOldX;

        pSrcL = pSrc;
        for ( y = 0; y < height; y++) 
        {
            // copy out this chunk
            memcpy(pBuf,pSrcL,width * sizeof(UINT8));//memcpy(pBuf,pSrcL,width * sizeof(UINT8));

            // paste this chunk back in in bizarro order
            iOldX = 0;
            for ( x = 0; x < wDiv2; x++) 
            {
                pSrcL[iOldX] = pBuf[x];
                iOldX += rx;
            }

            iOldX = 1;
            for (; x < width; x++) 
            {
                pSrcL[iOldX] = pBuf[x];
                iOldX += rx;
            }
            
            pSrcL+=stride;
        }

    }

    // interleave in y
    if (ry != 1) 
    {
        for ( x = 0; x < width; x++) 
        {
            // copy out this chunk
            pSrcL = &pSrc[x];
            for ( y = 0; y < height; y++) 
            {
                pBuf[y] = pSrcL[0];
                pSrcL += stride;
            }

            // paste this chunk back in in bizarro order
            pSrcL = &pSrc[x];
            
            for ( y = 0; y < hDiv2; y++) 
            {
                *pSrcL = pBuf[y];
                pSrcL += strideMul2;
            }

            pSrcL = &pSrc[x] + stride;
            for (; y < height; y++) 
            {
                *pSrcL = pBuf[y];
                pSrcL += strideMul2;
            }
            
        }

    }
}


  /********************************************************************************
 * Function Description:       
      1.   将小波参数分块存放
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pBuf   - [in] 临时缓冲区大小至少是宽高最大值
      > rx      - [in] =2表示对水平方向进行系数重排=1则不做
      > ry      - [in] =2表示对垂直方向进行系数重排=1则不做
      > width      - [in] 图像宽度(pSrc)
      > height      - [in]图像高度(pSrc)
      > Stride      - [in] 图像步长(pSrc)
 ********************************************************************************/
void ApplyDeinterleave(UINT8 *pSrc,UINT8 *pBuf, INT32 rx, INT32 ry,INT32 width, INT32 height, INT32 stride) 
{
    UINT8 *pSrcL;
    INT32 wDiv2 = width >> 1;
    INT32 hDiv2  = height >> 1;
    INT32 x,y;

    // interleave in x
    // 原图的系数放置顺序为L H L H L H ......  (L表示的是均值是低频,H 是差值是高频)
    // 经过调整后的系数放置如下:
    // L L L L ...... H H H H ....
    if (rx != 1) 
    {
        INT32 iOldX;
        
        pSrcL = pSrc;

        for ( y = 0; y < height; y++) 
        {
            // copy out this chunk
            memcpy(pBuf,pSrcL,width * sizeof(UINT8));//memcpy(pBuf,pSrcL,width * sizeof(UINT8));

            // paste this chunk back in in bizarro order
            iOldX = 0;
            for ( x = 0; x < wDiv2; x++) 
            {
                pSrcL[x] = pBuf[iOldX];
                iOldX += rx;
            }

            iOldX = 1;
            for (; x < width; x++) 
            {
                pSrcL[x] = pBuf[iOldX];
                iOldX += rx;
            }

            pSrcL += stride;
        }

    }

    // interleave in y
    if (ry != 1) 
    {
        INT32 oldY = 0;

        for ( x = 0; x < width; x++) 
        {
            // copy out this chunk
            pSrcL = &pSrc[x];
            for ( y = 0; y < height; y++) 
            {
                pBuf[y] = pSrcL[0];
                pSrcL += stride;
            }

            // paste this chunk back in in bizarro order
            oldY = 0;
            pSrcL = &pSrc[x];
            for ( y = 0; y < hDiv2; y++) 
            {
                pSrcL[0] = pBuf[oldY];
                oldY       += ry;
                pSrcL += stride;
            }
            
            oldY = 1;
            for ( ; y < height; y++) 
            {
                pSrcL[0] = pBuf[oldY];
                oldY      += ry;
                pSrcL += stride;
            }
        }
    }
}



 /********************************************************************************
 * Function Description:       
      1.  小波变换，将小波分成四块的步长是iStride
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pDst   - [out] 小波变换结果
      > pBuf   - [in] 临时缓冲区
      > width      - [in] 图像宽度(pSrc、pDst)
      > height      - [in]图像高度(pSrc、pDst)
      > Stride      - [in] 图像步长(pSrc、pDst)      
 ********************************************************************************/
void ApplyHaar(UINT8 *pSrc, UINT8 *pDst,UINT8 *pBuf,INT32 width, INT32 height, INT32 stride) 
{
    UINT8 *pSrcL = NULL;
    UINT8 *pSrcL2 = NULL;
    UINT8 *pDstL = NULL;
    UINT8 *pucDstL2 = NULL;
    INT32 x,y;
    UINT8 a,b,c,d;
    INT32 A,B;
    INT32 strideMul2 = stride <<1;

    // transform in x
    pSrcL   = pSrc;
    pSrcL2 = pSrc + stride;
    pDstL   = pDst;
    pucDstL2 = pDst + stride;

    for (y = 0; y < height; y+=2) 
    {           
        for (x = 0; x < width; x+=2) 
        {
            a = pSrcL[x];
            b = pSrcL[x+1];
            c = pSrcL2[x];
            d = pSrcL2[x+1];
            pDstL[x]     = MIN2(((INT32)a + b + c + d + 2)>>2,255);

            A = (INT32)b+d;
            B = (INT32)a+c;
            if(A >= B)
            {
                pDstL[x+1] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstL[x+1] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            A = (INT32)c+d;
            B = (INT32)a+b;
            if(A >= B)
            {
                pucDstL2[x] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pucDstL2[x] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            A = (INT32)a+d;
            B = (INT32)c+b;
            if(A >= B)
            {
                pucDstL2[x+1] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pucDstL2[x+1] = CLIP3((A - B - 2)/4 + 128,0,255);
            }
        }
        
        pSrcL += strideMul2;
        pDstL += strideMul2;
        pSrcL2 += strideMul2;
        pucDstL2 += strideMul2;
    }

    // separate into averages and differences
    ApplyDeinterleave(pDst,pBuf, 2, 1, width,height,stride);

    // separate into averages and differences
    ApplyDeinterleave(pDst,pBuf, 1, 2, width,height,stride);
    
}


 /********************************************************************************
 * Function Description:       
      1.  小波逆变换，步长是iStride
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pDst   - [out] 小波逆变换结果
      > width      - [in] 图像宽度(pSrc、pDst)
      > height      - [in]图像高度(pSrc、pDst)
      > Stride      - [in] 图像步长(pSrc、pDst)      
 ********************************************************************************/
void ApplyInverseHaar(UINT8 *pSrc ,UINT8 *pDst,INT32 width, INT32 height, INT32 stride) 
{

        UINT8 *pSrcLL = NULL;
        UINT8 *pSrcLH = NULL;
        UINT8 *pSrcHL = NULL;
        UINT8 *pSrcHH = NULL;
    
        UINT8 *pDstLine0 = NULL;
        UINT8 *pDstLine1 = NULL;
    
        INT32 x,y;
        INT16 pix0,pix1,pix2,pix3;
        INT16 temp0, temp1, temp2, temp3;
    
        INT32 dstStridex2 = stride << 1;
        INT32 wDiv2 = width >> 1;
        INT32 hDiv2 = height >> 1;

        pSrcLL = pSrc;
        pSrcLH = pSrcLL + wDiv2;
        pSrcHL = pSrcLL + hDiv2 * stride;
        pSrcHH = pSrcHL + wDiv2;
        
        pDstLine0 = pDst;
        pDstLine1 = pDstLine0 + stride; 
    
        for( y = 0; y < hDiv2; y++)
        {
            for( x = 0; x < wDiv2; x++)
            {
                temp0 = (INT16)pSrcLL[x] - (INT16)pSrcLH[x];
                temp1 = (INT16)pSrcLL[x] + (INT16)pSrcLH[x];
                temp2 = (INT16)pSrcHL[x] - (INT16)pSrcHH[x];
                temp3 = (INT16)pSrcHL[x] + (INT16)pSrcHH[x];
    
                pix0 = temp0 - temp2 + 128;
                pix1 = temp1 - temp3 + 128;
                pix2 = temp0 + temp2 + 128;
                pix3 = temp1 + temp3 - 384;
    
                pDstLine0[x * 2]     = (UINT8)(CLIP3(pix0,0,255));
                pDstLine0[x * 2 + 1] = (UINT8)(CLIP3(pix1,0,255));
                pDstLine1[x * 2]     = (UINT8)(CLIP3(pix2,0,255));
                pDstLine1[x * 2 + 1] = (UINT8)(CLIP3(pix3,0,255));
            }
    
            pSrcLL += stride;
            pSrcLH += stride;
            pSrcHL += stride;
            pSrcHH += stride;
    
            pDstLine0 += dstStridex2;
            pDstLine1 += dstStridex2;
        } 
}



 /********************************************************************************
 * Function Description:       
      1.  小波变换，将小波分成四块的步长是iW，输入格式为正常的yuv420p
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pDst   - [out] 小波变换结果
      > width      - [in] 图像宽度(pSrc、pDst)
      > height      - [in]图像高度(pSrc、pDst)
      > Stride      - [in] 图像步长(pSrc、pDst)          
 ********************************************************************************/
void ApplyHaar_new(UINT8 *pSrc, UINT8 *pDst,INT32 width, INT32 height, INT32 stride) 
{
    UINT8 *pSrcL = NULL;
    UINT8 *pSrcL2 = NULL;
    INT32 x,y;
    UINT8 a,b,c,d;
    INT32 A,B;
    INT32 strideMul2 = stride << 1;
    
    //INT32 dstW = width >> 1;
    INT32 dstH = height >> 1;
    INT32 dstStride = stride >> 1;
    UINT8 *pDstLL = pDst;
    UINT8 *pDstLH = pDstLL + dstH * dstStride;
    UINT8 *pDstHL = pDstLH + dstH * dstStride;
    UINT8 *pDstHH = pDstHL + dstH * dstStride;
    INT32 idx;
    
    //UINT8 *pBuf;
    //pBuf = (UINT8 *)malloc(height*stride);

    // transform in x
    pSrcL   = pSrc;
    pSrcL2 = pSrc + stride;

    for (y = 0; y < height; y+=2) 
    {        
        idx = 0;
            
        for (x = 0; x < width; x+=2) 
        {
            a = pSrcL[x];
            b = pSrcL[x+1];
            c = pSrcL2[x];
            d = pSrcL2[x+1];
            pDstLL[idx]     = MIN2(((INT32)a + b + c + d + 2)>>2,255);

            A = (INT32)b+d;
            B = (INT32)a+c;
            if(A >= B)
            {
                pDstLH[idx] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstLH[idx] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            A = (INT32)c+d;
            B = (INT32)a+b;
            if(A >= B)
            {
                pDstHL[idx] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstHL[idx] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            A = (INT32)a+d;
            B = (INT32)c+b;
            if(A >= B)
            {
                pDstHH[idx] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstHH[idx] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            idx++;
        }

        pSrcL += strideMul2;
        pSrcL2 += strideMul2;
        pDstLL += dstStride;
        pDstLH += dstStride;
        pDstHL += dstStride;
        pDstHH += dstStride;
    }

    // separate into averages and differences
    //ApplyDeinterleave(pDst,pBuf, 2, 1, width,height,stride);

    // separate into averages and differences
    //ApplyDeinterleave(pDst,pBuf, 1, 2, width,height,stride);

}

void ApplyInverseHaar_thread_new(UINT8 *pSrc ,UINT8 *pDst,INT32 SrcOffset,INT32 SrcH,INT32 width, INT32 height, INT32 stride) 
{
        UINT8 *pSrcLL = NULL;
        UINT8 *pSrcLH = NULL;
        UINT8 *pSrcHL = NULL;
        UINT8 *pSrcHH = NULL;
    
        UINT8 *pDstLine0 = NULL;
        UINT8 *pDstLine1 = NULL;
    
        INT32 x,y;
        INT16 pix0,pix1,pix2,pix3;
        INT16 temp0, temp1, temp2, temp3;
    
        INT32 dstStridex2 = stride << 1;
        INT32 srcStride = stride >> 1;
        INT32 srcWDiv2 = width >> 1;
        INT32 srcHDiv2 = SrcH >> 1;

        pSrcLL = pSrc+(SrcOffset>>2);
        pSrcLH = pSrcLL + srcHDiv2 *srcStride;
        pSrcHL = pSrcLH + srcHDiv2 *srcStride;
        pSrcHH = pSrcHL + srcHDiv2 *srcStride;
        
        pDstLine0 = pDst;
        pDstLine1 = pDstLine0 + stride; 
    
        for( y = 0; y < (height>>1); y++)
        {
            for( x = 0; x < srcWDiv2; x++)
            {
                temp0 = (INT16)pSrcLL[x] - (INT16)pSrcLH[x];
                temp1 = (INT16)pSrcLL[x] + (INT16)pSrcLH[x];
                temp2 = (INT16)pSrcHL[x] - (INT16)pSrcHH[x];
                temp3 = (INT16)pSrcHL[x] + (INT16)pSrcHH[x];
    
                pix0 = temp0 - temp2 + 128;
                pix1 = temp1 - temp3 + 128;
                pix2 = temp0 + temp2 + 128;
                pix3 = temp1 + temp3 - 384;
    
                pDstLine0[x * 2]     = (UINT8)(CLIP3(pix0,0,255));
                pDstLine0[x * 2 + 1] = (UINT8)(CLIP3(pix1,0,255));
                pDstLine1[x * 2]     = (UINT8)(CLIP3(pix2,0,255));
                pDstLine1[x * 2 + 1] = (UINT8)(CLIP3(pix3,0,255));
            }
    
            pSrcLL += srcStride;
            pSrcLH += srcStride;
            pSrcHL += srcStride;
            pSrcHH += srcStride;
    
            pDstLine0 += dstStridex2;
            pDstLine1 += dstStridex2;
        } 
}

 /********************************************************************************
 * Function Description:       
      1.  小波逆变换
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pDst   - [out] 小波逆变换结果
      > width      - [in] 图像宽度(pSrc、pDst)
      > height      - [in]图像高度(pSrc、pDst)
      > Stride      - [in] 图像步长(pSrc、pDst)          
 ********************************************************************************/
void ApplyInverseHaar_new(UINT8 *pSrc ,UINT8 *pDst,INT32 width, INT32 height, INT32 stride) 
{
        UINT8 *pSrcLL = NULL;
        UINT8 *pSrcLH = NULL;
        UINT8 *pSrcHL = NULL;
        UINT8 *pSrcHH = NULL;
    
        UINT8 *pDstLine0 = NULL;
        UINT8 *pDstLine1 = NULL;
    
        INT32 x,y;
        INT16 pix0,pix1,pix2,pix3;
        INT16 temp0, temp1, temp2, temp3;
    
        INT32 dstStridex2 = stride << 1;
        INT32 srcStride = stride >> 1;
        INT32 srcWDiv2 = width >> 1;
        INT32 srcHDiv2 = height >> 1;

        pSrcLL = pSrc;
        pSrcLH = pSrcLL + srcHDiv2 *srcStride;
        pSrcHL = pSrcLH + srcHDiv2 *srcStride;
        pSrcHH = pSrcHL + srcHDiv2 *srcStride;
        
        pDstLine0 = pDst;
        pDstLine1 = pDstLine0 + stride; 
    
        for( y = 0; y < srcHDiv2; y++)
        {
            for( x = 0; x < srcWDiv2; x++)
            {
                temp0 = (INT16)pSrcLL[x] - (INT16)pSrcLH[x];
                temp1 = (INT16)pSrcLL[x] + (INT16)pSrcLH[x];
                temp2 = (INT16)pSrcHL[x] - (INT16)pSrcHH[x];
                temp3 = (INT16)pSrcHL[x] + (INT16)pSrcHH[x];
    
                pix0 = temp0 - temp2 + 128;
                pix1 = temp1 - temp3 + 128;
                pix2 = temp0 + temp2 + 128;
                pix3 = temp1 + temp3 - 384;
    
                pDstLine0[x * 2]     = (UINT8)(CLIP3(pix0,0,255));
                pDstLine0[x * 2 + 1] = (UINT8)(CLIP3(pix1,0,255));
                pDstLine1[x * 2]     = (UINT8)(CLIP3(pix2,0,255));
                pDstLine1[x * 2 + 1] = (UINT8)(CLIP3(pix3,0,255));
            }
    
            pSrcLL += srcStride;
            pSrcLH += srcStride;
            pSrcHL += srcStride;
            pSrcHH += srcStride;
    
            pDstLine0 += dstStridex2;
            pDstLine1 += dstStridex2;
        } 
}


 /********************************************************************************
 * Function Description:       
      1.  小波变换，将小波分成四块的步长是iW, 输入格式为高通定制格式
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pDst   - [out] 小波变换结果
      > pBuf   - [in] 临时缓冲区
      > width      - [in] 图像宽度(pSrc、pDst)
      > height      - [in]图像高度(pSrc、pDst)
      > Stride      - [in] 图像步长(pSrc、pDst)            
 ********************************************************************************/
void ApplyHaar_Chroma_G(UINT8 *pSrc, UINT8 *pDst,INT32 width, INT32 height, INT32 stride) 
{
    UINT8 *pSrcL = NULL;
    UINT8 *pSrcL2 = NULL;
    INT32 x,y;
    UINT8 a,b,c,d;
    INT32 A,B;
    INT32 strideMul2 = stride << 1;
    
    //INT32 dstW = width >> 2;
    INT32 dstH = height >> 1;
    INT32 dstStride = stride >> 2;
    UINT8 *pDstLL = pDst;
    UINT8 *pDstLH = pDstLL + dstH * dstStride;
    UINT8 *pDstHL = pDstLH + dstH * dstStride;
    UINT8 *pDstHH = pDstHL + dstH * dstStride;
    INT32 idx;
    
    //UINT8 *pBuf;
    //pBuf = (UINT8 *)malloc(height*stride);

    // transform in x
    pSrcL   = pSrc;
    pSrcL2 = pSrc + stride;

    for (y = 0; y < height; y+=2) 
    {        
        idx = 0;
            
        for (x = 0; x < width; x+=4) 
        {
            a = pSrcL[x];
            b = pSrcL[x+2];
            c = pSrcL2[x];
            d = pSrcL2[x+2];
            pDstLL[idx]     = MIN2(((INT32)a + b + c + d + 2)>>2,255);

            A = (INT32)b+d;
            B = (INT32)a+c;
            if(A >= B)
            {
                pDstLH[idx] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstLH[idx] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            A = (INT32)c+d;
            B = (INT32)a+b;
            if(A >= B)
            {
                pDstHL[idx] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstHL[idx] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            A = (INT32)a+d;
            B = (INT32)c+b;
            if(A >= B)
            {
                pDstHH[idx] = CLIP3((A - B + 2)/4 + 128,0,255);
            }
            else
            {
                pDstHH[idx] = CLIP3((A - B - 2)/4 + 128,0,255);
            }

            idx++;
        }

        pSrcL += strideMul2;
        pSrcL2 += strideMul2;
        pDstLL += dstStride;
        pDstLH += dstStride;
        pDstHL += dstStride;
        pDstHH += dstStride;
    }

    // separate into averages and differences
    //ApplyDeinterleave(pDst,pBuf, 2, 1, width,height,stride);

    // separate into averages and differences
    //ApplyDeinterleave(pDst,pBuf, 1, 2, width,height,stride);

}


 /********************************************************************************
 * Function Description:       
      1.  高通格式yyy...  vuvuvuvu...小波逆变换
 * Parameters:          
      > pSrc   - [in] 小波系数
      > pDst   - [out] 小波逆变换结果
      > width      - [in] 图像宽度(pSrc、pDst)
      > height      - [in]图像高度(pSrc、pDst)
      > Stride      - [in] 图像步长(pSrc、pDst)          
 ********************************************************************************/
void ApplyInverseHaar_Chroma_G(UINT8 *pSrc ,UINT8 *pDst,INT32 width, INT32 height, INT32 stride) 
{

        UINT8 *pSrcLL = NULL;
        UINT8 *pSrcLH = NULL;
        UINT8 *pSrcHL = NULL;
        UINT8 *pSrcHH = NULL;
    
        UINT8 *pDstLine0 = NULL;
        UINT8 *pDstLine1 = NULL;
    
        INT32 x,y;
        INT16 pix0,pix1,pix2,pix3;
        INT16 temp0, temp1, temp2, temp3;
    
        INT32 dstStridex2 = stride << 1;
        INT32 srcStride = stride >> 2;
        INT32 srcW = width >> 2;
        INT32 srcHDiv2 = height >> 1;

        pSrcLL = pSrc;
        pSrcLH = pSrcLL + srcHDiv2 *srcStride;
        pSrcHL = pSrcLH + srcHDiv2 *srcStride;
        pSrcHH = pSrcHL + srcHDiv2 *srcStride;
        
        pDstLine0 = pDst;
        pDstLine1 = pDstLine0 + stride; 
    
        for( y = 0; y < srcHDiv2; y++)
        {
            for( x = 0; x < srcW; x++)
            {
                temp0 = (INT16)pSrcLL[x] - (INT16)pSrcLH[x];
                temp1 = (INT16)pSrcLL[x] + (INT16)pSrcLH[x];
                temp2 = (INT16)pSrcHL[x] - (INT16)pSrcHH[x];
                temp3 = (INT16)pSrcHL[x] + (INT16)pSrcHH[x];
    
                pix0 = temp0 - temp2 + 128;
                pix1 = temp1 - temp3 + 128;
                pix2 = temp0 + temp2 + 128;
                pix3 = temp1 + temp3 - 384;
    
                pDstLine0[x * 4]     = (UINT8)(CLIP3(pix0,0,255));
                pDstLine0[x * 4 + 2] = (UINT8)(CLIP3(pix1,0,255));
                pDstLine1[x * 4]     = (UINT8)(CLIP3(pix2,0,255));
                pDstLine1[x * 4 + 2] = (UINT8)(CLIP3(pix3,0,255));
            }
    
            pSrcLL += srcStride;
            pSrcLH += srcStride;
            pSrcHL += srcStride;
            pSrcHH += srcStride;
    
            pDstLine0 += dstStridex2;
            pDstLine1 += dstStridex2;
        } 
}



