package com.mackillergr.bf2;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.util.Log;
import android.widget.Toast;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * Exports engine logs to a document chosen through the Android system picker.
 * Runs from both the launcher and the separate :game process, without
 * MANAGE_EXTERNAL_STORAGE or a FileProvider.
 */
public final class BF2LogExporter {
    public static final int REQUEST_CODE = 8315;
    private static final int MAX_LOG_CHARS = 1200000;
    private static final int MAX_FILES = 18;
    private BF2LogExporter() {}

    public static void request(Activity activity) {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        String date = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
        intent.putExtra(Intent.EXTRA_TITLE, "BF2_Android_Log_" + date + ".txt");
        try {
            activity.startActivityForResult(intent, REQUEST_CODE);
        } catch (Exception e) {
            Toast.makeText(activity, "Δεν άνοιξε η εξαγωγή log: " + e.getMessage(),
                    Toast.LENGTH_LONG).show();
        }
    }

    public static boolean handleResult(Activity activity, int request,
                                       int result, Intent data) {
        if (request != REQUEST_CODE) return false;
        if (result != Activity.RESULT_OK || data == null || data.getData() == null) return true;
        Uri output = data.getData();
        new Thread(() -> save(activity, output), "bf2-export-log").start();
        return true;
    }

    private static void message(Activity activity, String text) {
        activity.runOnUiThread(() -> Toast.makeText(activity, text, Toast.LENGTH_LONG).show());
    }

    private static void writeFile(BufferedWriter writer, File file) throws Exception {
        writer.write("\n===== " + file.getName() + " (" + file.length() + " bytes) =====\n");
        long start = Math.max(0, file.length() - MAX_LOG_CHARS);
        try (FileInputStream input = new FileInputStream(file)) {
            long skipped = 0;
            while (skipped < start) {
                long n = input.skip(start - skipped);
                if (n <= 0) break;
                skipped += n;
            }
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(input, StandardCharsets.UTF_8))) {
                char[] chunk = new char[8192];
                int count;
                int remaining = MAX_LOG_CHARS;
                while (remaining > 0 && (count = reader.read(chunk, 0, Math.min(chunk.length, remaining))) > 0) {
                    writer.write(chunk, 0, count);
                    remaining -= count;
                }
            }
        }
        writer.write("\n");
    }

    private static void writeLogcat(BufferedWriter out) {
        java.lang.Process proc = null;
        try {
            proc = new ProcessBuilder("logcat", "-d", "-v", "threadtime",
                    "-t", "400", "--pid=" + Process.myPid())
                    .redirectErrorStream(true).start();
            out.write("\n===== Own process Android logcat (PID " + Process.myPid() + ") =====\n");
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(proc.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                int count = 0;
                while ((line = reader.readLine()) != null && count++ < 600) {
                    out.write(line);
                    out.newLine();
                }
            }
            proc.waitFor(3, TimeUnit.SECONDS);
        } catch (Exception e) {
            try { out.write("Logcat unavailable: " + e.getClass().getSimpleName() + "\n"); }
            catch (Exception ignored) {}
        } finally {
            if (proc != null) proc.destroy();
        }
    }

    private static void save(Activity activity, Uri target) {
        try (OutputStream stream = activity.getContentResolver().openOutputStream(target, "w")) {
            if (stream == null) throw new IllegalStateException("Δεν ανοίγει το αρχείο εξαγωγής");
            try (BufferedWriter writer = new BufferedWriter(
                    new OutputStreamWriter(stream, StandardCharsets.UTF_8))) {
                writer.write("Battlefield 2 Android - diagnostic export\n");
                writer.write("Timestamp: " + new Date().toString() + "\n");
                writer.write("Build: " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n");
                writer.write("Device: " + Build.MANUFACTURER + " " + Build.MODEL + "\n");
                writer.write("App: " + activity.getPackageName() + "\n");
                writer.write("ABI: " + Arrays.toString(Build.SUPPORTED_ABIS) + "\n");
                File lib = new File(activity.getApplicationInfo().nativeLibraryDir);
                writer.write("\n===== Native libraries present =====\n");
                String[] names = lib.list();
                if (names != null) {
                    Arrays.sort(names);
                    for (String name : names)
                        if (name.endsWith(".so"))
                            writer.write(name + ": " + new File(lib, name).length() + " bytes\n");
                }
                List<File> logs = new ArrayList<>();
                File root = activity.getExternalFilesDir(null);
                if (root != null) {
                    File[] candidates = root.listFiles();
                    if (candidates != null)
                        for (File f : candidates) {
                            String name = f.getName().toLowerCase(Locale.ROOT);
                            if (f.isFile() && (name.endsWith(".log") || name.equals("native_loader_failure.txt")
                                    || name.contains("crash") && name.endsWith(".txt")))
                                logs.add(f);
                        }
                }
                logs.sort((a, b) -> Long.compare(b.lastModified(), a.lastModified()));
                int found = 0;
                for (File f : logs) {
                    if (found++ >= MAX_FILES) break;
                    try { writeFile(writer, f); }
                    catch (Exception e) { writer.write("Unreadable " + f.getName() + ": " + e.getMessage() + "\n"); }
                }
                if (logs.isEmpty()) writer.write("\nNo engine log files were found in the app's files folder.\n");
                writeLogcat(writer);
                writer.flush();
            }
            message(activity, "Το BF2 log εξήχθη. Στείλε το αρχείο στο ChatGPT.");
        } catch (Exception e) {
            Log.e("BF2LogExport", "Export failed", e);
            message(activity, "Απέτυχε η εξαγωγή log: " + e.getMessage());
        }
    }
}
