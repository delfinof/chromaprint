phorton1/chromaprint
=========================

## Contents

This repository contains the source code, scripts, and makefiles to build specific versions of fpCalc executables and libraries for specific platforms.

This repository is capable of building (at least) the following targets to *execute* on the given **platforms**:

- Win32 executable **fpCalc.exe**
- Linux (x86 Ubuntu 12.04) executable **fpcalc**
- Android x86 **fpcalc** executable and **libfpcalc.so** shared library
- Android armeabi **fpcalc** executable and **libfpcalc.so** shared library
- Android armeabi-v7a **fpcalc** executable and **libfpcalc.so** shared library

It works specifically with the phorton1/ffmpeg repository to build these executables and libraries against four specific **versions** of ffmpeg:

- 0.9 - the official release version of ffmpeg that I believe to be closest in functionality to that which was used to create the November 23, 2013 "official acoustid" windows build of fpcalc.exe available [here](https://bitbucket.org/acoustid/chromaprint/downloads/chromaprint-fpcalc-1.1-win-i686.zip), and which is used in musicBrainz windows release of [picard](http://picard.musicbrainz.org/)
- 0.11 - a release slightly "later" than the 0.9 release
- 2.7 - the most current offical release of ffmpeg as of this writing
- tip - the tip of the ffmpeg repository as of it's fork to this site on or about July 23, 2015

The build mechanism presented in this repository has been tested on Linux (x86 Ubuntu 12.04) and Windows 8 **hosts**.  That is to say, that the above array of executables and libraries can generally be *built* on either platform. The only exception is that the Linux executable can *only* be built on a Linux host ... it is not built on a Windows machine with this system.

This project generally **depends** on, and **works with** the phorton1/ffmpeg repository and it is assumed that you have read that page and understood and complied with the **requirements** as stated there.


## Building

### Additional target platforms

In addition to the target platforms specified in phorton1/ffmpeg, this chromaprint repository adds three additional targets specifically for the android shared libraries: **arms**, **arm7s** and **x86s**:

- win = build fpcalc.exe executable for windows
- x86 = build fpcalc executable for android-x86 platform
- arm = build fpcalc executable for android-armeabi
- arm7 = build fpcalc executablefor android-armeabi-v7a
- host = (linux only) build for the host (linux) fpcalc executable
- x86s = build libfpcalc.so shared library for android-x86
- arms = build libfpcalc.so shared library for android-armeabi
- arm7s = build libfpcalc.so shared library for android-armeabi-v7a

### Example

As with the example(s) shown in the phorton1/ffmpeg repository, we will start from scratch, get the source, and build all of the above target exectuables on a linux host platform using ffmpeg version 0.9, starting in the empty /blah/fpcalc directory we create:

```bash
    mkdir /blah/fpcalc
    cd /blah/fpcalc
    git clone https://phorton1@bitbucket.org/phorton1/ffmpeg.git
    git clone https://phorton1@bitbucket.org/phorton1/chromaprint.git
    cd ffmpeg
    git checkout multi-0.9
    ./multi-configure all host
    ./multi-make all install
    cd ../chromaprint
    git checkout multi-0.9
    ./multi-configure all host
    ./multi-make all install
```

If you get through all that, congratulations! You have just built windows and linux executables and three flavors of android executables and shared libraries!  The result will be in the _install directory:

    +-- blah
        +-- fpcalc
            +-- ffmpeg              = the ffmpeg source
            +-- chromaprint         = the chromaprint source
            +-- _build              = the out of tree build
            +-- _install            = your final fpcalc executables and shared libraries
                +-- 0.9             = the version you built
                    +-- _linux      = the platform you built them on
                        +-- win     = contains the windows exectuable
                            fpcalc.exe
                        +-- x86     = contains the android-x86 executable
                            fpcalc
                        +-- arm     = contains the android-armeabi executable
                            fpcalc
                        +-- arm7    = contains the android-armeabi-v7a executable
                            fpcalc
                        +-- x86s     = contains the android-x86 shared library
                            libfpcalc.so
                        +-- arms    = contains the android-armeabi shared library
                            libfpcalc.so
                        +-- arm7s   = contains the android-armeabi-v7a shared library
                            libfpcalc.so
                        +-- host    = contains the Linux (host) executable
                            fpcalc


## Additional Features

These builds of fpCalc add additional features as described at phorton1/fpcalc-releases which were performed by careful modification to a single "C" file, fpcalc.c in the chromaprint/examples folder. Otherwise **the source code for chromaprint was not modified in any way** in general, or for any specific version of ffmpeg.

There were two source files added for the JNI android shared library builds, both in the the chromaprint/examples directory:

    fpcalc_jni.c - provides the "glue" between JNI and fpcalc
    jni_utils.c - contains the JNI_Onload() function

As mentioned in phorton1/fpalc-releases, the **jni_utils.c** file contains a novel approach to linking JNI libraries to Java Classes and may be of interest to readers separately from it's use here.  The file *should be* a "drop-in" bit of source code that *could* be added to any JNI library project.


## Notes

You can find a discussion of my motivation for this project, and an analysis of running all of the above executables and libraries in the phorton1/fpcalc-test repository.  The short answer is that **fpCalc appears to be sensitive to the the version of ffmpeg it is built against** and, to a lesser degree the platform it is built upon.

As such it is my recommendation that careful consideration of these sensitivities be given when producing "release" versions of fpCalc, inasmuch as different versions of ffmpeg will result in **significantly different fingerprints** for the same file, even on the same platform!


## Comments

It is worth noting that I found it all but impossible to utilize Lukas' CMake build mechanism with the Android NDK **stand-alone toolchain**.  CMake "hides" too much of the compiler/linker control to be useful, and is in fact, IMHO a hinderance to the general portability of the project.  After about 3 solid days of beating my head against a wall, I could not get CMake past it's "startup compiler identification" with the NDK, so, instead of trying to get CMake to play nicely with the NDK, I decided it would be faster, and more flexible, to create my own build mechanism.

This repository is the result.


## Credits and Licenses

This work is totally dependent on the ffmpeg and chromaprint open-source projects.

Credits to **Lukas Lalinsky** for his acoustid/chromaprint project.

This fork in no way alters the the Chromaprint license, which continues to be released under the GNU General Public License version 2.  Please see COPYING.txt for more information.
