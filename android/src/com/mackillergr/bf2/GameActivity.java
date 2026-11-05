package com.mackillergr.bf2;

import android.os.Bundle;
import android.view.ViewGroup;
import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] {"c++_shared", "SDL3", "dice_py", "dxvk_d3d9", "vkd3d-shader", "main"}; }
    @Override protected String[] getArguments() {
        String[] args = getIntent().getStringArrayExtra("args");
        return args != null ? args : new String[0];
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (getIntent().getBooleanExtra("touch", true)) {
            addContentView(new TouchOverlay(this), new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
    }
}
