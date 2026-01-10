# Platform Security Measures

## Assumptions

* Boot loader does not deal with secrets (but applcation and manufacturing data areas likely do deal with secrets).
* Encrypted images will be decypted by application loading new image into slot1.   As manufacturing data and slot0 needs to be in the clear, no reduction in security to have slot1 in the clear as well.
* Boot loader is compiled with public key of the root code signing server.  This root key has life of product.
* Adversary cannot modify Public Key of Root Signing Server or Boot Loader code after manufacture without long term physical access.
* Adversary cannot bypass the running of the Boot Loader and cause execution at known start address without long term physical access.
* Adversary cannot perform arbitraty reads of the MCU FLASH without long term physical access.

## Concerns

* Read internal MCU FLASH contents to extract secrets (private keys portion of Public Key crypto, pre-shared keys for Symetric crypto) to allow decrypting private data, for creating "fake" data.
* Read inernal MCU FLASH contents to extract code for reverse engineering.
* Modify internal MCU FLASH contents to replace public keys used for signature verfication or inject "fake" code.

## Physical Access

* Physical access allows monitoring signals between components, removing components (allow direct manipulation of components).  This can be used to decode communication between components, or extract memory contents of unprojected components (e.g. FLASH memory chips).
*  Physical access will allow adversary to inject HW faults into MCU - control of power supply, EMP, ... These injected HW faults cause read/write data errors, ALU computation errors, code execution errors (execute wrong instruction, bypass instruction, ....)
*  Physical access allows adversary to replace components (e.g. replace MCU with "fake" MCU).  Can be done with PCB replacement.
*  Physical access allow for erase/recovery of access to MCU/FLASH.  E.g. epose die to UV to erase only security portions of FLASH, or repair blown fuses, or interrupt recover procedure, ...
*  Physical access allows adversary to extract FLASH contents by direct probing of die (after removal from PCB and removing packaging).  Allows access to FLASH contents - code, private keys, ....

> There are no mitigations for attacks that directly access the MCU die.

> There are mitigations for the following, both require the use of private keys specific to the device and are not available to the Boot Loader:

> * MCU replacement - must be implemented by an application with the ability to communicate with a central "authentication" server.
> * External FLASH containing user data - must be implemented by an application with ability to encrypt data before sending to external FLASH.  This would handle monitoring signals and component replacement.

> The Boot Loader only attempts to mitigate fault injection attacks.


## Non-Physical Access

Boot loader has no direct inputs that would provide for non-physical access to the MCU FLASH/RAM (e.g. does not support wireless communication like Bluetooth).  Indirectly the boot loader can get input via new firmware update requests and during the procedure to restore the existing FW image if the upgrade fails.  New FW images can be controlled by non-physical access methods is the device supports it (e.g. hacking Bluetooth communications which can be done without physical access). Running an unsigned image can only happen if a FW image signature verification is some how bypassed.   All mitigations support ensuring that signature verification cannot be bypassed.

Additionally bugs (e.g. buffer overflow bugs) in the application may allow non-physical access (via hacking Bluetooth connections or direct access via normal Bluetooth connections).  All mitigations support ensuring that the application cannot write/erase any boot loader areas or write/erase directly to the running application or the maufacturing data areas.

## Physical Access Mitigations

* Goal of pysical access mitigation measures raise cost of extracting/modifying internal MCU FLASH contents to cost of directly probing silicon die.
* Assume attacker can relaibly inject a fault at a fixed time offset from reset and cannot inject a second fault closer than 2s from the first fault.
* HW System Deisgn power supply brown-out-reset (BOR) capability to mitigate power supply "bumping" to force injecting data/instruction read/write errors.

## Overall Counter Measures

