/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cantransport/TcpCanTransport.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace blueprint::can {

namespace {
#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif
}  // namespace

TcpCanTransport::TcpCanTransport(std::string host, uint16_t port)
    : mHost(std::move(host)), mPort(port) {}

TcpCanTransport::~TcpCanTransport() {
    close();
}

bool TcpCanTransport::open() {
    close();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    std::string port = std::to_string(mPort);
    if (getaddrinfo(mHost.c_str(), port.c_str(), &hints, &result) != 0) return false;

    for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
            setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
            mFd = fd;
            break;
        }
        ::close(fd);
    }
    freeaddrinfo(result);
    mFilled = 0;
    return mFd >= 0;
}

void TcpCanTransport::close() {
    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }
    mFilled = 0;
}

ReadResult TcpCanTransport::read(CanFrame* frame, std::chrono::milliseconds timeout) {
    if (mFd < 0) return ReadResult::kClosed;
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (mFilled < kWireFrameSize) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
        if (left.count() < 0) return ReadResult::kTimeout;
        pollfd pfd{mFd, POLLIN, 0};
        int ready = ::poll(&pfd, 1, static_cast<int>(left.count()));
        if (ready == 0) return ReadResult::kTimeout;
        if (ready < 0) {
            if (errno == EINTR) continue;
            return ReadResult::kError;
        }
        ssize_t n = ::recv(mFd, mBuffer + mFilled, kWireFrameSize - mFilled, 0);
        if (n == 0) return ReadResult::kClosed;
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return ReadResult::kError;
        }
        mFilled += static_cast<size_t>(n);
    }
    mFilled = 0;
    return deserializeFrame(mBuffer, frame) ? ReadResult::kFrame : ReadResult::kError;
}

bool TcpCanTransport::write(const CanFrame& frame) {
    if (mFd < 0) return false;
    uint8_t buf[kWireFrameSize];
    serializeFrame(frame, buf);
    size_t sent = 0;
    while (sent < kWireFrameSize) {
        ssize_t n = ::send(mFd, buf + sent, kWireFrameSize - sent, kSendFlags);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string TcpCanTransport::describe() const {
    return "tcp:" + mHost + ":" + std::to_string(mPort);
}

}  // namespace blueprint::can
