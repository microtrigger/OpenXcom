# Synthetic timing fixtures only; not evidence of real-time capture.
def timing(n=1):
    stamp=1000000+n*100000
    return dict(clock='posix_realtime_monotonic_us', first_realtime_us=stamp,
                first_monotonic_us=stamp, first_frame_offset=0, first_callback_frames=2205,
                last_realtime_us=stamp, last_monotonic_us=stamp,
                last_frame_end_offset=2205, last_callback_frames=2205)
