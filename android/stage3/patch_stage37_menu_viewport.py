#!/usr/bin/env python3
"""Stage 3.7 menu viewport + Android touch fix.

The previous Stage3 launcher hardcoded 1280x720 on a 2340x1080 Samsung.
This version sends physical landscape screen dimensions to the native DXVK
renderer, installs geometry-aware mouse events, and writes touch traces that
BF2LogExporter can capture from the game process. No shader or engine code
is falsely claimed to be corrected here.
"""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1]).resolve()
mapper = Path(sys.argv[2]).resolve()
src = root / "gameapp/src/main/java/com/mackillergr/bf2"
launcher = src / "LauncherActivity.java"
game = src / "GameActivity.java"
overlay = src / "TouchOverlay.java"
for f in (launcher, game, overlay, mapper):
    if not f.is_file():
        raise SystemExit("Missing required source: " + str(f))

def replace_once(value, target, replacement):
    n = value.count(target)
    if n != 1:
        raise SystemExit("Expected one match, saw %d for %r" % (n, target[:110]))
    return value.replace(target, replacement, 1)

shutil.copyfile(mapper, src / "MenuPointerMapper.java")

l = launcher.read_text(encoding="utf-8")
l = replace_once(l, "    private void startGame(boolean menu) {", """    /**
     * Display.getRealSize includes the full landscape physical screen, including
     * areas hidden behind system bars. Native BF2 uses this as its D3D9 viewport.
     */
    private int[] actualScreenResolution() {
        android.graphics.Point real = new android.graphics.Point();
        getWindowManager().getDefaultDisplay().getRealSize(real);
        int width = Math.max(real.x, real.y);
        int height = Math.min(real.x, real.y);
        if (width < 640 || height < 480) {
            android.util.DisplayMetrics dm = getResources().getDisplayMetrics();
            width = Math.max(dm.widthPixels, dm.heightPixels);
            height = Math.min(dm.widthPixels, dm.heightPixels);
        }
        return new int[]{width, height};
    }

    private void startGame(boolean menu) {""")
l = replace_once(l, "        String[] args;\n        try {",
"""        final int[] gamePixels = actualScreenResolution();
        status.setText("Ανάλυση παιχνιδιού: " + gamePixels[0] + "×" + gamePixels[1]);
        android.util.Log.i("BF2MenuTouch", "Requested native window: "
                + gamePixels[0] + "x" + gamePixels[1]);
        String[] args;
        try {""")
if l.count("1280, 720, name, d") != 2:
    raise SystemExit("Could not find both hardcoded menu and LAN resolutions")
l = l.replace("1280, 720, name, d", "gamePixels[0], gamePixels[1], name, d")
l = replace_once(l, '        launch.putExtra("touch", true);',
    '        launch.putExtra("gameWidth", gamePixels[0]);\n'
    '        launch.putExtra("gameHeight", gamePixels[1]);\n'
    '        launch.putExtra("touch", true);')
launcher.write_text(l,encoding="utf-8")

g = game.read_text(encoding="utf-8")
g = replace_once(g, "            TouchOverlay overlay = new TouchOverlay(this);",
"""            TouchOverlay overlay = new TouchOverlay(this);
            int gameWidth = getIntent().getIntExtra("gameWidth", 1280);
            int gameHeight = getIntent().getIntExtra("gameHeight", 720);
            overlay.setGameResolution(gameWidth, gameHeight);
            Log.i("BF2MenuTouch", "gameWidth=" + gameWidth + " gameHeight=" + gameHeight);""")
game.write_text(g,encoding="utf-8")

a = overlay.read_text(encoding="utf-8")
a = replace_once(a, "        logPointer = -1;\n        stickPointer = -1; setMove(0, 0); look.clear();",
"""        logPointer = -1;
        if (menuPressed) SDLActivity.onNativeMouse(0, MOUSE_UP, 0, 0, false);
        menuPressed = false;
        menuPointer = -1;
        stickPointer = -1; setMove(0, 0); look.clear();""")
