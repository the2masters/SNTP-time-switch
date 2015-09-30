# SNTP-time-switch
Glorified time switch based on an Atmel Atmega USB Controller which gets its time from an SNTP-Server over USB-Ethernet

Based on a slightly modified lufa Library and a heavily modified RNDIS Lowlevel Demo.
Network stack is rewritten around a single Ethernet buffer. USB Stack is switched to interrupt based processing of data.

This is currently beeing tested on an Atmega32u2 on a board with USB and two relays connected to PC4 and PC5.
