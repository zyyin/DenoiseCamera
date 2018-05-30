
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "basetype.h"
#include "define.h"
#include "CE_log.h"
#include "mathe.h"

#ifndef _MSC_VER
#include <arm_neon.h>
#include "bilateral_neon.h"
#define __TEST_ARM //ARM优化代码和C代码切换宏
#endif

void BilateralFilter(UINT8 *pImageSrcDst, INT32 nHeight, INT32 nWidth, INT32 nSigmaSpatial, INT32 nSigmaRange, UINT8 *pMemBuf)
{
    INT32 i = 0, j = 0, k = 0;
    INT32 lutR[511] = {0};
    INT32 *pLutS;

    INT32 win , nHeightLast , nWidthLast;
    INT32 pixelsum = 0, weight = 0, weightsum = 0;
    UINT8 ucTemp = 0;
    //UINT8 *pWeight;
    UINT8 *pSrc ;
    UINT8 *pDst ;
    
    //////////////////////////////////////////////////////////////////////////
    //invalid parameters
    if (nSigmaSpatial<=0 || nSigmaRange<=0)
    {
        return ;
    }

#ifdef __TEST_ARM
    if (nSigmaSpatial <= 2)
    {
        memcpy(pMemBuf, pImageSrcDst, nHeight*nWidth*sizeof(UINT8));
        bilateral_7x7_neon(pMemBuf, pImageSrcDst, nWidth, nHeight, nSigmaSpatial, nSigmaRange);
    }
    else
    {
        memcpy(pMemBuf, pImageSrcDst, nHeight*nWidth*sizeof(UINT8));
        bilateral_9x9_neon(pMemBuf, pImageSrcDst, nWidth, nHeight, nSigmaSpatial, nSigmaRange);
    }
#else

    //////////////////////////////////////////////////////////////////////////
    //lutR and pLutS

    //memset(lutR,0,sizeof(INT32)*511);
    for (i=-2*nSigmaRange; i<=2*nSigmaRange; i++)
    {
        lutR[i+255] = (INT32)(exp(-0.5f * i*i/nSigmaRange/nSigmaRange) * 64.0f + 0.5f);
        //printf("LUTR = %d.\n", lutR[i+255]);
    }

    //pLutS = (INT32 *)malloc(sizeof(INT32) * (4*nSigmaSpatial + 1));
    //for (i=-2*nSigmaSpatial; i<=2*nSigmaSpatial; i++)
    //{
    //    pLutS[i+2*nSigmaSpatial] = (INT32)(exp(-0.5f * i*i/nSigmaSpatial/nSigmaSpatial) * 64.0f + 0.5f);
    //}

    //////////////////////////////////////////////////////////////////////////
    //half winsize is 2*sigma
    //win = 2*nSigmaSpatial;
    if (nSigmaSpatial <= 2)
    {
        win = 3;
    }
    else
    {
        win = 4;
    }
    
    pLutS = (INT32 *)malloc(sizeof(INT32) * (2*win + 1));
    for (i=-win; i<=win; i++)
    {
        pLutS[i+win] = (INT32)(exp(-0.5f * i*i/nSigmaSpatial/nSigmaSpatial) * 64.0f + 0.5f);
        //printf("LUTS = %d.\n", pLutS[i+win]);
    }
  
    nHeightLast = nHeight - win;
    nWidthLast = nWidth - win;

    //////////////////////////////////////////////////////////////////////////
    //horizontal direction filter
    pSrc = pImageSrcDst;
    pDst = pMemBuf;
    for (i=0; i<nHeight; i++)
    {
        memcpy(pDst, pSrc, win*sizeof(UINT8));
        pSrc += win;
        pDst += win;

        for (j=win; j<nWidthLast; j++)
        {
            pixelsum = 0;
            weightsum = 0;
            for (k=-win; k<=win; k++)
            {
                ucTemp = *(pSrc+k);

                weight = lutR[*pSrc - ucTemp + 255] * pLutS[k+win];
                pixelsum += ucTemp * weight;

                weightsum += weight;
            }
            *pDst++ = ( pixelsum + (weightsum>>1) ) / weightsum;
            pSrc++;
        }

        memcpy(pDst, pSrc, win*sizeof(UINT8));
        pSrc += win;
        pDst += win;
    }

    //////////////////////////////////////////////////////////////////////////
    //vertical direction filter
    pSrc = pMemBuf + win*nWidth;
    pDst = pImageSrcDst + win*nWidth;
    for (i=win; i<nHeightLast; i++)
    {
        for (j=0; j<nWidth; j++)
        {
            pixelsum = 0;
            weightsum = 0;
            for (k=-win; k<=win; k++)
            {
                ucTemp = *(pSrc+k*nWidth);

                weight = lutR[*pSrc - ucTemp + 255] * pLutS[k+win];
                pixelsum += ucTemp * weight;

                weightsum += weight;
            }
            *pDst++ = ( pixelsum + (weightsum>>1) ) / weightsum;
            pSrc++;
        }
    }

    free(pLutS);
#endif

}

