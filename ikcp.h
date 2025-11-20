
//=====================================================================
//
// KCP - A Better ARQ Protocol Implementation
// skywind3000 (at) gmail.com, 2010-2011
//
// Features:
// + Average RTT reduce 30% - 40% vs traditional ARQ like tcp.
// + Maximum RTT reduce three times vs tcp.
// + Lightweight, distributed as a single source file.
//
//=====================================================================
#ifndef __IKCP_H__
#define __IKCP_H__

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>


//=====================================================================
// 32BIT INTEGER DEFINITION
//=====================================================================
#ifndef __INTEGER_32_BITS__
#define __INTEGER_32_BITS__
#if defined(_WIN64) || defined(WIN64) || defined(__amd64__) ||      \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(_M_AMD64)
typedef unsigned int ISTDUINT32;
typedef int ISTDINT32;
#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
typedef unsigned long ISTDUINT32;
typedef long ISTDINT32;
#elif defined(__MACOS__)
typedef UInt32 ISTDUINT32;
typedef SInt32 ISTDINT32;
#elif defined(__APPLE__) && defined(__MACH__)
#include <sys/types.h>
typedef u_int32_t ISTDUINT32;
typedef int32_t ISTDINT32;
#elif defined(__BEOS__)
#include <sys/inttypes.h>
typedef u_int32_t ISTDUINT32;
typedef int32_t ISTDINT32;
#elif (defined(_MSC_VER) || defined(__BORLANDC__)) && (!defined(__MSDOS__))
typedef unsigned __int32 ISTDUINT32;
typedef __int32 ISTDINT32;
#elif defined(__GNUC__)
#include <stdint.h>
typedef uint32_t ISTDUINT32;
typedef int32_t ISTDINT32;
#else
typedef unsigned long ISTDUINT32;
typedef long ISTDINT32;
#endif
#endif


//=====================================================================
// Integer Definition
//=====================================================================
#ifndef __IINT8_DEFINED
#define __IINT8_DEFINED
typedef char IINT8;
#endif

#ifndef __IUINT8_DEFINED
#define __IUINT8_DEFINED
typedef unsigned char IUINT8;
#endif

#ifndef __IUINT16_DEFINED
#define __IUINT16_DEFINED
typedef unsigned short IUINT16;
#endif

#ifndef __IINT16_DEFINED
#define __IINT16_DEFINED
typedef short IINT16;
#endif

#ifndef __IINT32_DEFINED
#define __IINT32_DEFINED
typedef ISTDINT32 IINT32;
#endif

#ifndef __IUINT32_DEFINED
#define __IUINT32_DEFINED
typedef ISTDUINT32 IUINT32;
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifndef __IUINT64_DEFINED
#define __IUINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 IUINT64;
#else
typedef unsigned long long IUINT64;
#endif
#endif

#ifndef INLINE
#if defined(__GNUC__)

#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define INLINE __inline__ __attribute__((always_inline))
#else
#define INLINE __inline__
#endif

#elif (defined(_MSC_VER) || defined(__BORLANDC__) || defined(__WATCOMC__))
#define INLINE __inline
#else
#define INLINE
#endif
#endif

#if (!defined(__cplusplus)) && (!defined(inline))
#define inline INLINE
#endif


//=====================================================================
// QUEUE DEFINITION
//=====================================================================
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;


//---------------------------------------------------------------------
// queue init
//---------------------------------------------------------------------
#define IQUEUE_HEAD_INIT(name) {&(name), &(name)}
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
	(type *)(((char *)((type *)ptr)) - IOFFSETOF(type, member)))

#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)


