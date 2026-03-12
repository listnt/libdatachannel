#include "jitterbuffer.hpp"

std::vector<std::byte> jitterbuffer::addVp8Packet(std::vector<std::byte> pkg,
                                                  std::int16_t prevMarkedPkg)
{
    if (this->decoding_started_ts == 0) {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        this->decoding_started_ts = std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                                        .count();
    }

    // First byte
    const uint8_t X = 0b10000000;
    // const uint8_t N = 0b00100000;
    const uint8_t S = 0b00010000;

    // Extension byte
    const uint8_t I = 0b10000000;
    const uint8_t L = 0b01000000;
    const uint8_t T = 0b00100000;
    const uint8_t K = 0b00010000;

    // PictureID byte
    const uint8_t M = 0b10000000;
    const uint8_t pid = 0b00000111;

    if (pkg.empty()) {
        return {};
    }

    if (this->isFormed) {
        return {};
    }

    auto rtpHeader = reinterpret_cast<const rtc::RtpHeader *>(pkg.data());
    auto rtpHeaderSize = rtpHeader->getSize() + rtpHeader->getExtensionHeaderSize();
    auto paddingSize = 0;
    if (rtpHeader->padding()) {
        paddingSize = uint8_t(pkg.back());
    }

    if (pkg.size() <= rtpHeaderSize + paddingSize) {
        return {}; // Empty payload
    }

    const std::byte *payloadData = reinterpret_cast<std::byte *>(pkg.data() + rtpHeaderSize);
    size_t payloadSize = pkg.size() - rtpHeaderSize - paddingSize;

    if (payloadSize < 1) {
        return {};
    }

    size_t descriptorSize = 1;
    uint8_t firstByte = std::to_integer<uint8_t>(payloadData[0]);

    // these bytes only valid if their according bit is set
    // do not, I repeat DO NOT consider them as valid
    uint8_t extensionByte = std::to_integer<uint8_t>(payloadData[1]);
    uint8_t pictureIdByte = std::to_integer<uint8_t>(payloadData[2]);
    uint8_t addPicId = std::to_integer<uint8_t>(
        payloadData[3]); // valid only in case if M bit is set

    if (firstByte & X) {
        descriptorSize++;

        if (extensionByte & I) {
            descriptorSize += (pictureIdByte & M) ? 2 : 1;
        }
        if (extensionByte & L) {
            descriptorSize++;
        }
        if ((extensionByte & T) || (extensionByte & K)) {
            descriptorSize++;
        }
    }

    if (payloadSize < descriptorSize) {
        return {};
    }

    payloadData += descriptorSize;
    payloadSize -= descriptorSize;

    if (firstByte & S) {
        if ((firstByte & pid) == 0) {
            this->isFirstPresent = true;
        }
    }

    if (rtpHeader->marker()) {
        this->isLastPresent = true;
    }

    if (this->_data.empty()) {
        this->firstSeqNum = rtpHeader->seqNumber();
        this->lenght = 0;
    } else {
        std::int16_t dist = static_cast<std::int16_t>(rtpHeader->seqNumber() - this->firstSeqNum);
        if (dist < 0) {
            this->firstSeqNum = rtpHeader->seqNumber();
            this->lenght += std::abs(dist);
        } else {
            if (dist > this->lenght) {
                this->lenght = dist;
            }
        }
    }

    this->_data[rtpHeader->seqNumber()] = std::vector<std::byte>(payloadData,
                                                                 payloadData + payloadSize);

    if (this->isFirstPresent && this->isLastPresent) {
        if (this->_data.size() == this->lenght + 1) {
            std::vector<std::byte> res;
            std::int64_t totalSize = 0;

            for (std::uint16_t i = 0; i <= this->lenght; i++) {
                totalSize += this->_data[this->firstSeqNum + i].size();
            }

            res.reserve(totalSize);

            for (std::uint16_t i = 0; i <= this->lenght; i++) {
                auto idx = this->firstSeqNum + i;
                res.insert(res.end(), this->_data[idx].begin(), this->_data[idx].end());
            }

            this->isFormed = true;

            return res;
        }
    }

    return {};
}

