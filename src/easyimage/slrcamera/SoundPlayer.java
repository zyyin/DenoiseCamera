
package easyimage.slrcamera;

import android.content.Context;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.Uri;

public class SoundPlayer {

    private static final String SHUTTER_SOUND = "file:///system/media/audio/ui/camera_click.ogg";
    private static final String FOCUS_SOUND = "file:///system/media/audio/ui/camera_focus.ogg";

    private static SoundPlayer sInst;

    public static synchronized SoundPlayer getInstance(Context context) {
        if (sInst == null) {
            sInst = new SoundPlayer(context.getApplicationContext());
        }
        return sInst;
    }

    private Context mContext;
    private AudioManager mAudioManager;
    private MediaPlayer mShutterMP, mFocusMP;

    private SoundPlayer(Context context) {
        mContext = context;
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        mShutterMP = MediaPlayer.create(mContext, Uri.parse(SHUTTER_SOUND));
        mFocusMP = MediaPlayer.create(mContext, Uri.parse(FOCUS_SOUND));
    }

    private void play(MediaPlayer player) {
        if (mAudioManager.getStreamVolume(AudioManager.STREAM_NOTIFICATION) == 0) {
            return;
        }
        if (player != null) {
            player.start();
        }
    }

    public void playShutterSound() {
        play(mShutterMP);
    }

    public void playFocusSound() {
        play(mFocusMP);
    }

}
