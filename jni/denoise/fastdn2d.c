/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : y00142940
 * Version        : uniVideoV100R001C02_fastnr2d_V1.0
 * Date           : 2012-7-3
 * Description    : ���������㷨
 * Function List  : 
 * History        : 
 ********************************************************************************/

#include "fastdn2d.h"


/*******************************************************************************
    diff = uiRefPix-uiCurrPix;                //��ǰ����ο���Ĳ��죻uiCurrPix��uiRefPix����ԭֵ����2^16��iDiff�ķ�Χ��-256*16 ~256*16 = -4096~4096
    calDiff   = ((diff + 0x10007FF)>>12);   // iDiff�ľ����ǳ���2^16Ȼ���ٳ�2^12�����ճ���2^4=16
    dstPix =  uiCurrPix + piDnCoef[calDiff];               // Coef��ֵҲ�ǳ���16
*******************************************************************************/

#define LP2dModify(RefPix,CurrPix,pDnCoef,DstPix)\
{\
    diff       = ((INT32)RefPix-CurrPix);               \
    calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   \
    DstPix =  CurrPix + pDnCoef[calDiff];\
}

 /********************************************************************************
 * Function Description:       
      1.  ���������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx            - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ���ַ������pSrc��ͬ
      > width      - [in] ͼ����(pSrc��pDst)
      > height      - [in]ͼ��߶�(pSrc��pDst)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)
 ********************************************************************************/
void FastDn2d(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst,INT32 width, INT32 height, INT32 stride)
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;         

    UINT8  * pSrcL  = pSrc;
    UINT8  * pDstL  = pDst;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;
    INT32 diff;
    
    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
    UINT32 calDiff;
        
    INT32 x, y;

    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc;
    pDstL  = pDst;

    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[0]<<OFFSET_PRECISION;
    pPrevLine[0]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = 1; x < width; x++) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        pPrevLine[x] =       prevPix;
        
        LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   += stride;
    pSrcL   += stride;
            
    for (y = 1; y < height; y++) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[0]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        LP2dModify(pPrevLine[0], curPix, pVertCoef,pPrevLine[0]);
        LP2dModify(curPix, pPrevLine[0], pTempCoef,dstPix);
        pDstL[0]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = 1; x < width; x++) 
        {
            curPix    =    pSrcL[x]<<OFFSET_PRECISION;
            LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
            LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
            LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);

            pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        }
        
        pDstL    +=    stride;
        pSrcL    +=    stride;
    
    }
}

 /********************************************************************************
 * Function Description:       
      1.  ���������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx            - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ���ַ������pSrc��ͬ
      > width      - [in] ͼ����(pSrc��pDst)
      > height      - [in]ͼ��߶�(pSrc��pDst)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)
 ********************************************************************************/
void FastDn2dInvert(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst,INT32 width, INT32 height, INT32 stride)
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;       

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;

    //INT32 srcOff = 0;
    //INT32 dstOff = 0;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;

    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
 
    INT32     diff;
    UINT32 calDiff;
        
    INT32 x, y;

    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc + stride * (height - 1) ;
    pDstL  = pDst + stride * (height - 1) ;

    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[width - 1]<<OFFSET_PRECISION;
    pPrevLine[width - 1]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = width - 2; x >= 0; x--) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        pPrevLine[x] =       prevPix;
        
        LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   -= stride;
    pSrcL   -= stride;
            
    for (y = height - 2; y >= 0; y--) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[width - 1]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        LP2dModify(pPrevLine[width - 1], curPix, pVertCoef,pPrevLine[width - 1]);
        LP2dModify(curPix, pPrevLine[width - 1], pTempCoef,dstPix);
        pDstL[width - 1]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = width - 2; x >= 0; x--) 
        {
            curPix    =    pSrcL[x]<<OFFSET_PRECISION;
            LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
            LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
            LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
            
            pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);
        }
        
        pDstL    -=    stride;
        pSrcL    -=    stride;
    
    }
}


 /********************************************************************************
 * Function Description:       
      1.  ���ڱ�Ե�Ĵ��������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx                  - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ��
      > pEdge            - [in]����ͼ��ı�Ե��Ϣ
      > width      - [in] ͼ����(pSrc��pDst��pEdge)
      > height      - [in]ͼ��߶�(pSrc��pDst��pEdge)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)      
      > edgeTH            - [in]����ͼ��ı�Ե��ֵ
 ********************************************************************************/
