/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_harrhfnr_V1.0
 * Date           : 2012-7-3
 * Description    : harrС����Ƶ����
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "haarhfnr.h"
#include "fastdn2d.h"

 /********************************************************************************
 * Function Description:       
      1.  С����Ƶ�����˲�������ȥ��
 * Parameters:          
      > pSrc      - [in] С��ϵ��
      > pEdge   - [out] ����ͼ���Ե��Ϣ
      > width    - [in] ͼ����(pSrc��pEdge)
      > height   - [in]ͼ��߶�(pSrc��pEdge)
      > Stride    - [in] ͼ�񲽳�(pSrc��pEdge)          
      > nrTH      - [in] ��Ƶ��Ϣ����ǿ��
      > edgeTH  - [in] ��Ե��Ϣ�����ж�ǿ��
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

                //absDiff > 1ȥ������ֵ��������ֹ�������ɫ��
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
      1.  С����Ƶ�����˲�������ȥ��
 * Parameters:          
      > pSrc      - [in] С��ϵ��
      > pEdge   - [out] ����ͼ�����ȱ�Ե��Ϣ
      > pEdgeUV   - [out] ����ͼ��ɫ�ȱ�Ե��Ϣ
      > width    - [in] ͼ����(pSrc��pEdge)
      > height   - [in]ͼ��߶�(pSrc��pEdge)
      > Stride    - [in] ͼ�񲽳�(pSrc��pEdge)          
      > nrTH      - [in] ��Ƶ��Ϣ����ǿ��
      > edgeTH  - [in] ��Ե��Ϣ���������ж�ǿ��
      > edgeUVTH  - [in] ��Ե��Ϣɫ�������ж�ǿ��
      > protectLevel  - [in] ȥ������ֵ��������ֹ�������ɫ��
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
      1.  С����Ƶ�����˲�������ȥ��
 * Parameters:          
      > pSrc         - [in] С��ϵ��
      > pDnCoef    - [in] ȥ��ƫ�Ʋ�ѯ��
      > pEdge   - [out] ����ͼ�����ȱ�Ե��Ϣ
      > width    - [in] ͼ����(pSrc��pEdge)
      > height   - [in]ͼ��߶�(pSrc��pEdge)
      > Stride    - [in] ͼ�񲽳�(pSrc��pEdge)          
      > nrTH      - [in] ��Ƶ��Ϣ����ǿ��
      > edgeTH  - [in] ��Ե��Ϣ���������ж�ǿ��
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
            //(abs((INT32)pSrcL[x] - 128) > 1)ȥ������ֵ��������ֹ�������ɫ��
            if(pEdgeL[x] < edgeTH && (abs((INT32)pSrcL[x] - 128) > 1)) //����ǲ��Ǳ�Ե�����˥��
            {
                //
                weight = maxWeight - pEdgeL[x]*edgeWeight;//
                currPix = (pSrcL[x] << OFFSET_PRECISION);
                diff       = centerValue - currPix;                                           // �õ���Ƶ��ֵ
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   //ƫ�Ƶ���������λ����������

                // weight Ϊ0~65536
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
      1.  С����Ƶ�����˲�������ȥ��
 * Parameters:          
      > pSrc         - [in] С��ϵ��
      > pDnCoef    - [in] ȥ��ƫ�Ʋ�ѯ��
      > pEdge   - [out] ����ͼ�����ȱ�Ե��Ϣ
      > pEdgeUV   - [out] ����ͼ��ɫ�ȱ�Ե��Ϣ
      > width    - [in] ͼ����(pSrc��pEdge)
      > height   - [in]ͼ��߶�(pSrc��pEdge)
      > Stride    - [in] ͼ�񲽳�(pSrc��pEdge)          
      > nrTH      - [in] ��Ƶ��Ϣ����ǿ��
      > edgeTH  - [in] ��Ե��Ϣ���������ж�ǿ��
      > edgeUVTH  - [in] ��Ե��Ϣɫ�������ж�ǿ��
      > protectLevel  - [in] ȥ������ֵ��������ֹ�������ɫ��
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
            if(pEdgeL[x] < edgeTH && pEdgeUVL[x] < edgeUVTH && (abs((INT32)pSrcL[x] - 128) > protectLevel)) //����ǲ��Ǳ�Ե�����˥��
            {
                //
                weight = maxWeight - pEdgeL[x]*edgeWeight;//
                currPix = (pSrcL[x] << OFFSET_PRECISION);
                diff       = centerValue - currPix;                                           // �õ���Ƶ��ֵ
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   //ƫ�Ƶ���������λ����������

                // weight Ϊ0~65536
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

                //absDiff > 1ȥ������ֵ��������ֹ�������ɫ��
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
            //(abs((INT32)pSrcL[x] - 128) > 1)ȥ������ֵ��������ֹ�������ɫ��
            if(abs((INT32)pSrcL[x] - 128) > 3) //����ǲ��Ǳ�Ե�����˥��
            {
                //
                orig     = pSrcL[x]; 
                currPix  = (orig << OFFSET_PRECISION);
                diff     = centerValue - currPix;                                           // �õ���Ƶ��ֵ
                calDiff  = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   //ƫ�Ƶ���������λ����������

                // weight Ϊ0~65536
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

