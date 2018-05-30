/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_harrhfnr_V1.0
 * Date           : 2012-7-3
 * Description    : harr小波高频降噪
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "haarhfnr.h"
#include "fastdn2d.h"

 /********************************************************************************
 * Function Description:       
      1.  小波高频部分滤波，单点去噪
 * Parameters:          
      > pSrc      - [in] 小波系数
      > pEdge   - [out] 输入图像边缘信息
      > width    - [in] 图像宽度(pSrc、pEdge)
      > height   - [in]图像高度(pSrc、pEdge)
      > Stride    - [in] 图像步长(pSrc、pEdge)          
      > nrTH      - [in] 高频信息降噪强度
      > edgeTH  - [in] 边缘信息噪声判断强度
 ********************************************************************************/
void HFNR_Edge_luma(UINT8* pSrc,UINT8 *pEdge,INT32 width, INT32 height, INT32 stride, INT32 nrTH ,INT32 edgeTH)
{
        UINT8 *pSrcL,*pEdgeL;
        INT32 x,y;
        
        INT32 diff;
        INT32 absDiff;        
        
        INT16   maxWeight   = 32767;//(1L<<15);
        INT32   edgeWeight  = maxWeight/edgeTH;//
        INT32   weight;
        
        pSrcL   = pSrc;
        pEdgeL = pEdge;

        for(y=0;y<height;y++)
        {
            for(x=0;x<width;x++)
            {
                diff       = (INT32)pSrcL[x] - 128;
                absDiff = abs(diff);

                //absDiff > 1去噪最少值保护，防止渐变出现色阶
                if((absDiff > 1&& absDiff < nrTH) && pEdgeL[x] < edgeTH )
                {
                    weight   = pEdgeL[x]*edgeWeight;
                    diff =  (diff * weight) >> 15 ;
                    pSrcL[x] = CLIP3(diff + 128,0,255);
                }
            }

            pSrcL   += stride;
            pEdgeL += width;
        }
        
}

 /********************************************************************************
 * Function Description:       
      1.  小波高频部分滤波，单点去噪
 * Parameters:          
      > pSrc      - [in] 小波系数
      > pEdge   - [out] 输入图像亮度边缘信息
      > pEdgeUV   - [out] 输入图像色度边缘信息
      > width    - [in] 图像宽度(pSrc、pEdge)
      > height   - [in]图像高度(pSrc、pEdge)
      > Stride    - [in] 图像步长(pSrc、pEdge)          
      > nrTH      - [in] 高频信息降噪强度
      > edgeTH  - [in] 边缘信息亮度噪声判断强度
      > edgeUVTH  - [in] 边缘信息色度噪声判断强度
      > protectLevel  - [in] 去噪最少值保护，防止渐变出现色阶
 ********************************************************************************/
void HFNR_Edge(UINT8* pSrc,UINT8 *pEdge,UINT8 *pEdgeUV,INT32 width, INT32 height, INT32 stride, INT32 nrTH ,INT32 edgeTH,INT32 edgeUVTH,INT32 protectLevel)
{
        UINT8 *pSrcL,*pEdgeL,*pEdgeUVL;
        INT32 x,y;
        
        INT32 diff;
        INT32 absDiff;        

        INT16   maxWeight   = 32767;//(1L<<15);
        INT32   edgeWeight  = maxWeight/edgeTH;//
        INT32   weight;
        
        pSrcL   = pSrc;
        pEdgeL = pEdge;
        pEdgeUVL = pEdgeUV;

        for(y=0;y<height;y++)
        {
            for(x=0;x<width;x++)
            {
                diff       = (INT32)pSrcL[x] - 128;
                absDiff = abs(diff);

                if( pEdgeL[x] < edgeTH && pEdgeUVL[x] < edgeUVTH &&(absDiff > protectLevel && absDiff < nrTH) )
                {
                    weight   = pEdgeL[x]*edgeWeight;
                    diff =  (diff * weight) >> 15 ;
                    pSrcL[x] = CLIP3(diff + 128,0,255);
                }
            }

            pSrcL   += stride;
            pEdgeL += width;
            pEdgeUVL += width;
        }
}


 /********************************************************************************
 * Function Description:       
      1.  小波高频部分滤波，单点去噪
 * Parameters:          
      > pSrc         - [in] 小波系数
      > pDnCoef    - [in] 去噪偏移查询表
      > pEdge   - [out] 输入图像亮度边缘信息
      > width    - [in] 图像宽度(pSrc、pEdge)
      > height   - [in]图像高度(pSrc、pEdge)
      > Stride    - [in] 图像步长(pSrc、pEdge)          
      > nrTH      - [in] 高频信息降噪强度
      > edgeTH  - [in] 边缘信息亮度噪声判断强度
 ********************************************************************************/
