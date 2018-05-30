package easyimage.slrcamera;
import android.graphics.Bitmap;

public class ImageProcessX {

static {
        System.loadLibrary("image_processX");
    }

 /* public static native int[] GrayScale(int[] buf, int w, int h);
    public static native int[] SoftGlow(int[] buf, int w, int h);
    public static native int[] CrossShader(int[] buf, int w, int h);
    public static native int[] Lomo(int[] buf, int w, int h);
    public static native int[] OldPicture(int[] buf, int w, int h);
    public static native int[] Emboss(int[] buf, int w, int h);
    public static native int[] Sketch(int[] buf, int w, int h); 
 */
    public static native void HDR(Bitmap  bitmap, int iso, double exp);
}
