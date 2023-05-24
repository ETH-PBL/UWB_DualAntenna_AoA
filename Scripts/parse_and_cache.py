#!/usr/bin/env python3


"""Generate cache file efficiently storing data fields required for later analysis.

-> For specific field names and descriptions check the comment in `CacheV4` below.

A configuration file is required to run this script. Specification:
- Each line corresponds to one data file to be processed
- File name: cache_config.csv
- Line format: title;filename;description
- Header line required (first line will be ignored)

Some preprocessing is performed to simplify the data (c.f. CacheV? classes below):
- PDoA decoded as float
- CIR restricted to a slice around the first path index
- Power levels computed according to the DW3000 user manual

Output file specification:
- HDF5
- Data tables for each processed data file
  + "cache_{title}/info" - Information about the data file
  + "cache_{title}/df" - Data table
"""

import csv
import math
import argparse
from pprint import pprint
from collections import OrderedDict

import pandas as pd

from serial_parser import parse_log_file, Frame


DEFAULT_CACHE_VERSION = '4'


class CacheV4:
    '''Cache version 4.

    Fields:
    - `timestamp`: Recording timestamp
    - `number`: Frame Number
    - `rotation`: True Rotation
    - `pdoa`: PDoA measurement
    - `tdoa`: TDoA measurement
    - `dist_mm`: TWR distance estimate
    - `rx_power_level`: Receive signal power estimate
    - `fp_power_level`: First path power estimate
    - `cir_sts1` and `cir_sts2`: Complex CIR samples (restricted to 5 samples
      before the first path index and 99 samples after the first path index as
      computed by the DW3220, i.e. 105 samples total)
    '''

    def __init__(self):
        # slice of the STS1 and STS2 CIR around the FP index
        self.cir_sts_slice = (-5, 100)
        self.cir_sts_slice_len = self.cir_sts_slice[1] - self.cir_sts_slice[0]

        # MultiIndex to group CIR slices
        self.columns = pd.MultiIndex.from_arrays((
            ['timestamp', 'number', 'rotation', 'pdoa', 'tdoa', 'dist_mm',
             'rx_power_level', 'fp_power_level',
             *(['cir_sts1']*self.cir_sts_slice_len),
             *(['cir_sts2']*self.cir_sts_slice_len)],
            ['', '', '', '', '', '', '', '',
             *range(*self.cir_sts_slice), *range(*self.cir_sts_slice)]
        ))

    def check_frame(self, frame):
        if frame.twr_data and frame.cir_analysis_sts1:
            return True
        else:
            return False

    def get_frame_data(self, frame: Frame):
        has_cir = True
        try:
            fp_sts1 = int(frame.cir_analysis_sts1.fp_index)
            fp_sts2 = int(frame.cir_analysis_sts2.fp_index)
        except AttributeError:
            has_cir = False

        if not (hasattr(frame.cir, 'cir_sts1')
                and hasattr(frame.cir, 'cir_sts2')):
            has_cir = False

        if has_cir:
            cir_sts1_slice = slice(fp_sts1+self.cir_sts_slice[0],
                                   fp_sts1+self.cir_sts_slice[1])
            cir_sts2_slice = slice(fp_sts2+self.cir_sts_slice[0],
                                   fp_sts2+self.cir_sts_slice[1])
        else:
            empty_cir = [None]*(self.cir_sts_slice[0]+self.cir_sts_slice[1])

        # Compute power levels, c.f. DW3000 user manual section 4.7
        try:
            C = frame.cir_analysis_ip.power
            N = frame.cir_analysis_ip.accum_count
            D = frame.toa_data.dgc_decision
            A = 121.7
            F1 = frame.cir_analysis_ip.F1
            F2 = frame.cir_analysis_ip.F2
            F3 = frame.cir_analysis_ip.F3
            rx_power_level = 10 * math.log10((C * 2**21) / N**2) + (6 * D) - A
            fp_power_level = 10 * math.log10((F1**2 + F2**2 + F3**2) / N**2) + (6 * D) - A
        except AttributeError:
            rx_power_level = None
            fp_power_level = None

        data = (
            frame.serial_timestamp,
            frame.serial_count,
            frame.twr_data.rotation,
            frame.toa_data.pdoa/2**11,
            frame.toa_data.tdoa,
            frame.twr_data.dist_mm,
            rx_power_level,
            fp_power_level,
            *(frame.cir.cir_sts1[cir_sts1_slice] if has_cir else empty_cir),
            *(frame.cir.cir_sts2[cir_sts2_slice] if has_cir else empty_cir),
        )
        return data


