#!/usr/bin/env python3


import base64
import struct
from collections import namedtuple


# mapping from "readable" c types to struct module codes
type_mapping = {
    'u64': 'Q',  # uint64_t
    'u32': 'L',  # uint32_t
    'u16': 'H',  # uint16_t
    'i16': 'h',  # int16_t
    'u8': 'B',  # uint8_t
}


toa_data = namedtuple('toa_data', 'cia_diag_1 ip_poa sts1_poa sts2_poa '
                      'pdoa xtal_offset sts_qual_index sts_qual tdoa '
                      'ip_toa ip_toast sts1_toa sts1_toast sts2_toa '
                      'sts2_toast fp_th_md dgc_decision')

cir_analysis_data = namedtuple('cir_analysis_data', 'peak power F1 F2 F3 '
                               'fp_index accum_count')

cir_data = namedtuple('cir_data', 'cir_ip cir_sts1 cir_sts2')

twr_data = namedtuple('twr_data', 'Treply1 Treply2 Tround1 Tround2 dist_mm '
                      'twr_count rotation')


def decode_40bit_int(buffer, negative=False):
    '''Decode a 40 bit (5 byte) integer.

    If `negative` is set the number is set, a 2's complement negative number
    is assumed and decoded.
    '''
    value = (buffer[0]
             + (buffer[1] << 8)
             + (buffer[2] << 16)
             + (buffer[3] << 24)
             + (buffer[4] << 32))

    if negative:
        # Number is negative and stored in 2's complement. To get the decimal
        # result we need to invert all bits add one and multiply by (-1)
        all_one = 0xFFFFFFFFFF
        value = -(value ^ all_one) - 1

    return value


def decode_24bit_int(data):
    '''Decode a 24 bit (3 byte) signed integer.'''
    # combine three bytes into one integer
    value = (data[2] << 16) + (data[1] << 8) + (data[0])

    if value & 0x800000:
        # If the 24th bit is not zero we have a negative number, stored in 2's
        # complement. To get the decimal result we need to invert all bits add
        # one and multiply by (-1)
        value = value ^ 0xFFFFFF - 1

    return value


def decode_48bit_complex_array(data):
    '''Decode an array of complex numbers.

    Each complex number has a three byte real part and three byte imaginary
    part. Those are decoded separately as integers and combined into complex
    numbers.
    '''
    # make groups of six
    groups = zip(*([iter(data)]*6), strict=True)

    decoded = []

    # decode complex numbers
    for group in groups:
        real = decode_24bit_int(group[:3])
        imag = decode_24bit_int(group[3:])
        number = real + imag*1j

        decoded.append(number)

    return decoded


def decode_blob_toa(b64_buffer, version):
    '''Decode the time/toa struct.

    Version 3:
    typedef struct
    {
    0      uint32_t cia_diag_1;    // Diagnostics common to both sequences (undocumented CIA_DIAG_1 register)
    1      uint16_t ip_poa;        // Preamble POA
    2      uint16_t sts1_poa;      // POA of STS block 1
    3      uint16_t sts2_poa;      // POA of STS block 2
    4      int16_t pdoa;           // PDoA from two STS POAs signed int [1:-11] in radians
    5      int16_t xtal_offset;    // Estimated xtal offset of remote device
    6      int16_t sts_qual_index; // STS quality value
    7      uint8_t sts_qual;       // STS quality indicator
    8      uint8_t tdoa_sign;      // TDoA is a signed 41-bit integer, store the sign bit here
    9-13   uint8_t tdoa[5];        // TDoA from two STS RX timestamps (40-bit without sign)
    14-18  uint8_t ip_toa[5];      // Preamble/Ipatov RX timestamp
    18     uint8_t ip_toast;       // RX status of preamble
    20-24  uint8_t sts1_toa[5];    // STS RX timestamp on antenna 1
    25     uint8_t sts1_toast;     // RX status of STS on antenna 1 (only high 8-bits, discarding reserved bit 23 in the register)
    26-30  uint8_t sts2_toa[5];    // STS RX timestamp on antenna 2
    31     uint8_t sts2_toast;     // RX status of STS on antenna 2 (only high 8-bits, discarding reserved bit 23 in the register)
    32     uint8_t fp_th_md;       // First path threshold test mode
    33     uint8_t dgc_decision;   // DGC decision index (used for RSSI estimation)
    -      uint8_t padding[1];     // 43 bytes of data, padded to multiple of 4 (because of uint32_t)
    } meas_time_poa_t;
    // note padding is not transmitted
    '''
    if version != 3:
        raise ValueError('Unsupported version: {}'.format(version))

    toa_blob_format = '< u32 u16 u16 u16 i16 i16 i16 u8 u8 5u8 5u8 u8 5u8 u8 5u8 u8 u8 u8'
    for k, v in type_mapping.items():
        toa_blob_format = toa_blob_format.replace(k, v)
    assert struct.calcsize(toa_blob_format) == 43  # padding not transmitted

    data = base64.b64decode(b64_buffer)
    unpacked = struct.unpack(toa_blob_format, data)

    decoded = toa_data(
        cia_diag_1=unpacked[0],
        ip_poa=unpacked[1],
        sts1_poa=unpacked[2],
        sts2_poa=unpacked[3],
        pdoa=unpacked[4],
        xtal_offset=unpacked[5],
        sts_qual_index=unpacked[6],
        sts_qual=unpacked[7],
        tdoa=decode_40bit_int(unpacked[9:14], unpacked[8]),
        ip_toa=decode_40bit_int(unpacked[14:19]),
        ip_toast=unpacked[19],
        sts1_toa=decode_40bit_int(unpacked[20:25]),
        sts1_toast=unpacked[25],
        sts2_toa=decode_40bit_int(unpacked[26:31]),
        sts2_toast=unpacked[31],
        fp_th_md=unpacked[32],
        dgc_decision=unpacked[33],
    )

    return decoded


