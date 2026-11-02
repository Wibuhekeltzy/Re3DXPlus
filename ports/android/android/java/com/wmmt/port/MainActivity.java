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
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/* The Android side of the port, as main.m is the iOS one: the game draws on a SurfaceView
 * from its own thread (src/main_android.c), the attract movies play in a VideoView over it,
 * the BGM in a MediaPlayer (both decode the AAC/H.264 in hardware); touches, the gravity for
 * steering by tilt and the lifecycle go down to the native side. */
public class MainActivity extends Activity implements SurfaceHolder.Callback, SensorEventListener {
  static { System.loadLibrary("wmmt"); }

  static native void nativeStart(MainActivity a, String dataDir, String saveDir, String sidecarDir);
  static native void nativeSurface(Surface s, int w, int h);
  static native void nativeTouch(int id, int phase, float x, float y);
  static native void nativePause(boolean p);
  static native void nativeTilt(float radians);
  static native boolean nativeWantsTilt();
  static native void nativeMovieDone();
  static native void nativePressStart();
  static native void nativeMemoryWarning();
  static native void nativeBgmFailure(MainActivity owner, int group);

  private static MainActivity currentActivity;
  private boolean installReady;
  private String saveDir;
  private FrameLayout root;
  private SurfaceView game;
  private VideoView movie;
  private MediaPlayer bgm;
  private float bgmVolume = 1.0f; /* legacy Android player default */
  private String dataDir;
  private File cardResources;
  private SensorManager sensors;
  private Sensor gravity;
  private TextView progress;
  private volatile boolean started, destroyed;
  private boolean resumed, bgmPreparing;
  private volatile long bgmGeneration;
  private ExecutorService bgmExecutor;
  private int surfaceWidth, surfaceHeight;
  private static final Object INSTALL_LOCK = new Object();
  /* Held until process exit: another cooperating process must not install while native runs. */
  private static DataCoordinator.ProcessLock processInstallLock;
  private static String nativeDataDir;
  private static File nativeCardResources;

  private void lockInstallProcess() throws IOException {
    if (processInstallLock != null && processInstallLock.valid()) return;
    processInstallLock = DataCoordinator.acquire(new File(getFilesDir(), "game-data-install.lock").toPath());
  }
  private static final String ASSET = "re3dxplus-data.zip";
  private static final String MARKER = ".re3dxplus-install-v1";
  private static final String CUTIN_ASSET = "story-cutin.bin";
  private static final long CUTIN_LIMIT = 256L * 1024 * 1024;

