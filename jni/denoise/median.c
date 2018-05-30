/********************************************************************************
 * Copyright (C), 2010-2014, Huawei Tech. Co., Ltd.
 * Author         : 
 * Version        : uniVideoV100R001C02_median_V1.0
 * Date           : 2012-7-3
 * Description    : 
 * Function List  : 
 * History        : 
 ********************************************************************************/

//////////////////////////////////////////////////////////////////////////
//Calculate median of a group numbers with length of 3, 5, 7, 9, 25.
/*
*  The  following  routines  have  been  built  from  knowledge  gathered
*  around  the  Web.  I  am  not  aware  of  any  copyright  problem  with
*  them,  so  use  it  as  you  want.
*  N.  Devillard  -  1998
*/
typedef  unsigned char  pixelvalue  ;
#define  PIX_SORT(a,b)  {  if  ((a)>(b))  PIX_SWAP((a),(b));  }
#define  PIX_SWAP(a,b)  {  pixelvalue  temp=(a);(a)=(b);(b)=temp;  }

/*----------------------------------------------------------------------------
Function  :     opt_med3()
In :     pointer  to  array  of  3  pixel  values
Out :     a  pixelvalue
Job :     optimized  search  of  the  median  of  3  pixel  values
Notice     :     found  on  sci.image.processing
cannot  go  faster  unless  assumptions  are  made
on  the  nature  of  the  input  signal.
---------------------------------------------------------------------------*/
pixelvalue  opt_med3(pixelvalue  *  p)
{
    PIX_SORT(p[0],p[1])  ;  PIX_SORT(p[1],p[2])  ;  PIX_SORT(p[0],p[1])  ;
    return(p[1])  ;
}

/*----------------------------------------------------------------------------
Function  :     opt_med5()
In :     pointer  to  array  of  5  pixel  values
Out :     a  pixelvalue
Job :     optimized  search  of  the  median  of  5  pixel  values
Notice     :     found  on  sci.image.processing
cannot  go  faster  unless  assumptions  are  made
on  the  nature  of  the  input  signal.
---------------------------------------------------------------------------*/
pixelvalue  opt_med5(pixelvalue  *  p)
{
    PIX_SORT(p[0],p[1])  ;  PIX_SORT(p[3],p[4])  ;  PIX_SORT(p[0],p[3])  ;
    PIX_SORT(p[1],p[4])  ;  PIX_SORT(p[1],p[2])  ;  PIX_SORT(p[2],p[3])  ;
    PIX_SORT(p[1],p[2])  ;  return(p[2])  ;
}

/*----------------------------------------------------------------------------
Function  :     opt_med7()
In :     pointer  to  array  of  7  pixel  values
Out :     a  pixelvalue
Job :     optimized  search  of  the  median  of  7  pixel  values
Notice     :     found  on  sci.image.processing
cannot  go  faster  unless  assumptions  are  made
on  the  nature  of  the  input  signal.
---------------------------------------------------------------------------*/
pixelvalue  opt_med7(pixelvalue  *  p)
{
    PIX_SORT(p[0],  p[5])  ;  PIX_SORT(p[0],  p[3])  ;  PIX_SORT(p[1],  p[6])  ;
    PIX_SORT(p[2],  p[4])  ;  PIX_SORT(p[0],  p[1])  ;  PIX_SORT(p[3],  p[5])  ;
    PIX_SORT(p[2],  p[6])  ;  PIX_SORT(p[2],  p[3])  ;  PIX_SORT(p[3],  p[6])  ;
    PIX_SORT(p[4],  p[5])  ;  PIX_SORT(p[1],  p[4])  ;  PIX_SORT(p[1],  p[3])  ;
    PIX_SORT(p[3],  p[4])  ;  return  (p[3])  ;
}

