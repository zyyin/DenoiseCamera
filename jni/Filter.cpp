#include <jni.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <android/bitmap.h>
#include <android/log.h>

#define  PRINT_LOG  0
#if      PRINT_LOG
#define  LOG_TAG    "CameraHDR"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI
#define  LOGE
#endif


#ifndef max
#define max(a, b)  ( (a) > (b) ? (a) : (b) )
#endif
#ifndef min
#define min(a, b)  ( (a) < (b) ? (a) : (b) )
#endif

typedef unsigned char uchar;

#define uchar_cast(x)  ( (uchar)max(0, min(255, (x))) )

typedef struct THREADPARAM {
  int     w;
  int     h;
  uchar*  data;
  uchar*  table;
}THREADPARAM;

void *HDRThread(void *arg)
{
    THREADPARAM* tp = (THREADPARAM*) arg;
    uchar minVal;
    uchar* table = tp->table;
    for(int i = 0; i < tp->w*tp->h; i++)
    {
        uchar* c = tp->data + i*4;
        minVal = max(c[0], min(c[1], c[2]));
        c[0] = table[c[0]*256+minVal];
        c[1] = table[c[1]*256+minVal];
        c[2] = table[c[2]*256+minVal];
    }
    return NULL;
}

bool GenerateGammaTable(uchar* table, float gamma, float gain)
{
    if (gamma <= 0.0f) return false;
    double dinvgamma = 1/gamma*gain;
    double dMax = pow(255.0, dinvgamma) / 255.0;
    for(int i = 0; i < 256; i++)
    {
        table[i] = uchar_cast( pow((double)i, dinvgamma) / dMax);
    }
    return true;
}

double meanY(uchar* data, int w, int h)
{
    unsigned long r, g, b;
    uchar* tmp = data;
    r = g = b = 0;
    for(int i = 0; i < w*h; i++)
    {
        r += *tmp++;
        g += *tmp++;
        b += *tmp++;
        tmp++;
    }
    r /= w*h;
    g /= w*h;
    b /= w*h;
    double y = 0.299*r + 0.587*g + 0.114*b;
    return y;
}
float MeanSaturation(uchar* data, int w, int h)
{
    float s, maxVal, minVal, c;
    uchar r, g, b;
    s = c = 0;
    uchar* tmp = data;
    for(int i = 0; i < w*h; i++)
    {
        r = *tmp++;
        g = *tmp++;
        b = *tmp++;
        tmp++;
        maxVal = max(max(r, g), b);
        minVal = min(min(r, g), b);
        if(maxVal > 0)
        {
            c++;
            s += (maxVal - minVal) / maxVal;
        }
    }
    if( c == 0) return 0.0f;
    return s / c;
}
extern "C" {

JNIEXPORT void JNICALL Java_easyimage_slrcamera_ImageProcessX_HDR(JNIEnv* env, jobject thiz, jobject bitmap, int iso, double exp)
{
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        return;
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        return;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        return;
    }
    uchar* data = (uchar*)pixels;

    double y = meanY(data, info.width, info.height);
    LOGE("y = %.2f", y);
    y = min(128., max(40., y));

    const double DEHAZE_RATIO = 0.0073863 *y - 0.295452;
    const double nGamma = 1.32-0.0025*y;
    double minVal;
    uchar gamma_table[256];
    GenerateGammaTable(gamma_table,  nGamma,  1.0);
    LOGE("meany = %.2f,  ratio=%.2f, ngamma=%.2f", y, DEHAZE_RATIO, nGamma);
    if(iso <= 100 && exp <= 0.04)
    {
        LOGE("do hdr, iso = %d, exp=%.4f", iso, exp);
        if ( MeanSaturation(data, info.width, info.height) < 0.1)
        goto end;
        uchar *table = new uchar[256*256];
        uchar tmp;
        for(int i = 0; i < 256; i++)
        {
            for(int j = 0; j < 256; j++)
            {
                double d = (255 - DEHAZE_RATIO * j) / 255.0;
                if (d < 0.1) d = 0.1;
                table[i*256+j] = uchar_cast( (i - 255) / d + 255);
            }
        }
        pthread_t thead[4];
        THREADPARAM tp[4];
        void *tret;
        int th = info.height/4;
        for(int i = 0; i < 4; i++)
        {
        tp[i].w = info.width;
            tp[i].h = th;
        tp[i].data = data + i*info.width*th*4;
            tp[i].table = table;
        }
        tp[3].h = info.height - 3*th;
        for(int i = 0; i < 4; i++)
        {
            pthread_create(&thead[i], NULL, HDRThread, &tp[i]);
        }
        for(int i = 0; i < 4; i++)
        {
            pthread_join(thead[i], &tret);
        }
        delete table;

    }
    else if( iso > 200)
    {
        LOGE("do gamma in low light");
        uchar *tmp = data;
        for( int i = 0; i < info.width*info.height; i++)
        {
            *tmp++ = gamma_table[*tmp];
            *tmp++ = gamma_table[*tmp];
            *tmp++ = gamma_table[*tmp];
            tmp++;
        }
    }
end:
    AndroidBitmap_unlockPixels(env, bitmap);
}

}

