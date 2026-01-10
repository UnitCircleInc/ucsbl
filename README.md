# Secure Boot Loader

The Secure Boot Loader performs the following operations on starup:

* Lock down processor (see [Platform Security Measures](doc/Security.md))
* If SW reset and Boot Loader state == IDLE and command == "update" run the following state machine:
    * Check that signature slot 1 is valid - if not then Verify/Run slot 0
    * Swap slot 0 and slot 1
    * Verify/Run slot 0
    * Copy slot 1 to slot 0 (i.e. restore original running code).
    * Verify/Run Slot 0
* If Boot Loader state != IDLE then continue running state machine from where left off.
* Otherwise Verify/Run slot 0.

> NOTE: In the above `Verify/Run slot 0` means: verify signature of slot 0, if valid run slot 0, otherwise proceed with next state or shutdown if no more states.

> NOTE: See [Signatures](doc/Signatures.md) for how signatures are verified.

The Secure Boot Loader is:

* Robust to resets happening at any point in FW update process (i.e. swap/restore processes).
* Robust to intentional, precision timed, double fault injection techniques (e.g. power rail brown outs, laser and EMP fault injection).
* Robust to attempting to install unauthorized FW updates.
* Small in size - less than 32kB.

Minimal Assumptions:

* Platform supports disabling of JTAG/SWD.
* Platform support limiting memory access to read only for at least 3 regions of FLASH (boot loader, manufacting data, and running application image (slot 0)).
* Access to a platform TNRG
* Does NOT rely on access to a RTC or similar monotonically increasing time based counter.  This is because many embedded systems do not have an RTC.  Even for those that do, there are very few systems that can guarantee that the local counter time matches (to within a few min) a national standard (or other time based centrally maintained source of "truth").  Even for those systems that could support maintaining that gurantee, they requrie online access to the time source, which the boot loader does not have access to.
* The application size less than ~1/2 the total amount of interal FLASH (after reserving space for Boot Loader, and manufacturing data and any internal FLASH databases).

![Example Memory Layout for nRF52840](doc/MemoryLayout.drawio.svg)

# FW Image Types

SBL supports the concept of different image types to support:

* Enginering Development and board bring up (Engineering Firmware Image - EFI)
* Manufacturing related testing/calibration (Manufacturing Firmware Image - MFI)
* End user application (Application Firmware Image - AFI)

The level of lock down SBL performs depends on the firmware image type that
is in slot 0.

* Once an AFI image is loaded it is not possible to load MFI or EFI images.
* Once an MFI image is loaded it is not possible to load an EFI image.
* All images enable read only access for the boot loader and running application image (slot 0)  FLASH areas.
* AFI images enable read only access for maufacturing data FLASH areas.

# Detailed Documentation

* [Release Testing](doc/ReleaseTesting.md)
* [Key Gen](doc/KeyGen.md) and [SBL Tool](doc/SblTool.md)
* [Integration of SBL and Application](doc/SblIntegration.md)
* [Signing Details](doc/Signing.md)
* [Signature Details](doc/Signatures.md)
* [SBL Build Machine Setup](doc/BuildSetup.md)
* [Plaform Security Measures](doc/Security.md)
* [Fault Injection](doc/FaultInjection.md)
* [Signing Authority](doc/SigningAuthority.md)
* [Ideas and Misc.](doc/Ideas.md)

# Building

```
make CONFIG=nrf52840-dk -j12    # enables logging for SBL developement
make CONFIG=nrf52840-none -j12  # disable logging for deployment
```

## Bootloaders Built
The Secure Bootloader make will generate two versions of the sbl

### sbl-dev
Intended for use by developers this version of the bootloader that disables the following security measures:

* Image type ratcheting.  Allows for EFI, MFI, and AFI images to be loaded in any order
* Timestamp checking on signature block.  Allows for loading earlier signed code.
* JTAG/SWD locking.  Allows the application to be debugged with GDB like tools, as well as allowing new application images to be loaded over JTAG/SWD.

### sbl-rel
The full version of the sbl with all security measures in place.

See `config` directory for config options.

# Load a Device From Scratch

* Bulk erase device (to remove JTAG/SWD lock if present)
* Load initial MFI over SWD
* Load Boot loader over SWD
* Reset device (or power cycle) to start running current APP (MFI).
* While running MFI can update to new MFIs/AFIs if needed using USB loader (MFIs/AFIs will need to have new signatures).
* While running AFI can update to new AFIs if needed using BLE loader (new AFI will need to have newer signature).

# FAQ

* How to save images in slot1?
    * See `prod-app/main.c` and `src-uc/fwu.c` and `tools/fwu.py` for examples.
* How to ask SBL to install "new" image in slot1?
    * Application should verify that there is sufficient battery capacity to perform the upgrade and possible restore.
    * Suggest at least 20mAh for upgrade/restore + what application needs to do to determine if upgrade is successfull (e.g. BIST analysis).
    * The application does not need to do any checks on the incomming binary other than to ensure that it does not write beyond the end of slot 1.
    * Call `sbl_cmd(CMD_INSTALL_APP)` (defined in `src-uc/sbl.h`).
* How long does SBL run?
    * Less than 30s for upgrade (on nRF52840).
    * Less than 15s for restore (on nRF52840).
* How to tell SBL to accept "new" image?
    * Minimallly erase the first page of slot 1.
    * You can of course erase all of slot 1 to prepare for the next FW update.
* How to tell SBL to reject "new" image and restore "old" image?
    * Feed the watchdog timers.
    * Force a reset using CMSIS `NVIC_SystemReset()`.
* How to retrive SBL install result/status?
    * Call `sbl_rsp()` (defined in `src-uc/sbl.h`).
* Does SBL modify the Reset Reason Flags?
    * Genereally no.
    * On nRF52840 to support errata 136 ([nRF52840 Errata](https://docs-be.nordicsemi.com/bundle/errata_nRF52840_Rev3/attach/nRF52840_Rev_3_Errata_v1.3.pdf)) the Reset Reason Flags are modified.
* How to retrive SBL version string?
    * Call `sbl_version()` (defined in `src-uc/sbl.h`).
* Why require both primary and secondary certs?
    * Root certs can't be rotated, ideally they are used only once to create
      the primary certifacte.  The primary key (and cert) can then be used
      to allow for secondary key rotation if needed (either time based or
      use based or on detection of key compromise).
* Why do certs and signatures not use version numbers to allow for changes
  to the cert/signature formats.
    * SBL is desiged to not be field updateable (too many security holes).
    * So the signature methods and ceritificate handling code only needs to
      deal with one format - the current/latest.
    * Adding a version #s adds more data and code, which expands the attack surface.
* Why does the root pk installed in the boot loader not need a more recent date than 1970-01-01T00:00Z?
    * When performing the initial SW installation during manufacturing you need to install both SBL and valid FW image.  This FW image has "recent" dates embedded in the signature block certificates and overall signature.
    * These dates then become the "lower bound" on any dates in future FW updates.
    * During a FW update SBL checks to ensure that:
        * primary cert of new FW is >= old FW
        * secondary cert of new FW is >= old FW
        * signature date of new FW > old FW
