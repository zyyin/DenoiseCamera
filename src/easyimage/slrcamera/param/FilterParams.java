
package easyimage.slrcamera.param;

import android.media.ExifInterface;

public class FilterParams {

    public String iso;
    public String aperture;
    public String exposure_time;

    public static FilterParams fromExif(ExifInterface exif) {
        FilterParams res = new FilterParams();
        res.iso = exif.getAttribute(ExifInterface.TAG_ISO);
        res.aperture = exif.getAttribute(ExifInterface.TAG_APERTURE);
        res.exposure_time = exif.getAttribute(ExifInterface.TAG_EXPOSURE_TIME);
        return res;
    }

}
