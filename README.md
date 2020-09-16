gbcamextract
============

Extracts photos from Game Boy Camera saves. Frames can be preserved. Requires [libpng](http://www.libpng.org/pub/png/libpng.html).

## Usage

```console
gbcamextract save.sav
```

This will produce 30 PNG files containing your photos. If you want the frames around your photos to be preserved, then you must provide a Game Boy Camera ROM, like so:

```console
gbcamextract save.sav camera.gb
```


## Install

You will first need to install [libpng](http://www.libpng.org/pub/png/libpng.html)

Then, simply:
```console
make
```


## License

Licensed under the expat license, which is sometimes called the MIT license.
See the `LICENSE` file for details.
