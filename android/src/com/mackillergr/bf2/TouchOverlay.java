package com.mackillergr.bf2;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import org.libsdl.app.SDLActivity;
import java.util.HashMap;

/** On-screen controls. They feed SDL the same keys and mouse a PC player uses. */
public class TouchOverlay extends View {
    private static final int MOUSE_DOWN = 0, MOUSE_UP = 1, MOUSE_MOVE = 2, LEFT_BUTTON = 1;

    private static class Button {
        final String label; final int key; final float rx, ry; int pointer = -1;
        Button(String label, int key, float rx, float ry) { this.label = label; this.key = key; this.rx = rx; this.ry = ry; }
    }
    private final Button[] buttons = {
        new Button("ΠΥΡ", 0, 0.90f, 0.62f),
        new Button("ΑΛΜΑ", KeyEvent.KEYCODE_SPACE, 0.80f, 0.82f),
        new Button("ΣΚΥΨΕ", KeyEvent.KEYCODE_CTRL_LEFT, 0.90f, 0.86f),
        new Button("ΤΡΕΞΕ", KeyEvent.KEYCODE_SHIFT_LEFT, 0.70f, 0.86f),
        new Button("ΓΕΜΙΣΕ", KeyEvent.KEYCODE_R, 0.80f, 0.60f),
        new Button("ΧΡΗΣΗ", KeyEvent.KEYCODE_E, 0.70f, 0.66f),
        new Button("ΣΚΟΡ", KeyEvent.KEYCODE_TAB, 0.95f, 0.10f),
        new Button("ΟΚ", KeyEvent.KEYCODE_ENTER, 0.86f, 0.10f),
        new Button("ΜΕΝΟΥ", KeyEvent.KEYCODE_ESCAPE, 0.05f, 0.10f),
    };
    private final HashMap<Integer, float[]> look = new HashMap<>();
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG), text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int stickPointer = -1;
    private float stickX, stickY, knobX, knobY;
    private boolean w, a, s, d;
    private boolean menu;
    // The intro movie uses the keyboard's DIK_ESCAPE (1) to advance skippable clips.
    // Hide this overlay once the native Flash main menu has appeared.
    private boolean introSkipVisible = true;
    private int introSkipPointer = -1;
    private static native boolean menuShown();
    private static native void toggleKeyboard();
    private final Runnable watch = new Runnable() {
        @Override public void run() {
            boolean now = menu;
            try { now = menuShown(); } catch (UnsatisfiedLinkError notLoaded) { }
            if (now && introSkipVisible) { introSkipVisible = false; invalidate(); }
            if (now != menu) { releaseAll(); menu = now; invalidate(); }
            postDelayed(this, 250);
        }
    };

    public TouchOverlay(Context context) {
        super(context);
        text.setColor(Color.WHITE); text.setTextAlign(Paint.Align.CENTER); text.setFakeBoldText(true);
        post(watch);
    }
    private void releaseAll() {
        for (Button b : buttons) if (b.pointer >= 0) { b.pointer = -1; press(b, false); }
        if (introSkipPointer != -1) {
            introSkipPointer = -1;
            key(KeyEvent.KEYCODE_ESCAPE, false);
        }
        stickPointer = -1; setMove(0, 0); look.clear();
    }
    private float keyboardX() { return getWidth() * 0.04f; }
    private float keyboardY() { return getHeight() * 0.92f; }
    private void menuTouch(MotionEvent e) {
        float x = e.getX(e.getActionIndex()), y = e.getY(e.getActionIndex());
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                if (Math.hypot(x - keyboardX(), y - keyboardY()) < radius() * 1.1f) { toggleKeyboard(); return; }
                SDLActivity.onNativeMouse(0, MOUSE_MOVE, x, y, false);
                SDLActivity.onNativeMouse(LEFT_BUTTON, MOUSE_DOWN, x, y, false);
                menuPressed = true; break;
            case MotionEvent.ACTION_MOVE:
                if (menuPressed) SDLActivity.onNativeMouse(LEFT_BUTTON, MOUSE_MOVE, x, y, false);
                break;
            case MotionEvent.ACTION_UP: case MotionEvent.ACTION_CANCEL:
                if (menuPressed) SDLActivity.onNativeMouse(0, MOUSE_UP, x, y, false);
                menuPressed = false; break;
            default: break;
        }
    }
    private boolean menuPressed;
    private float skipCenterX() { return getWidth() * 0.50f; }
    private float skipCenterY() { return getHeight() * 0.12f; }
    private float skipWidth() { return Math.max(160f, Math.min(260f, getWidth() * 0.22f)); }
    private float skipHeight() { return Math.max(40f, Math.min(64f, getHeight() * 0.09f)); }
    private boolean onSkipButton(float x, float y) {
        return Math.abs(x - skipCenterX()) < skipWidth() * 0.5f
                && Math.abs(y - skipCenterY()) < skipHeight() * 0.5f;
    }
    private void drawSkipButton(Canvas canvas) {
        float x = skipCenterX(), y = skipCenterY();
        float w = skipWidth(), h = skipHeight();
        fill.setColor(Color.argb(introSkipPointer >= 0 ? 245 : 212, 24, 25, 24));
        canvas.drawRoundRect(x - w * 0.5f, y - h * 0.5f,
                x + w * 0.5f, y + h * 0.5f, h * 0.19f, h * 0.19f, fill);
        fill.setColor(Color.rgb(255, 200, 0));
        fill.setStyle(Paint.Style.STROKE);
        fill.setStrokeWidth(Math.max(2f, h * 0.035f));
        canvas.drawRoundRect(x - w * 0.5f, y - h * 0.5f,
                x + w * 0.5f, y + h * 0.5f, h * 0.19f, h * 0.19f, fill);
        fill.setStyle(Paint.Style.FILL);
        text.setTextSize(h * 0.36f);
        canvas.drawText("SKIP INTRO", x, y + h * 0.13f, text);
    }
    private float radius() { return getHeight() * 0.075f; }
    private float stickRadius() { return getHeight() * 0.16f; }
    private float baseX() { return getWidth() * 0.14f; }
    private float baseY() { return getHeight() * 0.72f; }

    @Override protected void onDraw(Canvas c) {
        if (menu) {
            fill.setColor(Color.argb(80, 255, 255, 255));
            c.drawCircle(keyboardX(), keyboardY(), radius(), fill);
            text.setTextSize(radius() * 0.42f);
            c.drawText("ABC", keyboardX(), keyboardY() + radius() * 0.15f, text);
            return;
        }
        fill.setColor(Color.argb(60, 255, 255, 255));
        c.drawCircle(baseX(), baseY(), stickRadius(), fill);
        fill.setColor(Color.argb(110, 255, 255, 255));
        c.drawCircle(stickPointer >= 0 ? knobX : baseX(), stickPointer >= 0 ? knobY : baseY(), radius() * 0.8f, fill);
        text.setTextSize(radius() * 0.42f);
        for (Button b : buttons) {
            float x = b.rx * getWidth(), y = b.ry * getHeight();
            fill.setColor(b.pointer >= 0 ? Color.argb(150, 255, 170, 40) : Color.argb(80, 255, 255, 255));
            c.drawCircle(x, y, radius(), fill);
            c.drawText(b.label, x, y + radius() * 0.15f, text);
        }
        if (introSkipVisible) drawSkipButton(c);
    }
    private void key(int code, boolean down) {
        if (down) SDLActivity.onNativeKeyDown(code); else SDLActivity.onNativeKeyUp(code);
    }
    private void press(Button b, boolean down) {
        // SDL takes the button state after the event, so a release carries 0.
        if (b.key == 0) SDLActivity.onNativeMouse(down ? LEFT_BUTTON : 0, down ? MOUSE_DOWN : MOUSE_UP, 0, 0, true);
        else key(b.key, down);
    }
    private void setMove(float x, float y) {
        boolean nw = y < -0.35f, ns = y > 0.35f, na = x < -0.35f, nd = x > 0.35f;
        if (nw != w) key(KeyEvent.KEYCODE_W, w = nw);
        if (ns != s) key(KeyEvent.KEYCODE_S, s = ns);
        if (na != a) key(KeyEvent.KEYCODE_A, a = na);
        if (nd != d) key(KeyEvent.KEYCODE_D, d = nd);
    }
    private Button hit(float x, float y) {
        for (Button b : buttons) {
            if (Math.hypot(x - b.rx * getWidth(), y - b.ry * getHeight()) < radius() * 1.1f) return b;
        }
        return null;
    }
    private void began(int id, float x, float y) {
        Button b = hit(x, y);
        if (b != null) { b.pointer = id; press(b, true); return; }
        if (stickPointer < 0 && Math.hypot(x - baseX(), y - baseY()) < stickRadius() * 1.5f) {
            stickPointer = id; stickX = baseX(); stickY = baseY(); moved(id, x, y); return;
        }
        look.put(id, new float[] {x, y, x, y, System.currentTimeMillis()});
    }
    private void moved(int id, float x, float y) {
        if (id == stickPointer) {
            float dx = x - stickX, dy = y - stickY, r = stickRadius(), len = (float) Math.hypot(dx, dy);
            if (len > r) { dx *= r / len; dy *= r / len; }
            knobX = stickX + dx; knobY = stickY + dy;
            setMove(dx / r, dy / r);
            return;
        }
        float[] p = look.get(id);
        if (p != null) {
            SDLActivity.onNativeMouse(0, MOUSE_MOVE, x - p[0], y - p[1], true);
            p[0] = x; p[1] = y;
        }
    }
    private void ended(int id) {
        for (Button b : buttons) if (b.pointer == id) { b.pointer = -1; press(b, false); }
        if (id == stickPointer) { stickPointer = -1; setMove(0, 0); }
        float[] p = look.remove(id);
        // A quick tap is a click on whatever is under it: the spawn screen, the HUD.
        if (p != null && System.currentTimeMillis() - p[4] < 250 && Math.hypot(p[0] - p[2], p[1] - p[3]) < 24) {
            SDLActivity.onNativeMouse(0, MOUSE_MOVE, p[2], p[3], false);
            SDLActivity.onNativeMouse(LEFT_BUTTON, MOUSE_DOWN, p[2], p[3], false);
            SDLActivity.onNativeMouse(0, MOUSE_UP, p[2], p[3], false);
        }
    }
    @Override public boolean onTouchEvent(MotionEvent e) {
        int action = e.getActionMasked(), index = e.getActionIndex();
        // Handle the introduction independently of menu/gameplay pointer tracking.
        // Hold Escape until finger-up so the native input polling detects a press.
        if (introSkipVisible && !menu) {
            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
                int id = e.getPointerId(index);
                if (introSkipPointer == -1 && onSkipButton(e.getX(index), e.getY(index))) {
                    introSkipPointer = id;
                    key(KeyEvent.KEYCODE_ESCAPE, true);
                    invalidate();
                    return true;
                }
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
                if (e.getPointerId(index) == introSkipPointer) {
                    introSkipPointer = -1;
                    key(KeyEvent.KEYCODE_ESCAPE, false);
                    invalidate();
                    return true;
                }
            } else if (action == MotionEvent.ACTION_CANCEL && introSkipPointer != -1) {
                introSkipPointer = -1;
                key(KeyEvent.KEYCODE_ESCAPE, false);
                invalidate();
                return true;
            }
        }
        if (menu) { menuTouch(e); return true; }
        switch (action) {
            case MotionEvent.ACTION_DOWN: case MotionEvent.ACTION_POINTER_DOWN:
                began(e.getPointerId(index), e.getX(index), e.getY(index)); break;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < e.getPointerCount(); ++i) moved(e.getPointerId(i), e.getX(i), e.getY(i));
                break;
            case MotionEvent.ACTION_UP: case MotionEvent.ACTION_POINTER_UP:
                ended(e.getPointerId(index)); break;
            case MotionEvent.ACTION_CANCEL:
                for (int i = 0; i < e.getPointerCount(); ++i) ended(e.getPointerId(i));
                break;
            default: break;
        }
        invalidate();
        return true;
    }
}
