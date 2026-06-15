# USB CDC Text Protocol

The firmware exposes a USB CDC ACM virtual serial port on the board's native USB
Device Mini USB connector.

Device to host:

```text
T,<ms>,<raw_c_x100>,<filtered_c_x100>,<alarm>,<led>,<beep>
```

- `ms`: device uptime from `HAL_GetTick()`.
- `raw_c_x100`: raw internal temperature in centi-degrees C.
- `filtered_c_x100`: EWMA-filtered temperature in centi-degrees C.
- `alarm`: `0` or `1`.
- `led`: `AUTO`, `OFF`, `RED`, `GREEN`, `BLUE`, or `WHITE`.
- `beep`: `AUTO`, `ON`, or `OFF`.

Host to device:

```text
CMD,TH,<centi_c>
CMD,LED,AUTO|OFF|RED|GREEN|BLUE|WHITE
CMD,BEEP,AUTO|ON|OFF
CMD,ACK
```

`CMD,ACK` silences the buzzer while the alarm remains latched. The alarm clears
automatically only after filtered temperature falls to the recovery threshold.