void FastDn2dEdge(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst, UINT8 *pEdge,INT32 width, INT32 height, INT32 stride,INT32 edgeTH)
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16       *pHoriCoef,  *pVertCoef,  *pTempCoef;        

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;
    UINT8   *pEdgeL;

    //INT32 srcOff = 0;
    //INT32 dstOff = 0;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;

    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
 
    INT32     diff;
    UINT32 calDiff;
        
    INT32 x, y;

    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeTH;//
    INT32   weight;

    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc;
    pDstL  = pDst;
    pEdgeL  = pEdge;

    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[0]<<OFFSET_PRECISION;
    pPrevLine[0]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = 1; x < width; x++) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        
        diff       = ((INT32)prevPix-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        prevPix =  curPix + pHoriCoef[calDiff];
    
        pPrevLine[x] =       prevPix;
        
        diff       = ((INT32)curPix-prevPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  prevPix + pTempCoef[calDiff];
        
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   += stride;
    pSrcL   += stride;
    pEdgeL   += width;
            
    for (y = 1; y < height; y++) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[0]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        //LP2dModify(pPrevLine[0], curPix, pVertCoef,pPrevLine[0]);
        diff       = ((INT32)pPrevLine[0]-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        pPrevLine[0] =  curPix + pVertCoef[calDiff];
        
        //LP2dModify(curPix, pPrevLine[0], pTempCoef,dstPix);
        diff       = ((INT32)curPix-pPrevLine[0]);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  pPrevLine[0] + pTempCoef[calDiff];
        
        pDstL[0]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = 1; x < width; x++) 
        {
            if(pEdgeL[x] < edgeTH)//
            {
                curPix    =    pSrcL[x]<<OFFSET_PRECISION;
                //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
                diff       = ((INT32)prevPix-curPix);               
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
                prevPix =  curPix + pHoriCoef[calDiff];
                
                //LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
                diff       = ((INT32)pPrevLine[x]-prevPix);               
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
                pPrevLine[x] =  prevPix + pVertCoef[calDiff];
                
                //LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
                diff       = ((INT32)curPix-pPrevLine[x]);               
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
                dstPix =  pPrevLine[x] + pTempCoef[calDiff];
                
                pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

                weight = pEdgeL[x]*edgeWeight;

                prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
                
            }
            else
            {
                prevPix         = pSrcL[x]<<OFFSET_PRECISION;
                pPrevLine[x] = pSrcL[x]<<OFFSET_PRECISION;
                pDstL[x]      = pSrcL[x];
            }
        }
        
        pDstL    +=    stride;
        pSrcL    +=    stride;
        pEdgeL   += width;
    
    }
}




 /********************************************************************************
 * Function Description:       
      1.  ���ڱ�Ե�Ĵ��������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx                  - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ��
      > pEdge            - [in]����ͼ��ı�Ե��Ϣ
      > width      - [in] ͼ����(pSrc��pDst��pEdge)
      > height      - [in]ͼ��߶�(pSrc��pDst��pEdge)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)      
      > edgeTH            - [in]����ͼ��ı�Ե��ֵ
 ********************************************************************************/
void FastDn2dInvertEdge(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst, UINT8 *pEdge,INT32 width, INT32 height, INT32 stride,INT32 edgeTH)
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;         

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;
    UINT8   *pEdgeL;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;
    INT32     diff;

    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
    UINT32 calDiff;
        
    INT32 x, y;

    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeTH;//
    INT32   weight;
    
    
    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc + stride * (height - 1) ;
    pDstL  = pDst + stride * (height - 1) ;
    pEdgeL = pEdge + width * (height - 1) ;

    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[width - 1]<<OFFSET_PRECISION;
    pPrevLine[width - 1]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = width - 2; x >= 0; x--) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        diff       = ((INT32)prevPix-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        prevPix =  curPix + pHoriCoef[calDiff];
    
        pPrevLine[x] =       prevPix;
        
        //LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        diff       = ((INT32)curPix-prevPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  prevPix + pTempCoef[calDiff];
        
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   -= stride;
    pSrcL   -= stride;
    pEdgeL -= width;
            
    for (y = height - 2; y >= 0; y--) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[width - 1]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        //LP2dModify(pPrevLine[width - 1], curPix, pVertCoef,pPrevLine[width - 1]);
        diff       = ((INT32)pPrevLine[width - 1]-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        pPrevLine[width - 1] =  curPix + pVertCoef[calDiff];
        
        //LP2dModify(curPix, pPrevLine[width - 1], pTempCoef,dstPix);
        diff       = ((INT32)curPix-pPrevLine[width - 1]);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  pPrevLine[width - 1] + pTempCoef[calDiff];
        
        pDstL[width - 1]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = width - 2; x >= 0; x--) 
        {
            if(pEdgeL[x] < edgeTH)//40
            {
                curPix    =    pSrcL[x]<<OFFSET_PRECISION;
                //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
                diff       = ((INT32)prevPix-curPix);               
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
                prevPix =  curPix + pHoriCoef[calDiff];
                
                //LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
                diff       = ((INT32)pPrevLine[x]-prevPix);               
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
                pPrevLine[x] =  prevPix + pVertCoef[calDiff];
                
                //LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
                diff       = ((INT32)curPix- pPrevLine[x]);               
                calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
                dstPix =   pPrevLine[x] + pTempCoef[calDiff];
                
                pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

                weight = pEdgeL[x]*edgeWeight;

                prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
            }
            else
            {
                prevPix         = pSrcL[x]<<OFFSET_PRECISION;
                pPrevLine[x] = pSrcL[x]<<OFFSET_PRECISION;
                pDstL[x]      = pSrcL[x];
            }
            
        }
        
        pDstL    -=    stride;
        pSrcL    -=    stride;
        pEdgeL -= width;
    
    }
}



 /********************************************************************************
 * Function Description:       
      1.  ɫ�Ȼ��ڱ�Ե�Ĵ��������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx                  - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ��
      > pEdge            - [in]����ͼ������ȱ�Ե��Ϣ
      > pEdgeUV    - [in]����ͼ���ɫ�ȱ�Ե��Ϣ
      > width      - [in] ͼ����(pSrc��pDst��pEdge)
      > height      - [in]ͼ��߶�(pSrc��pDst��pEdge)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)      
      > edgeTH            - [in]����ͼ��ı�Ե��ֵ
      > edgeUVTH            - [in]����ͼ���ɫ�ȱ�Ե��ֵ
 ********************************************************************************/
