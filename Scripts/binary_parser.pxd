import cython

@cython.locals(value=cython.long, all_one=cython.long)
cdef long decode_40bit_int(tuple buffer, bint negative=*)

@cython.locals(value=cython.int)
cdef int decode_24bit_int(tuple data)

@cython.locals(real=cython.int, imag=cython.int, number=complex)
cdef decode_48bit_complex_array(data)

cdef decode_blob_toa(str b64_buffer, int version)
cdef decode_blob_cir_analysis(str b64_buffer, int version)
cdef decode_blob_cir(str b64_buffer, int version)
cdef decode_blob_twr(str b64_buffer, int version)
