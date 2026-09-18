## 2024-06-25 - [Precomputing Fade Scale]
**Learning:** In `smk-s3/components/audio/audio_task.cpp`, the `AudioTask::taskRoutine` performs an integer division on every sample of every channel during the initial 20ms fade-in. Microcontrollers do not have fast integer division.
**Action:** Replace `buffer[index] = (buffer[index] * fade_frame) / kAudioFadeInFrames` with `fade_q15 = (fade_frame << 15) / kAudioFadeInFrames` once per frame, and then `buffer[index] = (buffer[index] * fade_q15) >> 15` per sample. This avoids two integer divisions per frame.
