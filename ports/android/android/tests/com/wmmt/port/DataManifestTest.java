package com.wmmt.port;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

public final class DataManifestTest {
    private static final DataManifest.Limits L=new DataManifest.Limits(20,100000,100000,100000);
    interface Task { void run() throws Exception; }
    static void fails(Task t) throws Exception { try { t.run(); throw new AssertionError("expected failure"); } catch(IOException expected) {} }
    static void ok(boolean b) { if(!b) throw new AssertionError(); }
    static byte[] zip(String... names) throws IOException {
        ByteArrayOutputStream out=new ByteArrayOutputStream(); ZipOutputStream z=new ZipOutputStream(out);
        for(String name:names) { z.putNextEntry(new ZipEntry(name)); if(!name.endsWith("/")) z.write(("data-"+name).getBytes("UTF-8")); z.closeEntry(); }
        z.close(); return out.toByteArray();
    }
    static DataManifest manifest(byte[] bytes) throws IOException { return DataManifest.fromZip(() -> new ByteArrayInputStream(bytes),L,"resources","v1"); }
    static void largeArchive() throws Exception {
        final int blocks=12800; byte[] data=new byte[8192]; new Random(42).nextBytes(data); CRC32 crc=new CRC32();
        for(int i=0;i<blocks;i++) crc.update(data); long size=(long)blocks*data.length;
        Path archive=Files.createTempFile("manifest-large-", ".zip");
        try {
            try(ZipOutputStream out=new ZipOutputStream(Files.newOutputStream(archive))) {
                ZipEntry e=new ZipEntry("large.bin"); e.setMethod(ZipEntry.STORED); e.setSize(size); e.setCompressedSize(size); e.setCrc(crc.getValue()); out.putNextEntry(e);
                for(int i=0;i<blocks;i++) out.write(data); out.closeEntry();
            }
            ok(Files.size(archive)>100L*1024*1024);
            DataManifest m=DataManifest.fromZip(() -> Files.newInputStream(archive),new DataManifest.Limits(2,size,110*1024*1024,10000),"resources","large");
            ok(m.find("large.bin").length==size);
        } finally { Files.deleteIfExists(archive); }
    }
    public static void main(String[] args) throws Exception {
        byte[] data=zip("z", "empty/", "a/f"); DataManifest m=manifest(data);
        ok(m.files().keySet().toString().equals("[a/f, z]")); ok(m.find("a/f").length==8); ok(m.find("empty")==null);
        DataManifest reordered=manifest(zip("a/f", "z", "empty/")); ok(m.find("a/f").sha256.equals(reordered.find("a/f").sha256));
        for(String name:new String[]{"../x","/x","a//b","a/./b","a\\b","a:b",DataManifest.RESERVED_MARKER,"nested/.re3dxplus-install-v1","nested/.overlay-test","nested/MODS/file","nested/WmMt/file"}) fails(() -> manifest(zip(name)));
        byte[] dup=zip("aa","bb"); for(int i=0;i<dup.length-1;i++) if(dup[i]=='b' && dup[i+1]=='b') { dup[i]='a';dup[i+1]='a'; } fails(() -> manifest(dup));
        fails(() -> manifest(Arrays.copyOf(data,data.length-1)));
        byte[] broken=data.clone(); int at=30+1; broken[at]^=0x7f; fails(() -> manifest(broken));
        fails(() -> DataManifest.fromZip(() -> new ByteArrayInputStream(data),new DataManifest.Limits(1,1000,10000,10000),"resources","id"));
        fails(() -> DataManifest.fromZip(() -> new ByteArrayInputStream(data),new DataManifest.Limits(10,1,10000,10000),"resources","id"));
        final int[] calls={0}; fails(() -> DataManifest.fromZip(() -> new ByteArrayInputStream(calls[0]++==0?data:zip("other")),L,"resources","id"));
        Path dir=Files.createTempDirectory("data-manifest-test-"); Path target=dir.resolve("index");
        try {
            m.writeAtomic(target,null,L); DataManifest read=DataManifest.read(target,L); ok(read.zipSha256.equals(m.zipSha256)); ok(read.canonicalId.equals("v1")); ok(read.find("z").sha256.equals(m.find("z").sha256));
            byte[] old=Files.readAllBytes(target); fails(() -> reordered.writeAtomic(target,null,L)); ok(Arrays.equals(old,Files.readAllBytes(target)));
            DataManifest wrong=manifest(zip("wrong")); fails(() -> reordered.writeAtomic(target,wrong,L)); ok(Arrays.equals(old,Files.readAllBytes(target)));
            reordered.writeAtomic(target,read,L); ok(DataManifest.read(target,L).zipSha256.equals(reordered.zipSha256));
            byte[] valid=Files.readAllBytes(target); Files.write(target,Arrays.copyOf(valid,valid.length-1)); fails(() -> DataManifest.read(target,L));
            byte[] version=valid.clone(); version[7]=2; byte[] digest=MessageDigest.getInstance("SHA-256").digest(Arrays.copyOf(version,version.length-32)); System.arraycopy(digest,0,version,version.length-32,32); Files.write(target,version); fails(() -> DataManifest.read(target,L));
            byte[] before=Files.readAllBytes(target); fails(() -> m.writeAtomic(target,read,L)); ok(Arrays.equals(before,Files.readAllBytes(target)));
        } finally { Files.deleteIfExists(target); Files.deleteIfExists(dir); }
        largeArchive();
        System.out.println("DataManifest tests passed (including >100 MiB ZIP)");
    }
}
