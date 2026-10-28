# Callback-clock audio mux

Run `python mux_callback_audio.py CAPTURE_DIRECTORY` after capture finishes.
Requires Python, NumPy, ffmpeg and ffprobe. The directory must contain the
actual postmix PCM, per-window hashes/timestamps, final audio status, and raw
video used by the recorder. No capture, game resources or saves are included.
The tool validates sequential windows, hashes, monotonic callback clocks and
wall-clock stability, then resamples the real stereo signal against callback
anchors. It refuses an estimated alignment bound above 150 ms. It does not
fill missing windows or pad silence. This measures a digital timeline, not
physical speaker latency.

Run `python test_callback_audio.py CAPTURE_DIRECTORY` for adversarial checks
on a retained private capture. Those inputs are intentionally not distributed.
Longer sessions may still be rejected; a passed short session is not a
universal timing guarantee. Keep failed raw recordings and investigate.