void BilateralFilter_new(UINT8 *pImageSrc,UINT8 *pImageDst, INT32 nHeight, INT32 nWidth, INT32 nSigmaSpatial, INT32 nSigmaRange)
{
    INT32 i = 0, j = 0, k = 0;
    INT32 lutR[511] = {0};
    INT32 *pLutS;

    INT32 win , nHeightLast , nWidthLast;
    INT32 pixelsum = 0, weight = 0, weightsum = 0;
    UINT8 ucTemp = 0;
    //UINT8 *pWeight;
    UINT8 *pSrc ;
    UINT8 *pDst ;
    UINT8 *pMemBuf;
    
    //////////////////////////////////////////////////////////////////////////
    //invalid parameters
    if (nSigmaSpatial<=0 || nSigmaRange<=0)
    {
        return ;
    }

#ifdef __TEST_ARM
    if (nSigmaSpatial <= 2)
    {
        bilateral_7x7_neon(pImageSrc, pImageDst, nWidth, nHeight, nSigmaSpatial, nSigmaRange);
    }
    else
    {
        bilateral_9x9_neon(pImageSrc, pImageDst, nWidth, nHeight, nSigmaSpatial, nSigmaRange);
    }
#else
    pMemBuf = (UINT8 *)malloc(nWidth*nHeight);

    //////////////////////////////////////////////////////////////////////////
    //lutR and pLutS

    //memset(lutR,0,sizeof(INT32)*511);
    for (i=-2*nSigmaRange; i<=2*nSigmaRange; i++)
    {
        lutR[i+255] = (INT32)(exp(-0.5f * i*i/nSigmaRange/nSigmaRange) * 64.0f + 0.5f);
        //printf("LUTR = %d.\n", lutR[i+255]);
    }

    //pLutS = (INT32 *)malloc(sizeof(INT32) * (4*nSigmaSpatial + 1));
    //for (i=-2*nSigmaSpatial; i<=2*nSigmaSpatial; i++)
    //{
    //    pLutS[i+2*nSigmaSpatial] = (INT32)(exp(-0.5f * i*i/nSigmaSpatial/nSigmaSpatial) * 64.0f + 0.5f);
    //}

    //////////////////////////////////////////////////////////////////////////
    //half winsize is 2*sigma
    //win = 2*nSigmaSpatial;
    if (nSigmaSpatial <= 2)
    {
        win = 3;
    }
    else
    {
        win = 4;
    }
    
    pLutS = (INT32 *)malloc(sizeof(INT32) * (2*win + 1));
    for (i=-win; i<=win; i++)
    {
        pLutS[i+win] = (INT32)(exp(-0.5f * i*i/nSigmaSpatial/nSigmaSpatial) * 64.0f + 0.5f);
        //printf("LUTS = %d.\n", pLutS[i+win]);
    }
  
    nHeightLast = nHeight - win;
    nWidthLast = nWidth - win;

    //////////////////////////////////////////////////////////////////////////
    //horizontal direction filter
    pSrc = pImageSrc;
    pDst = pMemBuf;
    for (i=0; i<nHeight; i++)
    {
        memcpy(pDst, pSrc, win*sizeof(UINT8));
        pSrc += win;
        pDst += win;

        for (j=win; j<nWidthLast; j++)
        {
            pixelsum = 0;
            weightsum = 0;
            for (k=-win; k<=win; k++)
            {
                ucTemp = *(pSrc+k);

                weight = lutR[*pSrc - ucTemp + 255] * pLutS[k+win];
                pixelsum += ucTemp * weight;

                weightsum += weight;
            }
            *pDst++ = ( pixelsum + (weightsum>>1) ) / weightsum;
            pSrc++;
        }

        memcpy(pDst, pSrc, win*sizeof(UINT8));
        pSrc += win;
        pDst += win;
    }

    //////////////////////////////////////////////////////////////////////////
    //vertical direction filter
    pSrc = pMemBuf;
    pDst = pImageDst;

    for(i = 0; i < win; i++)
    {
        memcpy(pDst, pSrc, nWidth*sizeof(UINT8));
        pDst += nWidth;
        pSrc += nWidth;
    }

    pSrc = pMemBuf + nHeightLast*nWidth;
    pDst = pImageDst + nHeightLast*nWidth;
    for(i = 0; i < win; i++)
    {
        memcpy(pDst, pSrc, nWidth*sizeof(UINT8));
        pDst += nWidth;
        pSrc += nWidth;
    }
    
    
    pSrc = pMemBuf + win*nWidth;
    pDst = pImageDst + win*nWidth;
    for (i=win; i<nHeightLast; i++)
    {
        for (j=0; j<nWidth; j++)
        {
            pixelsum = 0;
            weightsum = 0;
            for (k=-win; k<=win; k++)
            {
                ucTemp = *(pSrc+k*nWidth);

                weight = lutR[*pSrc - ucTemp + 255] * pLutS[k+win];
                pixelsum += ucTemp * weight;

                weightsum += weight;
            }
            *pDst++ = ( pixelsum + (weightsum>>1) ) / weightsum;
            pSrc++;
        }
    }

    free(pMemBuf);
    free(pLutS);
#endif

}

