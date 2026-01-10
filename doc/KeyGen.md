# Overview

The `sbl.py` tool can be used to run the following processes:

1. Generate a Root Key
2. Generate a Primary Key
3. Generate a Prinary Key Certificate (Signed with Root Key)
4. Generate a Secondary Key
5. Generate a Secondary Key Certificate (Signed with Primary Key)
6. Sign a Binary FW Image (Signed with Secondary Key)
7. Re-split a Key
8. Configure SBL

These process are shown in the following diagram:

![SBL Processes](SBL-processes.drawio.svg)

# Release Key Generation

1. Generate Root key with 3,5 split
2. Generate Primary Key with 2,3 split
3. Generate Secondary Key with 2,3 split
4. Generate Primary Key Certificate
5. Generate Secondary Key Certificate
6. Customize SBL with Root key and Memory Layout paramaters
7. Sign Release Candidate Code with Secondary Key and Secondary Key Certificate

# Developement Key Generation

1. Generate Root key with 3,5 split
2. Generate Primary Key with 2,3 split
3. Generate Secondary Key with 1,1 split
4. Generate Primary Key Certificate
5. Generate Secondary Key Certificate
6. Customize SBL with Root key and Memory Layout paramaters
7. Configure local build processes using local environmant variable.
8. Configure GitHUb actions build processes using GitHub secrets
7. Build processes (local and GitHub actions) automatically sign code.

# Key Rotation

* Root keys are not rotated - or if they are, you will need to re-image each device.  You should however periodically (e.g. once per year) verify that you still have control of a quarum of key splits and could sign a new primary key if needed.  You can do this with the `sbl.py keyverify` command.
* Primary and secondary keys can be rotated, either periodicaly (e.g. once per year) or based on usage (e.g. after every N signing operations) or both.
* If you suspect primary or secondary key escape, you should immediately rotate the affected key and it's subordinates, re-sign the current active release, and force all devices to update to this newly signed FW image as quickly as possible.  By rotating the keys and re-signing you will force the timestmps to be later than any existing releases, preventing FW updates signed with the compromized keys.

# Resplitting a Key on Loss of a Key Split

In the following situations you can loose a key split (without lossing control of the key):

* The USB drive holding the key split is lost
* The corresponding passphrase is lost.
* The officer or employee responsible for the key split leaves the orgainization

In each of these cases, as soon as the condition is detected you should use the remaining key splits to **resplit** the key using the `sbl.py resplit` command.  The remaining old key splits should be permenently destroyed, ensuring no copies remain.  The new key splits should be distributed to the existing key split holders.

> Note: For primary and secondary keys you can generate new keys and certificates instead of re-splitting.  For root keys you must re-split.

# Choosing which SBL Version to Customize

SBL is build/distributed in two different configurations:

1. A release version which locks JTAG/SWD and ensures that FW udpates have timestamps that are later than the currently installed FW and the FW types can only progress from `EFI -> MFI -> AFI`.  This version should be the only version used for release testing and shipping to customers.
2. A development version which relaxes the extra steps taken by the release version.   This version should only be used by developers.

> Note: Developers can also use the release version of SBL, however it makes development more complex
> as you will need a method for restoring the manufacturing data if you want to load ealier FW type.
> e.g. go from an `AFI` back to an `MFI` or `EFI`.


A typical setup is to:

* Generate a set of develoment keys/certificates and use these keys/certifactes to:
    * customize a development version of SBL for distribution to developers
    * configure each developer's build machine using environment variables to enable automatic signing of code
    * configure GitHub action secrets to enable automatic signing of code
    * customize a release version of SBL for release candidate testing of automatically built/signed/tagged code.  The bulk of release testing

* Generate a set of release keys/certificates and use these keys/certifactes to:
    * manually (re-)sign release candidates that have successfully completed all release tests except root key compatibility testing.
    * customize a release version of SBL for building product (part of deliverables to manufacturing processes) and performing root key compatibility testing.

# Root key compatibility Testing

Before finalizing a release for distribution you need to ensure that:

* All current in-field FW releases will accept the new FW release.
* A device that has been updated with the new FW release will be able to upgrade the next FW release.  As you won't have the next FW release, you can use the current new FW release that has been signed a second time (thus having a later timestamp) as a proxy.
