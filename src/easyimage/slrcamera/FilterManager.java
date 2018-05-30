
package easyimage.slrcamera;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;

import easyimage.slrcamera.param.FilterParams;
import easyimage.slrcamera.param.FilterType;

import java.io.ByteArrayOutputStream;

public class FilterManager {

    public static class FilterException extends Exception {
        private static final long serialVersionUID = -7134928165700297059L;

        public FilterException(String message) {
            super(message);
        }
    }

    public interface FilterCallback {
        void onSuccess(byte[] data);

        void onError(FilterException e);
    }

    private FilterType mFilterType = FilterType.NONE;

    public void setFilterType(FilterType filterType) {
        mFilterType = filterType;
    }

    public FilterType getFilterType() {
        return mFilterType;
    }

    public void filter(byte[] data, FilterCallback callback, FilterParams params) {
        if (mFilterType == null || mFilterType == FilterType.NONE) {
            callback.onSuccess(data);
            return;
        }


        new FilterTask(data, mFilterType, callback, params).execute();
    }

    private static class FilterTask extends AsyncTask<Void, Void, Void> {

        private byte[] mData;
        private FilterCallback mCallback;
        private FilterType mType;
        private FilterParams mParams;
        private Bitmap mBitmap;
        private FilterException mError;

        public FilterTask(byte[] data, FilterType type, FilterCallback callback, FilterParams params) {
            mData = data;
            mType = type;
            mCallback = callback;
            mParams = params;
        }

        @Override
        protected Void doInBackground(Void... args) {
            BitmapFactory.Options opt = new BitmapFactory.Options();
            opt.inMutable = true;
            opt.inPreferQualityOverSpeed = true;
            opt.inSampleSize = 1;

            mBitmap = BitmapFactory.decodeByteArray(mData, 0, mData.length, opt);


            switch (mType) {
            /*
            case GREYSCALE:
                mResultPixels = ImageProcessX.GrayScale(pixels, mWidth,mHeight);
                break;
            
            case SOFTGLOW:
                mResultPixels = ImageProcessX.SoftGlow(pixels, mWidth,mHeight);
                break;
            case CROSSSHADER:
                mResultPixels = ImageProcessX.CrossShader(pixels, mWidth,mHeight);
                break;
            case LOMO:
                mResultPixels = ImageProcessX.Lomo(pixels, mWidth,mHeight);
                break;
            case OLDPICTURE:
                mResultPixels = ImageProcessX.OldPicture(pixels, mWidth,mHeight);
                break;
            case EMBOSS:
                mResultPixels = ImageProcessX.Emboss(pixels, mWidth,mHeight);
                break;
            case SKETCH:
                mResultPixels = ImageProcessX.Sketch(pixels, mWidth,mHeight);
                break;
                */
            case HDR:
                int iso = 100;
                if (mParams.iso != null) {
                    iso = Integer.valueOf(mParams.iso);
                }
                double exposure = 0.04;
                if (mParams.exposure_time != null) {
                    exposure = Double.valueOf(mParams.exposure_time);
                }
                ImageProcessX.HDR(mBitmap, iso, exposure);
                break;
            }
            return null;
        }

        private void tearDown() {
            mBitmap.recycle();
            mBitmap = null;
        }

        @Override
        protected void onPostExecute(Void result) {
            if (mError != null) {
                tearDown();
                mCallback.onError(mError);
                return;
            }

            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            boolean success = mBitmap.compress(Bitmap.CompressFormat.JPEG, 90, stream);
            tearDown();

            if (success) {
                mCallback.onSuccess(stream.toByteArray());
            } else {
                mCallback.onError(new FilterException("error compressing image"));
            }
        }

    }

}
