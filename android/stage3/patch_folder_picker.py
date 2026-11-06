#!/usr/bin/env python3
"""Patch the extracted Stage3 package, leaving the original uploaded ZIP unchanged."""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
launcher = root / "gameapp/src/main/java/com/mackillergr/bf2/LauncherActivity.java"
helper = Path(__file__).with_name("GameDirectoryPicker.java")
if not launcher.is_file() or not helper.is_file():
    raise SystemExit("Stage3 launcher or SAF helper source missing")
s = launcher.read_text(encoding="utf-8")
edits = [
    ("    private TextView status;\n",
     "    private TextView status;\n    private GameDirectoryPicker folderPicker;\n"),
    ('        status = label(content, "", 14);\n',
     '        status = label(content, "", 14);\n'
     '        folderPicker = new GameDirectoryPicker(this, new GameDirectoryPicker.Listener() {\n'
     '            @Override public void onStatus(String text) { status.setText(text); }\n'
     '            @Override public void onImported(String path) {\n'
     '                modPath.setText(path);\n'
     '                remember();\n'
     '            }\n'
     '        });\n'
     '        button(content, "Επιλογή φακέλου παιχνιδιού").setOnClickListener(v -> folderPicker.pick());\n'),
    ("        super.onActivityResult(request, result, data);\n",
     "        super.onActivityResult(request, result, data);\n"
     "        if (request == GameDirectoryPicker.REQUEST_CODE) {\n"
     "            folderPicker.onResult(result, data);\n"
     "            return;\n"
     "        }\n")
]
for old, new in edits:
    if s.count(old) != 1:
        raise SystemExit("Expected exactly one occurrence of: " + repr(old))
    s = s.replace(old, new, 1)
launcher.write_text(s, encoding="utf-8")
shutil.copyfile(helper, launcher.with_name("GameDirectoryPicker.java"))
print("SAF folder selection and background import integrated")