std::vector<std::byte> jitterbuffer::addVp9Packet(std::vector<std::byte> pkg,
                                                  std::int16_t prevMarkedPkg)
{
    // first byte
    const uint8_t I = 0b10000000;
    const uint8_t P = 0b01000000;
    const uint8_t L = 0b00100000;
    const uint8_t F = 0b00010000;
    const uint8_t B = 0b00001000;
    const uint8_t E = 0b00000100;
    const uint8_t V = 0b00000010;
    const uint8_t Z = 0b00000001;

    // optional bitmasks
    const uint8_t M = 0b10000000;
    const uint8_t N = 0b00000001;

    // SS bitmasks
    const uint8_t N_S = 0b11100000;
    const uint8_t Y = 0b00010000;
    const uint8_t G = 0b00001000;
    const uint8_t R = 0b00001100;

    if (pkg.empty()) {
        return {};
    }

    if (this->isFormed) {
        return {};
    }

    auto rtpHeader = reinterpret_cast<const rtc::RtpHeader *>(pkg.data());
    auto rtpHeaderSize = rtpHeader->getSize() + rtpHeader->getExtensionHeaderSize();
    auto paddingSize = 0;
    if (rtpHeader->padding()) {
        paddingSize = uint8_t(pkg.back());
    }

    if (pkg.size() <= rtpHeaderSize + paddingSize) {
        return {}; // Empty payload
    }

    const std::byte *pData = reinterpret_cast<std::byte *>(pkg.data() + rtpHeaderSize);
    size_t payloadSize = pkg.size() - rtpHeaderSize - paddingSize;

    if (payloadSize < 1) {
        return {};
    }

    size_t descriptorSize = 1;
    uint8_t firstByte = std::to_integer<uint8_t>(pData[0]);

    if (firstByte & I) {
        if (std::to_integer<uint8_t>(pData[descriptorSize]) & M) {
            descriptorSize++;
        }

        if (payloadSize < descriptorSize) {
            return {};
        }

        descriptorSize++;
    }

    if (firstByte & L) {
        descriptorSize++;
        if (!(firstByte & F)) {
            descriptorSize++;
        }
    }

    if (payloadSize < descriptorSize) {
        return {};
    }

    if (firstByte & F) {
        if (firstByte & P) {
            for (int i = 0; i < 3; i++) {
                if ((std::to_integer<uint8_t>(pData[descriptorSize]) & N) == 0) {
                    break;
                }

                descriptorSize++;
                if (payloadSize < descriptorSize) {
                    return {};
                }
            }

            descriptorSize++;
        }
    }
    if (payloadSize < descriptorSize) {
        return {};
    }

    if (firstByte & V) {
        std::uint8_t SSFirstByte = std::to_integer<uint8_t>(pData[descriptorSize]);
        descriptorSize++;
        if (payloadSize < descriptorSize) {
            return {};
        }

        std::uint8_t NS = ((SSFirstByte & N_S) >> 5) + 1;

        // byte valid only if G bit is set
        std::uint8_t N_G = 0;
        std::uint8_t RTimes = 0;

        if (SSFirstByte & Y) {
            descriptorSize += 4 * NS;
        }
        if (payloadSize < descriptorSize) {
            return {};
        }

        if (SSFirstByte & G) {
            N_G = std::to_integer<uint8_t>(pData[descriptorSize]);
            descriptorSize++;
        }
        if (payloadSize < descriptorSize) {
            return {};
        }

        for (int i = 0; i < N_G; i++) {
            RTimes = (std::to_integer<uint8_t>(pData[descriptorSize]) & R) >> 2;

            descriptorSize++;
            descriptorSize += RTimes;
            if (payloadSize < descriptorSize) {
                return {};
            }
        }
    }

    if (payloadSize < descriptorSize) {
        return {};
    }

    pData += descriptorSize;
    payloadSize -= descriptorSize;

    if (firstByte & B) {
        this->isFirstPresent = true;
    }

    if (firstByte & E) {
        this->isLastPresent = true;
    }

    if (this->_data.empty()) {
        this->firstSeqNum = rtpHeader->seqNumber();
        this->lenght = 0;
    } else {
        std::int16_t dist = static_cast<std::int16_t>(rtpHeader->seqNumber() - this->firstSeqNum);
        if (dist < 0) {
            this->firstSeqNum = rtpHeader->seqNumber();
            this->lenght += std::abs(dist);
        } else {
            if (dist > this->lenght) {
                this->lenght = dist;
            }
        }
    }

    this->_data[rtpHeader->seqNumber()] = std::vector<std::byte>(pData, pData + payloadSize);

    if (this->isFirstPresent && this->isLastPresent) {
        if (this->_data.size() == this->lenght + 1) {
            std::vector<std::byte> res;
            std::int64_t totalSize = 0;

            for (std::uint16_t i = 0; i <= this->lenght; i++) {
                totalSize += this->_data[this->firstSeqNum + i].size();
            }

            res.reserve(totalSize);

            for (std::uint16_t i = 0; i <= this->lenght; i++) {
                auto idx = this->firstSeqNum + i;
                res.insert(res.end(), this->_data[idx].begin(), this->_data[idx].end());
            }

            this->isFormed = true;

            return res;
        }
    }

    return {};
}

bool jitterbuffer::isKeyFrame()
{
    if (!this->isFirstPresent) {
        return false;
    }

    return (((std::uint8_t) 0b00000001)
            & std::to_integer<uint8_t>(this->_data[this->firstSeqNum][0]))
           == 0;
}

std::vector<std::uint32_t> jitterbuffer::getPacketsToNack() {
  if (this->isFormed) {
    return {};
  }

  std::deque<std::uint16_t> missingSequence;

  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  auto nowInMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                     .count();

  if (nowInMs - this->decoding_started_ts > 150) {
    for (std::uint16_t i = 0; i <= this->lenght; i++) {
      auto idx = this->firstSeqNum + i;
      if (!this->_data.contains(idx)) {
        missingSequence.push_back(idx);
      }
    }

    if (!this->isFirstPresent && !missingSequence.empty()) {
      std::uint16_t startVal =
          static_cast<std::uint16_t>(missingSequence.front());
      for (std::uint16_t i = 1; i <= 16; i++) {
        missingSequence.push_front(startVal - i);
      }
    }

    if (!this->isLastPresent && !missingSequence.empty()) {
      std::uint16_t endVal = static_cast<std::uint16_t>(missingSequence.back());
      for (std::uint16_t i = 1; i <= 16; i++) {
        missingSequence.push_back(endVal + i);
      }
    }
  }

  if (missingSequence.size() == 0) {
    return {};
  }

  std::uint16_t pid = missingSequence.front();
  std::uint16_t blp = 0;
  std::vector<std::uint32_t> nacks;

  for (size_t i = 1; i < missingSequence.size(); i++) {
    std::uint16_t dist = missingSequence[i] - pid;

    if (dist > 16) {
      nacks.push_back((std::uint32_t)pid << 16 | blp);

      pid = missingSequence[i];
      blp = 0;
    } else {
      blp |= (1 << (dist - 1));
    }
  }
  nacks.push_back((std::uint32_t)pid << 16 | blp);

  return nacks;
}
