# Preprocessed Measurement Data

To reduce the time and memory requirements and to efficiently access the
relevant measurement data it can be preprocessed and stored into a single HDF5
file. This is automated by the `../Scripts/parse_and_cache.py` script,
requiring a csv configuration file.

Configuration files are provided here for rotation and stability
datasets. Each takes all measurement runs from one dataset and combines them
into a single HDF store. The configuration file format is CSV with one header
line followed by one line for each measurement run. Each line contains the
fileds:

| Name          | Description                                                        |
|---------------|--------------------------------------------------------------------|
| `title`       | Short identifier of the measurement run                            |
| `filename`    | The filename of the compressed or uncompressed raw measurement log |
| `description` | Short description of the measurement run                           |

The HDF5 store will then contain two tables per measurement run with the names
`/cache_[title]/info` and `/cache_[title]/df` (replacing `[title]` with the
title as given in the configuration file). The first containing the
configuration information, as well as a timestamp and version number, while
the latter contains the measurement data. In version 4 of the cache file (as
provided here) the following fields are included:

| Name             | Description                                                                               |
|------------------|-------------------------------------------------------------------------------------------|
| `timestamp`      | Timestamp (s) of data collection relative to the start                                    |
| `number`         | TWR frame counter                                                                         |
| `rotation`       | Ground truth rotation value (deg), values above 359 should be considered module 360       |
| `pdoa`           | Phase Difference of Arrival (rad)                                                         |
| `tdoa`           | Time Difference of Arrival (15.65ps)                                                      |
| `dist_mm`        | TWR distance estimate (mm)                                                                |
| `rx_power_level` | Received signal power level (computed according to DW3000 family user manual section 4.7) |
| `fp_power_level` | First path power level (computed according to DW3000 family user manual section 4.7)      |
| `cir_sts1`       | Channel Impulse Response at first antenna (105 samples)                                   |
| `cir_sts2`       | Channel Impulse Response at second antenna (105 samples)                                  |

To recreate these HDF stores the following commands can be used:

```
../../Scripts/parse_and_cache.py -i cache_config_rotation.csv processed_data_cache_rotation.h5
../../Scripts/parse_and_cache.py -i cache_config_stability.csv processed_data_cache_stability.h5
```

For more details on how to change the data fields have a look at the comments
in the script.