* Signed Application Binaries (Images) using RFC8032 signatures.  Signatures verified by boot loader before running/installing/rolling-back.
* Non-correlating/constant-time/constant-power/side-channel-resistant code/peripherals for operations involving secrets.  If not available - use protocols that minimize reuse of key material (e.g. hashing keys before next use).  __NOTE: SBL does not contina any secrets.__
* Use MCU Watchdog timer to catch fault injections releated to manupulating external clock sources.
* Use of MCU True RNG to randomize timing of all other security measures.  Makes it harder for attacker to use repeatable (timed from reset) fault injection as an attack.
* Use of MCU True RNG to randomize order of reading of array contents, e.g. memcmp.
* Performing operations multiple times (with random delay) to ensure that single fault injection cannot reliably bypass signature checks and Lock Down procedures.   Makes it harder as now attacker has to trigger multiple fault injection given unpredictable timing of repeated operations.
* Use values for True/False other than 0x00000000 or 0xffffffff to indicate success/failure.   Other values are harder to "generate" with fault injection attacks.
* Use data driven computations rather than if/else constructs for handling flow control.  Makes it harder to bypass flow control with fault injection attacks.
* Verify function call sequencing using SHA512 of instrumented functions for signature checking.  Makes it harder to bypass signature checking using fault injection.
* Implement a statemachine that is persisted in internal FLASH such that FW update/restore procedures are resistent to resets or battery failures.   Once power is restored or reset removed, the Boot Loader with pickup where it left off.
* On Boot Loader exit or showndown ensure that "jump" instructions are padded with repeats to mitigate fault injection attacks.

## Lock Down Procedure

For release builds of SBL the following additional checks and HW configuration are performed:

* Lock down Access Port and Debug via SWD interface preventing SWD access to FLASH/RAM/DBG.
* Lock down to prevent down grades from AFI to MFI.
* Lock down to prevent down grades based on signature time stamps.
* Lock down FLASH access prevent write/erase to boot loader, manufacturing data area (if AFI) and slot0.
* Ensure WDT timer runs in all CPU states (RUN/SLEEP/HALT).

For development builds of SBL the above lock down procedures are not enforced.  The WDT timer is configured to only run in RUN/SLEEP states, in HALT state the WDT timer is disabled to allow debugging/single stepping.

# Encryped Images (Planned Feature)

## Encrypted images - single exchange between device and server (preferred approach)

* See [N-Variant](https://github.com/jedisct1/libhydrogen/wiki/N-variant) - the update server is the client (initiator) and the devices are the servers (responders) - packet1 - is included in the update - and session Tx key is use to encrypt the image - session Rx is not needed.
* Public key of device (along with device ID) registered with server during BLT/FAT.
* Phone checks for updates with update server (using device FW Update Pk)
* If update available then server send encrypted image including packet 1 of NN protocol
* Device can then process packet 1 and use the session Tx key to decrypt the incoming image data.
* Image is decrypted as it is being received so that it is stored in internal FLASH in decrypted form.
* Images are still signed.

## Encrypted images - pre-shared key (e.g. AES-256)

* Confidentiality of images in transit and at rest when not installed as the current executing image so that reverse engineering and IP theft are mitigated. 
* If the pre-shared key is "leaked" then the images are easily decrypted.
* No real mitigation strategy - so private key must not be "exportable" outside the MCU!
* Image is decrypted as it is being received so that it is stored in internal FLASH in decrypted form.
* Images are still signed.
* Each device will need to have the pre-shared key installed during BLT/FAT.
* Server encrypts image with pre-shared key for all clients and distributes - will need IV etc to ensure we can re-use key.
* Could use different pre-shared key for each device, then encrypt images on the fly (i.e. when image is requested by device).


# Comments

* Only code that has been appropriately signed should be run so that loading of unapproved SW on the device is mitigated.
* If the private key of the Image Signing root key is "leaked" then anyone can sign images that can then be loaded onto devices.
* Signing is done using a certificate chain that allows for creating intermediate signing keys that the device can sill accept.
* Rotating the intermediate key can mitigate the impact of "leaking" of the private key of an intermediate signing key.
* Certificates have ratcheting counters (timestamps)to mitigate the impact of "leaking of the private key of an intermediate signing key.
* Root signing is a rare event that can have a lot of ceremony around it (keys are in air gapped vault, ...) mitigating "leaking" of the private key of the root signing key.
* For development devices are provided with a "developer root signing key" that is installed on the device.
* Having two root signing keys mitigates leaking of private key of developer root signing key.
* Developer root signing key can be rotated at regular time intervals.
Developers get their own intermediate signing keys so that they can sign images locally for development.