void FastDn2dUVEdge(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst, UINT8 *pEdge,UINT8 *pEdgeUV,INT32 width, INT32 height, INT32 stride, INT32 edgeTH, INT32 edgeUVTH)
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;         

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;
    UINT8   *pEdgeL,*pEdgeUVL;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;
    INT32 diff;
    
    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
    UINT32 calDiff;
        
    INT32 x, y;

    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeUVTH;//
    INT32   weight;

    INT32   iYWEdge  = maxWeight/edgeTH;//
    
    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc;
    pDstL  = pDst;
    pEdgeL  = pEdge;
    pEdgeUVL = pEdgeUV;

    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[0]<<OFFSET_PRECISION;
    pPrevLine[0]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = 1; x < width; x++) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        diff       = ((INT32)prevPix-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        prevPix =  curPix + pHoriCoef[calDiff];
        
        pPrevLine[x] =       prevPix;
        
        //LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        diff       = ((INT32)curPix-prevPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  prevPix + pTempCoef[calDiff];
        
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   += stride;
    pSrcL   += stride;
    pEdgeL   += width;
    pEdgeUVL += width;
            
    for (y = 1; y < height; y++) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[0]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        //LP2dModify(pPrevLine[0], curPix, pVertCoef,pPrevLine[0]);
        diff       = ((INT32)pPrevLine[0]-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        pPrevLine[0] =  curPix + pVertCoef[calDiff];
        
        //LP2dModify(curPix, pPrevLine[0], pTempCoef,dstPix);
        diff       = ((INT32)curPix-pPrevLine[0]);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  pPrevLine[0] + pTempCoef[calDiff];   
        
        pDstL[0]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = 1; x < width; x++) 
        {
     
            curPix    =    pSrcL[x]<<OFFSET_PRECISION;
            //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
            diff       = ((INT32)prevPix-curPix);               
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            prevPix =  curPix + pHoriCoef[calDiff];   
        
            //LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
            diff       = ((INT32)pPrevLine[x]-prevPix);               
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            pPrevLine[x] =  prevPix + pVertCoef[calDiff];   
            
            //LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
            diff       = ((INT32)curPix-pPrevLine[x]);               
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            dstPix =  pPrevLine[x] + pTempCoef[calDiff];   

            pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

            if(pEdgeL[x] > edgeTH)
            {
                if(pEdgeUVL[x] < edgeUVTH)
                {
                    weight = pEdgeUVL[x]*edgeWeight;
                    prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                    pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
                }
                else
                {
                    //�ο�������Ų��
                    prevPix         = pSrcL[x+1]<<OFFSET_PRECISION;
                    pPrevLine[x] = pSrcL[x+stride]<<OFFSET_PRECISION;                    
                }
            }
            else
            {
                //�����Եǿ�Ƚ�С����ʹ��Y�ı�Ե��Ϣ��ΪȨ�زο�����ɫ�ȱ�Եǿ�Ƚ�С������������������Ҳ���������ֱ߽�
                weight = pEdgeL[x]*iYWEdge;

                prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15;                 
            }
        }
        
        pDstL    +=    stride;
        pSrcL    +=    stride;
        pEdgeL   += width;
        pEdgeUVL += width;
    
    }
}


 /********************************************************************************
 * Function Description:       
      1.  ɫ�Ȼ��ڱ�Ե�Ĵ��������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx                  - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ��
      > pEdge            - [in]����ͼ������ȱ�Ե��Ϣ
      > pEdgeUV    - [in]����ͼ���ɫ�ȱ�Ե��Ϣ
      > width      - [in] ͼ����(pSrc��pDst��pEdge)
      > height      - [in]ͼ��߶�(pSrc��pDst��pEdge)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)      
      > edgeTH            - [in]����ͼ��ı�Ե��ֵ
      > edgeUVTH            - [in]����ͼ���ɫ�ȱ�Ե��ֵ
 ********************************************************************************/