//---------------------------------------------------------------------
// queue operation
//---------------------------------------------------------------------
#define IQUEUE_ADD(node, head) (                        \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) (                   \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (              \
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) \
	do {                       \
		IQUEUE_DEL(entry);     \
		IQUEUE_INIT(entry);    \
	} while (0)

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init IQUEUE_INIT
#define iqueue_entry IQUEUE_ENTRY
#define iqueue_add IQUEUE_ADD
#define iqueue_add_tail IQUEUE_ADD_TAIL
#define iqueue_del IQUEUE_DEL
#define iqueue_del_init IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)            \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		 &((iterator)->MEMBER) != (head);                       \
		 (iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

#define iqueue_foreach_entry(pos, head) \
	for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)


#define __iqueue_splice(list, head)                              \
	do {                                                         \
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next;                          \
		(first)->prev = (head), (head)->next = (first);          \
		(last)->next = (at), (at)->prev = (last);                \
	} while (0)

#define iqueue_splice(list, head)        \
	do {                                 \
		if (!iqueue_is_empty(list))      \
			__iqueue_splice(list, head); \
	} while (0)

#define iqueue_splice_init(list, head) \
	do {                               \
		iqueue_splice(list, head);     \
		iqueue_init(list);             \
	} while (0)


#ifdef _MSC_VER
#pragma warning(disable : 4311)
#pragma warning(disable : 4312)
#pragma warning(disable : 4996)
#endif

#endif


//---------------------------------------------------------------------
// BYTE ORDER & ALIGNMENT
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN
#ifdef _BIG_ENDIAN_
#if _BIG_ENDIAN_
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
// 特定平台采用大端字节序
#if defined(__hppa__) ||                                           \
	defined(__m68k__) || defined(mc68000) || defined(_M_M68K) ||   \
	(defined(__MIPS__) && defined(__MIPSEB__)) ||                  \
	defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
	defined(__sparc__) || defined(__powerpc__) ||                  \
	defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
// 默认小端字节序
#define IWORDS_BIG_ENDIAN 0
#endif
#endif

#ifndef IWORDS_MUST_ALIGN
#if defined(__i386__) || defined(__i386) || defined(_i386_)
#define IWORDS_MUST_ALIGN 0
#elif defined(_M_IX86) || defined(_X86_) || defined(__x86_64__)
#define IWORDS_MUST_ALIGN 0
#elif defined(__amd64) || defined(__amd64__)
#define IWORDS_MUST_ALIGN 0
#else
#define IWORDS_MUST_ALIGN 1
#endif
#endif


//=====================================================================
// SEGMENT
// 一个 SEGMENT 对应一个 KCP 数据包
//=====================================================================
struct IKCPSEG {
	struct IQUEUEHEAD node;
	IUINT32 conv; // 会话 ID
	IUINT32 cmd; // KCP 命令 (实际发送为1字节, 详见 ikcp_encode_seg)
	// IKCP_CMD_ACK：这是个 ACK
	// IKCP_CMD_WASK：发送方探测接收方的窗口
	// IKCP_CMD_WINS：接收方回应自己的窗口大小

	IUINT32 frg; // fragment分段号，如果是流模式：默认为 0 (实际发送为1字节, 详见 ikcp_encode_seg)
	IUINT32 wnd; // 窗口大小 (实际发送为2字节, 详见 ikcp_encode_seg)
	IUINT32 ts; // 发送方：数据包的发送时间戳。
	// 接收方（ACK）：所接受数据包的发送时间，而不是发送 ACK 的时间，方便发送方收到 ACK 后计算 rtt。
	IUINT32 sn; // 发送方：发送数据包的序列号
	// 接收方（ACK）：ACK 号
	IUINT32 una; // 未确认序列号：期待下次收到的数据包
	IUINT32 len; // 数据包除去头部的字节数

	/*-----------------以下成员不会实际发送到网络中，主要是超时重传和快速重传计算的辅助数据-----------------*/
	IUINT32 resendts; // = current + rto, 超时重传的阈值, 当前时间超过resendts, 就要重发这个数据包
	IUINT32 rto; // Retransmission Timeout, 下次超时重传的间隔时间, 会随着超时次数增加, 增加速率取决于是不是快速模式
	IUINT32 fastack; // 数据包被跳过次数, 快速重传功能需要
	IUINT32 xmit; // 该数据包发送次数, transmit 的缩写, ,次数太多判断网络断开
	/*-----------------以上成员不会实际发送到网络中，主要是超时重传和快速重传计算的辅助数据-----------------*/

