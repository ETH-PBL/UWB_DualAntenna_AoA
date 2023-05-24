# Raw measurement logs from the double antenna UWB module

## Data collection setup

Data collected at 11 fixed distances ranging from 50cm to 5.5m in 50cm
steps. At each distance, the double antenna module was rotated in 1 degree
steps for a full rotation. At least five TWR measurements were performed in
each position/rotation.

One single antenna module and one double antenna module were placed in an 8.6m
x 7.7m room containing tables and office equipment placed along the walls and
a 6.5m x 5.5m empty space where the measurements were performed. The single
antenna module was fixed on a tripod at a height of 1.1m. The double antenna
module was mounted on a rotatable motorized platform at the same height,
providing the ground truth AoA.

## Data format

Each gzip compressed log file (e.g. `2022-08-24-cal-50cm.log.gz`) contains a
text file with the measurement results (e.g. `2022-08-24-cal-50cm.log`). Each
line contains either human readable metadata or a base64 encoded binary
measurement blob. Each human readable line starts with a timestamp in seconds
(e.g. `0010.87627`), while the data lines start with (`XXXX.XXXXX`) and are
always preceded by by a line containing information about the data blob.

The binary data contains a C struct with measurements which can be read by the
scripts provided in the `../Scripts` forlder. For details on the data format
check the `binary_reader.py` script. For analysing the data it is suggested to
use the `parse_and_cache.py` script to generate a HDF5 file containing only
the data fields of interest, greatly improving memory and time requirements of
further processing.

## File description

All file names follow the format `[Collection Date]-cal-[distance].log.gz`
where `[Collection Date]` is in YYYY-MM-DD format and distance in cm.
