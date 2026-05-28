# Release Testing

The script `tools/rel-test.py` runs a suite of automated tests used for release testing using a standard development board for the MCU (e.g. nRF52840-dk).  Assuming an nRF52840-dk is connected to a host computer, a typical invokation would be:

```bash
# Build board agnostic (what will be released) and board specific (for testing - with some byte serial transport for uclog)
$ make clean
$ make CONFIG=nrf52840-none
$ make CONFIG=nrf52840-dk

# In one terminal window run
$ make CONFIG=nrf52840-dk uclog

# In second terminal window run
$ tools/rel-test.py --sbl bin-nrf52840-none/sbl-rel/sbl-rel.hex bin-nrf52840-dk
```

The parameters specifies the boot loader to test and the bin directory that contains the instrumented example EFI, MFI, and AFI firmware images.

The example EFI, MFI, and AFI are instrumented with the following features implemented using commands that are sent over `uclog`.

* Ability to perform firmware updates with `fw-start`, `fw-block`, `fw-schedule`.
* Ability to `accept` or `reject` a recently installed firmware image.
* Ability to force a `watch-dog` reset.
* Ability to `flash-write` and `flash-erase`.
* Ability to retrieve information about the running firmware using `info-app`.  Information includes current signature block image timestamp and version string as well as firmware update installation status.

Additionally the `tools/rel-test.py` script makes use of SWD interface to recover (remove any protections and mass erase FLASH) a device, force a soft-reset (set SysResetReq or AIRCR).

The tests are organized into the following suites:

* __Normal__ - tests that verify normal operation
* __Double Updates__ - tests that verify multiple  updates without intevening `accept`/`reject` work.
* __White Box__ - tests that signature blocks with correct signatures, but incorrect/malformed data contents (bad cert chain, bad cert timestamps, bad type, bad version string, image too long, extra data in slot beyond end of image) are not accepted as valid.
* __Corruption__ - tests that verify corrupt images (bit flips) are not accepted as valid.  Tests force corruption of each of the fields in the signature block and several examples of bit flips in the firmware image.
* __Downgrade__ - tests that verify that the image type ratcheting (EFI->MFI->AFI) and timestamp ratcheting (primary certificate timestamp of new >= old, secondary certificate timestamp of new >= old, and image timestamp of new > old) are enforced.
* __Recovery__ - tests that verify after a firmware update, a reset before `accept` causes recovery to the original firmware
* __Accept__  - tests that verify after a firmware update, a reset after `accept` continues to run the new firmware.
* __Write/Erase EFI/MFI/AFI__ - tests that verify FLASH writes/erases pass or fail depending on the FLASH area and the current image that is running.   Writes/erases to the boot loader and app (slot0) always fail.  Writes/erases to manufacturing data area pass for EFI/MFI and fail for AFI.
* __Slot0 White Box__ - same tests as __White Box__ only performed as though the images had been installed as opposed to an attempt to install via a firmware update.
* __Slot0 Corruption__ - same tests as __Corruption__ only performed as though the images had been installed as opposed to an attempt to install via a firmware update.
* __SWD__ - tests to ensure that SWD is unlocked/locked depending on boot loader type.  For development boot loader SWD should be unlocked.  For release boot loader SWD should be locked.

Each test follows the rough pattern of:

* Use SWD to load a boot loader and known firmware image and verify that it is running
* Attempt to perform a firmware update and:
    * verify that the new firmmare is running if expecting success, or
    * verify that the original formware is running if expecting failure

> Note: __White Box__ and __Slot0 White Box__ tests might also be done using a [Fuzzing](https://en.wikipedia.org/wiki/Fuzzing) approach.  Given the interconnectedness of the signature fields within the signature block, this would be a very inefficient method to tests the signature block validation code.  Most fuzz (corruption) attempts would fail the signature tests, and non-of the remaining tests would be performed.   The __Corruption__ and __Slot0 Corruption__ are more similar to Fuzzing tests, just a select few specific corruption examples chosen based on the signature block layout.

> Note: __Slot0 White Box__ and __Slot0 Corruption__ test suites test conditions that, if present on shipped devices, will result in _bricking_ the device.  It is not expected that in-field devices will experience these conditions except in cases of HW failure of some kind, or if some form of fault injection attack is being performed.

> Note: These test suites do not cover any testing related to robustness to fault injection attacks.  To test robustness to fault injection attacks requries special HW setup and signficant time resources.  Although SBL is designed to be robust to fault injection attacks, its primary purpose is non-physical access attacks.  Non-physical access attacks are well covered by the above tests suites.

> Note: The SBL under test needs to be an uncomstimzed SBL as `rel-test.py` will perform the customization using the `--manu-data-size` and `--max-app-size` arguments (or default values) and using a randomly generated root key.
