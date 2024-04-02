# csp_proc

Lightweight, programmable procedures with a libcsp- and libparam-native runtime. This provides remote control of libparam-based coordination between nodes in a CSP network, essentially exposing the network as single programmable unit.

The library has a relatively small footprint suitable for microcontrollers, requiring no external libraries other than libcsp and libparam themselves for the core of the library. However, currently, the only runtime implementation relies on FreeRTOS.
