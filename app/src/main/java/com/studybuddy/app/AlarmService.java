package com.studybuddy.app;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.RingtoneManager;
import android.media.ToneGenerator;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.VibrationEffect;
import android.os.Vibrator;

/** Foreground service that plays the alarm sound until the challenge is solved. */
public class AlarmService extends Service {
    static final String ACTION_STOP = "com.studybuddy.app.STOP_ALARM";
    private static final long MAX_RING_MS = 20 * 60 * 1000L;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private MediaPlayer player;
    private ToneGenerator tone;
    private Vibrator vibrator;
    private PowerManager.WakeLock wakeLock;
    private int savedVolume = -1;
    private boolean running;

    static void start(Context c) {
        Intent s = new Intent(c, AlarmService.class);
        try {
            c.startForegroundService(s);
        } catch (Exception e) {
            // Fall back to just showing the alarm notification
            Notif.channels(c);
            try {
                c.getSystemService(android.app.NotificationManager.class).notify(Notif.ID_ALARM, Notif.alarm(c));
            } catch (SecurityException ignored) {}
        }
    }

    static void stop(Context c) {
        Store.setRinging(c, false);
        c.startService(new Intent(c, AlarmService.class).setAction(ACTION_STOP));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            Store.setRinging(this, false);
            stopRinging();
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
            return START_NOT_STICKY;
        }
        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(Notif.ID_ALARM, Notif.alarm(this), ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        } else {
            startForeground(Notif.ID_ALARM, Notif.alarm(this));
        }
        Store.setRinging(this, true);
        if (!running) {
            running = true;
            startRinging();
            handler.postDelayed(timeout, MAX_RING_MS);
        }
        try {
            startActivity(new Intent(this, AlarmActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (Exception ignored) {
            // Background launch may be blocked; the full-screen notification covers that case.
        }
        return START_STICKY;
    }

    private final Runnable timeout = () -> {
        Store.setRinging(this, false);
        stopRinging();
        stopForeground(STOP_FOREGROUND_REMOVE);
        Notif.reminder(this, "missed-alarm", "You missed your alarm", "It rang for 20 minutes. Hurry up!");
        stopSelf();
    };

    private void startRinging() {
        PowerManager pm = getSystemService(PowerManager.class);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "StudyBuddy:alarm");
        wakeLock.acquire(MAX_RING_MS + 60_000);

        AudioManager audio = getSystemService(AudioManager.class);
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_ALARM);
        int cur = audio.getStreamVolume(AudioManager.STREAM_ALARM);
        if (cur < max / 2) { // a silent alarm is useless for waking up
            savedVolume = cur;
            try { audio.setStreamVolume(AudioManager.STREAM_ALARM, Math.max(1, max * 7 / 10), 0); } catch (Exception ignored) {}
        }

        Uri uri = RingtoneManager.getActualDefaultRingtoneUri(this, RingtoneManager.TYPE_ALARM);
        if (uri == null) uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM);
        if (uri == null) uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE);
        try {
            player = new MediaPlayer();
            player.setAudioAttributes(new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_ALARM)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build());
            player.setDataSource(this, uri);
            player.setLooping(true);
            player.prepare();
            player.start();
        } catch (Exception e) {
            if (player != null) { try { player.release(); } catch (Exception ignored) {} }
            player = null;
            beepLoop();
        }

        vibrator = getSystemService(Vibrator.class);
        if (vibrator != null && vibrator.hasVibrator()) {
            AudioAttributes va = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM).build();
            vibrator.vibrate(VibrationEffect.createWaveform(new long[]{0, 700, 500}, 0), va);
        }
    }

    private final Runnable beep = new Runnable() {
        @Override public void run() {
            if (tone != null) tone.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, 600);
            handler.postDelayed(this, 1200);
        }
    };

    private void beepLoop() {
        try {
            tone = new ToneGenerator(AudioManager.STREAM_ALARM, 100);
            handler.post(beep);
        } catch (Exception ignored) {}
    }

    private void stopRinging() {
        running = false;
        handler.removeCallbacks(timeout);
        handler.removeCallbacks(beep);
        if (player != null) { try { player.stop(); player.release(); } catch (Exception ignored) {} player = null; }
        if (tone != null) { tone.release(); tone = null; }
        if (vibrator != null) vibrator.cancel();
        if (savedVolume >= 0) {
            try { getSystemService(AudioManager.class).setStreamVolume(AudioManager.STREAM_ALARM, savedVolume, 0); } catch (Exception ignored) {}
            savedVolume = -1;
        }
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        wakeLock = null;
    }

    @Override
    public void onDestroy() {
        stopRinging();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
