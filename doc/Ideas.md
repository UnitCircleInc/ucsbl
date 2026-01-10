# Ideas

* Shamir's secret sharing will be switching to finite field Z/pZ with p == 2^255-19 (the finite field used by the private keys for RFC8032 Ed25519 variant).  This has no impact on signatures.
* SBL to replace Version fields with Verify field.  New Verify Field would used to enable/disable various verification steps.
    * One bit field for each of the verification steps listed "Secure Boot Loader Image Verification" section.
    * The actual verifications perform would be the union of all the Verify fields.
    * Method allows for up to 32 selectable verification steps.
    * Some of the bit fields can be used to indicate what lock features are to be applied (e.g. make MFI data and Slot 0 read only).
    * All steps performed, verification field is used as a mask to determine which ones are used to select if image "good".
    * This would make ucssm a bit more complex (mostly due to having to spec which verification bits are to be set).
    * This would make SBL source simpler as we wouldn't need `#ifdef` like switches inside.
    * Still need to be able to build multiple SBLs as each one will need a different root public key.  Or we have reserved FLASH in boot loader where we store the root public key which can be written before first run of boot loader (which will lock things down).
* Switch to [AEGIS-256](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-aegis-aead) for AEAD encryption.
* Consider replacing hash of image with hash of everything.
    * Would still have hash of header (excusing overall signature of header + code) this would allow prequalifying without having to check sig on everything.
    * Is there value 2 signatures one for signatgure block and one for signature block || firwmare image - maybe just overall - although would be nice to qualify N.
    * Perhaps N is not needed - can just know input must be padded with 0xff's.  Then always takes max time - but then we know max time.
    * Maybe check time to compute full hash over entire length - signature generation can pad input with 0xffffff to max length.
* Use Brown-Out-Reset flag (if availble) to trigger a tamper detect and force power down until next Power-On-Reset or Pin-Reset.

# Build image server and log decoder

* allow uploading of images (or the processed elf files so they are smaller)
* allow fetching  of images (or processed binaries) for local real time decoding
* allow submitting log binary to get decoded structured text (JSON/CSV/colon separated fields)
* allow fecting of decoding tables for live uclog-gui like use cases

<hr>

# Notes on Encryption, Signatures, HASHes

## [libsodium Authenticated Encryption](https://doc.libsodium.org/public-key_cryptography/authenticated_encryption)

* box - message, recipient_pk, sender_sk, nonce (e.g. time)
* unbox - message, sender_pk, recipeint_sk
* can't decode portions easily - so use to send secret stream key - used to encrypt image.

## [libsodium Secret stream key] (https://doc.libsodium.org/secret-key_cryptography/secretstream)

* Make block size = flash page size of 4096.
* Can do decoding in application - which still leave signature as part of image - that way boot loader does not need to deal with it!

## [libsodium Signature](https://doc.libsodium.org/public-key_cryptography/public-key_signatures)

* verify(hash(prefix || image data))

## Hash 

* libsodium generic -  uses blake2b internally - 
* libhydrogen - uses Gimli hash - with NIST SP 800-185 KMAC contruction - H(pad(str_enc("kmac") || str_enc(context)) || pad(str_enc(k)) ||  msg || right_enc(msg_len) || 0x00)

> Notes:
> [Security of Gimli Hash](https://eprint.iacr.org/2019/1080.pdf)
