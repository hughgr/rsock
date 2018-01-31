//
// Created by System Administrator on 1/17/18.
//

#ifndef RSOCK_TCPINFO_H
#define RSOCK_TCPINFO_H

#include <cstdint>

#include "ConnInfo.h"

struct TcpInfo : ConnInfo {
    uint32_t seq = 0;
    uint32_t ack = 0;

    uint32_t UpdateAck(uint32_t ack);

    uint32_t UpdateSeq(uint32_t seq);

    bool IsUdp() const override { return false; };

    char *Encode(char *buf, int len) const override;

    const char *Decode(const char *buf, int len) override;

    std::string ToStr() const override;

    TcpInfo() = default;

    TcpInfo(const TcpInfo &info) = default;

    explicit TcpInfo(const ConnInfo &info);

    void Reverse() override;
};

#endif //RSOCK_TCPINFO_H