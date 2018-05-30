
package easyimage.slrcamera;

import android.app.Activity;
import android.content.Intent;
import android.hardware.Camera;
import android.hardware.Camera.AutoFocusCallback;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.ShutterCallback;
import android.media.ExifInterface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.Spinner;
import android.widget.Toast;

import easyimage.slrcamera.FileStore.FileStoreException;
import easyimage.slrcamera.FilterManager.FilterCallback;
import easyimage.slrcamera.FilterManager.FilterException;
import easyimage.slrcamera.param.FilterParams;
import easyimage.slrcamera.param.FilterType;
import easyimage.slrcamera.param.ISOType;

import java.io.IOException;

public class MainActivity extends Activity implements OnClickListener, OnItemSelectedListener {

    private static final String TAG = MainActivity.class.getCanonicalName();

    private static final long AUTO_FOCUS_RETRY_INTERVAL = 1000L;

    private static String BTN_CAPTURE = "capture_button";
    private static String BTN_GALLERY = "gallery_button";

    private CameraPreview mPreview;
    private Spinner mFilterSpinner, mPictureSizeSpinner, mISOSpinner, mExposureSpinner;
    private Button mCaptureButton;

    private MediaStore mMediaStore;
    private FilterManager mFilterManager;
    private Handler mHandler = new Handler();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        setContentView(R.layout.activity_main);

        mMediaStore = MediaStore.getInstance(this);
        mFilterManager = new FilterManager();
        mFilterManager.setFilterType(FilterType.HDR);

