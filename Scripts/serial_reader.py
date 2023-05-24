#!/usr/bin/env python3

import sys
import gzip
import time
import base64
import argparse
import functools
from dataclasses import dataclass

from tqdm import tqdm
import serial

import binary_parser


@dataclass
class Limits():
    twr: int
    full_rot: int
    last_twr_count: int


def write_log_and_print(msg, time=None, time_offset=0, log_file=None):
    log_format = '{:010.5f}: {}'
    log_format_no_time = 'XXXX.XXXXX: {}'

    if time:
        msg = log_format.format(time-time_offset, msg)
    else:
        msg = log_format_no_time.format(msg)

    if len(msg) > 200:
        tqdm.write(msg[:200] + ' ...')
    else:
        tqdm.write(msg)

    if log_file:
        log_file.write(msg)
        log_file.write('\n')


def serial_read(port, wait_for_reset, logger, limit, restart_count=0):
    connected = False
    twr_count = limit.last_twr_count
    last_rotation = 0
    full_rotation_count = 0
    timeout_count = 0
    blob_error_count = 0
    progress_bar = None
    try:
        with serial.Serial(port, baudrate=2250000, timeout=1) as ser:
            connected = True
            print('Connected', file=sys.stderr)

            progress_bar_set = True
            if limit.twr is not None:
                progress_bar = tqdm(total=limit.twr, unit=' frames')
            elif limit.full_rot is not None:
                progress_bar = tqdm(unit=' frames')
                progress_bar_set = False
            else:
                progress_bar = tqdm(unit=' frames')
            progress_bar.n = twr_count
            progress_bar.set_postfix({
                'restart count': restart_count,
                'rotation': last_rotation,
                '360 count': full_rotation_count,
                'timeouts': timeout_count,
                'blob decode errors': blob_error_count,
            })

            if wait_for_reset:
                while True:
                    line_raw = ser.read_until(b'\n')

                    try:
                        line = line_raw.decode('ascii').strip()
                    except UnicodeDecodeError:
                        continue

                    if 'DW3000' not in line:
                        print('\rWaiting for reset ...', end='', flush=True)
                        continue
                    else:
                        print('\nDevice reset, start logging', file=sys.stderr)
                        break

            while True:
                line_raw = ser.read_until(b'\n')

                try:
                    line = line_raw.decode('ascii').strip()
                except UnicodeDecodeError:
                    continue

                if not line:
                    continue

                logger(line, time.time())

                if 'BLOB' in line:
                    try:
                        serial_read_blob(ser, line, logger)
                    except (IndexError, ValueError):
                        tqdm.write(f'Error decoding blob. Line: {line}',
                                   file=sys.stderr)
                        blob_error_count += 1
                elif 'rotation' in line:  # rotation and 360 count
                    parts = line.split()
                    last_rotation = int(parts[1].strip().strip(','))
                    full_rotation_count_new = int(parts[3].strip().strip(','))
                    if full_rotation_count != full_rotation_count_new:
                        full_rotation_count = full_rotation_count_new
                elif 'dist_mm' in line:  # TWR successful
                    progress_bar.set_postfix({
                        'restart count': restart_count,
                        'rotation': last_rotation,
                        '360 count': full_rotation_count,
                        'timeouts': timeout_count,
                        'blob decode errors': blob_error_count,
                    })
                    progress_bar.update(1)
                    twr_count += 1
                elif 'Config' in line:
                    if limit.full_rot is not None and not progress_bar_set:
                        twr_per_angle = line.split()[2].strip()
                        if twr_per_angle == '-':
                            raise RuntimeError(
                                'Rotation is disabled on the receiver, cannot '
                                'use --limit-full-rot.'
                            )
                        else:
                            twr_per_angle = int(twr_per_angle)
                        # After receiving TWR per angle config switch to finer
                        # grained progress bar.
                        progress_bar_set = False
                        progress_bar.reset(twr_per_angle*360*limit.full_rot)
                        progress_bar.n = twr_count
                        progress_bar.refresh()
                elif 'Timeout' in line:
                    timeout_count += 1

                # Check if data collection limit is reached
                if ((limit.twr is not None and twr_count > limit.twr) or
                    (limit.full_rot is not None
                     and full_rotation_count >= limit.full_rot)):
                    break
    except serial.SerialException as e:
        print('Connection error:', e)
        if connected:
            tqdm.write('Lost connection.', file=sys.stderr)
    if progress_bar is not None:
        progress_bar.close()

    limit.last_twr_count = twr_count
    if limit.twr is not None:
        print(f'Collected {twr_count} TWR samples, {limit.twr-twr_count} remaining')
    elif limit.full_rot is not None:
        limit.full_rot -= full_rotation_count
        print('Collected {} full rotations, {} remaining'
              .format(full_rotation_count, limit.full_rot))
    return limit