void CrossBilateralFilter(UINT8 *pImageSrcDst, INT32 nHeight, INT32 nWidth, INT32 nSigmaSpatial, INT32 nSigmaRange, UINT8 *pMemBuf, UINT8 *pEdge)
{
    INT32 i = 0, j = 0, k = 0;
    INT32 lutR[511] = {0};
    INT32 *pLutS;

    INT32 win , nHeightLast , nWidthLast;
    INT32 pixelsum = 0, weight = 0, weightsum = 0;
    
    UINT8 *pWeight;
    UINT8 *pSrc ;
    UINT8 *pDst ;
    
    //////////////////////////////////////////////////////////////////////////
    //invalid parameters
    if (nSigmaSpatial<=0 || nSigmaRange<=0)
    {
        return ;
    }

    //////////////////////////////////////////////////////////////////////////
    //lutR and pLutS
    memset(lutR,0,sizeof(INT32)*511);
    for (i=-2*nSigmaRange; i<=2*nSigmaRange; i++)
    {
        lutR[i+255] = (INT32)(exp(-0.5f * i*i/nSigmaRange/nSigmaRange) * 64.0f + 0.5f);
        //printf("LUTR = %d.\n", lutR[i+255]);
    }

    //pLutS = (INT32 *)malloc(sizeof(INT32) * (4*nSigmaSpatial + 1));
    //for (i=-2*nSigmaSpatial; i<=2*nSigmaSpatial; i++)
    //{
    //    pLutS[i+2*nSigmaSpatial] = (INT32)(exp(-0.5f * i*i/nSigmaSpatial/nSigmaSpatial) * 64.0f + 0.5f);
    //}

    //////////////////////////////////////////////////////////////////////////
    //half winsize is 2*sigma
    //win = 2*nSigmaSpatial;
    if (nSigmaSpatial <= 2)
    {
        win = 3;
    }
    else
    {
        win = 4;
    }
    
    pLutS = (INT32 *)malloc(sizeof(INT32) * (2*win + 1));
    for (i=-win; i<=win; i++)
    {
        pLutS[i+win] = (INT32)(exp(-0.5f * i*i/nSigmaSpatial/nSigmaSpatial) * 64.0f + 0.5f);
        //printf("LUTS = %d.\n", pLutS[i+win]);
    }
  
    nHeightLast = nHeight - win;
    nWidthLast = nWidth - win;

    //////////////////////////////////////////////////////////////////////////
    //horizontal direction filter
    pSrc = pImageSrcDst;
    pDst = pMemBuf;
    pWeight = pEdge;
    for (i=0; i<nHeight; i++)
    {
        memcpy(pDst, pSrc, win*sizeof(UINT8));
        pSrc += win;
        pDst += win;
        pWeight += win;       
        for (j=win; j<nWidthLast; j++)
        {
            pixelsum = 0;
            weightsum = 0;
            for (k=-win; k<=win; k++)
            {
                weight = lutR[*pWeight - *(pWeight+k) + 255] * pLutS[k+win];
                pixelsum += *(pSrc+k) * weight;

                weightsum += weight;
            }
            *pDst++ = ( pixelsum + (weightsum>>1) ) / weightsum;
            pSrc++;
            pWeight++;
        }

        memcpy(pDst, pSrc, win*sizeof(UINT8));
        pSrc += win;
        pDst += win;
        pWeight += win;
    }

    //////////////////////////////////////////////////////////////////////////
    //vertical direction filter
    pSrc = pMemBuf + win*nWidth;
    pDst = pImageSrcDst + win*nWidth;
    pWeight = pEdge + win*nWidth;
    for (i=win; i<nHeightLast; i++)
    {
        for (j=0; j<nWidth; j++)
        {
            pixelsum = 0;
            weightsum = 0;
            for (k=-win; k<=win; k++)
            {
                weight = lutR[*pWeight - *(pWeight+k*nWidth) + 255] * pLutS[k+win];
                pixelsum += *(pSrc+k*nWidth) * weight;

                weightsum += weight;
            }
            *pDst++ = ( pixelsum + (weightsum>>1) ) / weightsum;
            pSrc++;
            pWeight++;
        }
    }

    free(pLutS);

}
