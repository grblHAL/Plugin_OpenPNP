## OpenPNP plugin

Under development. Adds 5 M-codes to allow grblHAL to be used for [OpenPNP](https://openpnp.org/) machines.

* `M42 P- S-` Set digital output. The P-word must be a valid ioport number, see below. The S-word is used to turn the output on or off. Use 0 for off, > 0 for on.

* `M114 <P->` reports current position. The P-word is ignored if provided.

* `M115` Reports firmware information.

* `M204 <P-> <S-> <T->` Use S- or T-word to set acceleration for all axes except the linear axis \(Z-axis\), use P-word to set it for all axes. If the word parameter is 0 acceleration is reset to default values.

* `M400` Waits until the motion buffers are cleared and motion stopped. Same function as `G4P0`.

Dependencies:

Driver must have one or more [ioports ports](https://github.com/grblHAL/Templates/blob/master/ioports.c) available for digital output to enable `M42` use.

---
2021-07-26