void FastDn2dInvertUVEdge(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst, UINT8 *pEdge,UINT8 *pEdgeUV,INT32 width, INT32 height, INT32 stride,INT32 edgeTH, INT32 edgeUVTH)
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;         

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;
    UINT8   *pEdgeL,*pEdgeUVL;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;
    INT32     diff;

    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
    UINT32 calDiff;
        
    INT32 x, y;

    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeUVTH;//
    INT32   weight;
    INT32   iYWEdge  = maxWeight/edgeTH;//
        
    
    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc + stride * (height - 1) ;
    pDstL  = pDst + stride * (height - 1) ;
    pEdgeL = pEdge + width * (height - 1) ;
    pEdgeUVL = pEdgeUV + width * (height - 1) ;
    
    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[width - 1]<<OFFSET_PRECISION;
    pPrevLine[width - 1]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = width - 2; x >= 0; x--) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        diff       = ((INT32)prevPix-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        prevPix =  curPix + pHoriCoef[calDiff];   
        
        pPrevLine[x] =       prevPix;
        
        //LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        diff       = ((INT32)curPix-prevPix);
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  prevPix + pTempCoef[calDiff];   
        
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   -= stride;
    pSrcL   -= stride;
    pEdgeL -= width;
    pEdgeUVL -= width;
    
            
    for (y = height - 2; y >= 0; y--) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[width - 1]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        //LP2dModify(pPrevLine[width - 1], curPix, pVertCoef,pPrevLine[width - 1]);
        diff       = ((INT32)pPrevLine[width - 1]-curPix);
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        pPrevLine[width - 1] =  curPix + pVertCoef[calDiff];
        
        //LP2dModify(curPix, pPrevLine[width - 1], pTempCoef,dstPix);
        diff       = ((INT32)curPix-pPrevLine[width - 1]);
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  pPrevLine[width - 1] + pTempCoef[calDiff];
        
        pDstL[width - 1]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = width - 2; x >= 0; x--) 
        {

            curPix    =    pSrcL[x]<<OFFSET_PRECISION;
            //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
            diff       = ((INT32)prevPix-curPix);
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            prevPix =  curPix + pHoriCoef[calDiff];          
            
            //LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
            diff       = ((INT32)pPrevLine[x]-prevPix);
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            pPrevLine[x] =  prevPix + pVertCoef[calDiff];            
            
            //LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
            diff       = ((INT32)curPix-pPrevLine[x]);
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            dstPix =  pPrevLine[x] + pTempCoef[calDiff];           
                        
            pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);
            
            if(pEdgeL[x] > edgeTH)
            {
                if(pEdgeUVL[x] < edgeUVTH)
                {
                    weight = pEdgeUVL[x]*edgeWeight;
                    prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                    pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
                }
                else
                {
                    //�ο�������Ų��
                    prevPix         = pSrcL[x-1]<<OFFSET_PRECISION;
                    pPrevLine[x] = pSrcL[x-stride]<<OFFSET_PRECISION;                     
                }
            }
            else
            {
                //�����Եǿ�Ƚ�С����ʹ��Y�ı�Ե��Ϣ��ΪȨ�زο�����ɫ�ȱ�Եǿ�Ƚ�С������������������Ҳ���������ֱ߽�
                weight = pEdgeL[x]*iYWEdge;
                prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
            }
        }
        
        pDstL    -=    stride;
        pSrcL    -=    stride;
        pEdgeL   -= width;
        pEdgeUVL -= width;
    
    }
}


 /********************************************************************************
 * Function Description:       
      1.  ɫ�Ȼ��ڱ�Ե�Ĵ��������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx                  - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ��
      > pEdge            - [in]����ͼ������ȱ�Ե��Ϣ
      > pEdgeUV    - [in]����ͼ���ɫ�ȱ�Ե��Ϣ
      > width      - [in] ͼ����(pSrc��pDst��pEdge)
      > height      - [in]ͼ��߶�(pSrc��pDst��pEdge)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)      
      > edgeTH            - [in]����ͼ��ı�Ե��ֵ
      > edgeUVTH            - [in]����ͼ���ɫ�ȱ�Ե��ֵ
 ********************************************************************************/