class CacheV4a:
    '''Cache version 4a.

    Differences to version 4:
    - Full CIR
    '''

    def __init__(self):
        self.cir_sts_len = 512
        # MultiIndex to group CIR slices
        self.columns = pd.MultiIndex.from_arrays((
            ['timestamp', 'number', 'rotation', 'pdoa', 'tdoa', 'dist_mm',
             'rx_power_level', 'fp_power_level', 'sts1_fp_index',
             'sts2_fp_index',
             *(['cir_sts1']*self.cir_sts_len),
             *(['cir_sts2']*self.cir_sts_len)],
            ['', '', '', '', '', '', '', '', '', '',
             *range(self.cir_sts_len), *range(self.cir_sts_len)]
        ))

    def check_frame(self, frame):
        if frame.twr_data and frame.cir_analysis_sts1:
            return True
        else:
            return False

    def get_frame_data(self, frame: Frame):
        has_cir = True
        if not (hasattr(frame.cir, 'cir_sts1')
                and hasattr(frame.cir, 'cir_sts2')):
            has_cir = False

        if not has_cir:
            empty_cir = [None]*self.cir_sts_len

        # Compute power levels, c.f. DW3000 user manual section 4.7
        C = frame.cir_analysis_ip.power
        N = frame.cir_analysis_ip.accum_count
        D = frame.toa_data.dgc_decision
        A = 121.7
        F1 = frame.cir_analysis_ip.F1
        F2 = frame.cir_analysis_ip.F2
        F3 = frame.cir_analysis_ip.F3
        rx_power_level = 10 * math.log10((C * 2**21) / N**2) + (6 * D) - A
        fp_power_level = 10 * math.log10((F1**2 + F2**2 + F3**2) / N**2) + (6 * D) - A

        data = (
            frame.serial_timestamp,
            frame.serial_count,
            frame.twr_data.rotation,
            frame.toa_data.pdoa/2**11,
            frame.toa_data.tdoa,
            frame.twr_data.dist_mm,
            rx_power_level,
            fp_power_level,
            frame.cir_analysis_sts1.fp_index,
            frame.cir_analysis_sts2.fp_index,
            *(frame.cir.cir_sts1 if has_cir else empty_cir),
            *(frame.cir.cir_sts2 if has_cir else empty_cir),
        )
        return data


class CacheV5:
    '''Cache version 5.

    Differences to version 4:
    - Store all frames, not only TWR result frames
    - Only store frames with CIR
    '''

    def __init__(self):
        # slice of the STS1 and STS2 CIR around the FP index
        self.cir_sts_slice = (-5, 100)
        self.cir_sts_slice_len = self.cir_sts_slice[1] - self.cir_sts_slice[0]

        # MultiIndex to group CIR slices
        self.columns = pd.MultiIndex.from_arrays((
            ['timestamp', 'number', 'rotation', 'pdoa', 'tdoa', 'dist_mm',
             'rx_power_level', 'fp_power_level',
             *(['cir_sts1']*self.cir_sts_slice_len),
             *(['cir_sts2']*self.cir_sts_slice_len)],
            ['', '', '', '', '', '', '', '',
             *range(*self.cir_sts_slice), *range(*self.cir_sts_slice)]
        ))

    def check_frame(self, frame):
        if (
                hasattr(frame.cir, 'cir_sts1')
                and hasattr(frame.cir, 'cir_sts2')
                and hasattr(frame.cir_analysis_sts1, 'fp_index')
                and hasattr(frame.cir_analysis_sts2, 'fp_index')
        ):
            return True
        else:
            return False

    def get_frame_data(self, frame: Frame):
        fp_sts1 = int(frame.cir_analysis_sts1.fp_index)
        fp_sts2 = int(frame.cir_analysis_sts2.fp_index)
        cir_sts1_slice = slice(fp_sts1+self.cir_sts_slice[0],
                               fp_sts1+self.cir_sts_slice[1])
        cir_sts2_slice = slice(fp_sts2+self.cir_sts_slice[0],
                               fp_sts2+self.cir_sts_slice[1])

        # Compute power levels, c.f. DW3000 user section 4.7
        C = frame.cir_analysis_ip.power
        N = frame.cir_analysis_ip.accum_count
        D = frame.toa_data.dgc_decision
        A = 121.7
        F1 = frame.cir_analysis_ip.F1
        F2 = frame.cir_analysis_ip.F2
        F3 = frame.cir_analysis_ip.F3
        rx_power_level = 10 * math.log10((C * 2**21) / N**2) + (6 * D) - A
        fp_power_level = 10 * math.log10((F1**2 + F2**2 + F3**2) / N**2) + (6 * D) - A

        try:
            rotation = frame.twr_data.rotation
            dist_mm = frame.twr_data.dist_mm
        except AttributeError:
            rotation = None
            dist_mm = None

        data = (
            frame.serial_timestamp,
            frame.serial_count,
            rotation,
            frame.toa_data.pdoa/2**11,
            frame.toa_data.tdoa,
            dist_mm,
            rx_power_level,
            fp_power_level,
            *frame.cir.cir_sts1[cir_sts1_slice],
            *frame.cir.cir_sts2[cir_sts2_slice],
        )
        return data


