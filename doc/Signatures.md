# Signing Authority

The signing authority accepts requests to:

* Create named public/private key pairs to be used for signing operations.
* Revoke existing keys, preventing their use for signing operations.
* List available keys and their corresponding public key values.
* Create named certificates for keys creating certificate chains.
* List available certificates
* Delete certificates
* Sign code
* Verify signatures

[Edwards-Curve Digital Signature Algorithm (EdDSA) [RC8032]](https://datatracker.ietf.org/doc/html/rfc8032) using the Ed25519 variant is used for all signatures.
The Signing Authority can provide random passphrases based on [Diceware](https://www.eff.org/dice) using the [EFF short wordlist 2.0](https://www.eff.org/files/2016/09/08/eff_short_wordlist_2_0.txt) with ~ 82 bits of entropy when generating the keys.

The private key portion of all keys are stored in encrypted form using one of the following methods:

1. The private key is not stored directly, but rather it is split using [Shamir's secret sharing](https://en.wikipedia.org/wiki/Shamir's_secret_sharing) when created.  The shares are then distributed to those individuals authorized to perform signing operations using the key.  Each share is encrypted with its own passphrase.  To perform a signing operation requires a quorum of the key shares (along with their corresponding passphrase) to be presented to the Signing Authority.  Typically this approach is used for root keys.  The number of shares and the quorum size can be selected when generating the key.
2. The private key is stored directly encrypted using a passphrase to encrypt its value . The passphrase is distributed to those individuals authorized to perform signing operations using the key. To perform a signing operation requires that the corresponding passphrase is presented to the Signing Authority.  Typically this approach is used for non-root keys.

For the case of Shamir's secret sharing for N shares for a quorum of M performs the following operations:


* Use the following prime field for all operations GF(2^256) = GF(2)[X]/(P) with P =  x^256+x^10+x^5+x^2+1
* Enure tha the secret to be shared is not 0 - stop processing if it is 0.
* Generate a random polynomial p of order M-1 over the given finite field.  If coefficient highest order term is zero then generate a new random polynomial.
* Replace the zeroth order coefficient of the polynomial with the secret data (which is an element of the prime field).
* Evaluate the polynomial p(x) for values from 1 to N, record each x value and the corresponding p(x) as a share.
* Use AEAD\_AES\_256\_GCM  (see below) encryption to encrypt each p(x) value from the share along with associated data x, using a passphrase.
* For each share output x, encrpyted p(x) and the passphrase.

For the case of the private key being directly stored in encrypted form using a passphrase, the following encryption method based on [RFC5116](https://www.rfc-editor.org/rfc/pdfrfc/rfc5116.txt.pdf) and [NIST 800-38D](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf) is used:

* Generate a random salt of length 16 bytes.
* Generate an encryption key of length 32 bytes using `encryption key = scrypt(passphrase, salt, keylen=32, N=2^20, r = 8, p = 1)`
* Generate a random nonce of length 16 bytes.
* Compute encrypted private key, MAC = AEAD\_AES\_256\_GCM\_Encrypt(msg = private key, ad = nonce || salt, key = encryption key, nonce = nonce)
* Save (nonce || salt || MAC || encrypted private key)


The Signing Authority saves all keys and certificates in the following python data structure that is saved in a local [CBOR](https://cbor.io) encoded file.

```
	{
		"keys": {
			<keyname>: (<public key portion bytes>, <encrypted secret key portion bytes>),
			...
		},
		"certs": {
			<certname>: (<cert bytes>, <name of parent cert>),
			...
		}
	}
```

# Signature Block (and Certificate) Details

![Memory Layout](SignatureBlock.drawio.svg)

# Secure Boot Loader Image Verification

 The Secure Boot Loader performs the following verification steps before running an image:

* Uses the Root Pk to verify the signatures in certificate chain (Primary Certificate and Secondary Certificate) and Signature Block.
* Verifies that the date of Primary Certificate is later than the build date of the Secure Boot Loader binary
* Verifies the Secondary Certificate date is later than or equal to than the Primary Certificate date
* Verifies the Signature Block date is later Secondary Certificate
* Verifies that the Version String is null termainated and image <type> is one of AFI (Application Firmware Image), MFI (Manufacturing Firmware Image) or EFI (Engineering Firmware Image).
* Verifies that the Hash of Image matches the hash of the Application Binary.
* Verifies that all bytes between the end of the Image and the Maximum Image Size are 0xff.

The Secure Boot Loader performs the following additional verification steps before installing a new image:

* Apply all the verification steps before running an image to the new image.
* Verify that the Primary Certificate date of the new image is greater than or equal to the existing image's Primary Certificate date.
* Verify that the Secondary Certificate date of the new image is greater than or equal to the existing image's Secondary Certificate date.
* Verify that the Signature Block date of the new image is later than the existing image.
* Verify that if the existing image is of type MFI, then the new image is of type either MFI or AFI.
* Verify that if the existing image is of type AFI, then the new image is of type AFI.

# Signature Integration with Zephyr

An empty signature block is compiled into the image from `<repo>/lib/uc/signature.c`.
The FLASH image layout is adjusted to include 512 byte reserved area at the start of the image in `<repo>/lib/uc/signature.ld`.
Once zephyr's build system (cmake) has completed compiling and producing the `build/zephyr/zephry.elf` file the following operations are performed to compute the signature and apply it to the image.

* `scripts/ucssm.py --db ucssm.db --passpharse <passphrase> sign --bin image.sig Release-L2-Code-Signing image.bin`
    * image.sig contains the signature
    * ucssm.db has encrypted private keys needed for signing - the passphrase is used to decrypt the private key.
* `arm-zephyr-eabi-objcopy --dump-section rom_start=image.rom_start image.elf`
    * extract the signature area from the built image (image.elf)
* `dd if=image.sig of=image.rom_start bs=1 seek=0 count=512 conv=notrunc status=none`
    * Update the signature area with the signature
* `arm-zephyr-eabi-objcopy --update-section rom_start=image.rom_start image.elf image.signed.elf`
    * Update the built image with the signature (image.signed.elf)
* `arm-zephyr-eabi-objcopy -O binary --gap-fill 0xff --remove-section-.comment --remove-section=COMMON --remove-section=.eh_frame image.signed.elf image.signed.bin`
    * Output binary form which is needed to update devices in the field
* `arm-zephyr-eabi-objcopy -O hex --gap-fill 0xff --remove-section-.comment --remove-section=COMMON --remove-section=.eh_frame image.signed.elf image.signed.hex`
    * Output hex form - handy for manually loading code during development.

The above steps are hooked into the standard Zephyr build using `<repo>/lib/uc/signature.cmake` (via `CMakeFiles.txt` includes).

# Automatic Signatures with Git push and Zephyr

Currently signing is "automatically" authorized image signing on every build pushed to git.
This makes use of the appropriate workflow in `.github/workflows` to:

* Setup a docker container to host a specific linux version.
* Install all the required Zephyr and NCS tools (along with any dependencies)
* Install all python dependencies.
* Install the user developed source code (i.e the code contained in the git repo).
* Install all the required Zephyr and NCS source code (along with any dependencies).
* Run the Zephyr build command to build the image and then sign the image.
    * The passphrase needed for signing (along with the encrypted secret key and certificates) are injected into the build environment as Github secrets.
    * The passphrase is needed when computing the signature.

# Manual Signatures with Git dispatch workflow

There is an optoin to switch to "manually" authorized image signing, and only for those builds for which it make sense to sign.
This would remove the automatic signatures that are currently part of the Zephyr build system resulting in unsigned FW image artifacts as a default.
A python script to manually authorize signing FW images would:

* Log start of signing operation to block chain based Code Signing Logging Server.
* Prompt for date/time to use on signature (use machine as default)
* Prompt for certificate details to use for signature  (certificates are public - no key material yet)
* Prompt for Git tag/release/??? for FW image to sign (use latest as default).
* Compute signature block
    * Download FW image from Github
    * Compute hash of FW image.
    * Prompt for key shares and pass-phrases associated with shares for signing key associated certificate (text - type in) - reconstruct signing key.
    * Compute signature block
    * Delete signing key
* Submit dispatch workflow to Github
    * Workflow attaches sisgnature block to FW image to create signed FW image
    * Workflow creates artifacts and release.
* Log end of signing operation to block chain Code Signing Logging Server.
    * Include signing details (user, machine, date/time, FW image, signature, ...)
* Python script exits - suggest power-cycle/reboot machine after signing complete.
* Need to trust code/machine/network/etc.

> NOTE: If the script is unable to contact the Code Signing Logging Server the script would exit without performing any signing operations.

# Mitigation Strategies

* Identity  (rogue device detection) - protocol is for service to ask device to sign a nonce - return value is sign(public key || nonce)
    * Anyone can ask - so a bit of a pain - as might be asked to sign a lot of nonces - need to rate limit so signing takes several seconds.
    * Could require that device know a root identity pk so that requests needs to be signed (with cert chain) - this can be compiled into the code.
* End-to-end encryption/authentication of data - protocol is to use sealed box construction or KK like construction - requires knowledge of server public key.
    * Need to add root data pk and then some way to process certificates this can be compiled into code so can change with new releases.
* Encrypted  FW Update
    * server uses public key to encrypt data to send to device - N variant of key exchange.  Don't need server PK because we already know root code pk and that is checked before landing code.
* Trusting Apps

# List of threats (and corresponding mitigations I)

List of handled threats:

* Loading of firwamre without valid signatures using firmware update process - images are signed - certs to allow for key rolling - forward up dates only
* Erasing of MCU (e.g. nRF recover over SWD) and loading of new FW - device identity public/private key.
* Clones - device identify public/private key.
* IP embedded in image - encrypted in transit - lock down of SWD/AP so can't be read out - could also encrypt image at server and decrypt on device.
* User data in external FLASH- encrypted while in transit (over BL) - database erase with secure overwrite - could add external FLASH encryption (to mitigate desoldering of FLASH and then read out) - could add encryption of data in transit so that can only be decrypted at servers.
* User data in internal FLASH - lock own of SWD/AP so can't be read out.

# List of threats (and corresponding mitigations II)

* Loading of non Manufacturer code using firmware update process - images are signed - certs to allow for key rolling - forward up dates only
* Erasing of MCU (e.g. nRF device recovery) and loading of new "fake" FW - device identity public/private key (challenge response on connect)
* Clones (looks like real HW but runs "fake" FW) - device identify public/private key (challenge response on connect)
* IP embedded in image - encrypted in transit - lock down of JTAG so can't be read out - could also encrypt image at server and decrypt on device (needs either pre-saved code encryption key or per device public/private key for code encryption)
* User session data in external FLASH- encrypted while in transit (over BL) - database erase (FLASH reset of all FFs) for "factory reset"- could add external FLASH encryption (to mitigate desolating of FLASH and read out) - could add encryption of data that can only be decrypted at servers (needs public/private dat encryption key).
* User configuration data in internal FLASH - lock down of JTAG so can't be read out - limited APIs for reading - "factory reset"
* "Fake" APP - could do a MITM attack - Device validates APP on connect (using per phone public/private keys assigned by Manufacturer's servers) - or do end to end data encryption - use phone manufacturer DeviceCheck APIs [Apple - Validating Apps that Connect to your Servier](https://developer.apple.com/documentation/devicecheck/validating_apps_that_connect_to_your_server) and [Android - Play integrity and signing services](https://developer.android.com/google/play/integrity) to validate app with servers.
* MAC address tracking - switch MAC addresses every 15min when advertising - App periodically disconnects to force a new MAC address on next connection (suggest to do just after all data transferred Bluetooth and perhaps every hour when inactive.
* Device tracking Device Id in publicly accessible form - in advertising data (can be used to filter - and avoid connection to "fake" devices - although this can easily be spoofed) - mitigated by limited advertising periods once a device is paired.  Device is normally connected to phone - so no advertising in this state.  Only advertises when out of range of phone (e.g. device left at home) or briefly after "hourly" reconnects initiated by phone to get new MAC address.
    * Can make Device ID the hash of the current time (e.g. nearest 1hr)
    * 2^16 is still a significant number of user base - so was enough to single people out by location.
* Just works pairing - requires double button press to "authorize" pairing (mitigates unauthorized pairing without physical control).   BLE encryption key extraction is possible if adversary can listen to pairing exchange - can mitigate by using public/private keys to establish secure link either bypassing or layering with BLE encryption.

# FAQ

* What happens if non-root signing key is compromised?
    *  E.g. If Secret Key Portion exposed adversary can now sign Evil images and make them look like they are from Manufacturer.
    * Evil image can brick device (or other behaviour change) - not much we can do - with reset pin and BL having BLE support could "upgrade" over image.

> __Mitigation:__ On detection create new key(s) corresponding certificate(s) for the affected keys.  Then release a new image (or re-sign the current image) signed with the new key.  Then force FW updates on in-field devices.  This will "rachet" the dates in the certificates, preventing the older certificates (which adversary will need to use) from being accepted as valid.  The goal is to generate the new key(s), certificate(s) and image as quickly as possible and disribute to as wide as possible to "lock out" and signatures created by the compromised key(s).  The adversary may be able to get their Evil image installed on some devices, for which the manufacturer may have RMA.  If this strategy is combined with a authenticated FW image delivery server, then the adversary would need to compromise the phone App, or the authenticated FW delivery server, increasing the "cost" of injecting Evil images to in-field devices.


# Reference Material
* [Public key authenticated encryption and why you want it](https://neilmadden.blog/2018/11/26/public-key-authenticated-encryption-and-why-you-want-it-part-ii/)
* [A Graduate Course in Applied Cryptography](https://toc.cryptobook.us) ([A Graduate Course in Applied Cryptography - Book](https://crypto.stanford.edu/~dabo/cryptobook/BonehShoup_0_6.pdf))
* [EdDSA for more curves](https://cryptojedi.org/papers/eddsa-20150704.pdf)
* [ED25519 Super Cop Reference code](https://github.com/warner/python-ed25519/tree/master/src/ed25519-supercop-ref)
* [Simpler Python Signature Library](https://cryptobook.nakov.com/digital-signatures/eddsa-sign-verify-examples)
* [NIST FIPS.186-5 Digital Signature Standard (DSS)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-5.pdf#page35)
* [NIST SP.800 Protecting Keys](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-57pt1r5.pdf#page43)
