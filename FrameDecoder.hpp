// Copyright (c) 2026 david laporte david@lightgraysoftware.com

#pragma once

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

// Custom deleters for RAII
struct AVDeleter
{
    void operator()(AVFormatContext *ptr) const { avformat_close_input(&ptr); }
    void operator()(AVCodecContext *ptr) const { avcodec_free_context(&ptr); }
    void operator()(AVPacket *ptr) const { av_packet_free(&ptr); }
    void operator()(AVFrame *ptr) const { av_frame_free(&ptr); }
};

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <fstream>

namespace cskip
{
    using Histogram256 = std::array<uint64_t, 256>;
    using Histogram64 = std::array<uint64_t, 64>;

    class FrameDecoder
    {
    public:
        bool open(std::string_view filename);

        void processAllFrames(std::function<void(const AVFrame *, size_t, double)> frameProcessor);

        void close();

        static Histogram64 toHistogram64(const Histogram256 &full_hist);
        static Histogram256 computeLuminanceHistogram256(const AVFrame *frame, int colStride = 8, int rowStride = 8);

        /// @brief To detect "very dark" frames (like black frames or fade-to-blacks) using a
        // 256-bin luminance (Y) histogram, you should focus on the distribution of weight rather than just the average brightness.
        // In an MPG file (which typically uses limited-range YUV), "true black" is usually value 16, while "super black" (noise) can go down to 0.
        // 1. The "Low-Bin Accumulator" Method (Recommended)
        // Instead of checking if the average is low, check if a high percentage of pixels fall into the "shadow" region (bins 0–32). This prevents a single bright candle in a dark room from "tricking" your detector.
        static bool isDarkFrame(const Histogram256 &hist, double threshold_pct = 0.95);

        // To serialize a YUV frame to a PPM (Portable Pixmap) file, you first need to convert it to RGB24. PPM is a very simple, uncompressed format that expects R G B bytes in sequence.
        // You will need libswscale to perform the conversion from the MPG's native YUV format (usually YUV420P) to RGB24.
        static void saveFrameAsPpm(const AVFrame *frame, int frame_num);

        size_t getFrameCount() const { return mFrameCount; }

    private:
        std::unique_ptr<AVFormatContext, AVDeleter> mFormatContext;
        size_t mFrameCount{0U};

        // Member variables for decoder state
        std::unique_ptr<AVCodecContext, AVDeleter> mCodecContext{nullptr};
        std::unique_ptr<AVPacket, AVDeleter> mPacket{nullptr};
        std::unique_ptr<AVFrame, AVDeleter> mFrame{nullptr};
        int mVideoStreamIndex{-1};
    };
}