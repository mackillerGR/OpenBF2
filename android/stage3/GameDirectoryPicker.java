package com.mackillergr.bf2;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * SAF directory picker and importer for the Android game client.
 * The native executable requires --mod with an actual filesystem directory;
 * content:// URIs cannot be passed directly to the BF2 filesystem.
 */
final class GameDirectoryPicker {
    static final int REQUEST_CODE = 4082;
    interface Listener {
        void onStatus(String status);
        void onImported(String path);
    }
    private static final String DIR = DocumentsContract.Document.MIME_TYPE_DIR;
    private static final String[] REQUIRED = {
        "ClientArchives.con", "Objects_client.zip", "Common_client.zip",
        "Menu_client.zip", "Fonts_client.zip", "Shaders_client.zip", "Booster_client.zip"
    };
    private final Activity activity;
    private final Listener listener;
    private boolean importing;

    GameDirectoryPicker(Activity activity, Listener listener) {
        this.activity = activity;
        this.listener = listener;
    }

    void pick() {
        if (importing) {
            listener.onStatus("Υπάρχει ήδη εισαγωγή αρχείων σε εξέλιξη.");
            return;
        }
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        activity.startActivityForResult(intent, REQUEST_CODE);
    }

    void onResult(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) return;
        Uri tree = data.getData();
        try {
            try {
                activity.getContentResolver().takePersistableUriPermission(
                        tree, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (SecurityException ignored) {
                // The one-time read grant still permits an immediate import.
            }
            Node mod = findModDirectory(tree);
            if (mod == null) {
                listener.onStatus("Δεν βρέθηκε ClientArchives.con. Επίλεξε mods/bf2 ή τον φάκελο εγκατάστασης Battlefield 2.");
                return;
            }
            new AlertDialog.Builder(activity)
                    .setTitle("Φάκελος Battlefield 2")
                    .setMessage("Βρέθηκαν τα δεδομένα του παιχνιδιού. Για να τα διαβάζει η native μηχανή, " +
                            "θα αντιγραφούν (μαζί με τους χάρτες) στον χώρο της εφαρμογής. " +
                            "Χρειάζεται επιπλέον ελεύθερος χώρος.\n\nΣυνέχεια;")
                    .setNegativeButton("Ακύρωση", null)
                    .setPositiveButton("Εισαγωγή", (dialog, which) -> importMod(tree, mod))
                    .show();
        } catch (Exception error) {
            listener.onStatus("Σφάλμα επιλογής φακέλου: " + error.getMessage());
        }
    }

    private static final class Node {
        final String id;
        final String name;
        final boolean directory;
        final long size;
        Node(String id, String name, boolean directory, long size) {
            this.id = id; this.name = name; this.directory = directory; this.size = size;
        }
    }

    private static final class Job {
        final Node folder;
        final File target;
        final int depth;
        Job(Node folder, File target, int depth) {
            this.folder = folder; this.target = target; this.depth = depth;
        }
    }

    private List<Node> children(Uri tree, String id) throws IOException {
        List<Node> result = new ArrayList<>();
        Uri uri = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
        String[] cols = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE
        };
        try (Cursor cursor = activity.getContentResolver().query(uri, cols, null, null, null)) {
            if (cursor == null) throw new IOException("Ο διαχειριστής αρχείων δεν απάντησε");
            while (cursor.moveToNext()) {
                String childId = cursor.getString(0);
                String name = cursor.getString(1);
                if (childId != null && name != null) {
                    result.add(new Node(childId, name, DIR.equals(cursor.getString(2)),
                            cursor.isNull(3) ? -1 : cursor.getLong(3)));
                }
            }
        } catch (SecurityException e) {
            throw new IOException("Δεν επιτρέπεται η ανάγνωση φακέλου", e);
        }
        return result;
    }

    private Node findChild(Uri tree, Node parent, String name, boolean directory) throws IOException {
        for (Node n : children(tree, parent.id)) {
            if (n.directory == directory && name.equalsIgnoreCase(n.name)) return n;
        }
        return null;
    }

    private Node findModDirectory(Uri tree) throws IOException {
        Node root = new Node(DocumentsContract.getTreeDocumentId(tree), "chosen folder", true, -1);
        String[][] candidates = {
            {}, {"bf2"}, {"mods", "bf2"}, {"Game Files", "mods", "bf2"},
            {"Battlefield 2", "mods", "bf2"}, {"Game Files", "bf2"}
        };
        for (String[] parts : candidates) {
            Node node = root;
            for (String part : parts) {
                node = findChild(tree, node, part, true);
                if (node == null) break;
            }
            if (node != null && findChild(tree, node, "ClientArchives.con", false) != null)
                return node;
        }
        return null;
    }