	char data[1]; // 数据包携带的数据，大小根据ikcp_segment_new的参数决定
};

//---------------------------------------------------------------------
// IKCPCB
// 一个 IKCPCB 对应一个 KCP 连接
//---------------------------------------------------------------------

struct IKCPCB {
	IUINT32 conv; // 会话ID
	IUINT32 mtu; // 最大传输单元(字节)
	IUINT32 mss; // 一个KCP传输单元的"数据部分"最大长度(字节), mss + kcp head = mtu
	IUINT32 state; // 连接状态 (-1时表示deadlink)

	IUINT32 snd_una; // (send unacknowledged), 已经发送但未被确认的包的下一个序列号
	IUINT32 snd_nxt; // (send next), 下一个要发送的包序列号, 这里的发送只是从 send_que 放到 send_buf
	IUINT32 rcv_nxt; // (receive next), 下一个要接受的包序列号, 这里的接收只是从 rev_buf 放到 recv_que

	IUINT32 ts_recent; // 没用到
	IUINT32 ts_lastack; // 没用到

	IUINT32 ssthresh; // 拥塞窗口从慢启动转换到拥塞避免的窗口阈值
	IINT32 rx_rttval; // 近4次rtt和srtt的平均差值，反应了rtt偏离srtt的程度
	IINT32 rx_srtt; // 平滑的rtt,近8次rtt平均值
	IINT32 rx_rto; // 系统的重传超时时间
	IINT32 rx_minrto; // 最小重传超时时间

	IUINT32 snd_wnd; // send window size, 发送窗口大小
	IUINT32 rcv_wnd; // recv window size, 接收窗口大小
	IUINT32 rmt_wnd; // remote recv window size, 对端的接收窗口大小
	IUINT32 cwnd; // congestion window size, 拥塞窗口大小
	IUINT32 probe; // probe window size, 探测窗口大小

	IUINT32 current; // 当前时间戳
	IUINT32 interval; // 内部flush刷新间隔
	IUINT32 ts_flush; // 下一次刷新输出的时间戳
	IUINT32 xmit; // 该KCP连接超时重传次数

	IUINT32 nrcv_buf; // rcv_buf的长度
	IUINT32 nsnd_buf; // snd_buf的长度
	IUINT32 nrcv_que; // rcv_que的长度
	IUINT32 nsnd_que; // snd_que的长度

	IUINT32 nodelay; // 是否启用nodelay模式, ==2为快速模式
	IUINT32 updated; // 是否调用过update函数
	IUINT32 ts_probe; // 下次探测窗口大小的时间戳
	IUINT32 probe_wait; // 探测窗口大小的间隔时间，每次探测对面窗口为0（失败）, 探测时间*1.5
	IUINT32 dead_link; // 断开连接的重传次数阈值
	IUINT32 incr; // k*mss , 拥塞窗口等于floor(k)
	struct IQUEUEHEAD snd_queue; // 发送队列
	struct IQUEUEHEAD rcv_queue; // 接收队列
	struct IQUEUEHEAD snd_buf; // 发送缓存, 还没收到 ACK 的包都在这里边
	struct IQUEUEHEAD rcv_buf; // 接收缓存, 将收到的数据暂存, 然后将其中连续的数据放到rcv_queue供上层读取
	IUINT32 *acklist; // 一个整数数组，存放要回复的ack，
	// 结构为 [sn0（接收数据包的序号）, ts0（接收数据包的发送时间）, sn1, ts1, ...]
	IUINT32 ackcount; // 本次需要回复的ack个数
	IUINT32 ackblock; // acklist的大小，会动态扩容，类似于 vector
	void *user; // 用户标识
	char *buffer; // 数据缓冲区
	int fastresend; // 快速重传的失序阈值, 发送方收到 fastresend 个冗余ACK就触发快速重传
	int fastlimit; // 快速重传的次数限制
	int nocwnd; // 0: 有拥塞控制, 1: 没有拥塞控制
	int stream; // 流模式
	int logmask;
	int (*output)(const char *buf, int len, struct IKCPCB *kcp, void *user); // 回调函数，数据发送到下层协议
	void (*writelog)(const char *log, struct IKCPCB *kcp, void *user);
};

