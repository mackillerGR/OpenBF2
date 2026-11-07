#!/usr/bin/env python3
"""Stage3.5: integrate document-export logging into launcher + in-game overlay."""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1]).resolve()
lib = Path(sys.argv[2]).resolve()
package = root / "gameapp/src/main/java/com/mackillergr/bf2"
launcher = package / "LauncherActivity.java"
game = package / "GameActivity.java"
overlay = package / "TouchOverlay.java"
if not (launcher.is_file() and game.is_file() and overlay.is_file() and lib.is_file()):
    raise SystemExit("Missing Stage3.5 source file(s)")

shutil.copyfile(lib, package / "BF2LogExporter.java")

def replace_exact(text, old, new):
    if text.count(old) != 1:
        raise SystemExit(f"Expected one unique text match, got {text.count(old)}: {old[:90]}")
    return text.replace(old, new, 1)

a = game.read_text(encoding="utf-8")
a = replace_exact(a, "    @Override public void onCreate(Bundle state) {",
'''    public void exportGameLog() {
        BF2LogExporter.request(this);
    }

    @Override protected void onActivityResult(int request, int result, android.content.Intent data) {
        super.onActivityResult(request, result, data);
        BF2LogExporter.handleResult(this, request, result, data);
    }

    @Override public void onCreate(Bundle state) {''')
game.write_text(a, encoding="utf-8")

a = launcher.read_text(encoding="utf-8")
a = replace_exact(a,
    '        button(content, "Έναρξη στο αυθεντικό μενού PC (--menu)")',
    '''        button(content, "Εξαγωγή BF2 log σε αρχείο")
                .setOnClickListener(v -> BF2LogExporter.request(this));
        button(content, "Έναρξη στο αυθεντικό μενού PC (--menu)")''')
a = replace_exact(a,
    '''        super.onActivityResult(request, result, data);
''',
    '''        super.onActivityResult(request, result, data);
        if (BF2LogExporter.handleResult(this, request, result, data)) return;
''')
launcher.write_text(a, encoding="utf-8")

source = overlay.read_text(encoding="utf-8")
for text in ('drawLogButton(c);', 'drawSkipButton(c);', 'onLogButton(', 'exportGameLog()'):
    if text not in source:
        raise SystemExit("Touch overlay missing new control: " + text)
if "if (now && introSkipVisible)" in source:
    raise SystemExit("Regression: intro skip hidden while Flash menu is active")
print("Stage3.5: SKIP INTRO and LOG always visible; exporter works from launcher and game.")