a = replace_once(a, "    private boolean menu;\n",
"""    private boolean menu;
    private int gameWidth = 1280;
    private int gameHeight = 720;
    private float lastTapX = -1, lastTapY = -1;
    public void setGameResolution(int width, int height) {
        if (width > 0 && height > 0) {
            gameWidth = width;
            gameHeight = height;
        }
        invalidate();
    }
    private float[] touchToSdl(float x, float y) {
        return MenuPointerMapper.map(x, y, getWidth(), getHeight(), gameWidth, gameHeight);
    }
    private void traceTouch(String phase, float rawX, float rawY, float[] converted) {
        android.util.Log.i("BF2MenuTouch", phase + " raw=(" + rawX + "," + rawY
                + ") sdl=(" + converted[0] + "," + converted[1]
                + ") overlay=" + getWidth() + "x" + getHeight()
                + " game=" + gameWidth + "x" + gameHeight);
    }
""")
start = a.index("    private void menuTouch(MotionEvent e) {")
end = a.index("    private boolean menuPressed;", start)
new_menu = """    private int menuPointer = -1;
    private void menuTouch(MotionEvent e) {
        final int action = e.getActionMasked();
        final int index = e.getActionIndex();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int id = e.getPointerId(index);
            if (menuPointer != -1) return;
            float x = e.getX(index), y = e.getY(index);
            if (Math.hypot(x - keyboardX(), y - keyboardY()) < radius() * 1.1f) {
                toggleKeyboard();
                return;
            }
            float[] p = touchToSdl(x, y);
            traceTouch("DOWN", x, y, p);
            menuPointer = id;
            lastTapX = x; lastTapY = y;
            SDLActivity.onNativeMouse(0, MOUSE_MOVE, p[0], p[1], false);
            SDLActivity.onNativeMouse(LEFT_BUTTON, MOUSE_DOWN, p[0], p[1], false);
            menuPressed = true;
            invalidate();
        } else if (action == MotionEvent.ACTION_MOVE) {
            if (!menuPressed) return;
            int idx = e.findPointerIndex(menuPointer);
            if (idx < 0) return;
            float x = e.getX(idx), y = e.getY(idx);
            float[] p = touchToSdl(x, y);
            lastTapX = x; lastTapY = y;
            SDLActivity.onNativeMouse(0, MOUSE_MOVE, p[0], p[1], false);
            invalidate();
        } else if (action == MotionEvent.ACTION_UP ||
                   action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_CANCEL) {
            if (!menuPressed) return;
            if (action != MotionEvent.ACTION_CANCEL && e.getPointerId(index) != menuPointer) return;
            int idx = action == MotionEvent.ACTION_CANCEL ? Math.max(0,e.findPointerIndex(menuPointer)) : index;
            float x = e.getX(idx), y = e.getY(idx);
            float[] p = touchToSdl(x, y);
            traceTouch("UP", x, y, p);
            SDLActivity.onNativeMouse(0, MOUSE_MOVE, p[0], p[1], false);
            SDLActivity.onNativeMouse(0, MOUSE_UP, p[0], p[1], false);
            menuPointer = -1;
            menuPressed = false;
            invalidate();
        }
    }
"""
a = a[:start] + new_menu + a[end:]
a = replace_once(a, "            drawLogButton(c);\n            return;",
"""            drawLogButton(c);
            if (lastTapX >= 0) {
                fill.setColor(Color.argb(175, 255, 200, 0));
                c.drawCircle(lastTapX, lastTapY, Math.max(5f, radius()*0.09f), fill);
            }
            return;""")
overlay.write_text(a,encoding="utf-8")

assert "1280, 720, name, d" not in l
assert "MenuPointerMapper.map" in a
assert 'Log.i("BF2MenuTouch"' in a or 'android.util.Log.i("BF2MenuTouch"' in a
print("Stage 3.7: physical-display renderer size, calibrated touch, and input trace inserted.")
