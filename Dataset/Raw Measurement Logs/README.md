# Raw measurement logs from the double antenna UWB module

This folder contains to sets of measurement logs. First, a dataset to evaluate
the performance of the DW3220 at different distances and roations ("rotation"
dataset) and second, a dataset to evaluate the stability of the measurements
over time ("stability" dataset).

## Data collection setup

For all measurements one single antenna module and one double antenna module
was set up in an 8.6m x 7.7m room containing tables and office equipment
placed along the walls and a 6.5m x 5.5m empty space where the measurements
were performed. The single antenna module was fixed on a tripod at a height of
1.1m. The double antenna module was mounted on a rotatable motorized platform
at the same height, providing the ground truth AoA.

### Rotation dataset

Data collected at 11 fixed distances ranging from 50cm to 5.5m in 50cm
steps. At each distance, the double antenna module was automatically rotated
in 1 degree steps for a full rotation. At least five TWR measurements were
performed in each position/rotation. Each measurement run contains data for a
single distance and all rotations.

### Stability dataset

Data collected at a fixed distance of 5.4m and with 0, 90, 180, and 270 degree
AoA. Approximately 3000 frames are collected over a period of 11 minutes with
no movement in the room. Each measurement run contains data at a fixed
distance and rotation.

## File description and data format

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
