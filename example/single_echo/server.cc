// echo_server.cpp
// 用法: ./echo_server <listen_port> [conv]
// 例子: ./echo_server 4000 123
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <chrono>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

extern "C" {
#include "../../ikcp.h"
}

static uint32_t now_ms()
{
	return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct KcpCtx {
	int sock = -1;
	sockaddr_storage peer{};
	socklen_t peer_len = 0;
	bool has_peer = false;
};

static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	KcpCtx *ctx = reinterpret_cast<KcpCtx *>(user);
	if (!ctx->has_peer)
		return 0; // 尚未得知对端地址
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
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <listen_port> [conv]\n";
		return 1;
	}
	int port = std::stoi(argv[1]);

	// 会话ID（conv）：双方必须一致（像“连接号”）
	uint32_t conv = (argc >= 3) ? (uint32_t)std::stoul(argv[2]) : 0x11223344u;

	// UDP 套接字
	int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}
	set_nonblock(sock);

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 1;
	}

	// KCP
	KcpCtx ctx;
	ctx.sock = sock;
	ikcpcb *kcp = ikcp_create(conv, &ctx);

	// 必须设置 output 回调（KCP 交还“要发的UDP裸数据”）
	kcp->output = kcp_output;

	// 调优参数
	// ikcp_nodelay(kcp, 1, 10, 2, 1); // 快速模式, 10ms 内部刷新, 2 次快速重传, 关闭拥塞控制=1(开启)
	// ikcp_wndsize(kcp, 128, 128); // 设置 发送,接收 窗口大小
	// ikcp_setmtu(kcp, 1400); // 设置 MTU 值

	std::cout << "KCP echo server started on UDP port " << port
			  << " conv=" << conv << "\n";

	uint32_t next_update = now_ms();

	char udp_buf[2048];
	char app_buf[4096];

	while (true) {
		// 1) recvfrom 接受原始数据 -> ikcp_input 喂给 KCP
		sockaddr_storage peer{};
		socklen_t peer_len = sizeof(peer);
		for (;;) {
			int n = recvfrom(sock, udp_buf, sizeof(udp_buf), 0, (sockaddr *)&peer, &peer_len);
			if (n <= 0)
				break;
			// 记住对端地址（单客户端版）
			if (!ctx.has_peer) {
				ctx.peer = peer;
				ctx.peer_len = peer_len;
				ctx.has_peer = true;
				char ip[64];
				inet_ntop(AF_INET, &((sockaddr_in *)&peer)->sin_addr, ip, sizeof(ip));
				std::cout << "Peer set: " << ip << ":" << ntohs(((sockaddr_in *)&peer)->sin_port) << "\n";
			}
			ikcp_input(kcp, udp_buf, n);
		}

		// 2) 然后从 KCP 中拉取完整消息（可能 0 条，可能多条）
		for (;;) {
			int n = ikcp_recv(kcp, app_buf, sizeof(app_buf));
			if (n < 0)
				break;
			std::string msg(app_buf, app_buf + n);
			std::cout << "[recv] " << msg << "\n";
			// 回显
			ikcp_send(kcp, app_buf, n);
		}

		// 3) 定时 ikcp_update
		uint32_t now = now_ms();
		if (now >= next_update) {
			ikcp_update(kcp, now);
			next_update = ikcp_check(kcp, now);
		}

		// 小歇
		usleep(1000); // 1ms
	}

	ikcp_release(kcp);
	close(sock);
	return 0;
}
