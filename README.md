# hbsdmon

* License: 2-Clause BSD
* Author: Shawn Webb

`hbsdmon` is the HardenedBSD infrastructure monitoring daemon. Its job
is to periodically ping the different bits of HardenedBSD's
infrastructure and notify via [Pushover](https://pushover.net/).

Included in this work is a `libpushover` C library. To make life
easier for me, I'm developing a modern C abstraction layer for
Pushover. The eventual goal is to mature `libpushover` enough such
that it takes on its own life and moves out of this repo. "Get off my
lawn, kid!"