void FastDn2dUVEdge2(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst, UINT8 *pEdge,INT32 width, INT32 height, INT32 stride, INT32 edgeTH )
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;         

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;
    UINT8   *pEdgeL;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;
    INT32 diff;
    
    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
    UINT32 calDiff;
        
    INT32 x, y;

    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   weight;
    INT32   edgeWeight  = maxWeight/edgeTH;//
    
    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc;
    pDstL  = pDst;
    pEdgeL  = pEdge;

    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[0]<<OFFSET_PRECISION;
    pPrevLine[0]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = 1; x < width; x++) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        diff       = ((INT32)prevPix-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        prevPix =  curPix + pHoriCoef[calDiff];
        
        pPrevLine[x] =       prevPix;
        
        //LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        diff       = ((INT32)curPix-prevPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  prevPix + pTempCoef[calDiff];
        
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   += stride;
    pSrcL   += stride;
    pEdgeL   += width;

    for (y = 1; y < height; y++) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[0]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        //LP2dModify(pPrevLine[0], curPix, pVertCoef,pPrevLine[0]);
        diff       = ((INT32)pPrevLine[0]-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        pPrevLine[0] =  curPix + pVertCoef[calDiff];
        
        //LP2dModify(curPix, pPrevLine[0], pTempCoef,dstPix);
        diff       = ((INT32)curPix-pPrevLine[0]);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  pPrevLine[0] + pTempCoef[calDiff];   
        
        pDstL[0]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = 1; x < width; x++) 
        {
     
            curPix    =    pSrcL[x]<<OFFSET_PRECISION;
            //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
            diff       = ((INT32)prevPix-curPix);               
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            prevPix =  curPix + pHoriCoef[calDiff];   
        
            //LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
            diff       = ((INT32)pPrevLine[x]-prevPix);               
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            pPrevLine[x] =  prevPix + pVertCoef[calDiff];   
            
            //LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
            diff       = ((INT32)curPix-pPrevLine[x]);               
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            dstPix =  pPrevLine[x] + pTempCoef[calDiff];   

            pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);


            if(pEdgeL[x] < edgeTH)
            {
                weight = pEdgeL[x]*edgeWeight;
                prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
            }
            else
            {
                //�ο�������Ų��
                prevPix         = pSrcL[x+1]<<OFFSET_PRECISION;
                pPrevLine[x] = pSrcL[x+stride]<<OFFSET_PRECISION;                    
            }

        }
        
        pDstL    +=    stride;
        pSrcL    +=    stride;
        pEdgeL   += width;
    
    }
}


 /********************************************************************************
 * Function Description:       
      1.  ɫ�Ȼ��ڱ�Ե�Ĵ��������㷨
 * Parameters:          
      > pstFastDn2d      - [in]  �����㷨����
      > planeIdx                  - [in]  yuv����ƽ������
      > pSrc             - [in] ����ͼ��
      > pDst            - [out]���ͼ��
      > pEdge            - [in]����ͼ������ȱ�Ե��Ϣ
      > pEdgeUV    - [in]����ͼ���ɫ�ȱ�Ե��Ϣ
      > width      - [in] ͼ����(pSrc��pDst��pEdge)
      > height      - [in]ͼ��߶�(pSrc��pDst��pEdge)
      > Stride      - [in] ͼ�񲽳�(pSrc��pDst)      
      > edgeTH            - [in]����ͼ��ı�Ե��ֵ
      > edgeUVTH            - [in]����ͼ���ɫ�ȱ�Ե��ֵ
 ********************************************************************************/