  @Override protected void onCreate(Bundle b) {
    super.onCreate(b);
    currentActivity = this;
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
    progress = new TextView(this);
    progress.setTextColor(0xffffffff);
    progress.setBackgroundColor(0xff202020);
    progress.setPadding(24, 24, 24, 24);
    progress.setText("Preparing game data…");
    root.addView(progress);
    sensors = (SensorManager) getSystemService(SENSOR_SERVICE);
    gravity = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_GRAVITY);
    File ext = getExternalFilesDir(null);
    if (ext == null) { installError("External app storage is unavailable."); return; }
    File save = new File(ext, "wmmt");
    new Thread(() -> {
      try {
        File data;
        File sidecar = null;
        synchronized (INSTALL_LOCK) {
          lockInstallProcess();
          if (!save.isDirectory() && !save.mkdirs()) throw new IOException("Cannot access save directory");
          data = nativeDataDir == null ? prepareData(ext, save) : new File(nativeDataDir);
          if (nativeDataDir == null) {
            try { prepareCutin(ext); }
            catch (Exception optional) { android.util.Log.w("WMMT", "Optional cut-in asset unavailable", optional); }
            try { prepareCardPages(ext); }
            catch (Exception optional) { android.util.Log.w("WMMT", "Optional card pages unavailable", optional); }
            try { prepareBackfire(ext); }
            catch (Exception optional) { android.util.Log.w("WMMT", "Optional backfire assets unavailable", optional); }
            try { sidecar = CardResources.prepare(getAssets(), ext); }
            catch (Exception optional) { android.util.Log.w("WMMT", "Optional card resources unavailable", optional); }
          } else sidecar = nativeCardResources;
        }
        final File ready = data;
        final File cardResources = sidecar;
        runOnUiThread(() -> {
          if (destroyed || currentActivity != this || isFinishing()) return;
          dataDir = ready.getAbsolutePath();
          this.cardResources = cardResources;
          saveDir = save.getAbsolutePath();
          installReady = true;
          startGame();
        });
      } catch (Exception ex) {
        runOnUiThread(() -> { if (!destroyed && !isFinishing()) installError("Game data installation failed: " + ex.getMessage()); });
      }
    }, "game-data-installer").start();
  }

  private void startGame() {
    if (!installReady || started || destroyed || currentActivity != this || isFinishing()) return;
    synchronized (INSTALL_LOCK) {
      if (nativeDataDir == null) { nativeDataDir = dataDir; nativeCardResources = cardResources; }
      dataDir = nativeDataDir; // nativeStart can retain the first process-wide data path.
      cardResources = nativeCardResources;
      nativeStart(this, dataDir, saveDir, cardResources == null ? null : cardResources.getAbsolutePath());
      started = true;
    }
    root.removeView(progress);
    Surface surface = game.getHolder().getSurface();
    if (surfaceWidth > 0 && surfaceHeight > 0 && surface != null && surface.isValid())
      nativeSurface(surface, surfaceWidth, surfaceHeight);
    nativePause(!resumed);
  }

  private void installError(String message) {
    progress.setText(message + "\nClose and reopen the app to retry. Existing saves were not removed.");
  }

  private void installProgress(String text) {
    runOnUiThread(() -> { if (!destroyed && !isFinishing()) progress.setText(text); });
  }

  /* Separate from the large ZIP version and from save files. Rehash both copies on every
   * launch; a same-directory temporary plus rename leaves the old version intact on failure. */
  private byte[] cutinDigest(InputStream in) throws Exception {
    MessageDigest sha = MessageDigest.getInstance("SHA-256");
    byte[] buf = new byte[65536]; long total = 0; int n;
    while ((n = in.read(buf)) != -1) {
      total += n;
      if (total > CUTIN_LIMIT) throw new IOException("Cut-in asset exceeds limit");
      sha.update(buf, 0, n);
    }
    if (total == 0) throw new IOException("Empty cut-in asset");
    return sha.digest();
  }

  private void prepareCutin(File ext) throws Exception {
    String[] assets = getAssets().list("");
    if (assets == null) throw new IOException("Cannot list APK assets");
    boolean present = false;
    for (String name : assets) if (CUTIN_ASSET.equals(name)) present = true;
    if (!present) return;
    byte[] expected;
    try (InputStream in = getAssets().open(CUTIN_ASSET)) { expected = cutinDigest(in); }
    File dir = new File(ext, "wmmt-sidecars");
    if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create cut-in directory");
    if (!dir.getCanonicalFile().equals(new File(ext.getCanonicalFile(), "wmmt-sidecars")))
      throw new IOException("Unsafe cut-in directory");
    File dest = new File(dir, CUTIN_ASSET);
    if (!dest.getCanonicalFile().equals(new File(dir.getCanonicalFile(), CUTIN_ASSET)))
      throw new IOException("Unsafe cut-in file");
    if (dest.isFile()) {
      try (InputStream in = new FileInputStream(dest)) {
        if (MessageDigest.isEqual(expected, cutinDigest(in))) return;
      } catch (IOException ignored) { /* replace a damaged copy */ }
    }
    File temp = File.createTempFile(".story-cutin-", ".tmp", dir);
    try {
      byte[] buffer = new byte[65536]; long total = 0;
      MessageDigest sha = MessageDigest.getInstance("SHA-256");
      try (InputStream in = getAssets().open(CUTIN_ASSET); FileOutputStream out = new FileOutputStream(temp)) {
        int n; while ((n = in.read(buffer)) != -1) {
          total += n;
          if (total > CUTIN_LIMIT) throw new IOException("Cut-in asset exceeds limit");
          sha.update(buffer, 0, n); out.write(buffer, 0, n);
        }
        out.getFD().sync();
      }
      if (!MessageDigest.isEqual(expected, sha.digest())) throw new IOException("Cut-in asset changed during copy");
      try (InputStream in = new FileInputStream(temp)) {
        if (!MessageDigest.isEqual(expected, cutinDigest(in))) throw new IOException("Cut-in copy verification failed");
      }
      if (!temp.renameTo(dest)) throw new IOException("Cannot publish cut-in asset");
    } finally { if (temp.exists() && !temp.delete()) android.util.Log.w("WMMT", "Cannot remove cut-in temporary file"); }
  }

  /* The card resource bank is independent of the game archive and save data. */
  private void prepareCardPages(File ext) throws Exception {
    final String asset = "card-pages.bin";
    String[] assets = getAssets().list("");
    if (assets == null) throw new IOException("Cannot list APK assets");
    boolean present = false;
    for (String name : assets) if (asset.equals(name)) present = true;
    if (!present) return;
    byte[] expected;
    try (InputStream in = getAssets().open(asset)) { expected = cutinDigest(in); }
    File dir = new File(ext, "wmmt-sidecars");
    if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create sidecar directory");
    if (!dir.getCanonicalFile().equals(new File(ext.getCanonicalFile(), "wmmt-sidecars")))
      throw new IOException("Unsafe sidecar directory");
    File dest = new File(dir, asset);
    if (!dest.getCanonicalFile().equals(new File(dir.getCanonicalFile(), asset)))
      throw new IOException("Unsafe card pages file");
    if (dest.isFile()) {
      try (InputStream in = new FileInputStream(dest)) {
        if (MessageDigest.isEqual(expected, cutinDigest(in))) return;
      } catch (IOException ignored) { /* retry using the packaged source */ }
    }
    File temp = File.createTempFile(".card-pages-", ".tmp", dir);
    try {
      byte[] buffer = new byte[65536]; long total = 0;
      MessageDigest sha = MessageDigest.getInstance("SHA-256");
      try (InputStream in = getAssets().open(asset); FileOutputStream out = new FileOutputStream(temp)) {
        int n; while ((n = in.read(buffer)) != -1) {
          total += n;
          if (total > CUTIN_LIMIT) throw new IOException("Card pages exceed limit");
          sha.update(buffer, 0, n); out.write(buffer, 0, n);
        }
        out.getFD().sync();
      }
      if (!MessageDigest.isEqual(expected, sha.digest())) throw new IOException("Card pages changed during copy");
      try (InputStream in = new FileInputStream(temp)) {
        if (!MessageDigest.isEqual(expected, cutinDigest(in))) throw new IOException("Card pages copy verification failed");
      }
      if (!temp.renameTo(dest)) throw new IOException("Cannot publish card pages");
    } finally { if (temp.exists() && !temp.delete()) android.util.Log.w("WMMT", "Cannot remove card pages temporary file"); }
  }

  /* Publish an immutable, verified pair directory before atomically replacing its pointer.
   * The pointer is exactly 64 lowercase hex bytes: SHA-256(WPK digest || BFX digest).
   * Native must read the pointer once and validate it before constructing either path. Old
   * versions are retained for concurrent readers. This is not a power-loss transaction. */
  private void prepareBackfire(File ext) throws Exception {
    final String[] names = { "backfire.wpk", "backfire.bfx" };
    String[] assets = getAssets().list("");
    if (assets == null) throw new IOException("Cannot list APK assets");
    boolean[] present = new boolean[2];
    for (String asset : assets)
      for (int i = 0; i < names.length; i++) if (names[i].equals(asset)) present[i] = true;
    if (!present[0] && !present[1]) return;
    if (!present[0] || !present[1]) throw new IOException("Incomplete packaged backfire pair");

    byte[][] expected = new byte[2][];
    for (int i = 0; i < names.length; i++)
      try (InputStream in = getAssets().open(names[i])) { expected[i] = cutinDigest(in); }
    File dir = new File(ext, "wmmt-sidecars");
    if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create sidecar directory");
    if (!dir.getCanonicalFile().equals(new File(ext.getCanonicalFile(), "wmmt-sidecars")))
      throw new IOException("Unsafe backfire directory");
    MessageDigest pairHash = MessageDigest.getInstance("SHA-256");
    pairHash.update(expected[0]); pairHash.update(expected[1]);
    StringBuilder hex = new StringBuilder();
    for (byte b : pairHash.digest()) hex.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
    String version = hex.toString();
    File dest = new File(dir, "backfire-" + version);
    if (!dest.getCanonicalFile().equals(new File(dir.getCanonicalFile(), dest.getName())))
      throw new IOException("Unsafe backfire version directory");
    File stage = null, pointerTemp = null;
    try {
      if (!dest.exists()) {
        File candidate = new File(dir, ".backfire-stage-" + UUID.randomUUID());
        if (!candidate.mkdir()) throw new IOException("Cannot create backfire staging directory");
        stage = candidate;
        for (int i = 0; i < names.length; i++) {
          File file = new File(stage, names[i]);
          MessageDigest sha = MessageDigest.getInstance("SHA-256");
          byte[] buffer = new byte[65536]; long total = 0;
          try (InputStream in = getAssets().open(names[i]); FileOutputStream out = new FileOutputStream(file)) {
            int n; while ((n = in.read(buffer)) != -1) {
              total += n;
              if (total > CUTIN_LIMIT) throw new IOException("Backfire asset exceeds limit: " + names[i]);
              sha.update(buffer, 0, n); out.write(buffer, 0, n);
            }
            out.getFD().sync();
          }
          if (!MessageDigest.isEqual(expected[i], sha.digest()))
            throw new IOException("Backfire asset changed during copy: " + names[i]);
        }
        verifyBackfirePair(stage, names, expected);
        if (!stage.renameTo(dest) && !dest.isDirectory())
          throw new IOException("Cannot publish backfire directory");
      }
      verifyBackfirePair(dest, names, expected);
      File pointer = new File(dir, "backfire-current");
      if (!pointer.getCanonicalFile().equals(new File(dir.getCanonicalFile(), "backfire-current")))
        throw new IOException("Unsafe backfire pointer");
      pointerTemp = File.createTempFile(".backfire-current-", ".tmp", dir);
      try (FileOutputStream out = new FileOutputStream(pointerTemp)) {
        out.write(version.getBytes("US-ASCII")); out.getFD().sync();
      }
      if (!pointerTemp.renameTo(pointer)) throw new IOException("Cannot publish backfire pointer");
    } finally {
      if (pointerTemp != null && pointerTemp.exists() && !pointerTemp.delete())
        android.util.Log.w("WMMT", "Cannot remove backfire pointer temporary file");
      if (stage != null && stage.exists()) {
        for (String name : names) {
          File file = new File(stage, name);
          if (file.exists() && !file.delete()) android.util.Log.w("WMMT", "Cannot remove backfire staging file");
        }
        if (!stage.delete()) android.util.Log.w("WMMT", "Cannot remove backfire staging directory");
      }
    }
  }

  private void verifyBackfirePair(File dir, String[] names, byte[][] expected) throws Exception {
    if (!dir.isDirectory()) throw new IOException("Backfire version is not a directory");
    for (int i = 0; i < names.length; i++) {
      File file = new File(dir, names[i]);
      if (!file.getCanonicalFile().equals(new File(dir.getCanonicalFile(), names[i])) || !file.isFile())
        throw new IOException("Unsafe or missing backfire file: " + names[i]);
      try (InputStream in = new FileInputStream(file)) {
        if (!MessageDigest.isEqual(expected[i], cutinDigest(in)))
          throw new IOException("Backfire verification failed: " + names[i]);
      }
    }
  }

  /* The private lock remains held for the lifetime of this process, including native use.
   * Other writers such as root/ADB must not mutate data concurrently. */
  private File prepareData(File ext, File legacy) throws Exception {
    String[] assets = getAssets().list("");
    if (assets == null) throw new IOException("Cannot list APK assets");
    boolean present = false;
    for (String name : assets) if (ASSET.equals(name)) present = true;
    File stateRoot = getNoBackupFilesDir();
    if (stateRoot == null) throw new IOException("Private update storage is unavailable");
    /* No released version uses this earlier draft path. Never silently discard it. */
    if (new File(getFilesDir(), "game-data-state").exists())
      throw new IOException("Unrecognized existing update state; preserve it and contact support");
    if (!present) {
      if (new File(stateRoot, "game-data-state/pending").exists())
        throw new IOException("Interrupted data update requires its packaged ZIP to repair");
      return legacy;
    }
    installProgress("Checking packaged game data…");
    DataManifest.Limits limits = new DataManifest.Limits(200000, 50L * 1024 * 1024 * 1024,
        Integer.MAX_VALUE, 64 * 1024 * 1024);
    File canonicalExt = ext.getCanonicalFile();
    File privateDir = stateRoot.getCanonicalFile();
    DataCoordinator coordinator = new DataCoordinator(new File(canonicalExt, "re3dxplus-data").toPath(),
        new File(privateDir, "game-data-state").toPath(), limits);
    return coordinator.prepare(() -> getAssets().open(ASSET), null).toFile();
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
    if (!started || currentActivity != this) return;
    int a = e.getActionMasked();
    if (a == MotionEvent.ACTION_MOVE) {
      for (int i = 0; i < e.getPointerCount(); ++i) nativeTouch(e.getPointerId(i), 1, e.getX(i), e.getY(i));
      return;
    }
    int phase = a == MotionEvent.ACTION_DOWN || a == MotionEvent.ACTION_POINTER_DOWN ? 0
        : a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_POINTER_UP ? 2 : a == MotionEvent.ACTION_CANCEL ? 3 : -1;
    if (phase < 0) return;
    if (phase == 3) {
      /* Preserve each cancelled pointer's identity and coordinates for the native bridge. */
      for (int i = 0; i < e.getPointerCount(); ++i) {
        nativeTouch(e.getPointerId(i), phase, e.getX(i), e.getY(i));
      }
      return;
    }
    int i = e.getActionIndex();
    nativeTouch(e.getPointerId(i), phase, e.getX(i), e.getY(i));
  }

  @Override public void surfaceCreated(SurfaceHolder h) {}
  @Override public void surfaceChanged(SurfaceHolder h, int fmt, int w, int ht) {
    surfaceWidth = w; surfaceHeight = ht;
    Surface surface = h.getSurface();
    // Hint only: the OS may retain a different display mode. Native pacing remains authoritative.
    if (android.os.Build.VERSION.SDK_INT >= 30 && surface != null && surface.isValid()) {
      try { surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE); }
      catch (IllegalStateException | IllegalArgumentException ignored) { }
    }
    if (started && currentActivity == this) nativeSurface(surface, w, ht);
  }
  @Override public void surfaceDestroyed(SurfaceHolder h) {
    surfaceWidth = surfaceHeight = 0;
    if (started && currentActivity == this) nativeSurface(null, 0, 0);
  }

  @Override protected void onPause() {
    super.onPause();
    resumed = false;
    if (started && currentActivity == this) nativePause(true);
    if (sensors != null) sensors.unregisterListener(this);
    if (bgm != null && !bgmPreparing) bgm.pause();
    if (movie != null) movie.pause();
  }

  @Override protected void onResume() {
    super.onResume();
    hideBars();
    resumed = true;
    if (started && currentActivity == this) nativePause(false);
    if (gravity != null && sensors != null) sensors.registerListener(this, gravity, SensorManager.SENSOR_DELAY_GAME);
    if (bgm != null && !bgmPreparing) bgm.start();
    if (movie != null) movie.start();
  }

  @Override protected void onDestroy() {
    destroyed = true;
    if (currentActivity == this) currentActivity = null;
    ++bgmGeneration;
    if (bgmExecutor != null) { bgmExecutor.shutdownNow(); bgmExecutor = null; }
    if (bgm != null) { bgm.release(); bgm = null; }
    bgmPreparing = false;
    if (movie != null) { movie.stopPlayback(); movie = null; }
    super.onDestroy();
  }

  @Override public void onTrimMemory(int level) {
    super.onTrimMemory(level);
    if (started && currentActivity == this && level >= TRIM_MEMORY_RUNNING_LOW) nativeMemoryWarning();
  }

  /* the phone turned like a wheel (main.m's tilt): gravity's angle in the screen's plane from
   * its down, clockwise positive; nothing when it lies flat. In landscape the screen's right is
   * the sensor's -y (rotation 90) or +y (270) and its down the sensor's +x or -x. */
  @Override public void onSensorChanged(SensorEvent e) {
    if (!started || currentActivity != this || !nativeWantsTilt()) return;
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
      if (destroyed || isFinishing()) return;
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

  /* All player access is on the UI thread, including changes to a playing track. */
  void setBgmVolume(final float volume) {
    if (Float.isNaN(volume) || Float.isInfinite(volume) || volume < 0 || volume > 1) return;
    runOnUiThread(() -> {
      if (destroyed || isFinishing()) return;
      bgmVolume = volume;
      if (bgm != null && !bgmPreparing) bgm.setVolume(volume, volume);
    });
  }

  /* data dir's sound/bgm/<name>.m4a over and over, or nothing for null */
  void playBgm(final String name) {
    runOnUiThread(() -> {
      if (destroyed || isFinishing()) return;
      final long generation = ++bgmGeneration;
      if (bgm != null) {
        bgm.release();
        bgm = null;
      }
      bgmPreparing = false;
      if (name == null) return;
      final int group = name.matches("wm2_[0-9][0-9]") ? 1 : name.matches("wmr_[0-9][0-9]") ? 2 : 0;
      if (group != 0) {
        if (started) queueExtraBgm(name, group, generation);
        return;
      }
      File f = new File(dataDir, "sound/bgm/" + name + ".m4a");
      if (f == null || !f.isFile()) {
        if (group != 0) nativeBgmFailure(this, group);
        android.util.Log.w("WMMT", "BGM unavailable: " + name);
        return;
      }
      try {
        bgm = new MediaPlayer();
        final MediaPlayer player = bgm;
        player.setOnErrorListener((mp, what, extra) -> {
          android.util.Log.w("WMMT", "BGM player error " + what + "/" + extra + ": " + name);
          if (!destroyed && !isFinishing() && bgm == mp) {
            if (group != 0) nativeBgmFailure(this, group);
            bgm = null;
            mp.release();
          }
          return true;
        });
        bgm.setDataSource(f.getAbsolutePath());
        bgm.setLooping(true);
        bgm.prepare();
        bgm.setVolume(bgmVolume, bgmVolume);
        if (resumed) bgm.start();
      } catch (Exception ex) {
        android.util.Log.w("WMMT", "BGM playback failed: " + name, ex);
        if (group != 0) nativeBgmFailure(this, group);
        if (bgm != null) bgm.release();
        bgm = null;
      }
    });
  }

  /* Generation belongs to this Activity. The worker only reads the immutable directory and
   * verifies bytes; creation, callbacks, and every MediaPlayer operation stay on the UI thread. */
  private boolean currentExtraBgm(long generation) {
    return started && !destroyed && !isFinishing() && generation == bgmGeneration;
  }

  private void queueExtraBgm(final String name, final int group, final long generation) {
    final File resources = cardResources;
    try {
      if (bgmExecutor == null) bgmExecutor = Executors.newSingleThreadExecutor();
      bgmExecutor.execute(() -> {
        if (destroyed || !started || generation != bgmGeneration) return;
        File verified = null;
        try { verified = CardResources.music(resources, name); }
        catch (Exception ex) { android.util.Log.w("WMMT", "BGM verification failed: " + name, ex); }
        if (destroyed || !started || generation != bgmGeneration) return;
        final File file = verified;
        runOnUiThread(() -> {
          if (!currentExtraBgm(generation)) return;
          if (file == null || !file.isFile()) {
            android.util.Log.w("WMMT", "BGM unavailable: " + name);
            nativeBgmFailure(this, group);
            return;
          }
          prepareExtraBgm(file, name, group, generation);
        });
      });
    } catch (RuntimeException ex) {
      android.util.Log.w("WMMT", "BGM verification scheduling failed: " + name, ex);
      if (currentExtraBgm(generation)) nativeBgmFailure(this, group);
    }
  }

  private void extraBgmFailed(MediaPlayer player, String name, int group, long generation) {
    if (bgm != player || !currentExtraBgm(generation)) return;
    bgm = null;
    bgmPreparing = false;
    player.release();
    android.util.Log.w("WMMT", "BGM playback failed: " + name);
    nativeBgmFailure(this, group);
  }

  private void prepareExtraBgm(File file, String name, int group, long generation) {
    MediaPlayer player = null;
    try {
      player = new MediaPlayer();
      final MediaPlayer pending = player;
      bgm = pending;
      bgmPreparing = true;
      pending.setOnErrorListener((mp, what, extra) -> {
        extraBgmFailed(mp, name, group, generation);
        return true;
      });
      pending.setOnPreparedListener(mp -> {
        if (bgm != mp || !currentExtraBgm(generation)) return;
        try {
          bgmPreparing = false;
          mp.setVolume(bgmVolume, bgmVolume);
          if (resumed) mp.start();
        } catch (Exception ex) {
          android.util.Log.w("WMMT", "BGM start failed: " + name, ex);
          extraBgmFailed(mp, name, group, generation);
        }
      });
      pending.setDataSource(file.getAbsolutePath());
      pending.setLooping(true);
      pending.prepareAsync();
    } catch (Exception ex) {
      android.util.Log.w("WMMT", "BGM preparation failed: " + name, ex);
      if (player != null) extraBgmFailed(player, name, group, generation);
      else if (currentExtraBgm(generation)) nativeBgmFailure(this, group);
    }
  }

  void fail(final String msg) {
    runOnUiThread(() -> new android.app.AlertDialog.Builder(this).setTitle("WMMT3DX").setMessage(msg)
        .setPositiveButton("OK", null).show());
  }
}
