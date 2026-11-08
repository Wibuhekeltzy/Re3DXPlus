package com.wmmt.port;

import android.app.Activity;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.VideoView;

import java.io.File;

/* The Android side of the port, as main.m is the iOS one: the game draws on a SurfaceView
 * from its own thread (src/main_android.c), the attract movies play in a VideoView over it,
 * the BGM in a MediaPlayer (both decode the AAC/H.264 in hardware); touches, the gravity for
 * steering by tilt and the lifecycle go down to the native side. */
public class MainActivity extends Activity implements SurfaceHolder.Callback, SensorEventListener {
  static { System.loadLibrary("wmmt"); }

  static native void nativeStart(MainActivity a, String dataDir, String saveDir);
  static native void nativeSurface(Surface s, int w, int h);
  static native void nativeTouch(int id, int phase, float x, float y);
  static native void nativePause(boolean p);
  static native void nativeTilt(float radians);
  static native boolean nativeWantsTilt();
  static native void nativeMovieDone();
  static native void nativePressStart();
  static native void nativeMemoryWarning();

  /* the activity the native side is bound to (the last one created): the process can outlive
   * its activity (back from the game, then the icon again: a new MainActivity on the same
   * running game), and the old one's late callbacks (its surface going, its onDestroy) must not
   * reach what now belongs to the new one */
  private static MainActivity current;

  private FrameLayout root;
  private SurfaceView game;
  private VideoView movie;
  private MediaPlayer bgm;
  private String dataDir;
  private SensorManager sensors;
  private Sensor gravity;

  @Override protected void onCreate(Bundle b) {
    super.onCreate(b);
    current = this;
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
    if (android.os.Build.VERSION.SDK_INT >= 28)
      getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    root = new FrameLayout(this);
    game = new SurfaceView(this);
    game.getHolder().addCallback(this);
    game.setOnTouchListener((v, e) -> { touch(e); return true; });
    root.addView(game);
    setContentView(root);
    hideBars();
    /* the data as adb pushes it: /sdcard/Android/data/com.wmmt.port/files/wmmt (needs no
     * storage permission); the player's files next to it */
    File ext = getExternalFilesDir(null);
    File data = new File(ext, "wmmt");
    data.mkdirs();
    dataDir = data.getAbsolutePath();
    sensors = (SensorManager) getSystemService(SENSOR_SERVICE);
    gravity = sensors.getDefaultSensor(Sensor.TYPE_GRAVITY);
    nativeStart(this, dataDir, dataDir);
  }

  private void hideBars() {
    getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
  }

  @Override public void onWindowFocusChanged(boolean f) {
    super.onWindowFocusChanged(f);
    if (f) hideBars();
  }

  private void touch(MotionEvent e) {
    if (current != this) return;
    int a = e.getActionMasked();
    if (a == MotionEvent.ACTION_MOVE) {
      for (int i = 0; i < e.getPointerCount(); ++i) nativeTouch(e.getPointerId(i), 1, e.getX(i), e.getY(i));
      return;
    }
    int phase = a == MotionEvent.ACTION_DOWN || a == MotionEvent.ACTION_POINTER_DOWN ? 0
        : a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_POINTER_UP ? 2 : a == MotionEvent.ACTION_CANCEL ? 3 : -1;
    if (phase < 0) return;
    if (phase == 3) {
      /* the system took the gesture away (a notification pulled down, the palm rejection, a
       * second window): every finger on it goes down as cancelled, each with its own id and
       * last place, so the game lets go of what it held without counting it a tap */
      for (int i = 0; i < e.getPointerCount(); ++i) nativeTouch(e.getPointerId(i), 3, e.getX(i), e.getY(i));
      return;
    }
    int i = e.getActionIndex();
    nativeTouch(e.getPointerId(i), phase, e.getX(i), e.getY(i));
  }