def decode_blob_cir_analysis(b64_buffer, version):
    '''Decode the cir analysis struct.

    Version 1:
    typedef struct
    {
    0    uint32_t    peak;         // index and amplitude of peak sample in CIR
    1    uint32_t    power;        // channel area allows estimation of channel power (note: 32-bit for preamble, 16-bit for STS)
    2    uint32_t    F1;           // F1
    3    uint32_t    F2;           // F2
    4    uint32_t    F3;           // F3
    5    uint16_t    fp_index;     // First path index
    6    uint16_t    accum_count;  // Number accumulated symbols
    } meas_cir_analysis_t;         // 24 bytes, no padding required
    '''
    if version != 1:
        raise ValueError('Unsupported version: {}'.format(version))

    cir_analysis_blob_format = '< u32 u32 u32 u32 u32 u16 u16'
    for k, v in type_mapping.items():
        cir_analysis_blob_format = cir_analysis_blob_format.replace(k, v)
    assert struct.calcsize(cir_analysis_blob_format) == 24

    data = base64.b64decode(b64_buffer)
    unpacked = list(struct.unpack(cir_analysis_blob_format, data))

    # fp_index is a [10.6] (ip, IP_FP) or [9.6] (sts, CP_FP) fixed point int
    unpacked[5] /= 64

    decoded = cir_analysis_data._make(unpacked)

    return decoded


def decode_blob_cir(b64_buffer, version):
    '''Decode the CIR buffer.

    The data is expected to be a readout of the full ACC_MEM register. It is
    split into preamble, STS1 and STS2 CIR parts and decoded into an array of
    complex numbers each.
    '''
    data = base64.b64decode(b64_buffer)

    # c.f. user manual page 229ff
    bytes_per_symbol = 6
    cir_preamble_start = 0*bytes_per_symbol
    cir_preamble_length = 1016*bytes_per_symbol  # 64MHz PRF
    cir_sts1_start = 1024*bytes_per_symbol
    cir_sts2_start = 1536*bytes_per_symbol
    cir_sts_length = 512*bytes_per_symbol

    cir_ip_bin = data[cir_preamble_start:cir_preamble_start+cir_preamble_length]
    cir_sts1_bin = data[cir_sts1_start:cir_sts1_start+cir_sts_length]
    cir_sts2_bin = data[cir_sts2_start:cir_sts2_start+cir_sts_length]

    cir_ip_decoded = decode_48bit_complex_array(cir_ip_bin)
    cir_sts1_decoded = decode_48bit_complex_array(cir_sts1_bin)
    cir_sts2_decoded = decode_48bit_complex_array(cir_sts2_bin)

    return cir_data(cir_ip_decoded, cir_sts1_decoded, cir_sts2_decoded)


def decode_blob_twr(b64_buffer, version):
    '''Decode the twr information struct.

    Version 2:
    typedef struct
    {
    1    uint64_t Treply1;   // Tag: tx response - rx poll
    2    uint64_t Treply2;   // Anchor: tx final - rx response
    3    uint64_t Tround1;   // Anchor: rx response - tx poll
    4    uint64_t Tround2;   // Tag: rx final - tx response
    5    uint32_t dist_mm;   // Estimated distance in mm
    6    uint16_t twr_count; // Counter of TWR ranging exchanges
    7    uint16_t rotation;  // Rotation in degrees form initial position
    } meas_twr_t;  // 40 bytes, no padding required
    '''
    if version != 2:
        raise ValueError('Unsupported version: {}'.format(version))

    twr_blob_format = '< u64 u64 u64 u64 u32 u16 u16'
    for k, v in type_mapping.items():
        twr_blob_format = twr_blob_format.replace(k, v)
    assert struct.calcsize(twr_blob_format) == 40

    data = base64.b64decode(b64_buffer)
    unpacked = struct.unpack(twr_blob_format, data)

    decoded = twr_data._make(unpacked)

    return decoded


# mapping of binary blob type to decoding function
decoders = {
    'toa': decode_blob_toa,
    'cir analysis ip': decode_blob_cir_analysis,
    'cir analysis sts1': decode_blob_cir_analysis,
    'cir analysis sts2': decode_blob_cir_analysis,
    'cir': decode_blob_cir,
    'twr': decode_blob_twr,
}