    private void importMod(Uri tree, Node mod) {
        if (importing) return;
        File root = activity.getExternalFilesDir(null);
        if (root == null) {
            listener.onStatus("Δεν υπάρχει διαθέσιμος χώρος αποθήκευσης για την εφαρμογή.");
            return;
        }
        File dst = new File(root, "Game Files/mods/bf2");
        importing = true;
        listener.onStatus("Γίνεται εισαγωγή δεδομένων στον φάκελο εφαρμογής…");
        new Thread(() -> {
            try {
                int total = copy(tree, mod, dst);
                for (String name : REQUIRED) {
                    File file = new File(dst, name);
                    if (!file.isFile() || file.length() == 0)
                        throw new IOException("Λείπει " + name + " από την εγκατάσταση");
                }
                activity.runOnUiThread(() -> {
                    importing = false;
                    listener.onImported(dst.getAbsolutePath());
                    listener.onStatus("Επιτυχής εισαγωγή: " + total + " αρχεία. Μπορείς να ξεκινήσεις το παιχνίδι.");
                });
            } catch (Exception error) {
                activity.runOnUiThread(() -> {
                    importing = false;
                    listener.onStatus("Αποτυχία εισαγωγής: " + error.getMessage() +
                            " — έλεγξε τον ελεύθερο χώρο και ξαναδοκίμασε.");
                });
            }
        }, "bf2-folder-import").start();
    }

    private int copy(Uri tree, Node src, File root) throws IOException {
        if (!root.isDirectory() && !root.mkdirs()) throw new IOException("Δεν δημιουργείται ο φάκελος παιχνιδιού");
        ArrayDeque<Job> queue = new ArrayDeque<>();
        Set<String> seen = new HashSet<>();
        queue.add(new Job(src, root, 0));
        String allowedRoot = root.getCanonicalPath() + File.separator;
        int count = 0;
        while (!queue.isEmpty()) {
            Job current = queue.removeFirst();
            if (current.depth > 24 || !seen.add(current.folder.id))
                throw new IOException("Μη έγκυρη δομή φακέλων");
            if (!current.target.isDirectory() && !current.target.mkdirs())
                throw new IOException("Δεν δημιουργείται " + current.target);
            for (Node file : children(tree, current.folder.id)) {
                if (file.name.isEmpty() || ".".equals(file.name) || "..".equals(file.name) ||
                        file.name.contains("/") || file.name.contains("\\") || file.name.indexOf('\0') >= 0)
                    throw new IOException("Μη ασφαλές όνομα αρχείου");
                File output = new File(current.target, file.name);
                if (!output.getCanonicalPath().startsWith(allowedRoot))
                    throw new IOException("Μη ασφαλής διαδρομή");
                if (file.directory) {
                    queue.addLast(new Job(file, output, current.depth + 1));
                    continue;
                }
                if (++count > 40000) throw new IOException("Πάρα πολλά αρχεία");
                if (output.isFile() && file.size > 0 && output.length() == file.size)
                    continue;
                if (count % 5 == 1 || file.size > 25000000) {
                    final int c = count;
                    final String n = file.name;
                    activity.runOnUiThread(() -> listener.onStatus("Εισαγωγή #" + c + ": " + n));
                }
                File part = new File(output.getParentFile(), output.getName() + ".bf2part");
                try {
                    Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, file.id);
                    try (InputStream input = activity.getContentResolver().openInputStream(document)) {
                        if (input == null) throw new IOException("Δεν ανοίγει το " + file.name);
                        try (FileOutputStream out = new FileOutputStream(part)) {
                            byte[] buffer = new byte[1024 * 1024];
                            int n;
                            while ((n = input.read(buffer)) != -1) out.write(buffer, 0, n);
                            out.getFD().sync();
                        }
                    }
                    if (file.size >= 0 && part.length() != file.size)
                        throw new IOException("Ατελής αντιγραφή " + file.name);
                    if (output.exists() && !output.delete()) throw new IOException("Δεν αντικαθίσταται " + file.name);
                    if (!part.renameTo(output)) throw new IOException("Δεν αποθηκεύεται " + file.name);
                } finally {
                    if (part.exists()) part.delete();
                }
            }
        }
        return count;
    }
}
