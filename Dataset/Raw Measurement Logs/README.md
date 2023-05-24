# Raw measurement logs from the double antenna UWB module

This folder contains the raw measurement logs from the double antenna UWB
module. It contains all data collected by the module for each UWB frame
received while performing two way ranging (TWR).

Each file name starts with the name of the dataset, i.e. "rotation" or
"stability", followed by an identifier of the specific setup. For the rotation
dataset this is the distance between the two modules in cm, while for the
stability dataset it is the AoA in degrees. For example the file
`rotation_150cm.log.gz` contains one measurement run of the rotation setup at
a distance of 1.5m, while the file `stability_180deg.log.gz` contains one
measurement run of the stability dataset at 180 deg AoA.

Each gzip compressed log file contains a text file with the measurement
results (same filename, except without the ".gz"). Each line in the log file
contains either human readable metadata or a base64 encoded binary measurement
blob. Each human readable line starts with a timestamp in seconds
(e.g. `0010.87627`), while the data lines start with (`XXXX.XXXXX`) and are
always preceded by by a line containing information about the data blob.

The binary data contains a C struct with measurements which can be read by the
scripts provided in the `../Scripts` forlder. For details on the data format
check the `binary_reader.py` script. For analysing the data it is suggested to
use the `parse_and_cache.py` script to generate a HDF5 file containing only
the data fields of interest, greatly improving memory and time requirements of
further processing.