def serial_read_blob(ser, line, logger):
    """Parse binary measurement data.

    Header line format: "BLOB / type / version / length"
    example: "BLOB / rx diag / v1 / 10"
    """
    header = line.split('/')
    title = header[1].strip()
    try:
        version = int(header[2].strip()[1:])
        length = int(header[3].strip())
    except ValueError:
        print('Error on input line: ', line)
        raise

    data = ser.read(length)
    data_b64 = base64.b64encode(data).decode('utf-8')

    logger('Data: ' + data_b64)

    try:
        decoder = binary_parser.decoders[title]
    except KeyError:
        tqdm.write('Unsupported binary!', file=sys.stderr)
        return

    try:
        decoded = str(decoder(data_b64, version))
    except ValueError as e:
        tqdm.write('Binary decoding error!', e)
        return

    if len(decoded) > 200:
        tqdm.write(decoded[:200] + ' ...')
    else:
        tqdm.write(decoded)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('port', help='Serial port to use (e.g. dev/ttyACM0)')
    parser.add_argument('log_file', nargs='?', help='Output file base name')
    parser.add_argument('--wait-for-reset', '-r', action='store_true',
                        help='Wait for device reset before saving the output')
    parser.add_argument('--compress', action='store_true',
                        help='Gzip compress output file')
    limit_group = parser.add_mutually_exclusive_group()
    limit_group.add_argument('--limit-twr', default=None, type=int,
                        help=('Minimum number of TWR exchanges to log before '
                             'stopping (note: requires --wait-for-reset)'))
    limit_group.add_argument('--limit-full-rot', default=None, type=int,
                        help='Minimum number of full rotations to log before stopping')

    args = parser.parse_args()

    if args.limit_full_rot and not args.wait_for_reset:
        print('Limiting full rotations always enables --wait-for-reset')
        args.wait_for_reset = True

    if args.log_file:
        if args.log_file.endswith('.log'):
            args.log_file = args.log_file[:-4]
        # only create a file, do not overwrite
        if args.compress:
            log_file = gzip.open(args.log_file + '.log.gz', 'xt')
        else:
            log_file = open(args.log_file + '.log', 'xt')
    else:
        log_file = None

    time_offset = time.time()

    logger = functools.partial(write_log_and_print, time_offset=time_offset,
                               log_file=log_file)

    t = time.strftime('%d %b %Y %H:%M:%S', time.localtime(time_offset))
    logger(f'Logging started at: {t}', time_offset)

    limit = Limits(args.limit_twr, args.limit_full_rot, 0)

    try:
        print(f'Trying to connect to {args.port}...', file=sys.stderr)
        restart_counter = 0
        while True:
            limit = serial_read(args.port, args.wait_for_reset, logger,
                                limit, restart_counter)
            if limit.twr is not None:
                remaining = limit.twr-limit.last_twr_count
                if remaining <= 0:
                    print(f'Collected enough ({limit.last_twr_count} of '
                          f'{limit.twr}) TWR exchanges, finishing...')
                    break
                else:
                    print(f'Collected {limit.last_twr_count} of {limit.twr} '
                          f'TWR samples. {remaining} remaining...')
            elif limit.full_rot is not None:
                if limit.full_rot <= 0:
                    print(f'Collected enough full rotations, finishing log.')
                    break
                else:
                    print(f'Still have to collect {limit.full_rot} rotations')
            else:
                print('No limit, continuing...')
            time.sleep(1)
            print('Trying to connect...', file=sys.stderr)
            restart_counter += 1
    finally:
        t = time.strftime('%d %b %Y %H:%M:%S', time.localtime(time.time()))
        logger(f'Logging finished at: {t}', time.time())
        if log_file:
            print('Closing log file...', file=sys.stderr)
            log_file.close()


if __name__ == '__main__':
    main()
