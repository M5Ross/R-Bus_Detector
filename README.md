# R-Bus_Detector

Roco R-Bus compatible feedback module

This ia a feedback module designed for Roco R-Bus protocol.
It is capable to acquire feedback data from 8 inputs.

The heart of the system is a PIC16F688 witch has excacly the minimun required I/Os and communication module (see [main688.c]).

Equivalent circuit can be done also with the most commonly used PIC16F88/627A/628A/648A, with minimal changes in the I/O mapping in the firmware (see [main628.c]).

Compilation was done in MPLAB IDE v8.56 + HI-TECH C COMPILER v9.83

[main688.c]: .../../src/main688.c
[main628.c]: .../../src/main628.c
