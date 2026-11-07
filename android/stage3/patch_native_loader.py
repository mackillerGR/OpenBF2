#!/usr/bin/env python3
"""Stage 3.4: lazy-load native dependencies and persist the exact SDL linker error.

The uploaded test13 ARM64 libraries are already packaged. Loading optional DXVK
and vkd3d-shader eagerly via System.loadLibrary is unnecessary: libmain.so has
DT_NEEDED entries for libdice_py and libSDL3, while the renderer loads later.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
src = root / "gameapp/src/main/java/com/mackillergr/bf2"
game = src / "GameActivity.java"
launcher = src / "LauncherActivity.java"
if not game.is_file() or not launcher.is_file():
    raise SystemExit("Stage3 GameActivity and LauncherActivity not found")

def replace_exact(content, old, new):
    if content.count(old) != 1:
        raise SystemExit(f"Expected one replacement site: {old[:100]!r}; found {content.count(old)}")
    return content.replace(old, new)

s = game.read_text(encoding="utf-8")
s = replace_exact(s,
    'return new String[] {"c++_shared", "SDL3", "dice_py", "dxvk_d3d9", "vkd3d-shader", "main"};',
    'return new String[] {"SDL3", "main"}; // Native linker resolves transitive dependencies.')
s = replace_exact(s,
    "import android.util.Log;",
    "import android.util.Log;\nimport java.io.File;\nimport java.io.FileOutputStream;\nimport java.nio.charset.StandardCharsets;")
needle = "    @Override protected String[] getArguments() {"
if s.count(needle) != 1:
    raise SystemExit("GameActivity arguments insertion point missing")
s = s.replace(needle, '''    @Override public void loadLibraries() {
        File folder = getExternalFilesDir(null);
        File errorFile = folder == null ? null : new File(folder, "native_loader_failure.txt");
        if (errorFile != null && errorFile.exists()) errorFile.delete();
        try {
            for (String name : getLibraries()) {
                File file = new File(getApplicationInfo().nativeLibraryDir, "lib" + name + ".so");
                Log.i(TAG, "Native library " + name + ": present=" + file.isFile()
                        + " bytes=" + (file.isFile() ? file.length() : -1));
            }
            super.loadLibraries();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Native library load failed", e);
            if (errorFile != null) {
                try (FileOutputStream out = new FileOutputStream(errorFile)) {
                    String message = "Native loader: " + e.getMessage()
                            + "\\nlibdir=" + getApplicationInfo().nativeLibraryDir + "\\n";
                    out.write(message.getBytes(StandardCharsets.UTF_8));
                } catch (Exception ignored) { }
            }
            throw e;
        }
    }

''' + needle, 1)
game.write_text(s, encoding="utf-8")

l = launcher.read_text(encoding="utf-8")
needle = '        button(content, "Έλεγχος αρχείων παιχνιδιού").setOnClickListener(v -> checkAssets());'
if l.count(needle) != 1:
    raise SystemExit("Launcher button insertion point missing")
l = l.replace(needle, needle + '''
        button(content, "Έλεγχος τελευταίου σφάλματος βιβλιοθηκών").setOnClickListener(v -> {
            File baseDir = getExternalFilesDir(null);
            File errorFile = baseDir == null ? null : new File(baseDir, "native_loader_failure.txt");
            if (errorFile == null || !errorFile.isFile()) {
                status.setText("Δεν έχει αποθηκευτεί σφάλμα από τον Java native loader.");
                return;
            }
            try {
                byte[] data = java.nio.file.Files.readAllBytes(errorFile.toPath());
                status.setText(new String(data, java.nio.charset.StandardCharsets.UTF_8));
            } catch (Exception e) {
                status.setText("Αδυναμία ανάγνωσης σφάλματος: " + e.getMessage());
            }
        });
''',1)
launcher.write_text(l,encoding="utf-8")
# The Stage3 source ZIP contains a test asserting test13's eager load order.
# Update that assertion while keeping a meaningful dependency check.
test_path = root / "tests/test_stage3_package.py"
if not test_path.is_file():
    raise SystemExit("Stage3 native package test missing")
t = test_path.read_text(encoding="utf-8")
t = replace_exact(t,
    "NAMES = ['c++_shared','SDL3','dice_py','dxvk_d3d9','vkd3d-shader','main']",
    "NAMES = ['SDL3','main']")
t = replace_exact(t,
    "def test_library_load_order_from_test13_dex(self):",
    "def test_native_library_load_order(self):")
anchor = "    def test_native_libraries_present(self):"
if t.count(anchor) != 1:
    raise SystemExit("Stage3 test insertion point missing")
t = t.replace(anchor, """    def test_transitive_elf_dependencies(self):
        import subprocess
        needed = subprocess.check_output(
            ['readelf', '-d', str(NATIVE/'libmain.so')], text=True)
        for dep in ('libdice_py.so','libSDL3.so','libc++_shared.so'):
            self.assertIn(dep, needed)
""" + anchor, 1)
test_path.write_text(t, encoding="utf-8")

print("Stage3.4 native-loader error diagnostics installed")
