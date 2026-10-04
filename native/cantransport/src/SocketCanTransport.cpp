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

#include "cantransport/SocketCanTransport.h"

#ifdef __linux__
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace blueprint::can {

SocketCanTransport::SocketCanTransport(std::string ifname) : mIfname(std::move(ifname)) {}

SocketCanTransport::~SocketCanTransport() {
    close();
}

std::string SocketCanTransport::describe() const {
    return "socketcan:" + mIfname;
}

#ifdef __linux__

bool SocketCanTransport::open() {
    close();
    int fd = ::socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW);
    if (fd < 0) return false;

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, mIfname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        ::close(fd);
        return false;
    }
    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }
    mFd = fd;
    return true;
}

void SocketCanTransport::close() {
    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }
}

ReadResult SocketCanTransport::read(CanFrame* frame, std::chrono::milliseconds timeout) {
    if (mFd < 0) return ReadResult::kClosed;
    pollfd pfd{mFd, POLLIN, 0};
    int ready = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (ready == 0) return ReadResult::kTimeout;
    if (ready < 0) return errno == EINTR ? ReadResult::kTimeout : ReadResult::kError;

    can_frame raw{};
    ssize_t n = ::read(mFd, &raw, sizeof(raw));
    if (n != static_cast<ssize_t>(sizeof(raw))) return ReadResult::kError;
    // This blueprint handles classic data frames with 11-bit ids only.
    if (raw.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) return ReadResult::kTimeout;

    frame->id = raw.can_id & CAN_SFF_MASK;
    frame->dlc = raw.can_dlc > 8 ? 8 : raw.can_dlc;
    frame->data.fill(0);
    std::memcpy(frame->data.data(), raw.data, frame->dlc);
    return ReadResult::kFrame;
}

bool SocketCanTransport::write(const CanFrame& frame) {
    if (mFd < 0) return false;
    can_frame raw{};
    raw.can_id = frame.id & CAN_SFF_MASK;
    raw.can_dlc = frame.dlc;
    std::memcpy(raw.data, frame.data.data(), frame.dlc);
    return ::write(mFd, &raw, sizeof(raw)) == static_cast<ssize_t>(sizeof(raw));
}

#else  // !__linux__

bool SocketCanTransport::open() {
    return false;
}
void SocketCanTransport::close() {}
ReadResult SocketCanTransport::read(CanFrame*, std::chrono::milliseconds) {
    return ReadResult::kClosed;
}
bool SocketCanTransport::write(const CanFrame&) {
    return false;
}

#endif

}  // namespace blueprint::can