/*----------------------------------------------------------------------------
Function  :     opt_med9()
In :     pointer  to  an  array  of  9  pixelvalues
Out :     a  pixelvalue
Job :     optimized  search  of  the  median  of  9  pixelvalues
Notice     :     in  theory,  cannot  go  faster  without  assumptions  on  the
signal.
Formula  from:
XILINX  XCELL  magazine,  vol.  23  by  John  L.  Smith
The  input  array  is  modified  in  the  process
The  result  array  is  guaranteed  to  contain  the  median
value
in  middle  position,  but  other  elements  are  NOT  sorted.
---------------------------------------------------------------------------*/
pixelvalue  opt_med9(pixelvalue  *  p)
{
    PIX_SORT(p[1],  p[2])  ;  PIX_SORT(p[4],  p[5])  ;  PIX_SORT(p[7],  p[8])  ;
    PIX_SORT(p[0],  p[1])  ;  PIX_SORT(p[3],  p[4])  ;  PIX_SORT(p[6],  p[7])  ;
    PIX_SORT(p[1],  p[2])  ;  PIX_SORT(p[4],  p[5])  ;  PIX_SORT(p[7],  p[8])  ;
    PIX_SORT(p[0],  p[3])  ;  PIX_SORT(p[5],  p[8])  ;  PIX_SORT(p[4],  p[7])  ;
    PIX_SORT(p[3],  p[6])  ;  PIX_SORT(p[1],  p[4])  ;  PIX_SORT(p[2],  p[5])  ;
    PIX_SORT(p[4],  p[7])  ;  PIX_SORT(p[4],  p[2])  ;  PIX_SORT(p[6],  p[4])  ;
    PIX_SORT(p[4],  p[2])  ;  return(p[4])  ;
}

