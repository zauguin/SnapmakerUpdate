# Snapmaker2 update bundle tools
Helpers to pack/unpack Snapmaker2 update bundles.

__WARNING: Flashing modified firmware to your device is dangerous and might
render your device inoperable. Since this also affect the updating mechanism
itself you might not be able to restore a working firmware version. Exercise
extreme caution and proceed at your own risk.__

__This program is distributed in the hope that it will be useful, but
without any warrenty; without even the implied warranty of merchantability
or fitness for a particular purpose.__

Snapmaker provides source code for the firmware of the Snapmaker2, but no way of
actually flashing the firmware to a device. Therefore this is an attempt to
reverse-engineer the update mechanism to allow you to modify the firmware on
your device.

All instructions will assume that you use Linux and know how to work with the
shell. For other platforms, the instructions should be relativly easy to adapt.

## Compiling

You need a C++ compiler with basic support for C++20 and [libfmt](https://github.com/fmtlib/fmt)
installed on your system to compile these tools.
(This last requirement does not apply if your C++ compiler supports C++20 completely, especially `std::format`.)

Run `make` in the directory containing the source files from this repository to compile.

## Example usage

I will assume a few set environment variables:

  * `$MARLIN`: The root directory of the Marlin source
    (the firmware should already have been compiled)
  * `$UPDATE`: An existing Snapmaker update from which we want to extract the
    Screen and Enclosure firmware. (The examples will use the current
    `Snapmaker2_V1.10.1.bin` but other versions should work too)
  * `$TOOLS`: The directory which contains `update` and `package`  compiled from the
    code in this repository.

First switch to some empty working directory. Then our first step is to extract
the original update with the `update` utility. By only passing one argument (the
name of the update file), it allows to extract the update into it's components. Run

    $TOOLS/update $UPDATE

This prints the version of the update you extracted, e.g.

    Snapmaker2_V1.10.1_20200822

Additionally there are new files in your current directory:

  * `controller.bin.packet`: The firmware version we want to replace.
  * `screen.apk`: The Android application runnin the touchscreen. 
    (As an android application it can only be updated what it is signed with the
    right certificate, so we can't mess with this one currently.)
  * `module0.bin.packet`: This seems to be the enclosure firmware (?). AFAICT the
    code isn't released (yet?), so only change it if you know what you are
    doing.

(If you are reading this in the distant future there might be additional
`module?.bin.packet` files for additional components.)

You might have noticed that firmware files have a `.packet` extension. This
indicates that they have an additional wrapper around the raw image. (Actually
it's just a 2048 byte header, so just drop the first 2048 byte if you need the
raw image).
We want to replace the controller firmware, so we need to add such a wrapper to
our compiler version. This is what `package` is used for. It needs some additional
arguments:

  * Do you want to set a flag? Then add `--flag` as first argument.
    I have no clue what this flag is doing, but it is set for more recent
    controller firmware versions, so I'll set it in this example. (The option
    will get a better name once I know what it does)
  * Are you wrapping controller formware (`controller`) for module firmware
    (`module`)?
  * A new version name. This is version e.g. displayed as Controller version in
    the settings menu. It should always have the prefix "`Snapmaker_`". (The
    settings menu tries to strip this prefix before displaying by just removing
    the first ten characters, this might lead to "interesting" effects if the
    prefix is missing) E.g. the controller firmware packaged with the V1.10.1
    update had version `Snapmaker_V3.2.2`. Maximal length are 32 byte. During an
    update, the controller firmware is only changed if the version changed, so
    try to avoid collisions with official firmware.
  * `StartID` and `EndID`: I *think* they are only relevant for module and not for
    controller firmware. Basically they define a range of CAN Ids
    (shifted by 20bit) to send the update to. In practise it is always set to
    the pair `0 20`. (probably at least in parts because there is a missing shift
    in the firmware code and therefore all components appear as id 0 in this
    test) (Should someone from Snapmaker read this: If you make your GitHub repo
    public, I'll send you a PR to fix this)

The to be wrapped firmware image is read from standard input, the wrapped image
is written to standard output. So we run

    $TOOLS/package --flag controller Snapmaker_V3.2.2_MK1 0 20 \
      < $MARLIN/.pioenvs/GD32F105/firmware.bin > controller_new.bin.packet

Now we have the wrapped fimware in `controller_new.bin.packet`.
To wrap this in a update bundle, we again have to decide if we want to apply a
flag (`--force` to indicate that this update should be "forced", but it does not
seem to have a big effect) and a version name. We pass these together with all
files we want to include in the update the the `update` utility. All official
updates (yet) contain all three components, but I it's also possible to create
"partial updates" e.g. only containing the controller firmware.
Here we run

    $TOOLS/update --force Snapmaker2_V1.10.1_20200821_MK1 controller_new.bin.packet > Snapmaker_FW.bin

Now you can copy Snapmaker_FW.bin to a Snapmaker2 printer and install it like
any other update.
