`anim.{}`
=========
?

`a_ui,5.{}`
===========
* 16 bit pixels (RGB565 Big Endian)
* some file header
* each image has a header:
  * `=HHHHHH4xHxxH14x`
  * some unknown things that seem to be the same accross the whole file. maybe pixel format?
  * width, height
* the header is followed by pixel data in rows
* there is some extra data at the end. not sure what.

`a_ui,6.{}`
===========
?

`axvi.{}`
=========
?

`bldg.{}`
=========
* indexed

`data.{}`
=========
?

`deco.{}`
=========
?

`d_ui,5.{}`
===========
* index + alpha
* same header as in a_ui,5.{} but not sure where the palette is
* there is some extra data at the end. not sure what.

`d_ui,6.{}`
===========
?

`flor.{}`
========
?

`font.{}`
=========
?

`germ,5.{}`
===========
?

`germ,6.{}`
===========
?

`m_ui,u.{}`
===========
* 256 pixels wide
* RGBA pixels
* some kind of header
* possibly with separator (see file)

`musi.{}`
=========
* mp3 file, 44.1kHz, 128kb/s
Output from `ffmpeg -i`:
```
Input #0, mp3, from 'musi.{}':
  Duration: 00:21:06.10, start: 0.000000, bitrate: 128 kb/s
    Stream #0:0: Audio: mp3, 44100 Hz, stereo, fltp, 128 kb/s
```

`soun.{}`
=======
* 16 bit signed PCM sampled at 22.05kHz
* separated by a series of `-inf`s - at least 10 between each

`text.{}`
=========
* starts with sequence: ```\x75\xB1\xC0\xBA\x00\x00\x01\x00\x5D\xE4\x07\x00\x3D\xE6\x07\x00```
* followed by ISO-8859-1 encoded null separated texts
* followed by ?

`unil.{}`
=========
?

`ep01 China.{}`
===============
Map appears to be 128x128. It can be seen by importing into gimp using an 8-bit gray just to visualise things
