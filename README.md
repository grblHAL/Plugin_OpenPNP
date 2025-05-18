## OpenPNP plugin

Under development. Adds some M-codes to allow grblHAL to be used for [OpenPNP](https://openpnp.org/) machines.

* `M42 P- S-` Set digital output. The P-word must be a valid ioport number, see below. The S-word is used to turn the output on or off. Use `0` for off, > `0` for on.  
> [!NOTE]
> `M42` is equivalent to the [standard](https://linuxcnc.org/docs/2.5/html/gcode/m-code.html#sec:M62-M65) commands, `M64` and `M65`, which are supported by the core.

* `M114 <P->` reports current position. The P-word is ignored if provided.

* `M115` Reports firmware information.

* `M204 <P-> <S-> <T->` Use S- or T-word to set acceleration for all axes except the linear axis \(Z-axis\), use P-word to set it for all axes. If the word parameter is 0 acceleration is reset to default values.

* `M205` - set [jerk](https://github.com/grblHAL/core/wiki/Jerk-acceleration), supports the same parameter words as `M204`.

* `M400` Waits until the motion buffers are cleared and motion stopped. Same function as `G4P0`.

M-codes under consideration, may change:

* `M143 P-` Read raw analog input, P-word must be a valid analog input port number.

* `M144 P-` Read scaled analog input, P-word must be a valid analog input port number.

* `M145 P- S- Q-` Set scaling data for analog input port, P-word must be a valid port number, S-word is the scaling factor and Q-word the scaling offset. Scaling data is volatile.

Returned data from `M143` and `M144` is in the format `A<n>:<value>` where `<n>` is the port number and `<value>` is the value read.
`M144` returns `raw value * P + Q`, `P` and `Q` values as set by a previous `M145` command, default is `1` and `0` respectively.

> [!NOTE]
> Max number if analog inputs is currently limited to eight by the plugin.

#### Dependencies:

Driver must have one or more [ioports ports](https://github.com/grblHAL/Templates/blob/master/ioports.c) available for digital output to enable `M42`.

---
2025-05-18