void HFNR_LUT_Edge_luma(UINT8* pSrc, INT16 *pDnCoef, UINT8 *pEdge, INT32 width, INT32 height, INT32 stride,INT32 edgeTH)
{
    UINT8 *pSrcL;
    UINT8 *pEdgeL;
    INT32 x,y;
    INT32 diff;
    UINT32 calDiff;
    INT32  centerValue = (128 << OFFSET_PRECISION);
    INT32   currPix;
    UINT32 dstPix;
    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeTH;//
    INT32   weight;
    //INT32   iRadius = 1;

    pSrcL   = pSrc;
    pEdgeL = pEdge;

    for(y=0;y<height;y++)
    {
        for(x=0;x<width;x++)
        {
            //(abs((INT32)pSrcL[x] - 128) > 1)去噪最少值保护，防止渐变出现色阶
            if(pEdgeL[x] < edgeTH && (abs((INT32)pSrcL[x] - 128) > 1)) //如果是不是边缘则进行衰减
            {
                //
                weight = maxWeight - pEdgeL[x]*edgeWeight;//
                currPix = (pSrcL[x] << OFFSET_PRECISION);
                diff       = centerValue - currPix;                                           // 得到高频数值
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   //偏移到正数并移位做四舍五入

                // weight 为0~65536
                dstPix =  currPix + ((weight * pDnCoef[calDiff]) >> 15);  
                pSrcL[x]    =    ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
            }
        }
        
        pSrcL   += stride;
        pEdgeL += width;
    }
}

 /********************************************************************************
 * Function Description:       
      1.  小波高频部分滤波，单点去噪
 * Parameters:          
      > pSrc         - [in] 小波系数
      > pDnCoef    - [in] 去噪偏移查询表
      > pEdge   - [out] 输入图像亮度边缘信息
      > pEdgeUV   - [out] 输入图像色度边缘信息
      > width    - [in] 图像宽度(pSrc、pEdge)
      > height   - [in]图像高度(pSrc、pEdge)
      > Stride    - [in] 图像步长(pSrc、pEdge)          
      > nrTH      - [in] 高频信息降噪强度
      > edgeTH  - [in] 边缘信息亮度噪声判断强度
      > edgeUVTH  - [in] 边缘信息色度噪声判断强度
      > protectLevel  - [in] 去噪最少值保护，防止渐变出现色阶
 ********************************************************************************/
void HFNR_LUT_Edge(UINT8* pSrc, INT16 *pDnCoef, UINT8 *pEdge, UINT8 *pEdgeUV, INT32 width, INT32 height, INT32 stride,INT32 edgeTH,INT32 edgeUVTH,INT32 protectLevel)
{
    UINT8 *pSrcL;
    UINT8 *pEdgeL;
    UINT8 *pEdgeUVL;

    UINT32 calDiff;
    UINT32 dstPix;
    
    INT32   x,y;
    INT32   diff;

    INT32   centerValue = (128 << OFFSET_PRECISION);
    INT32    currPix;
    
    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeTH;//
    INT32   weight;

    pSrcL   = pSrc;
    pEdgeL = pEdge;
    pEdgeUVL = pEdgeUV;

    for(y=0;y<height;y++)
    {
        for(x=0;x<width;x++)
        {
            //if(pEdgeL[x] < edgeTH)
            if(pEdgeL[x] < edgeTH && pEdgeUVL[x] < edgeUVTH && (abs((INT32)pSrcL[x] - 128) > protectLevel)) //如果是不是边缘则进行衰减
            {
                //
                weight = maxWeight - pEdgeL[x]*edgeWeight;//
                currPix = (pSrcL[x] << OFFSET_PRECISION);
                diff       = centerValue - currPix;                                           // 得到高频数值
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   //偏移到正数并移位做四舍五入

                // weight 为0~65536
                dstPix =  currPix + ((weight * pDnCoef[calDiff]) >> 15);  
                pSrcL[x]    =    ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
            }

        }

        pSrcL   += stride;
        pEdgeL += width;
        pEdgeUVL += width;
    }
}



