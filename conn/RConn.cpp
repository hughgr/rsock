//
// Created by System Administrator on 1/18/18.
//
#include <plog/Log.h>
#include "RawTcp.h"
#include "BtmUdpConn.h"
#include "RConn.h"
#include "../util/rhash.h"
#include "../util/rsutil.h"

using namespace std::placeholders;

const int RConn::HEAD_SIZE = EncHead::GetEncBufSize() + HASH_BUF_SIZE;

RConn::RConn(const std::string &hashKey, const std::string &dev, uv_loop_t *loop, TcpAckPool *ackPool, int datalink,
             bool isServer) : IGroup("RConn", nullptr), mHashKey(hashKey) {
    mRawTcp = new RawTcp(dev, loop, ackPool, datalink, isServer);
}

int RConn::Init() {
    IGroup::Init();
    auto fn = std::bind(&IConn::Input, this, _1, _2);
    mRawTcp->SetOnRecvCb(fn);
    return mRawTcp->Init();
}

void RConn::Close() {
    IGroup::Close();
    if (mRawTcp) {
        mRawTcp->Close();
        delete mRawTcp;
        mRawTcp = nullptr;
    }
}

void RConn::AddUdpConn(INetConn *conn) {
    auto rcv = std::bind(&IConn::Input, this, _1, _2);
    AddConn(conn, nullptr, rcv);
}

int RConn::OnRecv(ssize_t nread, const rbuf_t &rbuf) {
    const int MIN_LEN = HASH_BUF_SIZE + EncHead::GetEncBufSize();
    const char *hashed_buf = rbuf.base;
    if (nread > MIN_LEN) {    // nread == HASH_BUF_SIZE will not work
        EncHead head;
        const char *p = hashed_buf + HASH_BUF_SIZE;
        p = EncHead::DecodeBuf(head, p, nread - HASH_BUF_SIZE);
        if (p && hash_equal(hashed_buf, mHashKey, p, nread - (p - hashed_buf))) {
            ConnInfo *info = static_cast<ConnInfo *>(rbuf.data);
            info->head = &head;
            const rbuf_t buf = new_buf(nread - (p - hashed_buf), p, info);
            return IGroup::OnRecv(buf.len, buf);
        }
    }
    return -1;
}

int RConn::Output(ssize_t nread, const rbuf_t &rbuf) {
    ConnInfo *info = static_cast<ConnInfo *>(rbuf.data);
    assert(info);
    EncHead *head = info->head;

    const int ENC_SIZE = EncHead::GetEncBufSize();
    if (HASH_BUF_SIZE + ENC_SIZE + nread > OM_MAX_PKT_SIZE) {
        LOGE << "packet exceeds MTU. redefine MTU. MTU: " << OM_MAX_PKT_SIZE << ", HASH_BUF_SIZE: " << HASH_BUF_SIZE
             << ", ENC_SIZE: " << ENC_SIZE << ", nread: " << nread;
#ifndef NNDEBUG
        assert(HASH_BUF_SIZE + ENC_SIZE + nread <= OM_MAX_PKT_SIZE);
#else
        return -1;
#endif
    }
    char base[OM_MAX_PKT_SIZE] = {0};
    char *p = compute_hash((char *) base, mHashKey, rbuf.base, nread);
    p = head->Enc2Buf(p, OM_MAX_PKT_SIZE - (p - base));
    assert(p);
    memcpy(p, rbuf.base, nread);
    p += nread;

    const rbuf_t buf = new_buf((p - base), base, rbuf.data);
    if (info->IsUdp()) {
        auto key = ConnInfo::KeyForUdpBtm(info->src, info->sp);
        auto conn = ConnOfKey(key);
        if (conn) {
            return conn->Send(buf.len, buf);
        } else {
            LOGE << "no such conn " << key;    // todo. print details
        }
    } else {
        return mRawTcp->Send(buf.len, buf);
    }
    return -1;
}


void RConn::AddConn(IConn *conn, const IConn::IConnCb &outCb, const IConn::IConnCb &recvCb) {
    IGroup::AddConn(conn, outCb, recvCb);
}

void RConn::CapInputCb(u_char *args, const pcap_pkthdr *hdr, const u_char *packet) {
    RConn *conn = reinterpret_cast<RConn *>(args);
    conn->mRawTcp->RawInput(nullptr, hdr, packet);
}
