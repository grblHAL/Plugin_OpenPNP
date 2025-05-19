## OpenPNP plugin

Under development. Adds some M-codes to allow grblHAL to be used for [OpenPNP](https://openpnp.org/) machines.

* `M42 P- S-` Set digital output. The P-word must be a valid ioport number, see below. The S-word is used to turn the output on or off. Use `0` for off, > `0` for on.  
> [!NOTE]
> `M42` is equivalent to the [standard](https://linuxcnc.org/docs/2.5/html/gcode/m-code.html#sec:M62-M65) commands, `M64` and `M65`, which are supported by the core.

* `M114 <P->` reports current position. The P-word is ignored if provided.

* `M115` Reports firmware information.

* `M204 <P-> <S-> <T->` Use S- or T-word to set acceleration for all axes except the linear axis \(Z-axis\), use P-word to set it for all axes. If the word parameter is 0 acceleration is reset to default values.

* `M205 axes` - set [jerk](https://github.com/grblHAL/core/wiki/Jerk-acceleration) per axis. If the parameter value is `0` the configured value is restored.

* `M400` Waits until the motion buffers are cleared and motion stopped. Same function as `G4P0`.

M-codes under consideration, may change:

* `M143 P-` Read digital input, P-word must be a valid digital input port number.

* `M143 E-` Read raw analog input, E-word must be a valid analog input port number.
Returned data from `M143` is in the format `A<n>:<value>` for analog inputs and `D<n>:<value>` for digital. `<n>` is the port number and `<value>` is the value read.

* `M144 E-` Read scaled analog input, E-word must be a valid analog input port number.  
Returned data from `M144` is in the format `A<n>:<value>` where `<n>` is the port number and `<value>` is the value read.
The returned value is `raw value * P + Q`, `P` and `Q` values as set by a previous `M145` command, defaults for these are `1` and `0` respectively.

* `M145 E- S- Q-` Set scaling data for analog input port, E-word must be a valid port number, S-word is the scaling factor and Q-word the scaling offset. Scaling data is volatile.  

> [!NOTE]
> Max number of analog inputs is currently limited to eight by the plugin.

#### Dependencies:

The driver must have one or more auxiliary input/output ports available for the relevant M-codes documented above.

> [!NOTE]
> Information about available axes and auxiliary ports is available in the `$I+` [system information](https://github.com/grblHAL/core/wiki/Report-extensions#other-request-responses-or-push-messages)
output, see the `AXS` and `AUX IO` elements.


---
2025-05-19