void FastDn2dInvertUVEdge2(ST_FASTDN2D*pstFastDn2d ,INT32 planeIdx,UINT8 *pSrc, UINT8 *pDst, UINT8 *pEdge,INT32 width, INT32 height, INT32 stride,INT32 edgeTH )
{    
    UINT16 *pPrevLine = pstFastDn2d->pPrevLine;
    INT16   *pHoriCoef,  *pVertCoef,  *pTempCoef;         

    UINT8   * pSrcL  = pSrc;
    UINT8   * pDstL  = pDst;
    UINT8   *pEdgeL;

    INT32 coefIdx0 = 0 == planeIdx ? 0 : 2;
    INT32 coefIdx1 = 0 == planeIdx ? 1 : 3;
    INT32     diff;

    UINT32 curPix;
    UINT32 prevPix;
    UINT32 dstPix;
    UINT32 calDiff;
        
    INT32 x, y;

    INT16   maxWeight   = 32767;//(1L<<15);
    INT32   edgeWeight  = maxWeight/edgeTH;
    INT32   weight;
    
    pHoriCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pVertCoef   = &pstFastDn2d->iDnCoefs[coefIdx0][0];
    pTempCoef = &pstFastDn2d->iDnCoefs[coefIdx1][0];
    
    pSrcL  = pSrc + stride * (height - 1) ;
    pDstL  = pDst + stride * (height - 1) ;
    pEdgeL = pEdge + width * (height - 1) ;
    
    /*��һ������û�����������Ĳο� ��*/
    prevPix             =   pSrcL[width - 1]<<OFFSET_PRECISION;
    pPrevLine[width - 1]     =   prevPix;
    dstPix               =   prevPix;

    /* ��һ��ֻ����ߺ�ǰһ֡�Ĳο��� */
    for (x = width - 2; x >= 0; x--) 
    {
        curPix           = pSrcL[x]<<OFFSET_PRECISION;
        //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
        diff       = ((INT32)prevPix-curPix);               
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        prevPix =  curPix + pHoriCoef[calDiff];   
        
        pPrevLine[x] =       prevPix;
        
        //LP2dModify(curPix, prevPix, pTempCoef,dstPix);
        diff       = ((INT32)curPix-prevPix);
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  prevPix + pTempCoef[calDiff];   
        
        pDstL[x]     =       ((dstPix + ROUND_CAL)>>OFFSET_PRECISION);
    }

    pDstL   -= stride;
    pSrcL   -= stride;
    pEdgeL -= width;
    
            
    for (y = height - 2; y >= 0; y--) 
    {        
        /*��һ������û����ߵĲο���*/
        curPix     =    pSrcL[width - 1]<<OFFSET_PRECISION;
        prevPix    =    curPix;
        //LP2dModify(pPrevLine[width - 1], curPix, pVertCoef,pPrevLine[width - 1]);
        diff       = ((INT32)pPrevLine[width - 1]-curPix);
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        pPrevLine[width - 1] =  curPix + pVertCoef[calDiff];
        
        //LP2dModify(curPix, pPrevLine[width - 1], pTempCoef,dstPix);
        diff       = ((INT32)curPix-pPrevLine[width - 1]);
        calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
        dstPix =  pPrevLine[width - 1] + pTempCoef[calDiff];
        
        pDstL[width - 1]    =    ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);

        for (x = width - 2; x >= 0; x--) 
        {

            curPix    =    pSrcL[x]<<OFFSET_PRECISION;
            //LP2dModify(prevPix, curPix, pHoriCoef,prevPix);
            diff       = ((INT32)prevPix-curPix);
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            prevPix =  curPix + pHoriCoef[calDiff];          
            
            //LP2dModify(pPrevLine[x], prevPix, pVertCoef,pPrevLine[x]);
            diff       = ((INT32)pPrevLine[x]-prevPix);
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            pPrevLine[x] =  prevPix + pVertCoef[calDiff];            
            
            //LP2dModify(curPix, pPrevLine[x], pTempCoef,dstPix);
            diff       = ((INT32)curPix-pPrevLine[x]);
            calDiff     = ((diff + DIFF_CAL)>>DIFF_CAL_SHIFT);   
            dstPix =  pPrevLine[x] + pTempCoef[calDiff];           
                        
            pDstL[x] =  ((dstPix  + ROUND_CAL)>>OFFSET_PRECISION);
            

            if(pEdgeL[x] < edgeTH)
            {
                weight = pEdgeL[x]*edgeWeight;
                prevPix =  (curPix * weight  + prevPix * (maxWeight -weight) + 0x4000)  >> 15; 
                pPrevLine[x] =  (curPix * weight  + pPrevLine[x] * (maxWeight -weight) + 0x4000)  >> 15; 
            }
            else
            {
                //�ο�������Ų��
                prevPix         = pSrcL[x-1]<<OFFSET_PRECISION;
                pPrevLine[x] = pSrcL[x-stride]<<OFFSET_PRECISION;                     
            }
        }
        
        pDstL    -=    stride;
        pSrcL    -=    stride;
        pEdgeL   -= width;

    
    }
}


