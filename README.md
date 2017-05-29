WTF are they doing.

ToDo:
It has to be checked, if the cpld
has to be told, to output the MCLK for the AK4114.
After this spdif stream open and close must be written.
Open should check if the PLL is locked. On true
the MCLK source must be set to PLL.
On close the MCLK source must be set back to XTAL.

side effect:
If there is no input from outer world, the
card receives the output it produces as input.