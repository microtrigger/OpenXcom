// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded POSIX contract test, not a native Game/controller integration test.
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
}
int main()
{
    int pair[2]; check(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0, "anonymous seqpacket pair");
    sockaddr_un local = {}, peer = {};
    socklen_t localSize = sizeof(local), peerSize = sizeof(peer);
    check(getsockname(pair[0], reinterpret_cast<sockaddr *>(&local), &localSize) == 0 &&
        local.sun_family == AF_UNIX && localSize == offsetof(sockaddr_un, sun_path), "unnamed local endpoint");
    check(getpeername(pair[0], reinterpret_cast<sockaddr *>(&peer), &peerSize) == 0 &&
        peer.sun_family == AF_UNIX && peerSize == offsetof(sockaddr_un, sun_path), "unnamed peer endpoint");
    char bytes[512] = {}; iovec vector = {bytes, sizeof(bytes)}; msghdr message = {};
    message.msg_iov = &vector; message.msg_iovlen = 1;
    char large[513] = {};
    check(send(pair[1], large, sizeof(large), 0) == 513, "oversize packet sent atomically");
    check(recvmsg(pair[0], &message, MSG_DONTWAIT | MSG_TRUNC) == 513 && (message.msg_flags & MSG_TRUNC), "oversize detected without overflow");
    const int fd = open("/dev/null", O_RDONLY);
    check(fd >= 0, "bounded ancillary fixture");
    char payload = 'x'; iovec sendVector = {&payload, 1};
    char control[CMSG_SPACE(sizeof(int))] = {}; msghdr sent = {};
    sent.msg_iov = &sendVector; sent.msg_iovlen = 1; sent.msg_control = control; sent.msg_controllen = sizeof(control);
    cmsghdr *header = CMSG_FIRSTHDR(&sent); header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int)); std::memcpy(CMSG_DATA(header), &fd, sizeof(fd));
    check(sendmsg(pair[1], &sent, 0) == 1, "ancillary packet sent");
    message.msg_flags = 0;
    check(recvmsg(pair[0], &message, MSG_DONTWAIT | MSG_TRUNC) == 1 && (message.msg_flags & MSG_CTRUNC), "no ancillary buffer detects discarded descriptor");
    close(fd); close(pair[0]); close(pair[1]);
    std::cout << "AgentSocketContractTest PASS " << checks << " checks\n";
}
