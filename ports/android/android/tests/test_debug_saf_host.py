#!/usr/bin/env python3
"""Extract production SAF methods and run against host fakes; not an Android provider."""
from pathlib import Path
import tempfile, subprocess
root=Path(__file__).resolve().parent.parent
s=(root/'java/com/wmmt/port/MainActivity.java').read_text()
methods=s[s.index('  private boolean ownedLog('):s.index('  private DebugLogStorage.Journal journal()')]
methods+=s[s.index('  private boolean deleteVerifiedLog('):s.index('  private void finishWithoutLog(')]
start=s.index('  static boolean rotateAfterWriterStart(')
methods+=s[start:s.index('\n  }',start)+4]
harness=r'''package com.wmmt.port;
import java.io.*; import java.nio.file.*; import java.nio.charset.StandardCharsets; import java.util.*;
public class SafHost {
 static final List<String> events=new ArrayList<>();
 static final String AUTHORITY="fake";
 static class Uri {String s;Uri(String s){this.s=s;}static Uri parse(String s){return new Uri(s);}String getAuthority(){return s.startsWith("content://fake/")?"fake":"bad";}String getScheme(){return s.startsWith("content://")?"content":"bad";}public String toString(){return s;}public boolean equals(Object x){return x instanceof Uri && s.equals(((Uri)x).s);}}
 static class Doc {String name,mime="text/plain";Path path;Doc(String n,Path p){name=n;path=p;}}
 static class DocumentsContract {
  static class Document {static String COLUMN_DISPLAY_NAME="name",COLUMN_MIME_TYPE="mime";}
  static String getTreeDocumentId(Uri u){return "primary:WMMT";} static String getDocumentId(Uri u){if(!u.s.startsWith("content://fake/"))throw new IllegalArgumentException();return u.s.substring(15);}
  static Uri buildDocumentUriUsingTree(Uri u,String id){return new Uri("content://fake/"+id);}
  static Uri createDocument(Resolver r,Uri p,String m,String n)throws Exception{events.add("create:"+p+":"+m+":"+n);return r.create(n);}
  static boolean deleteDocument(Resolver r,Uri u)throws Exception{return r.delete(u);}
 }
 static class Cursor implements AutoCloseable {Doc d;Cursor(Doc d){this.d=d;}boolean moveToFirst(){return d!=null;}String getString(int i){return i==0?d.name:d.mime;}public void close(){}}
 static class ParcelFileDescriptor implements AutoCloseable {static int next;static Map<Integer,RandomAccessFile> fds=new HashMap<>();RandomAccessFile f;ParcelFileDescriptor(RandomAccessFile f){this.f=f;}FileDescriptor getFileDescriptor()throws Exception{return f.getFD();}int detachFd(){int x=++next;fds.put(x,f);f=null;return x;}public void close()throws Exception{if(f!=null)f.close();}}
 static class StructStat {int st_mode=1,st_nlink=1;} static class OsConstants {static boolean S_ISREG(int n){return n==1;}}
 static class Os {static StructStat stat=new StructStat();static boolean fail;static StructStat fstat(FileDescriptor f){return stat;}static int write(FileDescriptor f,byte[] b,int o,int n)throws Exception{if(fail)throw new IOException();events.add("write");n=Math.min(n,7);new FileOutputStream(f).write(b,o,n);return n;}}
 static class Resolver {Path dir;Map<String,Doc> docs=new HashMap<>();boolean suffix,failOpen,failDelete,failQuery;Resolver(Path p){dir=p;}Uri create(String n)throws Exception{if(suffix)n+=".txt";Uri u=DocumentsContract.buildDocumentUriUsingTree(null,"primary:WMMT/"+n);docs.put(u.s,new Doc(n,Files.createTempFile(dir,"d",null)));return u;}Cursor query(Uri u,String[] p,Object a,Object b,Object c){events.add("query:"+u);if(failQuery)throw new SecurityException();return new Cursor(docs.get(u.s));}ParcelFileDescriptor openFileDescriptor(Uri u,String mode)throws Exception{events.add("open:"+mode+":"+u);if(failOpen)throw new IOException();Doc d=docs.get(u.s);return d==null?null:new ParcelFileDescriptor(new RandomAccessFile(d.path.toFile(),mode.equals("w")?"rw":"r"));}boolean delete(Uri u)throws Exception{events.add("delete:"+u);if(failDelete)throw new IOException();Doc d=docs.remove(u.s);if(d==null)return false;Files.delete(d.path);return true;}}
 static class Prefs {Editor edit(){return new Editor();}static class Editor {Editor remove(String s){return this;}boolean commit(){return true;}}}
 static class android {static class util {static class Log {static void w(String a,String b){}}}}
 Uri tree=new Uri("content://fake/primary:WMMT");Resolver resolver;List<String> entries=new ArrayList<>();boolean logStarted,started,nativeSuccess=true;int logPhase;String logWarning;
 SafHost(Path p){resolver=new Resolver(p);}boolean validTree(Uri u){return tree.equals(u);}boolean lockLogProcess(){return true;}Resolver getContentResolver(){return resolver;}Prefs logPrefs(){return new Prefs();}void startGame(){started=true;}void finishWithoutLog(String s){logWarning=s;startGame();}
 DebugLogStorage.Journal journal(){return new DebugLogStorage.Journal(){public List<String> read(){return new ArrayList<>(entries);}public void write(List<String> v){events.add("journal:"+String.join(",",v));entries=new ArrayList<>(v);}};}
 boolean nativeDebugLogStart(int fd)throws Exception{events.add("native");RandomAccessFile f=ParcelFileDescriptor.fds.remove(fd);if(nativeSuccess)f.write("native\n".getBytes(StandardCharsets.US_ASCII));f.close();return nativeSuccess;}
''' + methods + r'''
 void ok(boolean b,String s){if(!b)throw new AssertionError(s+"; events="+events);}
 long count(String prefix){return events.stream().filter(e->e.startsWith(prefix)).count();}
 String old(int n,String ext)throws Exception{String token=String.format("%032x",n),name="wmmt-debug-"+token+ext;Uri u=resolver.create(name);Files.writeString(resolver.docs.get(u.s).path,"{\"event\":\"session\",\"session_id\":\""+token+"\"}\nold\n");return u.s;}
 void run()throws Exception{
  events.clear();setupLog(tree);ok(logStarted&&started&&entries.size()==1,"setup");ok(count("create:")==1&&count("native")==1&&count("journal:")==1&&count("delete:")==0,"one creation, handoff and registration");Doc d=resolver.docs.get(entries.get(0));String token=DebugLogStorage.tokenFromName(d.name);String created=entries.get(0);ok(events.get(0).equals("create:content://fake/primary:WMMT:text/plain:"+d.name),"create exact parent mime name");ok(events.indexOf("query:"+created)<events.indexOf("open:w:"+created)&&events.indexOf("open:w:"+created)<events.indexOf("write")&&events.lastIndexOf("write")<events.indexOf("native")&&events.indexOf("native")<events.indexOf("journal:"+created),"create/validate/write/native/journal order");ok(d.name.endsWith(".txt")&&token!=null,"name");ok(Files.readString(d.path).equals("{\"event\":\"session\",\"session_id\":\""+token+"\"}\nnative\n"),"header/native");
  List<String> a=new ArrayList<>();for(int i=1;i<=5;i++)a.add(old(i,i%2==0?".txt":".log"));String foreign=resolver.create("wmmt-debug-"+"f".repeat(32)+".log.txt").s;entries=a;logStarted=false;events.clear();setupLog(tree);ok(entries.size()==5&&!resolver.docs.containsKey(a.get(0)),"legacy rotation");ok(count("create:")==1&&count("delete:")==1&&events.contains("delete:"+a.get(0)),"exactly one creation and oldest deletion");ok(!events.contains("delete:"+foreign)&&resolver.docs.containsKey(foreign),"foreign never sent to delete");ok(events.indexOf("native")<events.indexOf("journal:"+String.join(",",a)+","+entries.get(4))&&events.indexOf("journal:"+String.join(",",a)+","+entries.get(4))<events.indexOf("delete:"+a.get(0)),"record before delete");
  String target=a.get(1);d=resolver.docs.get(target);String name=d.name;d.name="tampered";ok(!deleteVerifiedLog(tree,target),"name tamper");d.name=name;d.mime="bad";ok(!deleteVerifiedLog(tree,target),"mime tamper");d.mime="text/plain";byte[] saved=Files.readAllBytes(d.path);Files.writeString(d.path,"bad");ok(!deleteVerifiedLog(tree,target),"header tamper");Files.write(d.path,saved);ok(!deleteVerifiedLog(tree,target.replace("primary:WMMT/","primary:other/")),"id tamper");Os.stat.st_nlink=2;ok(!deleteVerifiedLog(tree,target),"nlink");Os.stat.st_nlink=1;Os.stat.st_mode=0;ok(!deleteVerifiedLog(tree,target),"mode");Os.stat.st_mode=1;
  resolver.failQuery=true;try{deleteVerifiedLog(tree,target);throw new AssertionError("query exception");}catch(SecurityException expected){}resolver.failQuery=false;resolver.failOpen=true;try{deleteVerifiedLog(tree,target);throw new AssertionError("open exception");}catch(IOException expected){}resolver.failOpen=false;resolver.failDelete=true;try{deleteVerifiedLog(tree,target);throw new AssertionError("delete exception");}catch(IOException expected){}resolver.failDelete=false;ok(resolver.docs.containsKey(target),"failed deletion retained");
  resolver.failDelete=true;logStarted=false;events.clear();setupLog(tree);ok(entries.size()==6&&resolver.docs.containsKey(target)&&logWarning!=null,"rotation failure retains journal");ok(count("delete:")==1&&events.contains("delete:"+target)&&!events.contains("delete:"+foreign),"failed deletion attempt counted");resolver.failDelete=false;logStarted=false;events.clear();setupLog(tree);ok(entries.size()==5&&!resolver.docs.containsKey(target),"new txt oldest rotation");ok(count("create:")==1&&count("delete:")==2&&events.contains("delete:"+target)&&!events.contains("delete:"+foreign),"recovery deletes two registered entries only");entries=new ArrayList<>(List.of(foreign));int before=resolver.docs.size();logStarted=false;events.clear();setupLog(tree);ok(!logStarted&&resolver.docs.size()==before&&count("create:")==0&&count("delete:")==0,"invalid journal performs no create or delete");entries.clear();resolver.suffix=true;setupLog(tree);ok(!logStarted&&entries.isEmpty(),"auto suffix rejected");resolver.suffix=false;nativeSuccess=false;setupLog(tree);ok(!logStarted&&entries.isEmpty(),"native failure");nativeSuccess=true;Os.fail=true;setupLog(tree);ok(!logStarted&&entries.isEmpty(),"write failure");System.out.println("SAF host simulation passed");
 }
 public static void main(String[] args)throws Exception{new SafHost(Paths.get(args[0])).run();}
}
'''
with tempfile.TemporaryDirectory(prefix='wmmt-saf-') as temp:
 p=Path(temp)/'SafHost.java';p.write_text(harness)
 subprocess.run(['javac','-d',temp,str(root/'java/com/wmmt/port/DebugLogStorage.java'),str(p)],check=True)
 subprocess.run(['java','-cp',temp,'com.wmmt.port.SafHost',temp],check=True)
