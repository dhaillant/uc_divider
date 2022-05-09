# uc_divider
Arduino based Clock Divider

This code divides the input Clock by incrementing individual counters, one per output. Once a counter reaches a defined value, it is set back to zero and the output goes high, and the count continues.
This way, different rhythmic patterns can be created.
This is not polyrhythm: the individual counters are incremented synchronously.