void CalcDn2dCoefs(INT16 *pCoef, INT32 devia25)
{
    INT32  i;
    INT32  offsetMul = ((INT32)1L << (OFFSET_PRECISION - STEP_PRECISION));
    INT32  range        = ((INT32)255L << STEP_PRECISION);
    INT32  outRange = ((INT32)256L << STEP_PRECISION);
    double dGamma, dSimilar, dCoef;

    dGamma  =  log(0.25) / log(1.0 - devia25/255.0 - 0.00001);//0.25

    memset(pCoef,0,COEF_NUM * sizeof(INT16)); //��ֹ����δ��ֵ
    
    for (i = -range; i <= 0; i++) 
    {
        dSimilar                      =      1.0 - ABS(i) / (double)range;
        dCoef                         =      pow(dSimilar, dGamma) * i *offsetMul; //32 = 128/4 : ϵ���Ŵ���2^7��
        pCoef[outRange+i]   =      (INT16)dCoef;//�����������룬����ľ��Ȳ���Ҫ�ܸ�
        pCoef[outRange-i]   =      - (INT16)dCoef;
    }

}

#if 0
//����Сֵ���˲�ǿ��
void CalcDn2dCoefs2(INT16 *pCoef, INT32 devia25)
{
    INT32  i;
    INT32  offsetMul = ((INT32)1L << (OFFSET_PRECISION - STEP_PRECISION));
    INT32  range        = ((INT32)255L << STEP_PRECISION);
    INT32  outRange = ((INT32)256L << STEP_PRECISION);
    double dGamma, dSimilar, dCoef;
    double  maxCoef = 0;
    INT32   maxIdx = 0;
    double  target = 34.0864*devia25; //devia25 = max������devia25<50ʱ�����Ͽ�����Ϊ�����ԵĶ����ǵ�devia25һ�㶼С��20

    double *pdcoef = (double *)malloc((range+1) * sizeof(double));

    devia25 += 5;//ͨ�����ڼ�����С���Ե������ֵ��λ��
    maxCoef   = 34.0864*devia25;
    
    dGamma  =  log(0.25) / log(1.0 - devia25/255.0 - 0.00001);//0.25

    memset(pCoef,0,COEF_NUM * sizeof(INT16)); //��ֹ����δ��ֵ

    for (i = 0; i <= range; i++) 
    {
        dSimilar                      =      1.0 - ABS(i) / (double)range;
        dCoef                         =      pow(dSimilar, dGamma) * i *offsetMul; //32 = 128/4 : ϵ���Ŵ���2^7��

        pdcoef[i] = dCoef;
/*
        if(maxCoef <  dCoef)
        {
            maxCoef =  dCoef;
            maxIdx = i;
        }
        */
    }


    for (i = 0; i <= range; i++) 
    {
        pdcoef[i] /= maxCoef;
       
        //if(i <= maxIdx)
        {
            pdcoef[i] *= pdcoef[i];
        }

        pCoef[outRange + i] = (INT16)(pdcoef[i] *target);
        
        pCoef[outRange - i] = -pCoef[outRange + i];
    }
   
    free(pdcoef);
}
#endif


//����Сֵ���˲�ǿ��
void CalcDn2dCoefs2(INT16 *pCoef, INT32 devia25)
{
    INT32  i;
    INT32  offsetMul = ((INT32)1L << (OFFSET_PRECISION - STEP_PRECISION));
    INT32  range        = ((INT32)255L << STEP_PRECISION);
    INT32  outRange = ((INT32)256L << STEP_PRECISION);
    double dGamma, dSimilar, dCoef;
    double  maxCoef = 0;

    double *pdcoef = (double *)malloc((range+1) * sizeof(double));

    maxCoef   = 34.0864*devia25;
    
    dGamma  =  log(0.25) / log(1.0 - devia25/255.0 - 0.00001);//0.25

    memset(pCoef,0,COEF_NUM * sizeof(INT16)); //��ֹ����δ��ֵ

    for (i = 0; i <= range; i++) 
    {
        dSimilar                      =      1.0 - ABS(i) / (double)range;
        dCoef                         =      pow(dSimilar, dGamma) * i *offsetMul; //32 = 128/4 : ϵ���Ŵ���2^7��

        pdcoef[i] = dCoef;
/*
        if(maxCoef <  dCoef)
        {
            maxCoef =  dCoef;
            maxIdx = i;
        }
        */
    }


    for (i = 0; i <= range; i++) 
    {
        pdcoef[i] /= maxCoef;
       
        //if(i <= maxIdx)
        {
            pdcoef[i] *= pdcoef[i];
        }

        pCoef[outRange + i] = (INT16)(pdcoef[i] *maxCoef);
        
        pCoef[outRange - i] = -pCoef[outRange + i];
    }
   
    free(pdcoef);
}


 /********************************************************************************
 * Function Description:       
      1.  ���ô����������
 * Parameters:          
      > pstFastDn2d        - [in]  �����㷨����
      > nrYStrength         - [in]  ����ƽ�潵��ǿ��
      > nrYDetailRatio      - [in]  ����ƽ��ϸ�ڱ�����
      > nrUVStrength       - [in]  ɫ��ƽ�潵��ǿ��
      > nrUVDetailRatio    - [in]  ɫ��ƽ��ϸ�ڱ�����
 ********************************************************************************/
