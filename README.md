# Longmynd [![Build Status](https://travis-ci.org/myorangedragon/longmynd.svg?branch=master)](https://travis-ci.org/myorangedragon/longmynd)

An Open Source Linux ATV Receiver.

Copyright 2019 Heather Lomond

## Dependencies

    sudo apt-get install libusb-1.0-0-dev

To run longmynd without requiring root, unplug the minitiouner and then install the udev rules file with:

    sudo cp minitiouner.rules /etc/udev/rules.d/

## Compile

    make

## Run

Please refer to the longmynd manual page via:

```
man -l longmynd.1
```

## Standalone

If running longmynd standalone (i.e. not integrated with the Portsdown software), you must create the status FIFO and (if you plan to use it) the TS FIFO:

```
mkfifo longmynd_main_status
mkfifo longmynd_main_ts
```

The test harness `fake_read` or a similar process must be running to consume the output of the status FIFO:

```
./fake_read &
```

A video player (e.g. VLC) must be running to consume the output of the TS FIFO. 

## Output

    The status fifo is filled with status information as and when it becomes available.
    The format of the status information is:
    
         $n,m<cr>
     
    Where:
         n = identifier integer of Status message
         m = integer value associated with this status message
      
    And the values of n and m are defined as:
    
    ID  Meaning             Value and Units
    ==============================================================================================
    1   State               0: initialising
                            1: searching
                            2: found headers
                            3: locked on a DVB-S signal
                            4: locked on a DVB-S2 signal 
    2   LNA Gain            On devices that have LNA Amplifiers this represents the two gain 
                            sent as N, where n = (lna_gain<<5) | lna_vgo
                            Though not actually linear, n can be usefully treated as a single
                            byte representing the gain of the amplifier
    3   Puncture Rate       During a search this is the pucture rate that is being trialled
                            When locked this is the pucture rate detected in the stream
                            Sent as a single value, n, where the pucture rate is n/(n+1)
    4   I Symbol Power      Measure of the current power being seen in the I symbols
    5   Q Symbol Power      Measure of the current power being seen in the Q symbols
    6   Carrier Frequency   During a search this is the carrier frequency being trialled
                            When locked this is the Carrier Frequency detected in the stream
                            Sent in KHz
    7   I Constellation     Single signed byte representing the voltage of a sampled I point
    8   Q Constellation     Single signed byte representing the voltage of a sampled Q point
    9   Symbol Rate         During a search this is the symbol rate being trialled
                            When locked this is the symbol rate detected in the stream
    10  Viterbi Error Rate  Viterbi correction rate as a percentage * 100
    11  BER                 Bit Error Rate as a Percentage * 100

## License

    Longmynd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    Longmynd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with longmynd.  If not, see <https://www.gnu.org/licenses/>.