void HFNR_Edge_night(UINT8* pSrc,INT32 width, INT32 height, INT32 stride, INT32 nrTH)
{
        UINT8 *pSrcL;
        INT32 x,y;
        
        INT32 diff;
        INT32 absDiff;        

        INT16   maxWeight   = 32767;//(1L<<15);
        INT32   edgeWeight  = maxWeight/nrTH;//
        INT32   weight;
        INT32   tmp;
        
        pSrcL   = pSrc;

        for(y=0;y<height;y++)
        {
            for(x=0;x<width;x++)
            {
                diff       = (INT32)pSrcL[x] - 128;
                absDiff = abs(diff);

                if(absDiff < nrTH) 
                {
                    weight  = absDiff*edgeWeight;
                    diff    =  (diff * weight) >> 15 ;
                    pSrcL[x]= CLIP3(diff + 128,0,255);
                }
            }

            pSrcL   += stride;
        }
}


void HFNR_Edge_luma_night(UINT8* pSrc, UINT8 *pEdge,INT32 width, INT32 height, INT32 stride, INT32 nrTH)
{
        UINT8 *pSrcL,*pEdgeL;
        INT32 x,y;
        
        INT32 diff;
        INT32 absDiff;        
        
        INT16   maxWeight   = 32767;//(1L<<15);
        INT32   edgeWeight  = maxWeight/nrTH;//
        INT32   weight;
        UINT8   value;
        
        pSrcL   = pSrc;
        pEdgeL = pEdge;
        
        for(y=0;y<height;y++)
        {
            for(x=0;x<width;x++)
            {
                diff    = (INT32)pSrcL[x] - 128;
                absDiff = abs(diff);

                //absDiff > 1去噪最少值保护，防止渐变出现色阶
                //if(absDiff > 1&& absDiff < nrTH)
                if(absDiff > 3 && absDiff < nrTH)
                {
                    weight   = absDiff*edgeWeight;
                    diff  =  (diff * weight) >> 15 ;
                    value = CLIP3(diff + 128,0,255);

                    if(pEdgeL[x] > 60)
                    {
                        value = (pSrcL[x] + value + 1)>>1;
                    }

                    pSrcL[x] = value;
                }
            }
            
            pEdgeL += width;
            pSrcL  += stride;
        }
        
}


void HFNR_LUT_Edge_luma_night(UINT8* pSrc, INT16 *pDnCoef, UINT8 *pEdge, INT32 width, INT32 height, INT32 stride)
{
    UINT8 *pSrcL;
    UINT8 *pEdgeL;
    INT32 x,y;
    INT32 diff;
    UINT32 calDiff;
    INT32  centerValue = (128 << OFFSET_PRECISION);
    INT32   currPix;
    UINT32 dstPix;
    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   weight;
    //INT32   iRadius = 1;
    INT32   orig;

    pSrcL   = pSrc;
    pEdgeL = pEdge;

    for(y=0;y<height;y++)
    {
        for(x=0;x<width;x++)
        {
            //(abs((INT32)pSrcL[x] - 128) > 1)去噪最少值保护，防止渐变出现色阶
            if(abs((INT32)pSrcL[x] - 128) > 3) //如果是不是边缘则进行衰减
            {
                //
                orig     = pSrcL[x]; 
                currPix  = (orig << OFFSET_PRECISION);
                diff     = centerValue - currPix;                                           // 得到高频数值
                calDiff  = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   //偏移到正数并移位做四舍五入

                // weight 为0~65536
                dstPix   =  currPix + pDnCoef[calDiff];  
                pSrcL[x] =    ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);

                if(pEdgeL[x] > 60)
                {
                    pSrcL[x] = (pSrcL[x] + orig + 1)>>1;
                }
            }
        }
        
        pSrcL   += stride;
        pEdgeL += width;
    }
}

