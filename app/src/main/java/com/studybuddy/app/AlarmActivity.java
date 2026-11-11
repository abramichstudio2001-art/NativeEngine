package com.studybuddy.app;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.text.DateFormat;
import java.util.Date;
import java.util.Random;

/** Full-screen alarm shown over the lock screen. Turn it off by solving the challenge 3 times. */
public class AlarmActivity extends Activity {
    private static final int BG_A = Color.rgb(11, 13, 18);
    private static final int BG_B = Color.rgb(43, 47, 122);
    private static final int GOAL = 3;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Random random = new Random();
    private LinearLayout root;
    private TextView clock, prompt, error;
    private EditText input;
    private LinearLayout dots;
    private boolean flash;
    private int count;
    private String answer = "stop";
    private boolean math;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        if (Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setStatusBarColor(BG_A);
        getWindow().setNavigationBarColor(BG_A);

        if (!Store.ringing(this)) { finish(); return; }
        math = "math".equals(Store.challenge(this));
        buildUi();
        nextQuestion();
    }

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private TextView text(String s, float sp, boolean bold, int color) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        t.setGravity(Gravity.CENTER);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD);
        return t;
    }

    private void buildUi() {
        root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(dp(28), dp(28), dp(28), dp(28));
        root.setBackgroundColor(BG_A);

        clock = text("", 72, true, Color.WHITE);
        clock.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(clock);

        TextView wake = text("WAKE UP", 30, true, Color.WHITE);
        wake.setLetterSpacing(0.08f);
        root.addView(wake);

        String school = Store.school(this);
        if (!school.isEmpty()) {
            TextView s = text("School starts at " + school, 17, false, Color.argb(200, 255, 255, 255));
            s.setPadding(0, dp(6), 0, dp(24));
            root.addView(s);
        }

        prompt = text("", 24, true, Color.WHITE);
        prompt.setPadding(0, dp(8), 0, dp(12));
        root.addView(prompt);

        dots = new LinearLayout(this);
        dots.setGravity(Gravity.CENTER);
        root.addView(dots);
        drawDots();

        input = new EditText(this);
        input.setTextSize(24);
        input.setTextColor(Color.WHITE);
        input.setHintTextColor(Color.argb(120, 255, 255, 255));
        input.setGravity(Gravity.CENTER);
        input.setSingleLine(true);
        input.setImeOptions(EditorInfo.IME_ACTION_DONE);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.argb(40, 255, 255, 255));
        bg.setCornerRadius(dp(14));
        input.setBackground(bg);
        input.setPadding(dp(14), dp(14), dp(14), dp(14));
        input.setOnEditorActionListener((v, id, ev) -> {
            if (id == EditorInfo.IME_ACTION_DONE || (ev != null && ev.getKeyCode() == KeyEvent.KEYCODE_ENTER && ev.getAction() == KeyEvent.ACTION_DOWN)) {
                check();
                return true;
            }
            return false;
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(300), LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(20);
        root.addView(input, lp);

        Button enter = new Button(this);
        enter.setText("Enter");
        enter.setAllCaps(false);
        enter.setTextSize(18);
        enter.setTypeface(Typeface.DEFAULT_BOLD);
        enter.setTextColor(BG_A);
        GradientDrawable bb = new GradientDrawable();
        bb.setColor(Color.rgb(124, 140, 255));
        bb.setCornerRadius(dp(14));
        enter.setBackground(bb);
        enter.setOnClickListener(v -> check());
        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(dp(300), dp(56));
        blp.topMargin = dp(12);
        root.addView(enter, blp);

        error = text("", 16, true, Color.rgb(255, 138, 160));
        error.setPadding(0, dp(12), 0, 0);
        root.addView(error);

        setContentView(root);
    }

    private void drawDots() {
        dots.removeAllViews();
        for (int i = 0; i < GOAL; i++) {
            View d = new View(this);
            GradientDrawable g = new GradientDrawable();
            g.setShape(GradientDrawable.OVAL);
            g.setStroke(dp(2), Color.WHITE);
            g.setColor(i < count ? Color.WHITE : Color.TRANSPARENT);
            d.setBackground(g);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(16), dp(16));
            lp.setMargins(dp(6), 0, dp(6), 0);
            dots.addView(d, lp);
        }
    }

    private void nextQuestion() {
        if (math) {
            int a = 12 + random.nextInt(38), b2 = 12 + random.nextInt(38);
            if (random.nextBoolean()) {
                prompt.setText(a + " + " + b2 + " = ?");
                answer = String.valueOf(a + b2);
            } else {
                int x = 3 + random.nextInt(7), y = 3 + random.nextInt(7);
                prompt.setText(x + " × " + y + " = ?");
                answer = String.valueOf(x * y);
            }
            input.setInputType(InputType.TYPE_CLASS_NUMBER);
            input.setHint("answer");
        } else {
            prompt.setText("Type “stop”  (" + count + " / " + GOAL + ")");
            answer = "stop";
            input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
            input.setHint("stop");
        }
    }

    private void check() {
        String v = input.getText().toString().trim().toLowerCase().replaceAll("\\s+", " ");
        input.setText("");
        if (!math && (v.equals("stop stop stop") || v.equals("stopstopstop"))) count = GOAL;
        else if (v.equals(answer)) { count++; error.setText(""); }
        else { count = 0; error.setText("Wrong — back to 0."); }
        drawDots();
        if (count >= GOAL) {
            AlarmService.stop(this);
            Toast.makeText(this, "Good morning! Have a great day.", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        nextQuestion();
        showKeyboard();
    }

    private void showKeyboard() {
        input.requestFocus();
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.showSoftInput(input, InputMethodManager.SHOW_IMPLICIT);
    }

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            clock.setText(DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date()));
            flash = !flash;
            root.setBackgroundColor(flash ? BG_B : BG_A);
            if (!Store.ringing(AlarmActivity.this)) { finish(); return; }
            handler.postDelayed(this, 700);
        }
    };

    @Override
    protected void onResume() {
        super.onResume();
        if (root == null) return;
        handler.post(tick);
        handler.postDelayed(this::showKeyboard, 300);
    }

    @Override
    protected void onPause() {
        handler.removeCallbacks(tick);
        super.onPause();
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        // Back does nothing: you have to solve the challenge.
    }
}
