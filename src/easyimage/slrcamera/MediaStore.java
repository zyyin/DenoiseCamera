
package easyimage.slrcamera;

import android.content.Context;
import android.net.Uri;

import easyimage.slrcamera.FileStore.Callback;

public class MediaStore {

    private static MediaStore sInst;

    public static synchronized MediaStore getInstance(Context context) {
        if (sInst == null) {
            sInst = new MediaStore(context.getApplicationContext());
        }
        return sInst;
    }

    private FileStore mFileStore;
    private MediaScanner mMediaScanner;

    private MediaStore(Context context) {
        mFileStore = FileStore.getInstance(context);
        mMediaScanner = MediaScanner.getInstance(context);
    }

    public void saveMediaFile(byte[] data, Callback callback) {
        mFileStore.saveMediaFile(data, callback);
    }

    public void scanFile(String path) {
        mMediaScanner.scanFile(path);
    }

    public Uri getLastUri() {
        return mMediaScanner.getLastScannedUri();
    }

}