/*----------------------------------------------------------------------------
Function  :     opt_med25()
In :     pointer  to  an  array  of  25  pixelvalues
Out :     a  pixelvalue
Job :     optimized  search  of  the  median  of  25  pixelvalues
Notice     :     in  theory,  cannot  go  faster  without  assumptions  on  the
signal.
Code  taken  from  Graphic  Gems.
---------------------------------------------------------------------------*/
pixelvalue  opt_med25(pixelvalue  *  p)
{
    PIX_SORT(p[0],  p[1])  ;     PIX_SORT(p[3],  p[4])  ;     PIX_SORT(p[2],  p[4])  ;
    PIX_SORT(p[2],  p[3])  ;     PIX_SORT(p[6],  p[7])  ;     PIX_SORT(p[5],  p[7])  ;
    PIX_SORT(p[5],  p[6])  ;     PIX_SORT(p[9],  p[10])  ;    PIX_SORT(p[8],  p[10])  ;
    PIX_SORT(p[8],  p[9])  ;     PIX_SORT(p[12],  p[13])  ;  PIX_SORT(p[11],  p[13])  ;
    PIX_SORT(p[11],  p[12])  ;  PIX_SORT(p[15],  p[16])  ;  PIX_SORT(p[14],  p[16])  ;
    PIX_SORT(p[14],  p[15])  ;  PIX_SORT(p[18],  p[19])  ;  PIX_SORT(p[17],  p[19])  ;
    PIX_SORT(p[17],  p[18])  ;  PIX_SORT(p[21],  p[22])  ;  PIX_SORT(p[20],  p[22])  ;
    PIX_SORT(p[20],  p[21])  ;  PIX_SORT(p[23],  p[24])  ;  PIX_SORT(p[2],  p[5])  ;
    PIX_SORT(p[3],  p[6])  ;     PIX_SORT(p[0],  p[6])  ;     PIX_SORT(p[0],  p[3])  ;
    PIX_SORT(p[4],  p[7])  ;     PIX_SORT(p[1],  p[7])  ;     PIX_SORT(p[1],  p[4])  ;
    PIX_SORT(p[11],  p[14])  ;  PIX_SORT(p[8],  p[14])  ;    PIX_SORT(p[8],  p[11])  ;
    PIX_SORT(p[12],  p[15])  ;  PIX_SORT(p[9],  p[15])  ;    PIX_SORT(p[9],  p[12])  ;
    PIX_SORT(p[13],  p[16])  ;  PIX_SORT(p[10],  p[16])  ;  PIX_SORT(p[10],  p[13])  ;
    PIX_SORT(p[20],  p[23])  ;  PIX_SORT(p[17],  p[23])  ;  PIX_SORT(p[17],  p[20])  ;
    PIX_SORT(p[21],  p[24])  ;  PIX_SORT(p[18],  p[24])  ;  PIX_SORT(p[18],  p[21])  ;
    PIX_SORT(p[19],  p[22])  ;  PIX_SORT(p[8],  p[17])  ;    PIX_SORT(p[9],  p[18])  ;
    PIX_SORT(p[0],  p[18])  ;    PIX_SORT(p[0],  p[9])  ;     PIX_SORT(p[10],  p[19])  ;
    PIX_SORT(p[1],  p[19])  ;    PIX_SORT(p[1],  p[10])  ;    PIX_SORT(p[11],  p[20])  ;
    PIX_SORT(p[2],  p[20])  ;    PIX_SORT(p[2],  p[11])  ;    PIX_SORT(p[12],  p[21])  ;
    PIX_SORT(p[3],  p[21])  ;    PIX_SORT(p[3],  p[12])  ;    PIX_SORT(p[13],  p[22])  ;
    PIX_SORT(p[4],  p[22])  ;    PIX_SORT(p[4],  p[13])  ;    PIX_SORT(p[14],  p[23])  ;
    PIX_SORT(p[5],  p[23])  ;    PIX_SORT(p[5],  p[14])  ;    PIX_SORT(p[15],  p[24])  ;
    PIX_SORT(p[6],  p[24])  ;    PIX_SORT(p[6],  p[15])  ;    PIX_SORT(p[7],  p[16])  ;
    PIX_SORT(p[7],  p[19])  ;    PIX_SORT(p[13],  p[21])  ;  PIX_SORT(p[15],  p[23])  ;
    PIX_SORT(p[7],  p[13])  ;    PIX_SORT(p[7],  p[15])  ;    PIX_SORT(p[1],  p[9])  ;
    PIX_SORT(p[3],  p[11])  ;    PIX_SORT(p[5],  p[17])  ;    PIX_SORT(p[11],  p[17])  ;
    PIX_SORT(p[9],  p[17])  ;    PIX_SORT(p[4],  p[10])  ;    PIX_SORT(p[6],  p[12])  ;
    PIX_SORT(p[7],  p[14])  ;    PIX_SORT(p[4],  p[6])  ;     PIX_SORT(p[4],  p[7])  ;
    PIX_SORT(p[12],  p[14])  ;  PIX_SORT(p[10],  p[14])  ;  PIX_SORT(p[6],  p[7])  ;
    PIX_SORT(p[10],  p[12])  ;  PIX_SORT(p[6],  p[10])  ;    PIX_SORT(p[6],  p[17])  ;
    PIX_SORT(p[12],  p[17])  ;  PIX_SORT(p[7],  p[17])  ;    PIX_SORT(p[7],  p[10])  ;
    PIX_SORT(p[12],  p[18])  ;  PIX_SORT(p[7],  p[12])  ;    PIX_SORT(p[10],  p[18])  ;
    PIX_SORT(p[12],  p[20])  ;  PIX_SORT(p[10],  p[20])  ;  PIX_SORT(p[10],  p[12])  ;
    return  (p[12]);
}
#undef  PIX_SORT
#undef  PIX_SWAP

