#ifndef JITTERBUFFER_H
#define JITTERBUFFER_H

#include "common.hpp"

#include <rtc/rtc.hpp>

class jitterbuffer {
    std::map<std::uint16_t, std::vector<std::byte>> _data;

    std::uint16_t firstSeqNum = 0;
    std::uint16_t lenght = 0;
    bool isFirstPresent = false;
    bool isLastPresent = false;
    bool isFormed = false;

public:
    int nackRequested = 0;

    jitterbuffer() {};
    std::vector<std::byte> addVp8Packet(std::vector<std::byte> pkg, std::uint32_t lastCompletedTs);
    std::vector<std::byte> addVp9Packet(std::vector<std::byte> pkg, std::uint32_t lastCompletedTs);

    // should be used only if addPacket returned frame, i.e. frame is formed
    bool isKeyFrame();
    std::vector<rtc::RtcpNackPart> getPacketsToNack();
};

#endif // JITTERBUFFER_H
