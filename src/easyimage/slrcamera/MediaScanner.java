
package easyimage.slrcamera;

import android.content.Context;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.Uri;
import android.util.Log;

public class MediaScanner {

    private static final String TAG = MediaScanner.class.getCanonicalName();

    private static MediaScanner sInst;

    public static synchronized MediaScanner getInstance(Context context) {
        if (sInst == null) {
            sInst = new MediaScanner(context.getApplicationContext());
        }
        return sInst;
    }

    private Context mContext;
    private MediaScannerConnection mConnection;
    private Uri mLastUri;

    private MediaScanner(Context context) {
        mContext = context;
    }

    public void scanFile(final String path) {
        mConnection = new MediaScannerConnection(mContext, new MediaScannerConnectionClient() {
            @Override
            public void onMediaScannerConnected() {
                Log.d(TAG, "scanning " + path);
                try {
                    mConnection.scanFile(path, null);
                } catch (Exception e) {
                    Log.e(TAG, e.getMessage());
                }
            }

            @Override
            public void onScanCompleted(String path, Uri uri) {
                Log.d(TAG, "scan complete for " + path + " uri: " + uri);
                mLastUri = uri;
                mConnection.disconnect();
            }
        });
        mConnection.connect();
    }

    public Uri getLastScannedUri() {
        return mLastUri;
    }

}
