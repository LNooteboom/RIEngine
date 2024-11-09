from PIL import Image
import sys
import struct
import os

if len(sys.argv) != 3:
    print("Usage: texCreator.py [Output] [Input]")
    sys.exit(1)

outfile = open(sys.argv[1], "wb")
outfile.write(bytes("TEX0", 'utf-8'))


im = Image.open(sys.argv[2]).convert("RGBA")
w, h = im.size
outfile.write(struct.pack("II", w, h))

d = im.getdata()
for px in d:
    if px[3] == 0:
        outfile.write(struct.pack("BBBB", 0, 0, 0, 0))
    else:
        outfile.write(struct.pack("BBBB", px[0], px[1], px[2], px[3]))