void FastDn2dControl(ST_FASTDN2D *pstFastDn2d, INT32 nrYStrength, INT32 nrYDetailRatio, INT32 nrUVStrength, INT32 nrUVDetailRatio )
{
    INT32 i;
    INT32  range        = ((INT32)255L << STEP_PRECISION);
    INT32  outRange = ((INT32)256L << STEP_PRECISION);
    INT32  coef;

    //�˲�ǿ�Ȳ���
    CalcDn2dCoefs(pstFastDn2d->iDnCoefs[0], nrYStrength);
    
    CalcDn2dCoefs(pstFastDn2d->iDnCoefs[2], nrUVStrength);
    
#if 0
    {
        FILE *pcoefs = fopen("pcoefs.txt","w");

        for(i = 0 ; i <= 2*range+1; i++)
        {
            fprintf(pcoefs,"%d %d %d\n",i,pstFastDn2d->iDnCoefs[0][i],pstFastDn2d->iDnCoefs[2][i]);
        }
        
        fclose(pcoefs);
    }
#endif    
    
    //ϸ�ڱ��ֲ���
    for(i = 0 ; i <= range; i++)
    {
        coef = ((pstFastDn2d->iDnCoefs[0][outRange + i]*nrYDetailRatio + 0x1000) >> 13);
        pstFastDn2d->iDnCoefs[1][outRange + i] = coef;  //3/5
        pstFastDn2d->iDnCoefs[1][outRange - i] = -coef;
    }

    for(i = 0 ; i <= range; i++)
    {
        coef = ((pstFastDn2d->iDnCoefs[2][outRange + i]*nrUVDetailRatio + 0x1000) >> 13);
        pstFastDn2d->iDnCoefs[3][outRange + i] = coef;  //3/5
        pstFastDn2d->iDnCoefs[3][outRange - i] = -coef;
    }
    
}

void FastDn2dUVControl(ST_FASTDN2D *pstFastDn2d, INT32 nrUVStrength, INT32 nrUVDetailRatio )
{
    INT32 i;
    INT32  range        = ((INT32)255L << STEP_PRECISION);
    INT32  outRange = ((INT32)256L << STEP_PRECISION);
    INT32  coef;

    //�˲�ǿ�Ȳ���
    //CalcDn2dCoefs(pstFastDn2d->iDnCoefs[0], nrYStrength);
    
    CalcDn2dCoefs(pstFastDn2d->iDnCoefs[2], nrUVStrength);
    
#if 0
    {
        FILE *pcoefs = fopen("pcoefs.txt","w");

        for(i = 0 ; i <= 2*range+1; i++)
        {
            fprintf(pcoefs,"%d %d %d\n",i,pstFastDn2d->iDnCoefs[0][i],pstFastDn2d->iDnCoefs[2][i]);
        }
        
        fclose(pcoefs);
    }
#endif    
    

    for(i = 0 ; i <= range; i++)
    {
        coef = ((pstFastDn2d->iDnCoefs[2][outRange + i]*nrUVDetailRatio + 0x1000) >> 13);
        pstFastDn2d->iDnCoefs[3][outRange + i] = coef;  //3/5
        pstFastDn2d->iDnCoefs[3][outRange - i] = -coef;
    }
    
}

void FastDn2dInit(void **hFastDn2d,UINT8 **pucMem,INT32   width, INT32 height)
{
    ST_FASTDN2D *pstFastDn2d =NULL;
    UINT8     *pucMemAlign;
    
    if(NULL == *pucMem)
    {
        return ;
    }

    pucMemAlign = (UINT8 *)MEM_ALIGN((UINT32)(*pucMem),MEM_ALIGN_LEN);
    pstFastDn2d = (ST_FASTDN2D *)pucMemAlign;
    pucMemAlign += sizeof(ST_FASTDN2D);
    
    memset(pstFastDn2d,0,sizeof(ST_FASTDN2D));

    pucMemAlign = (UINT8 *)MEM_ALIGN((UINT32)(pucMemAlign),MEM_ALIGN_LEN);
  
    pstFastDn2d->pPrevLine = (UINT16 *)pucMemAlign;
    pucMemAlign += MAX2(width, height) * sizeof(UINT16);
    
    *hFastDn2d = (void *)pstFastDn2d;
    *pucMem            = pucMemAlign;
    
}



