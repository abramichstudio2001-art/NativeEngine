package com.studybuddy.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.media.AudioAttributes;
import android.media.RingtoneManager;

final class Notif {
    static final String CH_ALARM = "alarm";
    static final String CH_REMIND = "reminders";
    static final int ID_ALARM = 1;

    private Notif() {}

    static void channels(Context c) {
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        NotificationChannel alarm = new NotificationChannel(CH_ALARM, "Wake-up alarm", NotificationManager.IMPORTANCE_HIGH);
        alarm.setDescription("Shows the alarm on the lock screen");
        alarm.setSound(null, null); // the alarm service plays the sound itself
        alarm.enableVibration(false);
        alarm.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
        nm.createNotificationChannel(alarm);

        NotificationChannel rem = new NotificationChannel(CH_REMIND, "Homework & note reminders", NotificationManager.IMPORTANCE_HIGH);
        rem.setDescription("Reminders before homework is due and for notes");
        rem.enableVibration(true);
        rem.enableLights(true);
        rem.setLightColor(Color.rgb(124, 140, 255));
        rem.setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION),
                new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_NOTIFICATION).build());
        nm.createNotificationChannel(rem);
    }

    static Notification alarm(Context c) {
        channels(c);
        Intent ring = new Intent(c, AlarmActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NO_USER_ACTION);
        PendingIntent pi = PendingIntent.getActivity(c, 10, ring, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        String school = Store.school(c);
        return new Notification.Builder(c, CH_ALARM)
                .setSmallIcon(R.drawable.ic_stat)
                .setColor(Color.rgb(124, 140, 255))
                .setContentTitle("WAKE UP!")
                .setContentText(school.isEmpty() ? "Tap to turn off the alarm" : "School starts at " + school + " — tap to turn off")
                .setCategory(Notification.CATEGORY_ALARM)
                .setVisibility(Notification.VISIBILITY_PUBLIC)
                .setOngoing(true)
                .setAutoCancel(false)
                .setContentIntent(pi)
                .setFullScreenIntent(pi, true)
                .addAction(new Notification.Action.Builder(null, "Turn off", pi).build())
                .build();
    }

    static void reminder(Context c, String id, String title, String body) {
        channels(c);
        Intent open = new Intent(c, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent pi = PendingIntent.getActivity(c, 20, open, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification n = new Notification.Builder(c, CH_REMIND)
                .setSmallIcon(R.drawable.ic_stat)
                .setColor(Color.rgb(124, 140, 255))
                .setContentTitle(title == null ? "Study Buddy" : title)
                .setContentText(body == null ? "" : body)
                .setStyle(new Notification.BigTextStyle().bigText(body == null ? "" : body))
                .setCategory(Notification.CATEGORY_REMINDER)
                .setAutoCancel(true)
                .setContentIntent(pi)
                .build();
        int nid = 1000 + ((id == null ? title : id).hashCode() & 0x3fffffff);
        try {
            c.getSystemService(NotificationManager.class).notify(nid, n);
        } catch (SecurityException ignored) {
            // notification permission not granted
        }
    }
}
