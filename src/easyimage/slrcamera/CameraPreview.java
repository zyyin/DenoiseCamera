
package easyimage.slrcamera;

import android.content.Context;
import android.hardware.Camera;
import android.hardware.Camera.Size;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import easyimage.slrcamera.param.ISOType;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class CameraPreview extends SurfaceView implements SurfaceHolder.Callback {

    private static final String TAG = CameraPreview.class.getCanonicalName();

    private static final String PARAM_ISO = "iso";

    private Preferences mPreferences;

    private SurfaceHolder mHolder;
    private Camera mCamera;
    private boolean mSurfaceAvailable;

    private List<Size> mPictureSizes;
    private int mMinExposure, mMaxExposure;
    private float mExposureStep;

    public CameraPreview(Context context) {
        super(context);

        mPreferences = Preferences.getInstance(context);

        mHolder = getHolder();
        mHolder.addCallback(this);
    }

    public Camera getCamera() {
        return mCamera;
    }

    public void onResume() {
        openCamera();
        setupCamera();
        startPreview();
    }

    public void onPause() {
        stopPreview();
        closeCamera();
    }

    private void openCamera() {
        try {
            mCamera = Camera.open();
        } catch (Exception e) {
            Log.d(TAG, "Error opening camera: " + e.getMessage());
        }
    }

    private void closeCamera() {
        if (mCamera == null) {
            return;
        }
        mCamera.release();
        mCamera = null;
    }

    private void startPreview() {
        if (mCamera == null) {
            return;
        }
        try {
            mCamera.setPreviewDisplay(mHolder);
            mCamera.startPreview();
        } catch (IOException e) {
            Log.d(TAG, "Error starting camera preview: " + e.getMessage());
        }
    }

    private void stopPreview() {
        if (mCamera == null) {
            return;
        }
        mCamera.stopPreview();
    }

    private void setFocusMode(Camera.Parameters params) {
        Log.d(TAG, "setting focus mode");
        for (String focusMode : params.getSupportedFocusModes()) {
            if (Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE.equals(focusMode)) {
                params.setFocusMode(focusMode);
                return;
            }
        }
        // TODO: handle manual auto focus
    }

    private void setupCamera() {
        if (mCamera == null) {
            return;
        }

        Camera.Parameters params = mCamera.getParameters();

        // Preview size
        Size previewSize = params.getPreviewSize();
        Log.d(TAG, "preview size: " + previewSize.width + " " + previewSize.height);

        setFocusMode(params);

        // Picture size
        mPictureSizes = params.getSupportedPictureSizes();
        int picWidth = mPreferences.getPictureWidth();
        int picHeight = mPreferences.getPictureHeight();
        setPictureSize(picWidth, picHeight);

        // ISO
        // params.set(PARAM_ISO, ISOType.AUTO.getValue());

        // Exposure
        mMinExposure = params.getMinExposureCompensation();
        mMaxExposure = params.getMaxExposureCompensation();
        mExposureStep = params.getExposureCompensationStep();

        mCamera.setParameters(params);
    }

    public List<String> getPictureSizes() {
        ArrayList<String> res = new ArrayList<String>();
        for (Size size : mPictureSizes) {
            res.add(String.format("%dx%d", size.width, size.height));
        }
        return res;
    }

    public int getPictureSizeIndex() {
        if (mCamera == null) {
            return 0;
        }

        int width = mPreferences.getPictureWidth();
        int height = mPreferences.getPictureHeight();
        for (int i = 0; i < mPictureSizes.size(); i++) {
            Size size = mPictureSizes.get(i);
            if (size.width == width && size.height == height) {
                return i;
            }
        }
        return 0;
    }

    private void setPictureSize(int width, int height) {
        if (mCamera == null) {
            return;
        }

        Camera.Parameters params = mCamera.getParameters();
        for (Size size : mPictureSizes) {
            if (size.width == width && size.height == height) {
                params.setPictureSize(width, height);
                mCamera.setParameters(params);
                return;
            }
        }
    }

    public void setPictureSize(int index) {
        Size size = mPictureSizes.get(index);
        setPictureSize(size.width, size.height);
        mPreferences.setPictureWidth(size.width);
        mPreferences.setPictureHeight(size.height);
    }

    public void setISO(ISOType iso) {
        if (mCamera == null) {
            return;
        }

        Camera.Parameters params = mCamera.getParameters();
        params.set(PARAM_ISO, iso.getValue());
        mCamera.setParameters(params);
    }

    public List<String> getEV() {
        ArrayList<String> res = new ArrayList<String>();
        for (int i = mMinExposure; i <= mMaxExposure; i++) {
            res.add(String.format("%.2f", i * mExposureStep));
        }
        return res;
    }

    public int getEVIndex() {
        if (mCamera == null) {
            return 0;
        }

        Camera.Parameters params = mCamera.getParameters();
        return params.getExposureCompensation() - mMinExposure;
    }

    public void setEV(int index) {
        if (mCamera == null) {
            return;
        }

        Camera.Parameters params = mCamera.getParameters();
        params.setExposureCompensation(mMinExposure + index);
        mCamera.setParameters(params);
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "surface created");

        mSurfaceAvailable = true;
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "surface destroyed");

        mSurfaceAvailable = false;
        stopPreview();
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        Log.d(TAG, "surface changed, format: " + format + " width: " + w + " height: " + h);

        if (!mSurfaceAvailable) {
            return;
        }

        stopPreview();
        setupCamera();
        startPreview();
    }

}
