# Scripts for data collection, log file parsing and data preprocessing.

The double antenna UWB module sends measurement data over the serial
connection for processing. The scripts in this directory help to save and
preprocess that data for analysis.

## Scripts
- `serial_reader.py` - Connect and receive measurements from the UWB
  module. The resulting log file will be an optionally compressed text file
  containing some plaintext metadata, as well as base64 encoded binary
  measurement blobs. Different options to limit the number of measurements are
  available. Check `serial_reader.py --help` for details on usage.
- `parse_and_cache.py` - Read UWB measurement logs and generate a HDF5 cache
  file for efficient access to all data fields required for later
  analysis. Check top comment in the file `parse_and_cache.py --help` for more
  details on usage and data format.

## Libraries
- `serial_parser.py` (use `parse_log_file()` funtion) - Read a UWB measurement
  log file and generate an object for each TWR frame / measurement containing
  all data reported by the double antenna module (see `Frame` class definition
  for available fields)
- `binary_reader.py` (no need to use this directly) - Parse base64 encoded
  binary blobs into namedtuple instances containing all data.

## Tips and Tricks
The processing speed can be significantly increased by compiling the binary
parser and caching script with Cython by running: `cythonize -3 -a -i
binary_parser.py serial_parser.py`

## External Requirements
- [pyserial](https://github.com/pyserial/pyserial): USB/serial connection to
  the UWB module
- [pandas](https://pandas.pydata.org/): Data processing
- [tqdm](https://github.com/tqdm/tqdm): Helper library to show progress
  information
- Optionally [Cython](https://cython.org/): Speed up processing by compiling
  the libraries
