#!/usr/bin/env python3


import os
import gzip
import functools

import tqdm

import binary_parser


class Frame:
    version = 7

    __slots__ = ('serial_timestamp', 'serial_count', 'frame_type',
                 'sequence_number', 'toa_data', 'cir_analysis_ip',
                 'cir_analysis_sts1', 'cir_analysis_sts2', 'cir', 'twr_data')

    binary_to_attr = {
        'toa': 'toa_data',
        'cir analysis ip': 'cir_analysis_ip',
        'cir analysis sts1': 'cir_analysis_sts1',
        'cir analysis sts2': 'cir_analysis_sts2',
        'cir': 'cir',
        'twr': 'twr_data',
    }

    def __init__(self):
        # Set initial values and types separately to allow for type checking
        self.serial_timestamp: float = None
        self.serial_count: int = None
        self.frame_type: str = None
        self.sequence_number: int = None
        self.toa_data: binary_parser.toa_data = None
        self.cir_analysis_ip: binary_parser.cir_analysis_data = None
        self.cir_analysis_sts1: binary_parser.cir_analysis_data = None
        self.cir_analysis_sts2: binary_parser.cir_analysis_data = None
        self.cir: binary_parser.cir_data = None
        self.twr_data: binary_parser.twr_data = None

    @property
    @functools.cache
    def cir_ip_abs(self):
        if self.cir:
            return tuple(abs(cir) for cir in self.cir.cir_ip)
        else:
            return None

    @property
    @functools.cache
    def cir_sts1_abs(self):
        if self.cir:
            return tuple(abs(cir) for cir in self.cir.cir_sts1)
        else:
            return None

    @property
    @functools.cache
    def cir_sts2_abs(self):
        if self.cir:
            return tuple(abs(cir) for cir in self.cir.cir_sts2)
        else:
            return None

    def print_frame(self):
        for attr in self.__slots__:
            if attr == 'cir' and self.cir:
                print('cir ip:', *(self.cir.cir_ip[i] for i in range(5)), '...')
                print('cir sts1:', *(self.cir.cir_sts1[i] for i in range(5)), '...')
                print('cir sts2:', *(self.cir.cir_sts2[i] for i in range(5)), '...')
                continue

            if attr.startswith('_'):
                attr = attr[1:]
            print(attr + ':', getattr(self, attr))


class Statistics:
    __slots__ = ('start_time', 'end_time', 'frame_count', 'twr_count',
                 'error_count_timeout', 'error_count_ranging',
                 'error_count_sts_qual', 'file_size')

    def __init__(self):
        for attr in self.__slots__:
            setattr(self, attr, 0)

    def print_stats(self):
        for attr in self.__slots__:
            print(attr + ':', getattr(self, attr))


def parse_cir_line(line):
    # remove serial receiver timestamp and line label
    line = line[line.find(':')+1:]
    # split line into separate samples and remove end marker
    split_line = line.split('|')[:-1]
    # split samples into separate numbers (real and imaginary part)
    cir_split = [x.split() for x in split_line]
    # convert samples into actual python complex numbers
    cir = [int(x[2]) + 1j*int(x[4]) for x in cir_split]
    return cir


def parse_log_file(logfile: str, progress=False):
    frames: list[Frame] = []
    current_frame = Frame()
    statistics = Statistics()

    compressed = logfile.endswith('.gz')
    if compressed:
        file_mode = 'rb'
    else:
        file_mode = 'rt'

    with open(logfile, file_mode) as fo:
        if compressed:
            f = gzip.open(fo, 'rt')
        else:
            f = fo

        statistics.file_size = os.stat(fo.fileno()).st_size

        if progress:
            progress_bar = tqdm.tqdm(total=statistics.file_size, unit='B',
                                     unit_scale=True)

        statistics.frame_count = 0
        bad_frame = False
        while True:
            line = f.readline()
            if not line:
                break

            if progress:
                progress_bar.update(fo.tell() - progress_bar.n)

            if 'New Frame' in line:
                if not bad_frame:
                    frames.append(current_frame)
                    statistics.frame_count += 1
                current_frame = Frame()
                bad_frame = False
                try:
                    info = line.split(':')
                    current_frame.serial_timestamp = float(info[0])
                    current_frame.frame_type = info[2].strip()
                    current_frame.sequence_number = int(info[3].strip())
                    current_frame.serial_count = statistics.frame_count
                except IndexError:
                    print(f'Error reading frame info: {line}')
                    bad_frame = True

            elif 'BLOB' in line:
                blob_data = f.readline()

                try:
                    header = line.split('/')
                    title = header[1].strip()
                    version = int(header[2].strip()[1:])
                except (IndexError, ValueError):
                    print(f'Error decoding blob. Line: {line}')
                    continue

                try:
                    decoder = binary_parser.decoders[title]
                    attribute = Frame.binary_to_attr[title]
                    decoded = decoder(blob_data.split(':')[2], version)
                    setattr(current_frame, attribute, decoded)
                except KeyError:
                    print('Unsupported binary!', line)
                except (ValueError, IndexError, AttributeError) as e:
                    print('Binary decoding error!', e)

            elif 'dist_mm' in line:
                # The distance is contained in the twr blob, this is only used
                # as marker to count successful TWR exchanges.
                statistics.twr_count += 1
            elif 'Timeout' in line:
                statistics.error_count_timeout += 1
            elif 'Ranging error' in line:
                # note this includes the sts count
                statistics.error_count_ranging += 1
            elif 'bad STS' in line:
                statistics.error_count_sts_qual += 1

    # add the last frame
    frames.append(current_frame)
    # remove data received before first frame header
    del frames[0]

    if progress:
        progress_bar.close()

    if frames:
        statistics.start_time = frames[0].serial_timestamp
        statistics.end_time = frames[-1].serial_timestamp
        statistics.twr_count
    else:
        print('No frames!!!')

    return frames, statistics


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('log_file')
    args = parser.parse_args()
    frames, statistics = parse_log_file(args.log_file, progress=True)
    statistics.print_stats()
    for frame in frames:
        frame.print_frame()
        print('---------------')
