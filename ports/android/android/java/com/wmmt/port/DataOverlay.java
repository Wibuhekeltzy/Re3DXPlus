package com.wmmt.port;

import java.io.*;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

/** Java 8 in-place overlay. Caller holds a global interprocess lock, stops the game,
 * and excludes all other writers (including root/ADB) throughout. State is private,
 * existing, and separate from root. apply never marks readiness; after it returns the
 * caller atomically/durably commits the manifest and then invokes confirm using the
 * same old/new identities. Persist the old manifest until confirmation completes.
 * After all new files verify, old-only regular files still matching their old hash
 * are deleted; modified, missing, symlink and unknown paths are preserved or skipped.
 * The journal directory must synchronise before any resource replacement. If other
 * directory synchronisation fails, directoriesSynced is false: do not commit ready;
 * retry apply until true. Java/filesystem fsync success does not prove that a real
 * Android device, controller or storage medium survives power loss. */
public final class DataOverlay {
    private DataOverlay() {}
    public static final class Entry {
        public final long length; public final String sha256;
        public Entry(long length,String sha256) {
            if(length<0||!isHash(sha256)) throw new IllegalArgumentException("invalid entry");
            this.length=length; this.sha256=sha256.toLowerCase(Locale.ROOT);
        }
    }
    public interface Source { InputStream open() throws IOException; }
    public interface Fault { void at(String stage,String path) throws IOException; }
    public static final class Result {
        public final boolean directoriesSynced;
        private Result(boolean v) { directoriesSynced=v; }
    }
    private static final String JOURNAL="overlay.journal";
    private static final int MAGIC=0x444f5631;
    private static void hit(Fault f,String s,String p) throws IOException { if(f!=null)f.at(s,p); }
    private static boolean isHash(String s) { return s!=null&&s.matches("[0-9a-fA-F]{64}"); }
    private static MessageDigest md() { try {return MessageDigest.getInstance("SHA-256");} catch(NoSuchAlgorithmException e){throw new AssertionError(e);} }
    private static String hex(byte[] b) { char[] h="0123456789abcdef".toCharArray(),o=new char[b.length*2]; for(int i=0;i<b.length;i++){o[2*i]=h[(b[i]&255)>>>4];o[2*i+1]=h[b[i]&15];}return new String(o); }
    private static String hash(InputStream in) throws IOException { MessageDigest d=md();byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1)if(n>0)d.update(b,0,n);return hex(d.digest()); }
    private static boolean syncDir(Path p,Fault fault) { try { hit(fault,"directory-sync",p.toString()); try(FileChannel c=FileChannel.open(p,StandardOpenOption.READ)){c.force(true);return true;} }catch(IOException|UnsupportedOperationException|SecurityException e){return false;} }
    // Retry every directory whose entry may have changed, even on recovery after
    // rename/mkdir but before its directory fsync. A failed call remains false.
    private static void syncAffected(Path root,Path state,SortedMap<String,Entry> manifest,Fault fault,boolean[] synced) throws IOException {
        Set<Path> dirs=new LinkedHashSet<>();dirs.add(root);
        for(String name:manifest.keySet()){
            Path p=target(root,name,false,fault,synced).getParent();
            while(p!=null&&p.startsWith(root)){dirs.add(p);if(p.equals(root))break;p=p.getParent();}
        }
        for(Path p:dirs)synced[0]&=syncDir(p,fault);
        synced[0]&=syncDir(state,fault);
    }
    // Old-only paths are optional: a missing, modified, unreadable, non-regular or
    // symlink path is user data and is never followed or deleted.
    private static Path oldPath(Path root,String name) {
        Path cur=root;String[] parts=name.split("/");
        for(int i=0;i<parts.length-1;i++) {cur=cur.resolve(parts[i]);if(Files.isSymbolicLink(cur)||!Files.isDirectory(cur,LinkOption.NOFOLLOW_LINKS))return null;}
        return cur.resolve(parts[parts.length-1]);
    }
    private static String folded(String s) {return java.text.Normalizer.normalize(s,java.text.Normalizer.Form.NFD).toLowerCase(Locale.ROOT);}
    private static void syncOldAffected(Path root,SortedMap<String,Entry> oldM,SortedMap<String,Entry> newM,Fault fault,boolean[] synced) {
        Set<Path> dirs=new LinkedHashSet<>();
        for(String name:oldM.keySet())if(!newM.containsKey(name)){Path p=oldPath(root,name);if(p==null)continue;p=p.getParent();while(p!=null&&p.startsWith(root)){dirs.add(p);if(p.equals(root))break;p=p.getParent();}}
        for(Path dir:dirs)synced[0]&=syncDir(dir,fault);
    }
    private static void cleanOld(Path root,SortedMap<String,Entry> oldM,SortedMap<String,Entry> newM,Fault fault,boolean[] synced) throws IOException {
        Set<String> newFolded=new HashSet<>();for(String n:newM.keySet())newFolded.add(folded(n));
        for(Map.Entry<String,Entry> e:oldM.entrySet()) {
            String name=e.getKey();if(newM.containsKey(name)||newFolded.contains(folded(name)))continue;
            Path p=oldPath(root,name);if(p==null||Files.isSymbolicLink(p)||!Files.isRegularFile(p,LinkOption.NOFOLLOW_LINKS))continue;
            boolean original;try{original=matches(p,e.getValue());}catch(IOException|SecurityException unknown){continue;}
            if(!original)continue;
            try {hit(fault,"before-old-delete",name);Files.delete(p);}
            catch(AccessDeniedException|SecurityException denied) {
                // Permission-denied leftovers are deliberately retained. Still sync
                // their parent conservatively; never swallow other I/O errors.
                synced[0]&=syncDir(p.getParent(),fault);continue;
            }
            hit(fault,"after-old-delete",name);synced[0]&=syncDir(p.getParent(),fault);hit(fault,"after-old-delete-sync",name);
        }
    }
    private static void syncFile(Path p) throws IOException {try(FileChannel c=FileChannel.open(p,StandardOpenOption.WRITE)){c.force(true);}}
    private static void move(Path a,Path b) throws IOException {try{Files.move(a,b,StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);}catch(AtomicMoveNotSupportedException e){throw new IOException("atomic rename unavailable",e);}}
    private static void validate(String s) throws IOException {
        if(s==null||s.isEmpty()||s.startsWith("/")||s.indexOf('\\')>=0||s.indexOf(':')>=0||s.indexOf('\0')>=0)throw new IOException("unsafe path: "+s);
        for(String c:s.split("/",-1))if(c.isEmpty()||c.equals(".")||c.equals("..")||c.equalsIgnoreCase("mods")||c.equalsIgnoreCase("wmmt")||c.startsWith(".overlay-"))throw new IOException("unsafe path: "+s);
    }
    private static SortedMap<String,Entry> manifest(Map<String,Entry> m) throws IOException {
        if(m==null)throw new IOException("missing manifest");SortedMap<String,Entry> out=new TreeMap<>();
        for(Map.Entry<String,Entry> e:m.entrySet()){validate(e.getKey());if(e.getValue()==null)throw new IOException("null entry");out.put(e.getKey(),e.getValue());}
        for(String k:out.keySet()){int i=k.indexOf('/');while(i>=0){if(out.containsKey(k.substring(0,i)))throw new IOException("file/directory conflict: "+k);i=k.indexOf('/',i+1);}}
        return out;
    }
    private static Path directory(Path p) throws IOException {
        if(p==null)throw new IOException("missing directory");Path a=p.toAbsolutePath().normalize(),c=a.getRoot();
        for(Path part:a){c=c.resolve(part);if(Files.isSymbolicLink(c)||!Files.isDirectory(c,LinkOption.NOFOLLOW_LINKS))throw new IOException("unsafe directory: "+c);}return a;
    }
    private static Path target(Path root,String name,boolean make,Fault fault,boolean[] synced) throws IOException {
        validate(name);Path c=root;String[] parts=name.split("/");
        for(int i=0;i<parts.length-1;i++){
            c=c.resolve(parts[i]);if(!Files.exists(c,LinkOption.NOFOLLOW_LINKS)){
                if(!make)return root.resolve(name);
                hit(fault,"before-mkdir",name);Files.createDirectory(c);hit(fault,"after-mkdir",name);synced[0]&=syncDir(c.getParent(),fault);
            }
            if(Files.isSymbolicLink(c)||!Files.isDirectory(c,LinkOption.NOFOLLOW_LINKS))throw new IOException("unsafe parent: "+c);
        }
        Path p=c.resolve(parts[parts.length-1]);if(Files.isSymbolicLink(p))throw new IOException("symlink: "+p);return p;
    }
    private static boolean matches(Path p,Entry e) throws IOException {
        if(!Files.exists(p,LinkOption.NOFOLLOW_LINKS))return false;
        if(Files.isSymbolicLink(p)||!Files.isRegularFile(p,LinkOption.NOFOLLOW_LINKS))throw new IOException("not a regular file: "+p);
        if(Files.size(p)!=e.length)return false;try(InputStream in=Files.newInputStream(p,LinkOption.NOFOLLOW_LINKS)){return hash(in).equals(e.sha256);}
    }
    private static byte[] binding(Path root,SortedMap<String,Entry> oldM,SortedMap<String,Entry> newM,String oldZip,String newZip) throws IOException {
        ByteArrayOutputStream b=new ByteArrayOutputStream();DataOutputStream d=new DataOutputStream(b);d.writeUTF(root.toString());d.writeUTF(oldZip.toLowerCase(Locale.ROOT));d.writeUTF(newZip.toLowerCase(Locale.ROOT));
        for(SortedMap<String,Entry> m:Arrays.asList(oldM,newM)){d.writeInt(m.size());for(Map.Entry<String,Entry> e:m.entrySet()){d.writeUTF(e.getKey());d.writeLong(e.getValue().length);d.writeUTF(e.getValue().sha256);}}d.flush();return md().digest(b.toByteArray());
    }
    private static String readJournal(Path p,byte[] expected) throws IOException {
        if(Files.isSymbolicLink(p)||!Files.isRegularFile(p,LinkOption.NOFOLLOW_LINKS))throw new IOException("invalid journal");
        try(DataInputStream d=new DataInputStream(Files.newInputStream(p,LinkOption.NOFOLLOW_LINKS))){if(d.readInt()!=MAGIC)throw new IOException("bad journal");byte[] b=new byte[32];d.readFully(b);String n=d.readUTF();if(!n.matches("[0-9a-f]{32}")||d.read()!=-1||!MessageDigest.isEqual(b,expected))throw new IOException("journal transaction mismatch");return n;}
    }
    private static Path temp(Path p,String nonce){return p.resolveSibling(".overlay-"+nonce+"-"+hex(md().digest(p.toString().getBytes(StandardCharsets.UTF_8)))+".tmp");}
    private static void removeTemp(Path p) throws IOException {if(Files.exists(p,LinkOption.NOFOLLOW_LINKS)){if(Files.isSymbolicLink(p)||!Files.isRegularFile(p,LinkOption.NOFOLLOW_LINKS))throw new IOException("unsafe temp: "+p);Files.delete(p);}}
    /** ZIP supplier must reopen the same immutable ZIP on each call. */
    public static Result apply(Path root,Path state,Map<String,Entry> oldManifest,Map<String,Entry> newManifest,Source source,String oldZip,String newZip,Fault fault) throws IOException {
        if(!isHash(oldZip)||!isHash(newZip)||source==null)throw new IOException("invalid ZIP identity/source");root=directory(root);state=directory(state);if(state.startsWith(root)||root.startsWith(state))throw new IOException("state must be separate");
        SortedMap<String,Entry> oldM=manifest(oldManifest),newM=manifest(newManifest);byte[] bind=binding(root,oldM,newM,oldZip,newZip);Path jp=state.resolve(JOURNAL);boolean[] synced={true};
        hit(fault,"before-zip-hash","");try(InputStream in=source.open()){if(!hash(in).equalsIgnoreCase(newZip))throw new IOException("ZIP hash mismatch");}hit(fault,"after-zip-hash","");
        String nonce;
        if(Files.exists(jp,LinkOption.NOFOLLOW_LINKS)){nonce=readJournal(jp,bind);if(!syncDir(state,fault))throw new IOException("cannot synchronise journal directory before update");}
        else {
            nonce=UUID.randomUUID().toString().replace("-","");
            for(Map.Entry<String,Entry> e:newM.entrySet()){
                Path p=target(root,e.getKey(),false,fault,synced);Entry old=oldM.get(e.getKey());
                if(old!=null){if(!matches(p,old))throw new IOException("old data missing or modified: "+e.getKey());}
                else if(Files.exists(p,LinkOption.NOFOLLOW_LINKS))throw new IOException("new path already exists: "+e.getKey());
                if(Files.exists(temp(p,nonce),LinkOption.NOFOLLOW_LINKS))throw new IOException("temporary path occupied");
            }
            Path t=state.resolve(".overlay-journal-"+nonce+".tmp");hit(fault,"before-journal-write","");
            try(FileChannel c=FileChannel.open(t,StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE);DataOutputStream d=new DataOutputStream(java.nio.channels.Channels.newOutputStream(c))){d.writeInt(MAGIC);d.write(bind);d.writeUTF(nonce);d.flush();c.force(true);}
            hit(fault,"after-journal-write","");move(t,jp);hit(fault,"after-journal-rename","");if(!syncDir(state,fault))throw new IOException("cannot synchronise journal directory before update");hit(fault,"after-journal-sync","");
        }
        Set<String> need=new HashSet<>();
        for(Map.Entry<String,Entry> e:newM.entrySet()){
            String name=e.getKey();Path p=target(root,name,false,fault,synced);Entry old=oldM.get(name);
            if(matches(p,e.getValue()))continue;
            if(old!=null){if(!matches(p,old))throw new IOException("old/new data missing or modified: "+name);}
            else if(Files.exists(p,LinkOption.NOFOLLOW_LINKS))throw new IOException("new path modified: "+name);
            need.add(name);
        }
        if(!need.isEmpty()){
            Set<String> seen=new HashSet<>();
            try(ZipInputStream zip=new ZipInputStream(source.open())){
                ZipEntry z;byte[] buf=new byte[65536];
                while((z=zip.getNextEntry())!=null){String name=z.getName();if(!need.contains(name))continue;if(z.isDirectory()||!seen.add(name))throw new IOException("duplicate/invalid ZIP entry: "+name);
                    Path p=target(root,name,true,fault,synced),t=temp(p,nonce);removeTemp(t);hit(fault,"before-temp-write",name);MessageDigest d=md();long size=0;Entry expected=newM.get(name);
                    try(OutputStream out=Files.newOutputStream(t,StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE)){int n;while((n=zip.read(buf))!=-1){if(n==0)continue;size+=n;if(size>expected.length)throw new IOException("ZIP entry too long: "+name);d.update(buf,0,n);out.write(buf,0,n);hit(fault,"during-temp-write",name);}}
                    if(size!=expected.length||!hex(d.digest()).equals(expected.sha256))throw new IOException("ZIP entry hash/length mismatch: "+name);
                    hit(fault,"after-temp-write",name);syncFile(t);hit(fault,"after-temp-sync",name);Entry prior=oldM.get(name);
                    if(prior==null?Files.exists(p,LinkOption.NOFOLLOW_LINKS):!matches(p,prior))throw new IOException("destination changed: "+name);
                    hit(fault,"before-replace",name);move(t,p);hit(fault,"after-replace",name);synced[0]&=syncDir(p.getParent(),fault);hit(fault,"after-replace-sync",name);
                }
            }
            if(!seen.containsAll(need))throw new IOException("ZIP missing manifest entry");
        }
        for(Map.Entry<String,Entry> e:newM.entrySet())if(!matches(target(root,e.getKey(),false,fault,synced),e.getValue()))throw new IOException("final verification failed: "+e.getKey());
        hit(fault,"after-verify","");syncAffected(root,state,newM,fault,synced);if(!synced[0])return new Result(false);
        cleanOld(root,oldM,newM,fault,synced);
        // On restart, also synchronise parents of old-only paths already absent.
        syncOldAffected(root,oldM,newM,fault,synced);return new Result(synced[0]);
    }
    /** Only after a successful apply with directoriesSynced true and caller commit. */
    public static Result confirm(Path root,Path state,Map<String,Entry> oldManifest,Map<String,Entry> newManifest,String oldZip,String newZip) throws IOException {
        return confirm(root,state,oldManifest,newManifest,oldZip,newZip,null);
    }
    /** Fault hook overload for host tests. */
    public static Result confirm(Path root,Path state,Map<String,Entry> oldManifest,Map<String,Entry> newManifest,String oldZip,String newZip,Fault fault) throws IOException {
        if(!isHash(oldZip)||!isHash(newZip))throw new IOException("invalid ZIP identity");root=directory(root);state=directory(state);if(state.startsWith(root)||root.startsWith(state))throw new IOException("state must be separate");
        SortedMap<String,Entry> oldM=manifest(oldManifest),newM=manifest(newManifest);Path jp=state.resolve(JOURNAL);boolean[] synced={true};
        // No journal is possible after a crash just after deletion; confirmation is
        // idempotent, but only the caller's durable commit proves readiness.
        String nonce=Files.exists(jp,LinkOption.NOFOLLOW_LINKS)?readJournal(jp,binding(root,oldM,newM,oldZip,newZip)):null;
        for(Map.Entry<String,Entry> e:newM.entrySet())if(!matches(target(root,e.getKey(),false,null,synced),e.getValue()))throw new IOException("not complete: "+e.getKey());
        syncAffected(root,state,newM,fault,synced);syncOldAffected(root,oldM,newM,fault,synced);
        if(!synced[0])return new Result(false);
        if(nonce!=null){for(String name:newM.keySet())removeTemp(temp(target(root,name,false,null,synced),nonce));syncAffected(root,state,newM,fault,synced);if(!synced[0])return new Result(false);Files.delete(jp);synced[0]&=syncDir(state,fault);}return new Result(synced[0]);
    }
}
