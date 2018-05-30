package easyimage.slrcamera.param;

import android.content.Context;

import easyimage.slrcamera.R;

public enum ISOType {
    AUTO("auto", R.string.iso_auto),
    ISO_100("100", R.string.iso_100),
    ISO_200("200", R.string.iso_200),
    ISO_400("400", R.string.iso_400),
    ISO_800("800", R.string.iso_800),
    ISO_1600("1600", R.string.iso_1600);

    private String mValue;
    private int mNameRes;

    private ISOType(String value, int nameRes) {
        mValue = value;
        mNameRes = nameRes;
    }

    public String getValue() {
        return mValue;
    }

    public String getName(Context context) {
        return context.getString(mNameRes);
    }
}
