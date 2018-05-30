
package easyimage.slrcamera.test;

import android.test.ActivityInstrumentationTestCase2;

import easyimage.slrcamera.MainActivity;

public class MainActivityTest extends ActivityInstrumentationTestCase2<MainActivity> {

    public MainActivityTest() {
        super(MainActivity.class);
    }

    public void testPreconditions() {
        assertNotNull(getActivity());
    }
}
