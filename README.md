phorton1/chromaprint
=========================

This is the **multi-tip** branch of my fork of the acoustid/chromaprint repository.  This is also the "master" branch that will be gotten from bitbucket.

This fork/branch is being done in order to implement *multi-fpcalc* builds.

More specifically, this multi-tip branch builds against the multi-tip branch of phorton1/ffmpeg, which contains the version of ffmpeg at the time that I forked it from the main ffmpeg.org repository ... which was **ffmpeg version v2.7+**.

Note that all my builds of fpcalc take place using the **TIP** version of chromaprint at the time of my fork of it, but that that there are minor modifications in the build system in order to build it against different (older) versions of ffmpeg.

Please see the other multi branches **multi-0.9** and **multi-0.11** for additional ffmpeg version specific builds.

Credits to Lukáš Lalinský, the original author and maintainer of acoustid/chromaprint.
