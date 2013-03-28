#!/usr/bin/python

from PIL import Image

image = Image.open("test.png")
px = image.load()

(width,height) =  image.size

out = open("test.dat", "wb")

print width
print height
for y in range(0,height):
	for x in range(0,width):
		pixel = px[x,y]
		r = pixel[0]
		g = pixel[1]
		r = int(r/255.0 * 32)
		g = int(r/255.0 * 32)
		out.write(chr(r))
		out.write(chr(g))
out.close()
