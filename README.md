Matrix-Mixer
============

matrixmixer.lv2 is a matrix mixer :)

It is available as [LV2 plugin](http://lv2plug.in/) and standalone [JACK](http://jackaudio.org/)-application.

Install
-------

Compilation requires the LV2 SDK, jack-headers, gnu-make, a c-compiler,
libpango, libcairo and openGL (sometimes called: glu, glx, mesa).

```bash
  git clone git://github.com/x42/matrixmixer.lv2
  cd matrixmixer.lv2
  make submodules
  make
  sudo make install PREFIX=/usr
```

Note to packagers: The Makefile honors `PREFIX` and `DESTDIR` variables as well
as `CFLAGS`, `LDFLAGS` and `OPTIMIZATIONS` (additions to `CFLAGS`), also
see the first 10 lines of the Makefile.
You really want to package the superset of [x42-plugins](https://github.com/x42/x42-plugins).
