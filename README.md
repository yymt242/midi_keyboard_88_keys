# Arduino code for my MIDI keyboard

> My MIDI keyboard is consist of 2 parts from 2 used CASIO Organ Keyboards (CASIO CTK 660L & CASIO CTK 601)
>

- Microcontroller: Arduino Mega 2560

## Software required
- Hairless MIDI< - >Serial Bridge
- loopMIDI

# Feature

## Channel selection

## Sustain

- Support up to **88 different keys** sustaining in **8 seconds** (that is $90.9$ ms/note, the world's record is $55$ ms/note)

## Velocity sensitive

Fomula of velocity

$y = \left(129\ -\ 107\cdot\log^{2}_{500}\left(x-2\right)\right)\cdot\log_{700}\left(400-x\right)\cdot1.08$

- $x \in [3,399]$ time between 2 buttons pressed in the same key, in miliseconds 
- $y \in [0,127]$ velocity

<img src=".\fig\velocity.png" style="zoom:50%;" />