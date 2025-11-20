// echo_client.cpp
// 用法: ./echo_client <server_ip> <server_port> [conv]
// 例子: ./echo_client 127.0.0.1 4000 123
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

extern "C" {
#include "../../ikcp.h"
}

static uint32_t now_ms()
{
	using namespace std::chrono;
	return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

struct KcpCtx {
	int sock = -1;
	sockaddr_storage peer{};
	socklen_t peer_len = 0;
};

static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	KcpCtx *ctx = reinterpret_cast<KcpCtx *>(user);
	int n = sendto(ctx->sock, buf, len, 0, (sockaddr *)&ctx->peer, ctx->peer_len);
	return n < 0 ? -1 : n;
}

static void set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> [conv]\n";
		return 1;
	}
	std::string server_ip = argv[1];
	int server_port = std::stoi(argv[2]);
	uint32_t conv = (argc >= 4) ? (uint32_t)std::stoul(argv[3]) : 0x11223344u;

	// UDP 套接字
	int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}
	set_nonblock(sock);

	sockaddr_in peer{};
	peer.sin_family = AF_INET;
	peer.sin_port = htons(server_port);
	if (inet_pton(AF_INET, server_ip.c_str(), &peer.sin_addr) != 1) {
		std::cerr << "Invalid IP\n";
		return 1;
	}

	// KCP
	KcpCtx ctx;
	ctx.sock = sock;
	std::memset(&ctx.peer, 0, sizeof(ctx.peer));
	std::memcpy(&ctx.peer, &peer, sizeof(peer));
	ctx.peer_len = sizeof(peer);

	ikcpcb *kcp = ikcp_create(conv, &ctx);
	kcp->output = kcp_output;
	// ikcp_nodelay(kcp, 1, 10, 2, 1);
	// ikcp_wndsize(kcp, 128, 128);
	// ikcp_setmtu(kcp, 1400);

	std::cout << "KCP echo client -> " << server_ip << ":" << server_port
			  << " conv=" << conv << "\n";
	std::cout << "Type text and press Enter. Ctrl-D to quit.\n";

	// 先发一个探测包，帮助服务端记住对端地址
	const char *hello = "hello";
	ikcp_send(kcp, hello, (int)strlen(hello)); // 把消息交给 KCP

	uint32_t next_update = now_ms();

	// 将 stdin 设为非阻塞
	set_nonblock(STDIN_FILENO);

	char udp_buf[2048];
	char app_buf[4096];
	std::string line;

	while (true) {
		// 1) 读UDP -> ikcp_input
		for (;;) {
			int n = recvfrom(sock, udp_buf, sizeof(udp_buf), 0, nullptr, nullptr);
			if (n <= 0)
				break;
			ikcp_input(kcp, udp_buf, n);
		}

		// 2) 从KCP收 -> 打印
		for (;;) {
			int n = ikcp_recv(kcp, app_buf, sizeof(app_buf));
			if (n < 0)
				break;
			std::string msg(app_buf, app_buf + n);
			std::cout << "[echo] " << msg << std::endl;
		}

		// 3) 读取stdin新行并发送
		char buf[1024];
		ssize_t nread = read(STDIN_FILENO, buf, sizeof(buf));
		if (nread > 0) {
			// 按行切分（可能批量读入）
			size_t start = 0;
			for (ssize_t i = 0; i < nread; ++i) {
				if (buf[i] == '\n') {
					ikcp_send(kcp, buf + start, (int)(i - start));
					start = i + 1;
				}
			}
			// 若最后没有换行，可缓存，这里为简化直接发送剩余片段
			if (start < (size_t)nread) {
				ikcp_send(kcp, buf + start, (int)(nread - start));
			}
		} else if (nread == 0) {
			// EOF（Ctrl-D）
			break;
		}

		// 4) 定时 ikcp_update
		uint32_t now = now_ms();
		if (now >= next_update) {
			ikcp_update(kcp, now);
			next_update = ikcp_check(kcp, now);
		}

		usleep(1000); // 1ms
	}

	ikcp_release(kcp);
	close(sock);
	return 0;
}
