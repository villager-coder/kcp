// kcp_echo_server_epoll.cpp
// 用法: ./kcp_echo_server_epoll <listen_port>
// 说明: 多客户端（基于 UDP 的 (conv, 对端地址) 会话），epoll + timerfd 定时驱动 KCP
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "../../ikcp.h"
}

using namespace std;

// ---- 时间 & 工具 ----
static uint32_t now_ms()
{
	return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static string addr_to_string(const sockaddr_storage &ss)
{
	char ip[64];
	uint16_t port = 0;
	if (ss.ss_family == AF_INET) {
		auto *a = (sockaddr_in *)&ss;
		inet_ntop(AF_INET, &a->sin_addr, ip, sizeof(ip));
		port = ntohs(a->sin_port);
	} else if (ss.ss_family == AF_INET6) {
		auto *a = (sockaddr_in6 *)&ss;
		inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof(ip));
		port = ntohs(a->sin6_port);
	} else {
		snprintf(ip, sizeof(ip), "unknown");
	}
	return string(ip) + ":" + to_string(port);
}

// KCP 头部的前 4 字节是 conv（小端）
static inline uint32_t read_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// ---- 会话 ----
struct Session {
	ikcpcb *kcp = nullptr;
	sockaddr_storage peer{};
	socklen_t peer_len = 0;
	uint32_t next_update_ms = 0;
	uint64_t last_active_ms = 0;
	int udp_fd = -1;

	static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
	{
		Session *s = reinterpret_cast<Session *>(user);
		int n = sendto(s->udp_fd, buf, len, 0, (sockaddr *)&s->peer, s->peer_len);
		return n < 0 ? -1 : n;
	}
};

// key: conv + '|' + addr:port
static string make_key(uint32_t conv, const sockaddr_storage &peer)
{
	return to_string(conv) + "|" + addr_to_string(peer);
}

// ---- 服务器 ----
struct Server {
	int udp_fd = -1;
	int ep = -1;
	int tfd = -1;
	uint16_t port = 0;

	unordered_map<string, Session *> sessions;

	// 参数
	int interval_ms = 10; // KCP 驱动粒度
	int gc_idle_ms = 120000; // 无活动会话回收阈值（120s）

	~Server()
	{
		for (auto &kv : sessions) {
			ikcp_release(kv.second->kcp);
			delete kv.second;
		}
		if (tfd >= 0)
			close(tfd);
		if (udp_fd >= 0)
			close(udp_fd);
		if (ep >= 0)
			close(ep);
	}

	bool init(uint16_t listen_port)
	{
		port = listen_port;

		// UDP
		udp_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (udp_fd < 0) {
			perror("socket");
			return false;
		}
		set_nonblock(udp_fd);

		int yes = 1;
		setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(udp_fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind");
			return false;
		}

		// epoll
		ep = epoll_create1(0);
		if (ep < 0) {
			perror("epoll_create1");
			return false;
		}

		epoll_event ev{};
		ev.events = EPOLLIN;
		ev.data.fd = udp_fd;
		if (epoll_ctl(ep, EPOLL_CTL_ADD, udp_fd, &ev) < 0) {
			perror("epoll_ctl udp");
			return false;
		}

		// timerfd，周期性唤醒做 KCP update + GC
		tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
		if (tfd < 0) {
			perror("timerfd_create");
			return false;
		}
		itimerspec its{};
		its.it_interval.tv_sec = 0;
		its.it_interval.tv_nsec = interval_ms * 1000000LL; // 周期
		its.it_value = its.it_interval; // 首次
		if (timerfd_settime(tfd, 0, &its, nullptr) < 0) {
			perror("timerfd_settime");
			return false;
		}
		ev = {};
		ev.events = EPOLLIN;
		ev.data.fd = tfd;
		if (epoll_ctl(ep, EPOLL_CTL_ADD, tfd, &ev) < 0) {
			perror("epoll_ctl tfd");
			return false;
		}

		std::cout << "KCP multi-client echo server listening UDP " << port << "\n";
		return true;
	}

