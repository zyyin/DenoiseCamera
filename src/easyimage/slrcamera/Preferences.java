
package easyimage.slrcamera;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

public class Preferences {

    private static final String PREF_PICTURE_SIZE_WIDTH = "pref_picture_size_width";
    private static final String PREF_PICTURE_SIZE_HEIGHT = "pref_picture_size_height";

    private static Preferences sInst;

    public static Preferences getInstance(Context context) {
        if (sInst == null) {
            sInst = new Preferences(context.getApplicationContext());
        }
        return sInst;
    }

    private SharedPreferences mSharedPreferences;

    private Preferences(Context context) {
        mSharedPreferences = context.getSharedPreferences("slrcamera", Context.MODE_PRIVATE);
    }

    public int getPictureWidth() {
        int width = mSharedPreferences.getInt(PREF_PICTURE_SIZE_WIDTH, 0);
        Log.d("xxx", "get picture width " + width);
        return width;
    }

    public void setPictureWidth(int width) {
        Log.d("xxx", "set picture width " + width);
        mSharedPreferences.edit().putInt(PREF_PICTURE_SIZE_WIDTH, width).commit();
    }

    public int getPictureHeight() {
        return mSharedPreferences.getInt(PREF_PICTURE_SIZE_HEIGHT, 0);
    }

    public void setPictureHeight(int height) {
        mSharedPreferences.edit().putInt(PREF_PICTURE_SIZE_HEIGHT, height).commit();
    }

}
