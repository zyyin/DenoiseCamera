
package easyimage.slrcamera;

import android.app.Activity;
import android.content.Context;

import com.google.analytics.tracking.android.EasyTracker;
import com.google.analytics.tracking.android.MapBuilder;

public class GAHelper {

    private static GAHelper sInst;

    public synchronized static GAHelper getInstance(Context context) {
        if (sInst == null) {
            sInst = new GAHelper(context.getApplicationContext());
        }
        return sInst;
    }

    private EasyTracker mTraker;

    public GAHelper(Context context) {
        mTraker = EasyTracker.getInstance(context);
    }

    public void activityStart(Activity activity) {
        mTraker.activityStart(activity);
    }

    public void activityStop(Activity activity) {
        mTraker.activityStop(activity);
    }

    public void activityPause(Activity activity) {
        mTraker.send(MapBuilder.createEvent("activity_lifecycle", "pause",
                activity.getClass().getCanonicalName(), null).build());
    }

    public void activityResume(Activity activity) {
        mTraker.send(MapBuilder.createEvent("activity_lifecycle", "resume",
                activity.getClass().getCanonicalName(), null).build());
    }

    public void buttonClick(String btn) {
        mTraker.send(MapBuilder.createEvent("ui_action", "button_press", btn, null).build());
    }
}
