#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
/*
 * Based on http://www.libsdl.org/projects/flxplay/
 */
#include <SDL.h>
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>
#endif

namespace OpenXcom
{

class Screen;
class Game;
#ifdef OPENXCOM_RESPONSIVE_LOADING
class LoadingAudioHandoff;
class LoadingVideoAudioTestAccess;
#endif

class FlcPlayer
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
public:
    // The lease has no Game, Screen, Mod, VideoState, Options or callback-owner
    // pointer. Its only mutable playback field is the callback's private cursor.
    class AudioLease
    {
    public:
        enum class Route { InternalPCM, ExplicitTrack, UfoSequence, Silent };
        enum class PrepareStatus { Ready, NoPCM, InvalidSpec, InvalidPCM,
            AllocationFailure, ConversionFailure };
        enum class AttachmentState { NeverAttached, Attached, Detached };
        class AttachReceipt
        {
        public:
            AttachReceipt() noexcept : _lease(nullptr), _generation(0), _route(Route::Silent), _hook(false) {}
            bool validFor(std::uint64_t generation, const void *lease) const noexcept { return _lease && _lease == lease && _generation == generation; }
            bool hookInstalled() const noexcept { return _hook; }
        private:
            friend class LoadingAudioHandoff;
            friend class AudioLease;
            AttachReceipt(AudioLease *lease, std::uint64_t generation, Route route, bool hook) noexcept
                : _lease(lease), _generation(generation), _route(route), _hook(hook) {}
            AudioLease *const _lease;
            const std::uint64_t _generation;
            const Route _route;
            const bool _hook;
        };
        class NoHookReceipt
        {
            friend class LoadingAudioHandoff;
            friend class AudioLease;
            NoHookReceipt(AudioLease *lease, std::uint64_t generation, Route route) noexcept
                : _lease(lease), _generation(generation), _route(route) {}
            AudioLease *const _lease;
            const std::uint64_t _generation;
            const Route _route;
        };
        class DetachReceipt
        {
        public:
            DetachReceipt() noexcept : _lease(nullptr), _generation(0), _route(Route::Silent) {}
            bool validFor(std::uint64_t generation, const void *lease) const noexcept { return _lease && _lease == lease && _generation == generation; }
        private:
            friend class LoadingAudioHandoff;
            friend class AudioLease;
            DetachReceipt(AudioLease *lease, std::uint64_t generation, Route route) noexcept
                : _lease(lease), _generation(generation), _route(route) {}
            AudioLease *const _lease;
            const std::uint64_t _generation;
            const Route _route;
        };
        ~AudioLease() noexcept;
        PrepareStatus prepareForDevice(int rate, Uint16 format, int channels) noexcept;
        void acceptAttach(const AttachReceipt &) noexcept;
        void acceptNoHook(const NoHookReceipt &) noexcept;
        void acceptDetach(const DetachReceipt &) noexcept;
        void notePlaybackStarted() noexcept;
        Route route() const noexcept { return _route; }
        std::uint64_t generation() const noexcept { return _generation; }
        bool hasPCM() const noexcept { return _status == PrepareStatus::Ready && !_pcm.empty(); }
        AttachmentState attachmentState() const noexcept { return _attachment; }
        // Lifetime fact only. Caller still needs the matching actual checked reap.
        bool canRelease() const noexcept { return _attachment != AttachmentState::Attached; }
        static void SDLCALL callback(void *, Uint8 *, int) noexcept;
        AudioLease(const AudioLease &) = delete;
        AudioLease &operator=(const AudioLease &) = delete;
    private:
        friend class FlcPlayer;
#ifdef OPENXCOM_RESPONSIVE_TESTING
        friend class LoadingVideoAudioTestAccess;
#endif
        AudioLease(Route, std::uint64_t, std::vector<Uint8> &&, unsigned sourceRate, double gain);
        void requireWorker() const noexcept;
        const Route _route;
        const std::uint64_t _generation;
        const std::thread::id _owner;
        const std::vector<Uint8> _source;
        const unsigned _sourceRate;
        const double _gain;
        std::atomic_flag _prepareClaim = ATOMIC_FLAG_INIT;
        std::vector<Uint8> _pcm;
        std::size_t _cursor;
        unsigned _frameBytes;
        PrepareStatus _status;
        AttachmentState _attachment;
        bool _hookInstalled, _playbackStarted;
    };
    enum class ResponsiveFailure { None, File, Header, Record, Rate, SizeLimit,
        Allocation, Surface, State };
    enum class ResponsiveStep { Waiting, Presented, Finished, Skipped, Failed };
    bool initResponsive(const char *, Game *, AudioLease::Route, std::uint64_t,
        double gain, bool sourceAudioTiming, int dx, int dy) noexcept;
    void beginResponsive(Uint32 now) noexcept;
    ResponsiveStep stepResponsive(Uint32 now, bool skipLastFrame, bool present = true);
    void skipResponsive() noexcept;
    std::shared_ptr<AudioLease> responsiveLease() const noexcept { return _responsiveLease; }
    ResponsiveFailure responsiveFailure() const noexcept { return _responsiveFailure; }
private:
    struct ResponsiveFrame { std::size_t offset, size; Uint16 chunks, delayOverride; };
    struct ResponsiveData
    {
        std::vector<ResponsiveFrame> frames;
        std::vector<Uint8> audio;
        unsigned sourceRate, firstAudioBytes;
        Uint16 type, width, height, speed;
        ResponsiveData() : sourceRate(0), firstAudioBytes(0), type(0), width(0), height(0), speed(0) {}
    };
    static ResponsiveFailure inspectResponsive(const Uint8 *, std::size_t, ResponsiveData &);
    Uint32 responsiveFrameDelay(const ResponsiveFrame &) const noexcept;
    std::shared_ptr<AudioLease> _responsiveLease;
    std::vector<ResponsiveFrame> _responsiveFrames;
    std::size_t _responsiveIndex;
    Uint32 _responsiveLastTick;
    bool _responsiveBegun, _responsiveFirst, _responsiveOwnSurface, _responsiveSourceTiming, _responsivePresent;
    unsigned _responsiveSourceRate, _responsiveFirstAudioBytes;
    ResponsiveFailure _responsiveFailure;
#ifdef OPENXCOM_RESPONSIVE_TESTING
    friend class LoadingVideoAudioTestAccess;
    friend class LoadingVideoIntegrationTestAccess;
#endif
#endif

private:

