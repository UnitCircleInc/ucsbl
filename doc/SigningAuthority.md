# Option 0 - Use SBL.py

* Python script with secret portion of keys only decrypted for duration of signing activity.
* Root key not stored in data base.
* Passphrase based for access.
* Easy to integrate into build processes (no need to manage HSM and associted key distribution/compromise concerns).


# Option 1 - Air Gapped Laptop with PIN encrypted USB FLASH Drives for Communication
* Existing HSM on a Laptop
* Example USB drives
    * [Kingston Ironkey](https://www.kingston.com/en/usb-flash-drives/ironkey-kp200-encrypted-usb-flash-drive?capacity=8gb&connector=usb-c)
    * [Apricorn AEGIS Secure Key](https://apricorn.com/aegis-secure-key-3nx)

# Option 2 - Air Gapped Phone App with QR Code Video for Communication.

* Central single private signing key that has been split.
* Each split is encrypted with a pass phrase
* Splits ore stored on paper (which can be copied for backups) in QR code form
* Pass phrases are stored separately from QR codes (e.g. in 1Password or similar pass word keeper).
* Key Generation
    * "Computer" is asked to generate a new key with K,N splits and corresponding pass phrases
    * Computer displays QR code of public key
    * For each split
        * Computer display QR code of split so that photo can be taken
        * Computer display's pass phrase so that it can be record (memorized or saved in password keeper).
    * It is recommended that an attempt is made to sign and then verify a signature.
* At signing time
    * Data (or hash of Data) is provided to "computer" - transfer of image (or hash) is over QR code (or a sequence of QR codes with fountain coding).  Dat can be FW image to sign or Certificate to sign (previously formatted).
    * "Computer" presents details of what data is to be signed along with current date/time (which can be updated by users).
    * All parties (K of N) gather and provide their QR codes and pass phrases to "computer"
    * "Computer" computes signature and outputs QR code of signature.
    * "Computer" erases memory and forgets all details of transaction.
    * Users should verify signature (using a separate computer that has the public key).
* "Computer" can be a a cell phone running an app or a laptop running an app - anything that supports C/Swift/Flutter/Python code and can display QR codes and accept QR codes and has a keyboard.
    * After loading app - "computer" is disconnected from network and remains in a physically secure location with battery charge being maintained.
    * Assumption is that "computer" is now an appliance that is not upgraded - no new OS versions no new app versions  - like an HSM (except it does not store keys).
    * It would be prudent to have at least 2 "computers" configured and maintained.
* Long term support (assuming UnitCircle/StarIC go out of business):
    * By forcing no update on "computers" we can avoid maintenance of the SW - at least for expected life time of appliance ~ 10yrs.
    * Source code can be made available to "customer" if they want to take on maintenance or get a 3rd party to maintain as a backup plan.
* Key Backup
    * Keys are printed on paper and or scanned into phones.
    * Pass phrases are memorized or stored in password keepers.
* Split Key Revocation
    * Destroy all copies of split.
    * Possible to "rekey" shares that were not "compromised" (basically generate a new random polynomial with perhaps different x values).
* Key Revocation
    * Destroy at least N-K+1 spits.
* +ves
    * Fairly simple process
    * Low maintenance on app and it is a "once and done" - don't need to track OS/platform changes, etc.
* -ves
    * Requires that all signers be physically present for signing.
    * Hard to get from key/split get to paper - would be nice to have "normal" app that scans QR code and converts to something to print.  And can scan passphrase (perhaps OCR or QR) and scan store in password keeper.  Should test photo of QR code - can it be scanned?  Photo of text will OCR pick it up.
* Phone App Implementation details:
    * Write program to transfer data video camera using a sequence of QR codes.
QR codes contain blocks of data that are protected using a fountain code to allow for easy sync and reconstruction.
    * Phone App to:
        * After installing App - wireless networking is disabled.
        * Run app - verify date/time.
        * Select signing type - file or certificate
        * Receive a file/cert for signing - use fountain code to break file into blocks, then sent as a sequence of QR coded images.
        * Once transfer is complete, its size and hash are displayed if file, otherwise display cert details (public key being signed, date).
        * User should verify that hash and size are correct.
        * Sending end also displays size and hash
        * Could have app display size and hash as QR code to send back to originator.
        * Receive key/cert for signing  - app scans K of the N QR codes
        * Key split using Shamir Secret Splitting into N QR codes of which K are needed for reconstruction.
        * Key is reconstructed and used to generate signature.
        * Then key is erased.
        * Signature is QR coded and displayed.
        * Signature is upto 512 bytes.
* Need PC app to:
    * Read file or cert to sign
    * Break into blocks and fountain code and then send as sequence of QR coded images.
    * Receive and display hash of file or certificate.
    * Receive signature and safe (insert in file or save as cert file). 
* Need Phone app to do key gen as well as signing:
    * Generate a random key
    * Receive K/N from user
    * Split key and output N shares as printable PDFs or as apple wallet item or ??

> Notes:
> 
>    * Photos of text on screen work with OCR - so key could be base 64 code 5 tuples or something similar that someone cuts and pastes.
>    * Perhaps this can work both ways - take picture and then just present picture.
>    * Phone Apps could be Laptop Apps
>    * Split keys are encoded so you need a pass phrase to unlock the split.
>    * Instead of pass phrase could be TOTP secret imported into authy or 1password or ...

# Option 3 - Switch Use Threshold Signatures

Threshold signing would allow for decentralized approach - but would probably require a service (or some sort of email setup) to make practical.

Can be applied to Options 0 and 2.

* U Waterloo - [https://eprint.iacr.org/2020/852.pdf](https://eprint.iacr.org/2020/852.pdf) - [https://eprint.iacr.org/2020/852](https://eprint.iacr.org/2020/852)  - and [https://github.com/isislovecruft/frost-dalek](https://github.com/isislovecruft/frost-dalek) - and [https://git.uwaterloo.ca/ckomlo/frost/](https://git.uwaterloo.ca/ckomlo/frost/)
* Looks to be getting standardized - [Draft RFC - draft-irtf-cfrg-frost](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-frost-15).  There also [Draft RFC - draft-hallambaker-threshold-sigs](https://datatracker.ietf.org/doc/html/draft-hallambaker-threshold-sigs-06) - but it looks less promissing.
* [Threshold Ed25519 in Golang](https://asecuritysite.com/signatures/ted25519)
* [Threshold Signature using Ed25519 in Java](https://github.com/weavechain/threshold-sig)
* [A Survey of ECDSA Threshold Signing](https://eprint.iacr.org/2020/1390.pdf)
* [Does Ed25519 support cryptographic threshold signatures?](https://crypto.stackexchange.com/questions/20581/does-ed25519-support-cryptographic-threshold-signatures)

> Notes:
> Threshold signatures need protection from replay attacks
>
>    - requests should be signed by “dealer“
>    - requests should have timestamps/sequence numbers
>    - clients should track timestamps/sequence numbers and report anomalies.

# FAQ

* How do we know it is the correct "file" to sign? (wasn't modified by build process, adversary, ...)
* How do we know it is the correct "hash" to sign (if use hash instead of file)?
* How to protect against changes to source code fed to build process?
* How to protect against changes to build process/tools?
* How to protect against build output being modified?
* How to protect against wrong data/file being fed to hashing process?
* How to project against hash output being modified?
* How to we ensure that the signature should be generated?
* Who is authorized to generate signatures?
* What is backup strategy?
* How to deal with rekeying (with threhsold signatures like frost)?
* Development keys vs release keys?