typedef struct IKCPCB ikcpcb;

#define IKCP_LOG_OUTPUT 1
#define IKCP_LOG_INPUT 2
#define IKCP_LOG_SEND 4
#define IKCP_LOG_RECV 8
#define IKCP_LOG_IN_DATA 16
#define IKCP_LOG_IN_ACK 32
#define IKCP_LOG_IN_PROBE 64
#define IKCP_LOG_IN_WINS 128
#define IKCP_LOG_OUT_DATA 256
#define IKCP_LOG_OUT_ACK 512
#define IKCP_LOG_OUT_PROBE 1024
#define IKCP_LOG_OUT_WINS 2048

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// interface
//---------------------------------------------------------------------

// create a new kcp control object, 'conv' must equal in two endpoint
// from the same connection. 'user' will be passed to the output callback
// output callback can be setup like this: 'kcp->output = my_udp_output'
ikcpcb *ikcp_create(IUINT32 conv, void *user);

// release kcp control object
void ikcp_release(ikcpcb *kcp);

// set output callback, which will be invoked by kcp
void ikcp_setoutput(ikcpcb *kcp, int (*output)(const char *buf, int len,
											   ikcpcb *kcp, void *user));

// user/upper level recv: returns size, returns below zero for EAGAIN
int ikcp_recv(ikcpcb *kcp, char *buffer, int len);

// user/upper level send, returns below zero for error
int ikcp_send(ikcpcb *kcp, const char *buffer, int len);

// update state (call it repeatedly, every 10ms-100ms), or you can ask
// ikcp_check when to call it again (without ikcp_input/_send calling).
// 'current' - current timestamp in millisec.
void ikcp_update(ikcpcb *kcp, IUINT32 current);

// Determine when should you invoke ikcp_update:
// returns when you should invoke ikcp_update in millisec, if there
// is no ikcp_input/_send calling. you can call ikcp_update in that
// time, instead of call update repeatly.
// Important to reduce unnacessary ikcp_update invoking. use it to
// schedule ikcp_update (eg. implementing an epoll-like mechanism,
// or optimize ikcp_update when handling massive kcp connections)
IUINT32 ikcp_check(const ikcpcb *kcp, IUINT32 current);

// when you received a low level packet (eg. UDP packet), call it
int ikcp_input(ikcpcb *kcp, const char *data, long size);

// flush pending data
void ikcp_flush(ikcpcb *kcp);

// check the size of next message in the recv queue
int ikcp_peeksize(const ikcpcb *kcp);

// change MTU size, default is 1400
int ikcp_setmtu(ikcpcb *kcp, int mtu);

// set maximum window size: sndwnd=32, rcvwnd=32 by default
int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);

// get how many packet is waiting to be sent
int ikcp_waitsnd(const ikcpcb *kcp);

// fastest: ikcp_nodelay(kcp, 1, 20, 2, 1)
// nodelay: 0:disable(default), 1:enable
// interval: internal update timer interval in millisec, default is 100ms
// resend: 0:disable fast resend(default), 1:enable fast resend
// nc: 0:normal congestion control(default), 1:disable congestion control
int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc);


void ikcp_log(ikcpcb *kcp, int mask, const char *fmt, ...);

// setup allocator
void ikcp_allocator(void *(*new_malloc)(size_t), void (*new_free)(void *));

// read conv
IUINT32 ikcp_getconv(const void *ptr);


#ifdef __cplusplus
}
#endif

#endif
