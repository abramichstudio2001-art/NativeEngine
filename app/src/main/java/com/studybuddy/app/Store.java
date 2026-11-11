package com.studybuddy.app;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

/** Small persistent store for the data the native side needs while the UI is closed. */
final class Store {
    private static final String PREFS = "native_store";

    private Store() {}

    private static SharedPreferences p(Context c) {
        return c.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static JSONObject wake(Context c) {
        try {
            return new JSONObject(p(c).getString("wake", "{}"));
        } catch (Exception e) {
            return new JSONObject();
        }
    }

    static void setWake(Context c, JSONObject o) {
        p(c).edit().putString("wake", o.toString()).apply();
    }

    static JSONArray reminders(Context c) {
        try {
            return new JSONArray(p(c).getString("reminders", "[]"));
        } catch (Exception e) {
            return new JSONArray();
        }
    }

    static void setReminders(Context c, JSONArray a) {
        p(c).edit().putString("reminders", a.toString()).apply();
    }

    static void removeReminder(Context c, String id) {
        JSONArray a = reminders(c), out = new JSONArray();
        for (int i = 0; i < a.length(); i++) {
            JSONObject o = a.optJSONObject(i);
            if (o != null && !id.equals(o.optString("id"))) out.put(o);
        }
        setReminders(c, out);
    }

    static boolean ringing(Context c) {
        return p(c).getBoolean("ringing", false);
    }

    static void setRinging(Context c, boolean v) {
        p(c).edit().putBoolean("ringing", v).apply();
    }

    static String challenge(Context c) {
        return wake(c).optString("challenge", "stop");
    }

    static String school(Context c) {
        return wake(c).optString("school", "");
    }
}
