
package easyimage.slrcamera;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class FileStore {

    private static final String TAG = FileStore.class.getCanonicalName();

    public static class FileStoreException extends Exception {
        private static final long serialVersionUID = -320930445627116673L;

        public FileStoreException(String message) {
            super(message);
        }
    }

    public interface Callback {
        void onSuccess(byte[] data, String path);

        void onError(FileStoreException e);
    }

    private static FileStore sInst;

    public static synchronized FileStore getInstance(Context context) {
        if (sInst == null) {
            sInst = new FileStore(context.getApplicationContext());
        }
        return sInst;
    }

    private Context mContext;

    private FileStore(Context context) {
        mContext = context;
    }

    private File getOutputMediaFile() throws FileStoreException {
        String storageState = Environment.getExternalStorageState();
        if (!Environment.MEDIA_MOUNTED.equals(storageState)) {
            throw new FileStoreException("storage not ready");
        }

        File mediaStorageDir = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES),
                mContext.getString(R.string.app_name));

        if (!mediaStorageDir.exists() && !mediaStorageDir.mkdirs()) {
            throw new FileStoreException("failed to create directory");
        }

        String timeStamp = new SimpleDateFormat("yyyyMMdd_HHmmss_SSS", Locale.US)
                .format(new Date());
        String fileName = String.format("%s%sIMG_%s.jpg", mediaStorageDir.getAbsolutePath(),
                File.separator, timeStamp);
        Log.d(TAG, "saving to " + fileName);

        return new File(fileName);
    }

    private String save(byte[] data) throws FileStoreException {
        File mediaFile = getOutputMediaFile();
        try {
            FileOutputStream fos = new FileOutputStream(mediaFile);
            try {
                fos.write(data);
            } finally {
                fos.close();
            }
        } catch (FileNotFoundException e) {
            throw new FileStoreException("cannot open file for writing");
        } catch (IOException e) {
            throw new FileStoreException("cannot write to file");
        }
        return mediaFile.getAbsolutePath();
    }

    public void saveMediaFile(byte[] data, Callback callback) {
        new SaveFileTask(data, callback).execute();
    }

    private class SaveFileTask extends AsyncTask<Void, Void, String> {

        private byte[] mData;
        private FileStoreException mError;
        private Callback mCallback;

        public SaveFileTask(byte[] data, Callback callback) {
            mData = data;
            mCallback = callback;
        }

        @Override
        protected String doInBackground(Void... args) {
            try {
                return save(mData);
            } catch (FileStoreException e) {
                mError = e;
            }
            return null;
        }

        @Override
        protected void onPostExecute(String result) {
            if (result == null) {
                mCallback.onError(mError);
            } else {
                mCallback.onSuccess(mData, result);
            }
        }

    }

}
