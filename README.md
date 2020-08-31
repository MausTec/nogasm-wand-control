# nogasm-wand-control
Firmware for the Nogasm Phase Angle Control adapter.

This code uses two interrupt routines, one to measure an incoming PWM signal, and the other to trigger a triac gate at a specific phase angle aligned
with the natural commutation of the mains.
