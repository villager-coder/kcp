// echo_client.cpp
// 用法: ./echo_client <server_ip> <server_port> [conv]
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

extern "C" {
#include "../../ikcp.h"
}

static uint32_t now_ms()
{
	using namespace std::chrono;
	return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
static void set_nonblock(int fd)
{
	int f = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, f | O_NONBLOCK);
}
struct Ctx {
	int sock;
	sockaddr_storage peer;
	socklen_t peer_len;
};
static int kcp_output(const char *b, int l, ikcpcb *, void *u)
{
	auto *c = (Ctx *)u;
	return sendto(c->sock, b, l, 0, (sockaddr *)&c->peer, c->peer_len);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <ip> <port> [conv]\n";
		return 1;
	}
	std::string ip = argv[1];
	int port = std::stoi(argv[2]);
	uint32_t conv = (argc >= 4) ? (uint32_t)std::stoul(argv[3]) : 0x11223344;

	int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}
	set_nonblock(sock);

	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &a.sin_addr);

	Ctx ctx{sock};
	std::memcpy(&ctx.peer, &a, sizeof(a));
	ctx.peer_len = sizeof(a);

	ikcpcb *kcp = ikcp_create(conv, &ctx);
	kcp->output = kcp_output;
	ikcp_nodelay(kcp, 1, 10, 2, 0);
	ikcp_wndsize(kcp, 128, 128);
	ikcp_setmtu(kcp, 1400);

	// 先发个握手/探测
	const char *hello = "hello";
	ikcp_send(kcp, hello, (int)strlen(hello));

	set_nonblock(STDIN_FILENO);
	uint32_t next = now_ms();
	char udp[2048], app[4096];

	std::cout << "Type then Enter, Ctrl-D to quit\n";
	while (true) {
		// UDP -> KCP
		for (;;) {
			int n = recvfrom(sock, udp, sizeof(udp), 0, nullptr, nullptr);
			if (n <= 0)
				break;
			ikcp_input(kcp, udp, n);
		}
		// KCP -> APP
		for (;;) {
			int n = ikcp_recv(kcp, app, sizeof(app));
			if (n < 0)
				break;
			std::cout << "[echo] " << std::string(app, app + n) << std::endl;
		}
		// STDIN -> KCP
		char buf[1024];
		ssize_t r = read(STDIN_FILENO, buf, sizeof(buf));
		if (r > 0)
			ikcp_send(kcp, buf, (int)r);
		else if (r == 0)
			break;

		uint32_t now = now_ms();
		if (now >= next) {
			ikcp_update(kcp, now);
			next = ikcp_check(kcp, now);
		}
		usleep(1000);
	}
	ikcp_release(kcp);
	close(sock);
	return 0;
}
