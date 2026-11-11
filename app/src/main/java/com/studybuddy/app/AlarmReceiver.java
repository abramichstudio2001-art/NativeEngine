package com.studybuddy.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** Receives the wake alarm, reminders, and boot / time-change events. */
public class AlarmReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context c, Intent i) {
        String a = i.getAction();
        if (Scheduler.ACTION_WAKE.equals(a)) {
            Scheduler.scheduleWake(c); // queue the next day's alarm
            AlarmService.start(c);
        } else if (Scheduler.ACTION_REMINDER.equals(a)) {
            String id = i.getStringExtra("id");
            Notif.reminder(c, id, i.getStringExtra("title"), i.getStringExtra("body"));
            if (id != null) Store.removeReminder(c, id);
        } else {
            // BOOT_COMPLETED, TIME_SET, TIMEZONE_CHANGED, MY_PACKAGE_REPLACED
            Scheduler.rescheduleAll(c);
        }
    }
}
