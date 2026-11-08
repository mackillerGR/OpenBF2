package com.mackillergr.bf2;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;

public class LauncherActivity extends Activity {
    private static final int PICK_FOLDER = 1, EXPORT_LOG = 2;
    private SharedPreferences prefs;
    private TextView status;
    private Spinner levels, resolution;
    private EditText server, name, cdKey;
    private CheckBox touch, turnip, skipMovies;

    private Button button(LinearLayout parent, String label, View.OnClickListener action) {
        Button b = new Button(this);
        b.setText(label); b.setAllCaps(false); b.setOnClickListener(action);
        parent.addView(b);
        return b;
    }
    private TextView label(LinearLayout parent, String value) {
        TextView t = new TextView(this);
        t.setText(value); t.setTextColor(Color.WHITE); t.setPadding(0, 16, 0, 4);
        parent.addView(t);
        return t;
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("bf2", MODE_PRIVATE);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(24, 28, 22));
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL); root.setPadding(48, 32, 48, 32);
        scroll.addView(root);
        TextView title = label(root, "Battlefield 2 by mackillerGR");
        title.setTextSize(26); title.setGravity(Gravity.CENTER);
        status = label(root, "");
        button(root, "Εισαγωγή φακέλου παιχνιδιού", new View.OnClickListener() { @Override public void onClick(View v) { pickFolder(); } });
        label(root, "Χάρτης");
        levels = new Spinner(this); root.addView(levels);
        label(root, "Διακομιστής (IP:θύρα, κενό για τοπικό παιχνίδι)");
        server = new EditText(this); server.setTextColor(Color.WHITE); server.setSingleLine();
        server.setText(prefs.getString("server", "")); root.addView(server);
        label(root, "Όνομα παίκτη");
        name = new EditText(this); name.setTextColor(Color.WHITE); name.setSingleLine();
        name.setText(prefs.getString("name", "mackillerGR")); root.addView(name);
        label(root, "CD Key (για σύνδεση σε server)");
        cdKey = new EditText(this); cdKey.setTextColor(Color.WHITE); cdKey.setSingleLine();
        cdKey.setText(prefs.getString("cdkey", "")); root.addView(cdKey);
        label(root, "Ρυθμίσεις: ανάλυση");
        resolution = new Spinner(this);
        resolution.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item,
            new String[] {"720p", "1080p"}));
        resolution.setSelection(prefs.getInt("resolution", 0)); root.addView(resolution);
        touch = new CheckBox(this); touch.setText("Χειριστήρια αφής"); touch.setTextColor(Color.WHITE);
        touch.setChecked(prefs.getBoolean("touch", true)); root.addView(touch);
        turnip = new CheckBox(this); turnip.setText("Ενσωματωμένος driver Turnip (προτείνεται)"); turnip.setTextColor(Color.WHITE);
        turnip.setChecked(prefs.getBoolean("turnip", true)); root.addView(turnip);
        skipMovies = new CheckBox(this); skipMovies.setText("Παράλειψη βίντεο εισαγωγής"); skipMovies.setTextColor(Color.WHITE);
        skipMovies.setChecked(prefs.getBoolean("skipMovies", true)); root.addView(skipMovies);
        button(root, "Έναρξη", new View.OnClickListener() { @Override public void onClick(View v) { start(true); } });
        button(root, "Απευθείας σε χάρτη / διακομιστή (χωρίς μενού)", new View.OnClickListener() { @Override public void onClick(View v) { start(false); } });
        button(root, "Εξαγωγή log", new View.OnClickListener() { @Override public void onClick(View v) { exportLog(); } });
        setContentView(scroll);
    }
    @Override protected void onResume() {
        super.onResume();
        if (!Environment.isExternalStorageManager()) {
            status.setText("Χρειάζεται άδεια πρόσβασης σε όλα τα αρχεία.");
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName())));
            return;
        }
        refresh();
    }
    private File modDir() {
        String base = prefs.getString("folder", "");
        if (base.isEmpty()) return null;
        for (String candidate : new String[] {"mods/bf2", "Game Files/mods/bf2", ""}) {
            File dir = candidate.isEmpty() ? new File(base) : new File(base, candidate);
            if (new File(dir, "ClientArchives.con").exists()) return dir;
        }
        return null;
    }
    // The address a PC on this network uses to join a game hosted here.
    private String localAddress() {
        try {
            for (java.net.NetworkInterface n : java.util.Collections.list(java.net.NetworkInterface.getNetworkInterfaces())) {
                if (!n.isUp() || n.isLoopback()) continue;
                for (java.net.InetAddress a : java.util.Collections.list(n.getInetAddresses())) {
                    if (a instanceof java.net.Inet4Address && a.isSiteLocalAddress()) return a.getHostAddress();
                }
            }
        } catch (Exception ignored) { }
        return "";
    }
    private void refresh() {
        File mod = modDir();
        ArrayList<String> names = new ArrayList<>();
        if (mod != null) {
            String[] found = new File(mod, "Levels").list();
            if (found != null) { Arrays.sort(found, String.CASE_INSENSITIVE_ORDER); names.addAll(Arrays.asList(found)); }
        }
        levels.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, names));
        int last = names.indexOf(prefs.getString("level", ""));
        if (last >= 0) levels.setSelection(last);
        status.setText(mod == null ? "Επίλεξε τον φάκελο εγκατάστασης του Battlefield 2 (έκδοση 1.5)."
                                   : "Βρέθηκε το παιχνίδι: " + names.size() + " χάρτες");
        String address = localAddress();
        if (mod != null && !address.isEmpty()) status.append("\nIP στο LAN: " + address);
    }
    private void pickFolder() {
        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_FOLDER);
    }
    private void exportLog() {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE); i.setType("text/plain");
        i.putExtra(Intent.EXTRA_TITLE, "openbf2.log");
        startActivityForResult(i, EXPORT_LOG);
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (result != RESULT_OK || data == null) return;
        if (request == PICK_FOLDER) {
            String id = DocumentsContract.getTreeDocumentId(data.getData());
            String[] parts = id.split(":", 2);
            String root = parts[0].equals("primary") ? Environment.getExternalStorageDirectory().getPath() : "/storage/" + parts[0];
            prefs.edit().putString("folder", parts.length > 1 ? root + "/" + parts[1] : root).apply();
            refresh();
        } else if (request == EXPORT_LOG) {
            try (InputStream in = new FileInputStream(new File(getExternalFilesDir(null), "openbf2.log"));
                 OutputStream out = getContentResolver().openOutputStream(data.getData())) {
                byte[] buffer = new byte[65536];
                for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
                Toast.makeText(this, "Το log αποθηκεύτηκε", Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                Toast.makeText(this, "Δεν υπάρχει log ακόμα", Toast.LENGTH_LONG).show();
            }
        }
    }
    private void start(boolean menu) {
        File mod = modDir();
        if (mod == null) { pickFolder(); return; }
        String host = server.getText().toString().trim();
        Object level = levels.getSelectedItem();
        if (!menu && host.isEmpty() && level == null) {
            new AlertDialog.Builder(this).setMessage("Δεν βρέθηκαν χάρτες.").setPositiveButton("ΟΚ", null).show();
            return;
        }
        // The height is the player's choice; the width follows the screen's shape.
        android.util.DisplayMetrics metrics = new android.util.DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        int longSide = Math.max(metrics.widthPixels, metrics.heightPixels);
        int shortSide = Math.min(metrics.widthPixels, metrics.heightPixels);
        int height = resolution.getSelectedItemPosition() == 0 ? 720 : 1080;
        int width = (height * longSide / shortSide) & ~1;
        String[] size = {String.valueOf(width), String.valueOf(height)};
        ArrayList<String> args = new ArrayList<>(Arrays.asList("--mod", mod.getPath(),
            "--width", size[0], "--height", size[1], "--name", name.getText().toString().trim(),
            "--libdir", getApplicationInfo().nativeLibraryDir,
            "--driver", turnip.isChecked() ? "turnip" : "system",
            "--cdkey", cdKey.getText().toString().trim()));
        if (skipMovies.isChecked()) args.add("--skipmovies");
        if (menu) args.add("--menu");
        else if (host.isEmpty()) args.addAll(Arrays.asList("--hosted", "--level", level.toString()));
        else args.addAll(Arrays.asList("--connect", host));
        prefs.edit().putString("server", host).putString("name", name.getText().toString().trim())
            .putString("level", level == null ? "" : level.toString())
            .putInt("resolution", resolution.getSelectedItemPosition())
            .putBoolean("touch", touch.isChecked())
            .putBoolean("turnip", turnip.isChecked())
            .putBoolean("skipMovies", skipMovies.isChecked())
            .putString("cdkey", cdKey.getText().toString().trim()).apply();
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra("args", args.toArray(new String[0]));
        i.putExtra("touch", touch.isChecked());
        startActivity(i);
    }
}
