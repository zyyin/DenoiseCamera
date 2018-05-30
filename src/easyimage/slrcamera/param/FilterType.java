
package easyimage.slrcamera.param;

import android.content.Context;

import easyimage.slrcamera.R;

public enum FilterType {
    HDR(R.string.filter_hdr),
    NONE(R.string.filter_none),
    GREYSCALE(R.string.filter_greyscale),
    SOFTGLOW(R.string.filter_softglow),
    CROSSSHADER(R.string.filter_crossshader),
    LOMO(R.string.filter_lomo),
    OLDPICTURE(R.string.filter_oldpicture),
    EMBOSS(R.string.filter_emboss),
    SKETCH(R.string.filter_sketch);
    
    private int mNameRes;

    private FilterType(int nameRes) {
        mNameRes = nameRes;
    }

    public String getName(Context context) {
        return context.getString(mNameRes);
    }
}