        // UI
        mPreview = new CameraPreview(this);
        FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview);
        preview.addView(mPreview);

        mFilterSpinner = (Spinner) findViewById(R.id.filter_name);
        mFilterSpinner.setOnItemSelectedListener(this);
        mFilterSpinner.setSelection(0);

        mPictureSizeSpinner = (Spinner) findViewById(R.id.picture_size);
        mPictureSizeSpinner.setOnItemSelectedListener(this);

        mISOSpinner = (Spinner) findViewById(R.id.iso);
        mISOSpinner.setOnItemSelectedListener(this);
        mISOSpinner.setSelection(0);

        mExposureSpinner = (Spinner) findViewById(R.id.exposure);
        mExposureSpinner.setOnItemSelectedListener(this);

        // Events
        mCaptureButton = (Button) findViewById(R.id.button_capture);
        mCaptureButton.setOnClickListener(this);
        findViewById(R.id.button_gallery).setOnClickListener(this);
    }

    @Override
    protected void onStart() {
        super.onStart();
        GAHelper.getInstance(this).activityStart(this);
    }

    @Override
    protected void onStop() {
        super.onStop();
        GAHelper.getInstance(this).activityStop(this);
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.button_capture:
                GAHelper.getInstance(this).buttonClick(BTN_CAPTURE);

                takePicture();
                break;
            case R.id.button_gallery:
                GAHelper.getInstance(this).buttonClick(BTN_GALLERY);

                showGallery(mMediaStore.getLastUri());
                break;
        }
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
        if (parent == mFilterSpinner) {
            // TestFlight.passCheckpoint("select filter");

            selectFilter(pos);
        } else if (parent == mISOSpinner) {
            // TestFlight.passCheckpoint("select iso");

            selectISO(pos);
        } else if (parent == mExposureSpinner) {
            // TestFlight.passCheckpoint("select exposure");

            selectExposure(pos);
        } else if (parent == mPictureSizeSpinner) {
            // TestFlight.passCheckpoint("select picture size");

            selectPictureSize(pos);
        }
    }

    private void selectFilter(int pos) {
        for (FilterType filterType : FilterType.values()) {
            if (filterType.ordinal() == pos) {
                mFilterManager.setFilterType(filterType);
            }
        }
    }

    private void selectISO(int pos) {
        for (ISOType isoType : ISOType.values()) {
            if (isoType.ordinal() == pos) {
                mPreview.setISO(isoType);
            }
        }
    }

    private void selectExposure(int pos) {
        mPreview.setEV(pos);
    }

    private void selectPictureSize(int pos) {
        mPreview.setPictureSize(pos);
    }

    private void setupPictureSizeSpinner() {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, mPreview.getPictureSizes());
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mPictureSizeSpinner.setAdapter(adapter);
        mPictureSizeSpinner.setSelection(mPreview.getPictureSizeIndex());
    }

    private void setupExposureSpinner() {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, mPreview.getEV());
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mExposureSpinner.setAdapter(adapter);
        mExposureSpinner.setSelection(mPreview.getEVIndex());
    }

    @Override
    public void onNothingSelected(AdapterView<?> arg0) {
    }

    @Override
    protected void onResume() {
        super.onResume();
        mPreview.onResume();

        setupPictureSizeSpinner();
        setupExposureSpinner();

        GAHelper.getInstance(this).activityResume(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        mPreview.onPause();

        GAHelper.getInstance(this).activityResume(this);
    }

    private void takePicture() {
        mCaptureButton.setEnabled(false);
        mAutoFocusRunnable.run();
    }

    private void showGallery(Uri uri) {
        Intent intent = new Intent();
        intent.setType("image/*");
        intent.setAction(Intent.ACTION_VIEW);
        if (uri != null) {
            intent.setData(uri);
        }
        startActivity(intent);
    }

    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    private AutoFocusCallback mAutoFocusCallback = new AutoFocusCallback() {
        @Override
        public void onAutoFocus(boolean success, Camera camera) {
            if (success) {
                SoundPlayer.getInstance(getApplicationContext()).playFocusSound();
                camera.takePicture(mShutterCallback, null, mTakePictureCallback);
                camera.cancelAutoFocus();
            } else {
                mHandler.postDelayed(mAutoFocusRunnable, AUTO_FOCUS_RETRY_INTERVAL);
            }
        }
    };

    private Runnable mAutoFocusRunnable = new Runnable() {
        @Override
        public void run() {
            Camera camera = mPreview.getCamera();
            if (camera != null) {
                camera.autoFocus(mAutoFocusCallback);
            }
        }
    };

    private ShutterCallback mShutterCallback = new ShutterCallback() {
        @Override
        public void onShutter() {
            SoundPlayer.getInstance(getApplicationContext()).playShutterSound();
        }
    };

    private PictureCallback mTakePictureCallback = new PictureCallback() {
        @Override
        public void onPictureTaken(byte[] data, Camera camera) {
            mCaptureButton.setEnabled(true);
            mMediaStore.saveMediaFile(data, mSavePictureCallback);
        }
    };

    private FilterCallback mFilterCallback = new FilterCallback() {

        @Override
        public void onSuccess(byte[] data) {
            mMediaStore.saveMediaFile(data, mSaveFilteredPictureCallback);
        }

        @Override
        public void onError(FilterException e) {
            showToast("Error: " + e.getMessage());
        }
    };

    private FileStore.Callback mSavePictureCallback = new FileStore.Callback() {
        @Override
        public void onSuccess(byte[] data, String path) {
            showToast("Saved to " + path);
            mMediaStore.scanFile(path);
            FilterParams params = new FilterParams();
            try {
                params = FilterParams.fromExif(new ExifInterface(path));
            } catch (IOException e) {
                Log.d(TAG, e.getMessage());
            }
            mFilterManager.filter(data, mFilterCallback, params);
        }

        @Override
        public void onError(FileStoreException e) {
            showToast("Error: " + e.getMessage());
        }
    };

    private FileStore.Callback mSaveFilteredPictureCallback = new FileStore.Callback() {
        @Override
        public void onSuccess(byte[] data, String path) {
            showToast("Saved to " + path);
            mMediaStore.scanFile(path);
        }

        @Override
        public void onError(FileStoreException e) {
            showToast("Error: " + e.getMessage());
        }
    };

}
