#!/usr/bin/env python3
"""Restore the actual test13 TouchOverlay to Stage3's SDLActivity game window.

The ARM64 libmain.so exports TouchOverlay_menuShown and _toggleKeyboard
for menu detection and text entry. SDL3 converts the injected absolute
mouse events to the input expected by Battlefield 2's Flash frontend.
"""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
original = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
src = root / 'gameapp/src/main/java/com/mackillergr/bf2'
activity = src / 'GameActivity.java'
launcher = src / 'LauncherActivity.java'
if not activity.is_file() or not launcher.is_file() or original is None or not original.is_file():
    raise SystemExit('Missing Stage3 GameActivity, LauncherActivity or TouchOverlay.java')

new_overlay = src / 'TouchOverlay.java'
shutil.copyfile(original, new_overlay)
raw = new_overlay.read_text(encoding='utf-8')
for required in ('onNativeMouse(', 'menuShown()', 'toggleKeyboard()', 'menuTouch('):
    if required not in raw:
        raise SystemExit('Original TouchOverlay missing ' + required)

s = activity.read_text(encoding='utf-8')
old = 'import android.util.Log;'
if s.count(old) != 1:
    raise SystemExit('Unexpected GameActivity imports')
s = s.replace(old, old + '\nimport android.view.ViewGroup;', 1)
old = '        // TouchOverlay from test13 is not yet recovered. SDL input is provided by SDLActivity.'
new = '''        // Feed touchscreen taps to SDL3 as PC mouse moves/clicks.
        // menuShown() switches automatically between Flash-menu pointing and FPS controls.
        if (getIntent().getBooleanExtra("touch", true)) {
            TouchOverlay overlay = new TouchOverlay(this);
            addContentView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }'''
if s.count(old) != 1:
    raise SystemExit('GameActivity insertion point missing')
s = s.replace(old, new, 1)
activity.write_text(s, encoding='utf-8')

s = launcher.read_text(encoding='utf-8')
old = 'launch.putExtra("touch", false); // Test13 overlay not yet reconstructed; use SDL input/controller.'
new = 'launch.putExtra("touch", true); // Flash menu mouse taps and FPS touchscreen controls.'
if s.count(old) != 1:
    raise SystemExit('Launcher touch flag insertion point missing')
s = s.replace(old, new, 1)
launcher.write_text(s, encoding='utf-8')
print('Stage3: TouchOverlay enabled, menu touch mapped to SDL mouse events.')
