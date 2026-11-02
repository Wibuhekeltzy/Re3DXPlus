#!/usr/bin/env python3
"""Run the real Activity's pure startup decisions on a host JVM, without Android or user data."""
from pathlib import Path
import subprocess
import tempfile

ANDROID = Path(__file__).resolve().parent.parent
SOURCE = ANDROID / "java/com/wmmt/port/MainActivity.java"
POLICY = ANDROID / "java/com/wmmt/port/DebugLogStorage.java"


def main():
    source = SOURCE.read_text(encoding="utf-8")
    start = source.index("  static int restorePendingPhase(")
    end = source.index("  @Override protected void onCreate", start)
    methods = source[start:end]
    harness = """package com.wmmt.port;
import java.util.*;
public class ActivityLogicHostTest {
""" + methods + """
  public static void main(String[] args) throws Exception {
    if (restorePendingPhase(1,0)!=0 || restorePendingPhase(1,1)!=1 ||
        restorePendingPhase(1,3)!=3 || restorePendingPhase(3,3)!=3 ||
        restorePendingPhase(3,1)!=0 || restorePendingPhase(0,3)!=0)
      throw new AssertionError("restore");
    final List<String> original = Arrays.asList("a","b","c","d","e");
    final List<String> list = new ArrayList<>(original);
    DebugLogStorage.Journal journal = new DebugLogStorage.Journal() {
      public List<String> read() { return new ArrayList<>(list); }
      public void write(List<String> next) { list.clear(); list.addAll(next); }
    };
    boolean status = rotateAfterWriterStart(false, journal,
        e -> { throw new AssertionError("deleted"); }, "new");
    if (status || !list.equals(original))
      throw new AssertionError("failed writer changed journal");
    System.out.println("Activity logic host tests passed");
  }
}
"""
    with tempfile.TemporaryDirectory(prefix="wmmt-activity-test-") as tmp:
        path = Path(tmp)
        java = path / "ActivityLogicHostTest.java"
        java.write_text(harness, encoding="utf-8")
        subprocess.run(["javac", "-d", str(path), str(POLICY), str(java)], check=True)
        subprocess.run(["java", "-cp", str(path), "com.wmmt.port.ActivityLogicHostTest"], check=True)

        start = source.index('  @Override protected void onActivityResult(')
        end = source.index('  /** The private lock', start)
        result_methods = source[start:end]
        lifecycle = r'''package com.wmmt.port;
class Base { protected void onActivityResult(int q,int r,Intent d) {} }
class Uri { String value; Uri(String x){value=x;} public String toString(){return value;} }
class Intent { static final int FLAG_GRANT_READ_URI_PERMISSION=1,FLAG_GRANT_WRITE_URI_PERMISSION=2; Uri uri; int flags; Intent(Uri u,int f){uri=u;flags=f;} Uri getData(){return uri;} int getFlags(){return flags;} }
public class MainActivity extends Base {
 static final int LOG_TREE_REQUEST=2071,RESULT_OK=-1;
 static int logPhase; static MainActivity currentActivity; static boolean deferredLogResult;
 static int deferredLogResultCode; static Intent deferredLogResultData;
 static boolean failGrant, failCommit, failSetup; static int grants, setups, starts;
 static boolean logStarted; static String logWarning;
 static class android { static class util { static class Log { static void w(String t,String s) {} } } }
 boolean installReady, destroyed, finishing; int warnings;
 boolean isFinishing(){return finishing;}
 static class Resolver {void takePersistableUriPermission(Uri u,int f){grants++;if(failGrant)throw new SecurityException();}}
 static class Prefs { Editor edit(){return new Editor();} }
 static class Editor { Editor putString(String k,String v){return this;} Editor putInt(String k,int v){return this;} boolean commit(){return !failCommit;} }
 Resolver getContentResolver(){return new Resolver();} Prefs logPrefs(){return new Prefs();}
 static boolean validTree(Uri u){return u!=null && u.value.equals("valid");}
 void setupLog(Uri u){if(logPhase!=3)throw new AssertionError(); setups++;logStarted=true; logWarning="writer active";logPhase=2; if(failSetup){starts++;throw new IllegalStateException();}}
 void finishWithoutLog(String s){warnings++;logPhase=2;starts++;}
''' + result_methods + r'''
 static void check(boolean b){if(!b)throw new AssertionError();}
 static void reset(){logPhase=1;currentActivity=null;deferredLogResult=false;deferredLogResultData=null;failGrant=false;failCommit=false;failSetup=false;logStarted=false;logWarning=null;grants=0;setups=0;starts=0;}
 static Intent good(){return new Intent(new Uri("valid"),3);}
 public static void main(String[] a){
  reset(); MainActivity old=new MainActivity(), fresh=new MainActivity();fresh.installReady=true;currentActivity=fresh;old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(grants==1&&setups==1&&logPhase==2);old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(grants==1&&setups==1);
  reset();fresh=new MainActivity();fresh.installReady=true;currentActivity=fresh;fresh.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(grants==1&&setups==1);
  reset();old=new MainActivity();old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(deferredLogResult&&grants==0);fresh=new MainActivity();currentActivity=fresh;fresh.consumeDeferredLogResult();check(grants==1&&setups==0&&logPhase==3&&!deferredLogResult);fresh.installReady=true;fresh.setupLog(new Uri("valid"));check(setups==1);
  reset();old=new MainActivity();fresh=new MainActivity();currentActivity=fresh;old.onActivityResult(LOG_TREE_REQUEST,0,null);check(fresh.warnings==1&&logPhase==2&&grants==0&&setups==0);
  reset();old=new MainActivity();fresh=new MainActivity();currentActivity=fresh;failGrant=true;old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(fresh.warnings==1&&setups==0&&logPhase==2);
  reset();old=new MainActivity();fresh=new MainActivity();currentActivity=fresh;failCommit=true;old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(fresh.warnings==1&&setups==0&&logPhase==2);
  reset();old=new MainActivity();fresh=new MainActivity();fresh.installReady=true;currentActivity=fresh;failSetup=true;
  try {old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());throw new AssertionError();} catch(IllegalStateException expected){}
  check(setups==1&&starts==1&&fresh.warnings==0&&logPhase==2&&logStarted&&"writer active".equals(logWarning));
  reset();old=new MainActivity();old.finishing=true;old.installReady=true;currentActivity=old;old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(deferredLogResult&&grants==0&&setups==0);old.consumeDeferredLogResult();check(deferredLogResult);fresh=new MainActivity();fresh.installReady=true;currentActivity=fresh;fresh.consumeDeferredLogResult();check(grants==1&&setups==1&&logPhase==2);
  reset();old=new MainActivity();old.destroyed=true;old.installReady=true;currentActivity=old;old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());check(deferredLogResult&&grants==0&&setups==0);fresh=new MainActivity();fresh.installReady=true;currentActivity=fresh;fresh.consumeDeferredLogResult();check(grants==1&&setups==1&&logPhase==2);
  reset();old=new MainActivity();old.finishing=true;currentActivity=old;old.onActivityResult(LOG_TREE_REQUEST,RESULT_OK,good());fresh=new MainActivity();currentActivity=fresh;failGrant=true;fresh.consumeDeferredLogResult();check(grants==1&&setups==0&&fresh.warnings==1&&logPhase==2);
  System.out.println("Activity result lifecycle host tests passed");
 }
}'''
        java = path / "MainActivity.java"
        java.write_text(lifecycle, encoding="utf-8")
        subprocess.run(["javac", "-d", str(path), str(java)], check=True)
        subprocess.run(["java", "-cp", str(path), "com.wmmt.port.MainActivity"], check=True)


if __name__ == "__main__":
    main()
