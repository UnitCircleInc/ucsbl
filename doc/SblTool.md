# SBL Tool Design

SBL Tool is used to create signing keys and support management of a 2-level certifictate chain.  It is also used to customize a "generic" SBL to set the root signing key of the key chain and the memory layout of the internal FLASH.

## Keys

SBL uses Ed25519 (RFC8032) for signing keys.  In order to protect the private portion of any generated signing keys, two methods are employed:

1. Use of SSS - [Shamir's Secret Sharing](https://en.wikipedia.org/wiki/Shamir%27s_secret_sharing)  - to split the signing key private portion in to N splits.   With a quarum of K (K <= N) splits, the signing key private portion can be reconstructed.  SSS is implement using the finite field GF(2)[X]/[X^256+X^10+X^5+X^2+1].
2. Each split is key wrapped using [XChaCha20-Poly1305-IEFT](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-xchacha) construction  with a key derived from a passphrase using [scrypt](https://en.wikipedia.org/wiki/Scrypt) with memory and operation limits set to "interactive".  The passphrase is generated to ensure at least 82 bits of entropy using 8 [Diceware](https://theworld.com/~reinhold/diceware.html).  The diceware words are chosen from [EFF's Long Word List](https://www.eff.org/files/2016/07/18/eff_large_wordlist.txt) and separated with hyphens.

## Customization

SBL has a configuration struct for holding both the public portion of the root signing key and the memory layout.  The configration is stored in FLASH using the C "static const" construction and is initialized to a random (but known) root signing key.  The SBL tool can find the locaiton of the struct using the random (but known) root signing key.  It can then replace the root signing key with the desried value and configure the memory layout.

> NOTE: This approach avoids having to use special memory segments and linker instructions to place the configuration struct at a specific memory location.