  @Override public void surfaceCreated(SurfaceHolder h) {}
  @Override public void surfaceChanged(SurfaceHolder h, int fmt, int w, int ht) {
    Surface surface = h.getSurface();
    /* the game keeps its own 60 Hz time (src/timing60.h); this only asks a 90/120 Hz screen to
     * show its frames at a steady 60 (a hint: the system may keep the mode it has) */
    if (android.os.Build.VERSION.SDK_INT >= 30 && surface != null && surface.isValid()) {
      try { surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE); }
      catch (IllegalStateException | IllegalArgumentException ignored) { }
    }
    if (current == this) nativeSurface(surface, w, ht);
  }
  @Override public void surfaceDestroyed(SurfaceHolder h) { if (current == this) nativeSurface(null, 0, 0); }

  @Override protected void onPause() {
    super.onPause();
    if (current == this) nativePause(true);
    sensors.unregisterListener(this);
    if (bgm != null) bgm.pause();
    if (movie != null) movie.pause();
  }

  @Override protected void onResume() {
    super.onResume();
    hideBars();
    if (current == this) nativePause(false);
    if (gravity != null) sensors.registerListener(this, gravity, SensorManager.SENSOR_DELAY_GAME);
    if (bgm != null) bgm.start();
    if (movie != null) movie.start();
  }

  @Override protected void onDestroy() {
    if (current == this) current = null;
    if (bgm != null) {
      bgm.release();
      bgm = null;
    }
    if (movie != null) {
      movie.stopPlayback();
      movie = null;
    }
    super.onDestroy();
  }

  @Override public void onTrimMemory(int level) {
    super.onTrimMemory(level);
    if (current == this && level >= TRIM_MEMORY_RUNNING_LOW) nativeMemoryWarning();
  }

  /* the phone turned like a wheel (main.m's tilt): gravity's angle in the screen's plane from
   * its down, clockwise positive; nothing when it lies flat. In landscape the screen's right is
   * the sensor's -y (rotation 90) or +y (270) and its down the sensor's +x or -x. */
  @Override public void onSensorChanged(SensorEvent e) {
    if (current != this || !nativeWantsTilt()) return;
    float gx = e.values[0], gy = e.values[1];
    int rot = getWindowManager().getDefaultDisplay().getRotation();
    float right = rot == Surface.ROTATION_270 ? gy : -gy, down = rot == Surface.ROTATION_270 ? -gx : gx;
    float g2 = right * right + down * down;
    nativeTilt(g2 < 0.04f * 96.2f ? 0.0f : (float) Math.atan2(-right, down));
  }
  @Override public void onAccuracyChanged(Sensor s, int a) {}

  /* ---- called by the game's thread ---- */

  /* the attract movie (data dir's intro.mp4 etc.), or none for null */
  void showMovie(final String name) {
    runOnUiThread(() -> {
      if (current != this) return;
      if (movie != null) {
        movie.stopPlayback();
        root.removeView(movie);
        movie = null;
      }
      if (name == null) return;
      File f = new File(dataDir, name);
      if (!f.exists()) {
        nativeMovieDone();
        return;
      }
      movie = new VideoView(this);
      movie.setZOrderMediaOverlay(true);
      movie.setVideoPath(f.getAbsolutePath());
      movie.setOnCompletionListener(mp -> nativeMovieDone());
      movie.setOnErrorListener((mp, what, extra) -> { nativeMovieDone(); return true; });
      /* a tap on the movie is the arcade's "press the gas" */
      movie.setOnTouchListener((v, e) -> {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) nativePressStart();
        return true;
      });
      FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
          FrameLayout.LayoutParams.MATCH_PARENT, android.view.Gravity.CENTER);
      /* the intro's picture is letterboxed in its 4:3 (640x326 at y 76): on a wider screen it
       * goes the screen's width, its black bands off the top and bottom; playdemo and tutorial
       * fill their 4:3 and stay so */
      int w = root.getWidth(), h = root.getHeight();
      if (name.equals("intro.mp4") && w * 3 > h * 4) {
        lp.width = w;
        lp.height = w * 3 / 4;
      }
      root.addView(movie, lp);
      movie.start();
    });
  }

  /* data dir's sound/bgm/<name>.m4a over and over, or nothing for null */
  void playBgm(final String name) {
    runOnUiThread(() -> {
      if (current != this) return;
      if (bgm != null) {
        bgm.release();
        bgm = null;
      }
      if (name == null) return;
      File f = new File(dataDir, "sound/bgm/" + name + ".m4a");
      if (!f.exists()) return;
      try {
        bgm = new MediaPlayer();
        bgm.setDataSource(f.getAbsolutePath());
        bgm.setLooping(true);
        bgm.prepare();
        bgm.start();
      } catch (Exception ex) {
        bgm = null;
      }
    });
  }

  /* the BGM's level (the story ending's fade out; a new track starts at full) */
  void bgmLevel(final float level) {
    runOnUiThread(() -> {
      if (current == this && bgm != null) bgm.setVolume(level, level);
    });
  }

  void fail(final String msg) {
    runOnUiThread(() -> {
      if (current != this || isFinishing()) return;
      new android.app.AlertDialog.Builder(this).setTitle("WMMT3DX").setMessage(msg)
          .setPositiveButton("OK", null).show();
    });
  }
}
