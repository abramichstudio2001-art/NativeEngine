package com.studybuddy.app;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ContentValues;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.provider.MediaStore;
import android.provider.Settings;
import android.view.View;
import android.view.Window;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class MainActivity extends Activity {
    private static final int REQ_FILE = 7;
    private static final int REQ_NOTIF = 8;

    private WebView web;
    private ValueCallback<Uri[]> fileCallback;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Notif.channels(this);

        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(11, 13, 18));
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setDatabaseEnabled(true);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(false);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setTextZoom(100);
        web.addJavascriptInterface(new Bridge(), "Android");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView v, WebResourceRequest r) {
                Uri u = r.getUrl();
                if ("file".equals(u.getScheme())) return false;
                try { startActivity(new Intent(Intent.ACTION_VIEW, u)); } catch (Exception ignored) {}
                return true;
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView v, ValueCallback<Uri[]> cb, FileChooserParams p) {
                if (fileCallback != null) fileCallback.onReceiveValue(null);
                fileCallback = cb;
                Intent i = new Intent(Intent.ACTION_GET_CONTENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
                try {
                    startActivityForResult(Intent.createChooser(i, "Choose backup file"), REQ_FILE);
                } catch (Exception e) {
                    fileCallback = null;
                    return false;
                }
                return true;
            }
        });
        setContentView(web);
        web.loadUrl("file:///android_asset/index.html");

        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIF);
        }
        Scheduler.rescheduleAll(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (Store.ringing(this)) {
            startActivity(new Intent(this, AlarmActivity.class));
        }
        if (web != null) web.evaluateJavascript("window.onNativeResume&&window.onNativeResume()", null);
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        web.evaluateJavascript("(window.onAndroidBack&&window.onAndroidBack())?'1':'0'", v -> {
            if (!"\"1\"".equals(v)) moveTaskToBack(true);
        });
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req == REQ_FILE && fileCallback != null) {
            Uri[] r = null;
            if (res == RESULT_OK && data != null && data.getData() != null) r = new Uri[]{data.getData()};
            fileCallback.onReceiveValue(r);
            fileCallback = null;
        }
    }

    private static int parseColor(String c, int def) {
        try { return Color.parseColor(c.trim()); } catch (Exception e) { return def; }
    }

    /** Methods callable from JavaScript as window.Android.* */
    private class Bridge {
        @JavascriptInterface
        public String version() {
            try {
                return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
            } catch (Exception e) {
                return "";
            }
        }

        @JavascriptInterface
        public String setWakeAlarm(String json) {
            try {
                Store.setWake(MainActivity.this, new JSONObject(json));
                return String.valueOf(Scheduler.scheduleWake(MainActivity.this));
            } catch (Exception e) {
                return "-1";
            }
        }

        @JavascriptInterface
        public void setReminders(String json) {
            try {
                Scheduler.setReminders(MainActivity.this, new JSONArray(json));
            } catch (Exception ignored) {}
        }

        @JavascriptInterface
        public void testAlarm() {
            Store.setRinging(MainActivity.this, true);
            AlarmService.start(MainActivity.this);
            runOnUiThread(() -> startActivity(new Intent(MainActivity.this, AlarmActivity.class)));
        }

        @JavascriptInterface
        public boolean canNotify() {
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) return false;
            return getSystemService(android.app.NotificationManager.class).areNotificationsEnabled();
        }

        @JavascriptInterface
        public void requestNotifications() {
            runOnUiThread(() -> {
                if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
                        && shouldShowRequestPermissionRationale(Manifest.permission.POST_NOTIFICATIONS)) {
                    requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIF);
                    return;
                }
                if (canNotify()) {
                    Toast.makeText(MainActivity.this, "Notifications are on", Toast.LENGTH_SHORT).show();
                    return;
                }
                Intent i = new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, getPackageName());
                try { startActivity(i); } catch (Exception ignored) {}
            });
        }

        @JavascriptInterface
        public void vibrate(int ms) {
            Vibrator v = getSystemService(Vibrator.class);
            if (v != null && v.hasVibrator()) v.vibrate(VibrationEffect.createOneShot(Math.max(1, ms), VibrationEffect.DEFAULT_AMPLITUDE));
        }

        @JavascriptInterface
        public void setBars(String bg, String nav, boolean light) {
            runOnUiThread(() -> {
                Window w = getWindow();
                w.setStatusBarColor(parseColor(bg, Color.BLACK));
                w.setNavigationBarColor(parseColor(nav, Color.BLACK));
                web.setBackgroundColor(parseColor(bg, Color.BLACK));
                int f = w.getDecorView().getSystemUiVisibility();
                f = light ? (f | View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR)
                          : (f & ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR & ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
                w.getDecorView().setSystemUiVisibility(f);
            });
        }

        /** Saves a backup to the Downloads folder, then offers to share it. */
        @JavascriptInterface
        public void saveFile(String name, String content) {
            runOnUiThread(() -> {
                boolean saved = false;
                if (Build.VERSION.SDK_INT >= 29) {
                    try {
                        ContentValues cv = new ContentValues();
                        cv.put(MediaStore.Downloads.DISPLAY_NAME, name);
                        cv.put(MediaStore.Downloads.MIME_TYPE, "application/json");
                        cv.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
                        Uri uri = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
                        if (uri != null) {
                            try (OutputStream os = getContentResolver().openOutputStream(uri)) {
                                os.write(content.getBytes(StandardCharsets.UTF_8));
                            }
                            saved = true;
                        }
                    } catch (Exception ignored) {}
                }
                if (saved) {
                    Toast.makeText(MainActivity.this, "Backup saved to Downloads/" + name, Toast.LENGTH_LONG).show();
                } else {
                    Intent send = new Intent(Intent.ACTION_SEND).setType("text/plain")
                            .putExtra(Intent.EXTRA_SUBJECT, name)
                            .putExtra(Intent.EXTRA_TEXT, content);
                    startActivity(Intent.createChooser(send, "Save backup"));
                }
            });
        }
    }
}