void MedianFilter(unsigned char *pImageSrcDst, int nHeight, int nWidth, unsigned char *pMemBuf, int winsize)
{
    ///*
    int i, j;
    int nHeightLast, nWidthLast;
    unsigned char *pSrc, *pDst;

    if ( winsize == 3)                          //window size is 3x3
    {
        unsigned char temp[3];

        //////////////////////////////////////////////////////////////////////////
        //horizontal direction filter
        nHeightLast = nHeight - 1, nWidthLast = nWidth - 1;
        pSrc = pImageSrcDst + nWidth, pDst = pMemBuf + nWidth;

        for (i=1; i<nHeightLast; i++)
        {
            pSrc ++;
            pDst ++;
            for (j=1; j<nWidthLast; j++)
            {
                temp[0] = *(pSrc-1);
                temp[1] = *pSrc;
                temp[2] = *(pSrc+1);
                
                *pDst ++ = opt_med3(temp);
                pSrc ++;
            }
            pSrc ++;
            pDst ++;
        }

        //////////////////////////////////////////////////////////////////////////
        //vertical direction filter
        nHeightLast = nHeight - 2, nWidthLast = nWidth - 2;
        pSrc = pMemBuf + nWidth + nWidth, pDst = pImageSrcDst + nWidth + nWidth;
        
        for (i=2; i<nHeightLast; i++)
        {
            pSrc += 2;
            pDst += 2;
            for (j=2; j<nWidthLast; j++)
            {
                temp[0] = *(pSrc-nWidth);
                temp[1] = *pSrc;
                temp[2] = *(pSrc+nWidth);
                
                *pDst ++ =  opt_med3(temp);
                pSrc ++;
            }
            pSrc += 2;
            pDst += 2;
        }
    }
    else if (winsize == 5)                         //window size is 5x5
    {
        unsigned char temp[5];
        int nWidthX2 = (nWidth<<1);

        //////////////////////////////////////////////////////////////////////////
        //horizontal direction filter
        nHeightLast = nHeight - 2, nWidthLast = nWidth - 2;
        pSrc = pImageSrcDst + nWidthX2, pDst = pMemBuf + nWidthX2;

        for (i=2; i<nHeightLast; i++)
        {
            pSrc += 2;
            pDst += 2;
            for (j=2; j<nWidthLast; j++)
            {
                temp[0] = *(pSrc-2);
                temp[1] = *(pSrc-1);
                temp[2] = *pSrc;
                temp[3] = *(pSrc+1);
                temp[4] = *(pSrc+2);
                
                *pDst ++ = opt_med5(temp);
                pSrc ++;
            }
            pSrc += 2;
            pDst += 2;
        }

        //////////////////////////////////////////////////////////////////////////
        //vertical direction filter
        nHeightLast = nHeight - 4, nWidthLast = nWidth - 4;
        pSrc = pMemBuf + nWidthX2 + nWidthX2, pDst = pImageSrcDst + nWidthX2 + nWidthX2;

        for (i=4; i<nHeightLast; i++)
        {
            pSrc += 4;
            pDst += 4;
            for (j=4; j<nWidthLast; j++)
            {
                temp[0] = *(pSrc-nWidthX2);
                temp[1] = *(pSrc-nWidth);
                temp[2] = *pSrc;
                temp[3] = *(pSrc+nWidth);
                temp[4] = *(pSrc+nWidthX2);
                
                *pDst ++ =  opt_med5(temp);
                pSrc ++;
            }
            pSrc += 4;
            pDst += 4;
        }
    }

    //*/

    /*
    //迭代式中值
    int i = 0, j = 0;
    int nHeightLast = nHeight - 2, nWidthLast = nWidth - 2;

    unsigned char a, b, center, x, y;

    unsigned char *pSrc = pImageSrcDst + nWidth + nWidth;
    for (i=2; i<nHeightLast; i++)
    {
        pSrc +=2;
        for (j=2; j<nWidthLast; j++)
        {
            center = *pSrc;

            a = *(pSrc-2);
            b = *(pSrc-1);
            x = MEDIAN3(a,b,center);

            a = *(pSrc+2);
            b = *(pSrc+1);
            y = MEDIAN3(a,b,center);

            *pSrc ++ = MEDIAN3(x,y,center);
        }
        pSrc +=2;
    }

    pSrc = pImageSrcDst + nWidth + nWidth;
    for (i=2; i<nHeightLast; i++)
    {
        pSrc += 2;
        for (j=2; j<nWidthLast; j++)
        {
            center = *pSrc;

            a = *(pSrc-nWidth-nWidth);
            b = *(pSrc-nWidth);
            x = MEDIAN3(a,b,center);

            a = *(pSrc+nWidth+nWidth);
            b = *(pSrc+nWidth);
            y = MEDIAN3(a,b,center);

            *pSrc ++ = MEDIAN3(x,y,center);
        }
        pSrc += 2;
    }
    */

}

