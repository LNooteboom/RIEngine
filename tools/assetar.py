import zlib
import sys
import os
import struct

if len(sys.argv) != 3:
    print("Usage:", sys.argv[0], "<Output file> <Input directory>")
    sys.exit(1)

destfile = open(sys.argv[1], "wb")

os.chdir(sys.argv[2])
rootdir = "."
found = [];

#for subdir, dirs, files in os.walk(rootdir):
#    for file in files:
#        path = os.path.join(subdir, file)[2:]
#        found.append(path)

allowedDirs = [
    "ascii",
    "bgm",
    "dan",
    "danbg",
    "danpl",
    "dlg",
    "dvm",
    "mesh",
    "sfx",
    "shaders",
    "tex",
    "tex/bg",
    "tex/card",
    "tex/char",
    "tex/dan",
    "tex/ui"
]
for d in allowedDirs:
    for f in os.listdir(d):
        path = os.path.join(d, f)
        fn, ext = os.path.splitext(f);
        if ext != '.i' and os.path.isfile(path):
            found.append(path.replace('\\', '/'))



found.sort()

destfile.write(bytes("RI_0", 'utf-8'))
destfile.write(struct.pack("I", len(found)))
destfile.write(struct.pack("Q", 0))

entryHeader = bytearray()

index = 0
offset = 16
for f in found:
    print(f)
    if (len(f) >= 35):
        print("Filename too long")
        sys.exit(1)
    fi = open(f, "rb")
    contents = fi.read()
    fi.close()
    uncompressedSize = len(contents)
    adler = zlib.adler32(contents)

    # Compress the file with zlib
    compressed = zlib.compress(contents)
    compressedSize = len(compressed)

    # Write compressed file
    destfile.write(compressed)

    # append file entry
    name = bytes(f.ljust(36, '\0'), 'utf-8')
    entryHeader += name
    entryHeader += struct.pack("I", adler)
    entryHeader += struct.pack("QQQ", offset, compressedSize, uncompressedSize)

    offset += compressedSize
    index += 1

ehOffset = destfile.tell()
destfile.write(entryHeader)
destfile.seek(8)
destfile.write(struct.pack("Q", ehOffset))
destfile.close()
