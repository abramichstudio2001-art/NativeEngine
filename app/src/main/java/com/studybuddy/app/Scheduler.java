package com.studybuddy.app;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Calendar;

/** Schedules the wake-up alarm and homework / note reminders with AlarmManager. */
final class Scheduler {
    static final String ACTION_WAKE = "com.studybuddy.app.WAKE";
    static final String ACTION_REMINDER = "com.studybuddy.app.REMINDER";

    private Scheduler() {}

    private static AlarmManager am(Context c) {
        return (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
    }

    static boolean canExact(Context c) {
        return Build.VERSION.SDK_INT < 31 || am(c).canScheduleExactAlarms();
    }

    private static int flags() {
        return PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE;
    }

    private static PendingIntent wakeIntent(Context c) {
        Intent i = new Intent(c, AlarmReceiver.class).setAction(ACTION_WAKE);
        return PendingIntent.getBroadcast(c, 1, i, flags());
    }

    /** Next time the wake alarm should ring, or -1 if it is off. */
    static long nextWake(JSONObject w, long from) {
        if (!w.optBoolean("on", false)) return -1;
        JSONArray days = w.optJSONArray("days");
        if (days == null || days.length() == 0) return -1;
        String[] t = w.optString("time", "07:30").split(":");
        int h, m;
        try {
            h = Integer.parseInt(t[0]);
            m = Integer.parseInt(t[1]);
        } catch (Exception e) {
            return -1;
        }
        Calendar cal = Calendar.getInstance();
        cal.setTimeInMillis(from);
        cal.set(Calendar.HOUR_OF_DAY, h);
        cal.set(Calendar.MINUTE, m);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);
        for (int d = 0; d < 8; d++) {
            if (d > 0) cal.add(Calendar.DAY_OF_MONTH, 1);
            int jsDay = cal.get(Calendar.DAY_OF_WEEK) - 1; // 0 = Sunday, like JavaScript
            boolean on = false;
            for (int k = 0; k < days.length(); k++) if (days.optInt(k, -1) == jsDay) on = true;
            if (on && cal.getTimeInMillis() > from) return cal.getTimeInMillis();
        }
        return -1;
    }

    static long scheduleWake(Context c) {
        AlarmManager am = am(c);
        PendingIntent pi = wakeIntent(c);
        am.cancel(pi);
        // +30 s so an alarm that is firing right now schedules the next occurrence, not itself.
        long at = nextWake(Store.wake(c), System.currentTimeMillis() + 30_000);
        if (at < 0) return -1;
        Intent show = new Intent(c, MainActivity.class).putExtra("tab", "alarm");
        PendingIntent showPi = PendingIntent.getActivity(c, 2, show, flags());
        try {
            am.setAlarmClock(new AlarmManager.AlarmClockInfo(at, showPi), pi);
        } catch (SecurityException e) {
            am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
        }
        return at;
    }

    private static PendingIntent reminderIntent(Context c, String id, String title, String body) {
        Intent i = new Intent(c, AlarmReceiver.class)
                .setAction(ACTION_REMINDER)
                .setData(Uri.parse("studybuddy://reminder/" + Uri.encode(id)))
                .putExtra("id", id)
                .putExtra("title", title)
                .putExtra("body", body);
        return PendingIntent.getBroadcast(c, 100, i, flags());
    }

    private static void scheduleOne(Context c, JSONObject r) {
        long at = r.optLong("at", 0);
        if (at <= System.currentTimeMillis()) return;
        PendingIntent pi = reminderIntent(c, r.optString("id"), r.optString("title"), r.optString("body"));
        AlarmManager am = am(c);
        try {
            if (canExact(c)) am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
            else am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
        } catch (SecurityException e) {
            am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
        }
    }

    /** Replaces every scheduled reminder with the given list. */
    static void setReminders(Context c, JSONArray list) {
        JSONArray old = Store.reminders(c);
        for (int i = 0; i < old.length(); i++) {
            JSONObject o = old.optJSONObject(i);
            if (o != null) am(c).cancel(reminderIntent(c, o.optString("id"), "", ""));
        }
        Store.setReminders(c, list);
        for (int i = 0; i < list.length(); i++) {
            JSONObject o = list.optJSONObject(i);
            if (o != null) scheduleOne(c, o);
        }
    }

    static void rescheduleAll(Context c) {
        scheduleWake(c);
        JSONArray list = Store.reminders(c);
        for (int i = 0; i < list.length(); i++) {
            JSONObject o = list.optJSONObject(i);
            if (o != null) scheduleOne(c, o);
        }
    }
}
