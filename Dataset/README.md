# Measurement Data

This folder contains two sets of measurement data. First, a dataset to
evaluate the performance of the DW3220 at different distances and roations
("rotation" dataset) and second, a dataset to evaluate the stability of the
measurements over time ("stability" dataset).

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

## Data format

The data provided in the two subfolders is based on the same
measurements. First the raw measurements and second a more directly usable
preprocessed HDF datastore containing a subset of the data. For a detailed
description check the README in the respective folders.

| Folder                        | Data available | Format                | Difficulty to use |
|-------------------------------|----------------|-----------------------|-------------------|
| Raw Measurement Logs          | All            | Compressed text files | medium            |
| Preprocessed Measurement Data | Subset         | HDF5 Store            | low               |