	Uint8 *_fileBuf;
	Uint32 _fileSize;
	Uint8 *_videoFrameData;
	Uint8 *_chunkData;
	Uint8 *_audioFrameData;
	Uint16 _frameCount;    /* Frame Counter */
	Uint32 _headerSize;    /* Fli file size */
	Uint16 _headerType;    /* Fli header check */
	Uint16 _headerFrames;  /* Number of frames in flic */
	Uint16 _headerWidth;   /* Fli width */
	Uint16 _headerHeight;  /* Fli height */
	Uint16 _headerDepth;   /* Color depth */
	Uint16 _headerSpeed;   /* Number of video ticks between frame */
	Uint32 _videoFrameSize;     /* Frame size in bytes */
	Uint16 _videoFrameType;
	Uint16 _frameChunks;   /* Number of chunks in frame */
	Uint32 _chunkSize;     /* Size of chunk */
	Uint16 _chunkType;     /* Type of chunk */
	Uint16 _delayOverride; /* FRAME_TYPE extension */
	Uint32 _audioFrameSize;
	Uint16 _audioFrameType;

	void (*_frameCallBack)();

	SDL_Surface *_mainScreen;
	Screen *_realScreen;
	SDL_Color _colors[256];
	int _screenWidth;
	int _screenHeight;
	int _screenDepth;
	int _dx, _dy;
	int _offset;
	int _playingState;
	bool _hasAudio, _useInternalAudio;
	int _videoDelay;
	double _volume;

	typedef struct AudioBuffer
	{
		Sint16 *samples;
		int sampleCount;
		int sampleBufSize;
		int currSamplePos;
	}AudioBuffer;


	typedef struct AudioData
	{
		int sampleRate;
		AudioBuffer *loadingBuffer;
		AudioBuffer *playingBuffer;
		SDL_sem *sharedLock;

	}AudioData;

	AudioData _audioData;

	Game *_game;

	void readU16(Uint16 &dst, const Uint8 *const src);
	void readU32(Uint32 &dst, const Uint8 *const src);
	void readS16(Sint16 &dst, const Sint8 *const src);
	void readS32(Sint32 &dst, const Sint8 *const src);
	void readFileHeader();

	bool isValidFrame(Uint8 *frameHeader, Uint32 &frameSize, Uint16 &frameType);
	void decodeVideo(bool skipLastFrame);
	void decodeAudio(int frames);
	void waitForNextFrame(Uint32 delay);
	void SDLPolling();
	bool shouldQuit();

	void playVideoFrame();
	void color256();
	void fliBRun();
	void fliCopy();
	void fliSS2();
	void fliLC();
	void color64();
	void black();

	void playAudioFrame(Uint16 sampleRate);
	void initAudio(Uint16 format, Uint8 channels);
	void deInitAudio();

	bool isEndOfFile(Uint8 *pos);

	static void audioCallback(void *userData, Uint8 *stream, int len);

public:

	FlcPlayer();
	~FlcPlayer();

	/// Open FLC or FLI file, read header, prepare to play it
	bool init(const char *filename, void(*frameCallBack)(), Game *game, bool useAudio, int dx, int dy);
	/// Play the loaded file; set flc.mainScreen first!
	void play(bool skipLastFrame);
	/// Free memory, free love, etc.
	void deInit();
	// Stop FLC Player
	void stop();
	/// Delay player at the end
	void delay(Uint32 milliseconds);
	void setHeaderSpeed(int speed);
	int getFrameCount();
	bool wasSkipped();
};

}
