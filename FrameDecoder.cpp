#include "FrameDecoder.hpp"

#include <iostream>
#include <string>

namespace cskip
{
    bool FrameDecoder::open(std::string_view filename)
    {
        // 1. Open the video file
        AVFormatContext *fmt_ctx_raw = nullptr;
        if (avformat_open_input(&fmt_ctx_raw, std::string(filename).c_str(), nullptr, nullptr) < 0)
        {
            return false;
        }
        mFormatContext.reset(fmt_ctx_raw);

        if (avformat_find_stream_info(mFormatContext.get(), nullptr) < 0)
            return false;

        // Find the video stream and decoder
        const AVCodec *codec = nullptr;
        mVideoStreamIndex = av_find_best_stream(mFormatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (mVideoStreamIndex < 0)
            return false;

        // Initialize decoder context
        mCodecContext.reset(avcodec_alloc_context3(codec));
        avcodec_parameters_to_context(mCodecContext.get(), mFormatContext->streams[mVideoStreamIndex]->codecpar);
        if (avcodec_open2(mCodecContext.get(), codec, nullptr) < 0)
            return false;

        return true;
    }

    void FrameDecoder::processAllFrames(std::function<void(const AVFrame *, size_t, double)> frameProcessor)
    {
        // // Allocate Packet and Frame
        mPacket.reset(av_packet_alloc());
        mFrame.reset(av_frame_alloc());

        while (av_read_frame(mFormatContext.get(), mPacket.get()) >= 0)
        {
            if (mPacket->stream_index == mVideoStreamIndex)
            {
                if (avcodec_send_packet(mCodecContext.get(), mPacket.get()) >= 0)
                {
                    while (avcodec_receive_frame(mCodecContext.get(), mFrame.get()) >= 0)
                    {
                        mFrameCount++;
                        double seconds = mFrame->pts * av_q2d(mFormatContext->streams[mVideoStreamIndex]->time_base);

                        if (mFrame->pts == AV_NOPTS_VALUE)
                        {
                            seconds = 0; // Handle missing PTS
                        }

                        frameProcessor(mFrame.get(), mFrameCount, seconds);

                        av_packet_unref(mPacket.get());
                    }
                }
            }
            else
            {
                av_packet_unref(mPacket.get());
            }
        }
    }
 
    void FrameDecoder::close()
    {
        // TODO
        // Free av resources
    }

    Histogram64 FrameDecoder::toHistogram64(const Histogram256 &full_hist)
    {
        Histogram64 binned_hist = {0};
        const int bin_size = 4;

        for (size_t i = 0; i < binned_hist.size(); ++i)
        {
            // Sum 4 consecutive values from the original histogram
            uint64_t sum = 0;
            for (int j = 0; j < bin_size; ++j)
            {
                sum += full_hist[i * bin_size + j];
            }
            binned_hist[i] = sum;
        }

        return binned_hist;
    }

    Histogram256 FrameDecoder::computeLuminanceHistogram256(const AVFrame *frame, int colStride, int rowStride)
    {
        Histogram256 hist = {0};

        // The Y (Luminance) plane is always at index 0
        for (int y = 0; y < frame->height; y += rowStride)
        {
            uint8_t *row = frame->data[0] + (y * frame->linesize[0]);
            for (int x = 0; x < frame->width; x += colStride)
            {
                hist[row[x]]++;
            }
        }
        return hist;
    }

    bool FrameDecoder::isDarkFrame(const Histogram256 &hist, double threshold_pct)
    {
        uint64_t total_pixels = 0;
        for (auto count : hist)
            total_pixels += count;

        // Sum up the "dark" bins (0 to 32 is a safe range for MPG shadows)
        uint64_t dark_pixels = 0;
        for (int i = 0; i <= 32; ++i)
        {
            dark_pixels += hist[i];
        }

        double dark_ratio = static_cast<double>(dark_pixels) / total_pixels;
        return dark_ratio >= threshold_pct;
    }

    void FrameDecoder::saveFrameAsPpm(const AVFrame *frame, int frame_num)
    {
        int width = frame->width;
        int height = frame->height;

        // 1. Prepare the SwsContext for YUV -> RGB24 conversion
        // SWS_BICUBIC is standard; we use the frame's internal format
        struct SwsContext *sws_ctx = sws_getContext(
            width, height, (AVPixelFormat)frame->format,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BICUBIC, nullptr, nullptr, nullptr);

        // 2. Allocate buffer for RGB data
        // RGB24 uses 3 bytes per pixel
        std::vector<uint8_t> buffer(width * height * 3);
        uint8_t *data[1] = {buffer.data()};
        int linesize[1] = {width * 3};

        // 3. Perform the conversion
        sws_scale(sws_ctx, frame->data, frame->linesize, 0, height, data, linesize);
        sws_freeContext(sws_ctx);

        // 4. Write to PPM File
        std::string filename = "frame_" + std::to_string(frame_num) + ".ppm";
        std::ofstream out(filename, std::ios::binary);

        // PPM Header: P6 (binary), Width, Height, MaxVal (255)
        out << "P6\n"
            << width << " " << height << "\n255\n";
        out.write(reinterpret_cast<char *>(buffer.data()), buffer.size());
        out.close();
    }

}