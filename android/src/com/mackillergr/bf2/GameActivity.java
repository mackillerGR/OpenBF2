package com.mackillergr.bf2;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.view.ViewGroup;
import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] {"c++_shared", "SDL3", "dice_py", "dxvk_d3d9", "vkd3d-shader", "main"}; }
    @Override protected String[] getArguments() {
        String[] args = getIntent().getStringArrayExtra("args");
        return args != null ? args : new String[0];
    }
    /** Wi-Fi drops broadcast packets without this, which hides LAN servers in both directions. */
    private WifiManager.MulticastLock lanLock;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wifi != null) {
            lanLock = wifi.createMulticastLock("bf2-lan");
            lanLock.setReferenceCounted(false);
            lanLock.acquire();
        }
        if (getIntent().getBooleanExtra("touch", true)) {
            addContentView(new TouchOverlay(this), new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
    }

    @Override protected void onDestroy() {
        if (lanLock != null) lanLock.release();
        super.onDestroy();
    }
}
