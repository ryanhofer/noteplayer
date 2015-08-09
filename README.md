# noteplayer

Embedded software for the [AVR XMEGA-A1 Xplained](http://www.atmel.com/tools/XMEGA-A1XPLAINED.aspx).

## Features:
- plays a sequence of notes (loaded with "Hot Cross Buns")
- 40KHz audio sampling rate
- stores high-resolution 1Hz sine wave from 0 to π/2
- transforms this table in order to create the full waveform from 0 to 2π
- skips through sine table in order to generate higher frequencies