cache_versions = {
    '4': CacheV4,
    '4a': CacheV4a,
    '5': CacheV5,
}


def parse_and_cache(log_files, cache_file, version):
    # open an check output file
    store = pd.HDFStore(cache_file, mode='a')
    if store.keys():
        print('Cache File exists => content may be overwritten')
        print('Current keys:', store.keys())
    else:
        print('Cache file did not exists or is empty...')

    cache_cls = cache_versions[version]()

    # read input files and update cache
    file_count = len(log_files)
    for i, (title, (filename, info)) in enumerate(log_files.items()):
        print('-------------------------------------')
        print(f'*** Processing file {i+1}/{file_count} "{title}" '
              f'(Filename: {filename}, Description: {info})')

        frames, stats = parse_log_file(filename, progress=True)

        if not frames:
            print('No frames, skip building dataframe!')
            continue

        print('*** Input statistics')
        stats.print_stats()

        df = build_dataframe(frames, cache_cls)
        store_dataframe(i, filename, title, info, df, store, version)
        print(f'Finished {title}')

    print('Final cache keys:', store.keys())
    store.flush(fsync=True)
    store.close()


def build_dataframe(frames, cache_cls):
    """Select relevant frame data and create a DataFrame."""

    data = []
    skipped = 0
    for frame in frames:
        if not cache_cls.check_frame(frame):
            # skip e.g. non TWR result frames
            skipped += 1
            continue
        data.append(cache_cls.get_frame_data(frame))

    df = pd.DataFrame(data)

    # Create multiindex to group CIR slices
    df.columns = cache_cls.columns

    print(f'*** Cached {len(data)} frames (skipped {skipped})')
    print('*** DataFrame statistics')
    df.info(memory_usage='deep')

    return df


def store_dataframe(i, filename, title, description, df, store, version):
    cache_time = pd.Timestamp.now()
    try:
        version = int(version)
    except ValueError:
        pass
    info_df = pd.DataFrame(
        [(i, title, filename, description, cache_time, version)],
        columns=['i', 'title', 'file_name', 'description', 'timestamp', 'version']
    )
    store.put('cache_'+title+'/info', info_df, format='fixed')
    store.put('cache_'+title+'/df', df, format='table')


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--input-file', '-i', default='cache_config.csv',
                        help='Cache configuration file to use.')
    parser.add_argument('output_file', nargs='?',
                        default='processed_data_cache.h5',
                        help='Cache output file.')
    parser.add_argument('--version', choices=cache_versions.keys(),
                        default=DEFAULT_CACHE_VERSION,
                        help='Choose cache file/data version to compute.')

    args = parser.parse_args()

    log_files = OrderedDict()

    with open(args.input_file, newline='') as f:
        reader = csv.reader(f, delimiter=';')
        files = list(reader)[1:]  # skip header

    for entry in files:
        if not entry:
            continue
        log_files[entry[0]] = (entry[1], entry[2])

    print('Input configuration')
    pprint(log_files)

    parse_and_cache(log_files, args.output_file, args.version)


if __name__ == '__main__':
    main()
