package com.wmmt.port;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class DebugLogStorageTest {
  static final class Journal implements DebugLogStorage.Journal {
    List<String> entries = new ArrayList<>();
    boolean fail;
    public List<String> read() { return new ArrayList<>(entries); }
    public void write(List<String> next) throws Exception {
      if (fail) throw new Exception("commit");
      entries = new ArrayList<>(next);
    }
  }
  static void check(boolean b) { if (!b) throw new AssertionError(); }
  public static void main(String[] args) throws Exception {
    String token = "0123456789abcdef0123456789abcdef";
    check(token.equals(DebugLogStorage.tokenFromName("wmmt-debug-" + token + ".log")));
    check(token.equals(DebugLogStorage.tokenFromName("wmmt-debug-" + token + ".txt")));
    check(DebugLogStorage.tokenFromName("wmmt-debug-" + token + ".log.txt") == null);
    check(DebugLogStorage.tokenFromName("wmmt-debug-" + token.toUpperCase() + ".txt") == null);
    // Model the external-storage provider's MIME suffix correction, not an Android SDK guarantee.
    String oldRequest = "wmmt-debug-" + token + ".log";
    String newRequest = "wmmt-debug-" + token + ".txt";
    String oldResponse = oldRequest + ".txt";
    String newResponse = newRequest;
    check(!oldRequest.equals(oldResponse) && DebugLogStorage.tokenFromName(oldResponse) == null);
    check(newRequest.equals(newResponse) && token.equals(DebugLogStorage.tokenFromName(newResponse)));
    check(DebugLogStorage.tokenFromName(newResponse + ".txt") == null);
    Journal j = new Journal();
    List<String> deleted = new ArrayList<>();
    for (int i = 0; i < 6; i++)
      check(DebugLogStorage.recordAndRotate(j, e -> { deleted.add(e); return true; }, "log" + i));
    check(deleted.equals(Arrays.asList("log0")));
    check(j.entries.equals(Arrays.asList("log1", "log2", "log3", "log4", "log5")));
    check(!DebugLogStorage.recordAndRotate(j, e -> false, "log6"));
    check(j.entries.size() == 6 && j.entries.get(0).equals("log1"));
    j.fail = true;
    try { DebugLogStorage.recordAndRotate(j, e -> { throw new AssertionError("deleted before commit"); }, "log7"); throw new AssertionError(); }
    catch (Exception expected) { }
    j.fail = false;
    try { DebugLogStorage.recordAndRotate(j, e -> true, "log6"); throw new AssertionError(); }
    catch (IllegalStateException expected) { }
    Journal missing = new Journal();
    missing.entries.addAll(Arrays.asList("gone", "a", "b", "c", "d"));
    check(DebugLogStorage.recordAndRotate(missing, e -> e.equals("gone"), "new"));
    check(missing.entries.equals(Arrays.asList("a", "b", "c", "d", "new")));
    Journal denied = new Journal();
    denied.entries.addAll(Arrays.asList("protected", "a", "b", "c", "d"));
    try { DebugLogStorage.recordAndRotate(denied, e -> { throw new SecurityException(); }, "new"); throw new AssertionError(); }
    catch (SecurityException expected) { }
    check(denied.entries.size() == 6 && denied.entries.get(0).equals("protected"));
    System.out.println("DebugLogStorage tests passed");
  }
}
