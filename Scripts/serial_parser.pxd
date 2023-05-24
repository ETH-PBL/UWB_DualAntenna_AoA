import cython

cdef class Statistics:
    cdef public double start_time, end_time
    cdef public int frame_count, twr_count
    cdef public int error_count_timeout, error_count_ranging, error_count_sts_qual
    cdef public long file_size
    cpdef print_stats(self)

@cython.locals(line=str, split_line=list, cir_split=list, cir=list)
cdef parse_cir_line(str line)

@cython.locals(line=str, info=list)
cpdef parse_log_file(str: logfile, bint progress=*)
