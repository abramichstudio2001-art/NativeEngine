# Study Buddy (Android)

A homework, notes, sleep and wake-up alarm app for students, packaged as a native Android app.

## Get the APK

Every push builds the app with GitHub Actions and publishes it under **Releases**:
open the latest release on your phone, download `StudyBuddy.apk`, and install it
(allow "Install unknown apps" for your browser if Android asks). Requires Android 8.0+.

Updates install over the previous version and keep your data.

## Features

- **Today** – homework progress, next alarm, tonight's suggested bedtime, what's due next, pinned notes.
- **Homework** – subjects with colours, due dates with quick picks, priority, grouped by Overdue / Today / Tomorrow / This week, done list, undo.
- **Reminders** – real Android notifications before homework is due (15 min – 1 day) and for notes, even when the app is closed.
- **Notes** – search, pin to top, optional reminder.
- **Sleep** – log bed/wake times and how you feel, 7-night chart against your goal, averages, bedtime suggestion.
- **Wake-up alarm** – rings via Android's alarm system even when the app is closed or the phone is locked, uses your alarm sound and volume, shows over the lock screen. Turn it off by typing `stop` 3 times or solving 3 sums – a wrong answer resets to 0.
- Dark / light / system theme, backup export & import.

## Build locally

Requires JDK 17 and the Android SDK (platform 34):

```sh
./gradlew assembleRelease
# -> app/build/outputs/apk/release/app-release.apk
```

## Signing

By default builds are signed with `keystore/studybuddy.jks` (password `studybuddy`) so that each
new build can be installed over the old one. Because this repository is public, that key is not
secret. To use your own private key, add these repository secrets and the workflow uses them instead:
`SIGNING_KEYSTORE_BASE64` (base64 of a .jks file), `SIGNING_STORE_PASSWORD`, `SIGNING_KEY_ALIAS`,
`SIGNING_KEY_PASSWORD`. (Switching keys means uninstalling the old build once – export a backup first.)

The UI lives in `app/src/main/assets/index.html` and also runs in a normal browser.