	Session *get_or_create(uint32_t conv, const sockaddr_storage &peer, socklen_t peer_len)
	{
		string key = make_key(conv, peer);
		auto it = sessions.find(key);
		if (it != sessions.end())
			return it->second;

		Session *s = new Session();
		s->udp_fd = udp_fd;
		s->peer = peer;
		s->peer_len = peer_len;
		s->last_active_ms = now_ms();

		s->kcp = ikcp_create(conv, s);
		s->kcp->output = Session::kcp_output;
		ikcp_nodelay(s->kcp, 1, interval_ms, 2, 0); // 快速模式、开启拥塞控制(nc=0)更稳
		ikcp_wndsize(s->kcp, 128, 128);
		ikcp_setmtu(s->kcp, 1400);

		s->next_update_ms = now_ms();
		sessions.emplace(key, s);

		std::cout << "[new] conv=" << conv << " peer=" << addr_to_string(peer)
				  << " total=" << sessions.size() << "\n";
		return s;
	}

	void destroy_idle_sessions()
	{
		uint64_t now = now_ms();
		vector<string> rm;
		rm.reserve(8);
		for (auto &kv : sessions) {
			Session *s = kv.second;
			if (now - s->last_active_ms > (uint64_t)gc_idle_ms) {
				rm.push_back(kv.first);
			}
		}
		for (auto &key : rm) {
			Session *s = sessions[key];
			std::cout << "[gc] close conv=" << s->kcp->conv << " peer=" << addr_to_string(s->peer) << "\n";
			ikcp_release(s->kcp);
			delete s;
			sessions.erase(key);
		}
	}

	void handle_udp_readable()
	{
		uint8_t buf[2048];
		sockaddr_storage peer{};
		socklen_t peer_len = sizeof(peer);

		for (;;) {
			int n = recvfrom(udp_fd, buf, sizeof(buf), 0, (sockaddr *)&peer, &peer_len);
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					break;
				perror("recvfrom");
				break;
			}
			if (n < 24) {
				// KCP 头都不完整，忽略
				continue;
			}

			uint32_t conv = read_le32(buf); // 小端
			Session *s = get_or_create(conv, peer, peer_len);
			s->last_active_ms = now_ms();

			// 喂给 KCP
			ikcp_input(s->kcp, (const char *)buf, n);

			// 从 KCP 拉消息并回显
			char app[4096];
			for (;;) {
				int m = ikcp_recv(s->kcp, app, sizeof(app));
				if (m < 0)
					break;
				// 业务处理：回显
				ikcp_send(s->kcp, app, m);
			}
		}
	}

	void on_timer_tick()
	{
		// 必须读走 timerfd
		uint64_t exp;
		ssize_t r = read(tfd, &exp, sizeof(exp));
		(void)r;

		uint32_t now = now_ms();
		for (auto &kv : sessions) {
			Session *s = kv.second;
			if (now >= s->next_update_ms) {
				ikcp_update(s->kcp, now);
				s->next_update_ms = ikcp_check(s->kcp, now);
			}
		}
		destroy_idle_sessions();
	}

	void run()
	{
		const int MAXEV = 16;
		epoll_event evs[MAXEV];

		while (true) {
			int n = epoll_wait(ep, evs, MAXEV, 1000);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				perror("epoll_wait");
				break;
			}
			for (int i = 0; i < n; ++i) {
				int fd = evs[i].data.fd;
				if (fd == udp_fd) {
					handle_udp_readable();
				} else if (fd == tfd) {
					on_timer_tick();
				}
			}
		}
	}
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <listen_port>\n";
		return 1;
	}
	uint16_t port = (uint16_t)std::stoi(argv[1]);

	Server s;
	if (!s.init(port))
		return 1;
	s.run();
	return 0;
}
