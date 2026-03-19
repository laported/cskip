// Video Decoder using FFmpeg
// Copyright (c) 2026 David LaPorte david@lightgraysoftware.com

#include "FrameDecoder.hpp"

#include <algorithm>
#include <array>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <vector>

using namespace cskip;

void printHistogram(const Histogram64 &hist)
{
    uint64_t max_val = *std::max_element(hist.begin(), hist.end());
    if (max_val == 0)
        max_val = 1;

    // Use a wide-to-utf8 converter
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;

    std::cout << "[";
    for (size_t i = 0; i < 64; i += 2)
    {
        int left_height = (hist[i] * 4) / max_val;
        int right_height = (hist[i + 1] * 4) / max_val;

        int offset = 0;
        // Correct dot mapping for 8-dot Braille (Standard)
        if (left_height >= 1)
            offset |= 0x01; // Dot 1
        if (left_height >= 2)
            offset |= 0x02; // Dot 2
        if (left_height >= 3)
            offset |= 0x04; // Dot 3
        if (left_height >= 4)
            offset |= 0x40; // Dot 7 (Bottom left)

        if (right_height >= 1)
            offset |= 0x08; // Dot 4
        if (right_height >= 2)
            offset |= 0x10; // Dot 5
        if (right_height >= 3)
            offset |= 0x20; // Dot 6
        if (right_height >= 4)
            offset |= 0x80; // Dot 8 (Bottom right)

        char32_t braille_char = 0x2800 + offset;
        std::cout << convert.to_bytes(braille_char);
    }
    std::cout << "]";
}

void hideCursor()
{
    std::cout << "\033[?25l" << std::flush;
}

void showCursor()
{
    std::cout << "\033[?25h" << std::flush;
}

int main(int argc, char *argv[])
{
    FrameDecoder decoder;

    size_t darkFrames{0U};
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_file.mpg>\n";
        return 1;
    }

    const std::string filename = argv[1];

    hideCursor();

    if (decoder.open(filename))
    {
        size_t darkFrames{0U};

        decoder.processAllFrames([&darkFrames](const AVFrame* frame, size_t frameNum, double seconds){
            std::cout << "Decoded Frame " << frameNum << " [PTS: " << seconds << ")] ";
            // Process frame...
            auto hist256 = FrameDecoder::computeLuminanceHistogram256(frame);
            auto hist64 = FrameDecoder::toHistogram64(hist256);
            printHistogram(hist64);
            std::cout << "\r";

            if (FrameDecoder::isDarkFrame(hist256))
            {
                darkFrames++;
                if (darkFrames < 32)
                {
                    FrameDecoder::saveFrameAsPpm(frame, frameNum);
                    std::cout << "DARK\n";
                }
            }

        });

        std::cout << "\nTotal dark frames: " << darkFrames << std::endl;
    }
    showCursor();
    std::cout << "\n";

    decoder.close();

    return 0;
}
