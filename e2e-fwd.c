#define	__MODULE__	"E2E-FWD"
#define	__IDENT__	"X.00-04"
#define	__REV__		"00.04.00"


/*
**++
**
**  FACILITY:  Ethernet-To-Ethernet forwarder
**
**  DESCRIPTION: A simple application to performs fowarding Ethernet packets "as-is" from one <eth>
**	to a yet another one <eth>. This commpact application is for debug\\test purpose only.
**
**
**  AUTHORS: Ruslan R. (The BadAss SysMan) Laishev
**
**  CREATION DATE:  12-NOV-2024
**
**  USAGE:
**	$ E2E-FWD [/options]<ENTER>
**
**  MODIFICATION HISTORY:
**
**	15-NOV-2024	RRL	Added using of fanout (based on : AF_PACKET TPACKET_V3 example¶ )
**
**	19-NOV-2024	RRL	Split I/O processing to two threads: IF1 -> IF2 and IF1 <- IF2
**
**	21-NOV-2024	RRL	X.00-04: Some code reorganizing.
**
**--
*/
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include	<sched.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<arpa/inet.h>
#include	<errno.h>
#include	<time.h>
#include	<signal.h>
#include	<linux/ip.h>
#include	<linux/tcp.h>
#include	<linux/udp.h>
#include	<linux/if_ether.h>
#include	<linux/if_packet.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<poll.h>
#include	<net/if.h>
#include	<net/ethernet.h> /* the L2 protocols */
#include	<sys/ioctl.h>
#include	<sys/mman.h>
#include	<pthread.h>
#include	<stdatomic.h>


#define		__FAC__	"E2E-FWD"
#define		__TFAC__ __FAC__ ": "		/* Special prefix for $TRACE			*/
#include	"utility_routines.h"


#ifndef	__ARCH__NAME__
#define	__ARCH__NAME__	"VAX"
#endif




/* Global configuration parameters */
#define	E2EFWD$K_MAXPROCS		256						/* A limit of thread\executors */
enum {
	E2EFWD$K_IF1 = 0,
	E2EFWD$K_IF_RX = 0,

	E2EFWD$K_IF2 = 1,
	E2EFWD$K_IF_TX = 1,

	E2EFWD$K_IFMAX
};

typedef void *(* pthread_func_t) (void *);
struct th_arg_t {
		int	th_idx;								/* Thread\\executor index */
		ASC	*if_rx,								/* Name of NIC for receive data */
			*if_tx;								/* NAme of NIC to send data */

};


static ASC	q_logfspec = {0},
	q_confspec = {0},
	q_if[E2EFWD$K_IFMAX] = {0},
	q_fanout = {$ASCINI("NONE")}
	;


static int	s_exit_flag = 0,							/* Global flag 'all must to be stop'	*/
	g_trace = 0,									/* A flag to produce extensible logging	*/
	g_logsize = 0,
	g_fanout = -1,									/* Disable FANOUT mode by default */
	g_nprocs = 1,									/* A number of I/O threads */
	g_number_of_processors = 1							/* A number of CPU\\Cores */
	;


static	const	int s_one = 1, s_off = 0;

enum {
	E2EFWD$K_STAT_RX_BYTES = 0,
	E2EFWD$K_STAT_RX_PKTS,
	E2EFWD$K_STAT_RX_ERRS,

	E2EFWD$K_STAT_TX_BYTES,
	E2EFWD$K_STAT_TX_PKTS,
	E2EFWD$K_STAT_TX_ERRS,

	E2EFWD$K_STAT_MAX
};

struct e2e_fwd_stats_t {
	uint64_t	count[E2EFWD$K_IFMAX][E2EFWD$K_STAT_MAX];			/* array of statistic counters */
};

struct e2e_fwd_stats_t g_e2e_fwd_stats[E2EFWD$K_MAXPROCS];


static const OPTS optstbl [] =
{
	{$ASCINI("config"),	&q_confspec, ASC$K_SZ,	OPTS$K_CONF},

	{$ASCINI("trace"),	&g_trace, 0,		OPTS$K_OPT},
	{$ASCINI("logfile"),	&q_logfspec, ASC$K_SZ,	OPTS$K_STR},
	{$ASCINI("logsize"),	&g_logsize, 0,		OPTS$K_INT},

	{$ASCINI("if1"),	&q_if[E2EFWD$K_IF1],  ASC$K_SZ,	OPTS$K_STR},
	{$ASCINI("if2"),	&q_if[E2EFWD$K_IF2],  ASC$K_SZ,	OPTS$K_STR},
	{$ASCINI("fanout"),	&q_fanout,  ASC$K_SZ,	OPTS$K_STR},
	{$ASCINI("nprocs"),	&g_nprocs, 0,		OPTS$K_INT},

	OPTS_NULL		/* End-of-List marker*/
};



static KWDENT s_fanout_kwds [] = {
	{$ASCINI("NONE"),	-1},
	{$ASCINI("HASH"),	PACKET_FANOUT_HASH},
	{$ASCINI("LB"),		PACKET_FANOUT_LB},
	{$ASCINI("CPU"),	PACKET_FANOUT_CPU},
	{$ASCINI("RND"),	PACKET_FANOUT_RND},
	{$ASCINI("ROLLOVER"),	PACKET_FANOUT_ROLLOVER},
	{$ASCINI("QM"),		PACKET_FANOUT_QM}
};
static size_t s_fanout_kwds_nr = $ARRSZ(s_fanout_kwds);





static void	s_sig_handler (int a_signo)
{
	if ( s_exit_flag )
		{
		fprintf(stdout, "Exit flag has been set, exiting ...\n");
		fflush(stdout);
		_exit(a_signo);
		}

	else if ( a_signo == SIGUSR1 )
		{
		fprintf(stdout, "Set /TRACE=%s\n", (g_trace != g_trace) ? "ON" : "OFF");
		fflush(stdout);
		signal(a_signo, SIG_DFL);
		}
	else if ( (a_signo == SIGTERM) || (a_signo == SIGINT) || (a_signo == SIGQUIT))
		{
		fprintf(stdout, "Get the %d/%#x (%s) signal, set exit_flag!\n", a_signo, a_signo, strsignal(a_signo));
		fflush(stdout);
		s_exit_flag = 1;
		return;
		}
	else	{
		fprintf(stdout, "Get the %d/%#x (%s) signal\n", a_signo, a_signo, strsignal(a_signo));
		fflush(stdout);
		}

	_exit(a_signo);
}

static void	s_init_sig_handler(void)
{
const int l_siglist [] = {SIGTERM, SIGINT, SIGUSR1, SIGQUIT, 0 };
int i;

	/*
	 * Establishing a signals handler
	 */
	signal(SIGPIPE, SIG_IGN);	/* We don't want to crash the server due fucking unix shit */

	for (  i = 0; l_siglist[i]; i++)
		{
		if ( (signal(l_siglist[i], s_sig_handler)) == SIG_ERR )
			$LOG(STS$K_ERROR, "Error establishing handler for signal %d/%#x, error=%d", l_siglist[i], l_siglist[i], errno);
		}
}


static int	s_config_validate	(void)
{



	g_number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
	$LOG(STS$K_INFO, "A number of CPU(-s): %d", g_number_of_processors);

	if ( g_nprocs > g_number_of_processors )
		$LOG(STS$K_INFO, "Number of execution threads (%d) is over of number of CPU (%d)", g_nprocs, g_number_of_processors);




	if ( !$ASCLEN(&q_if[E2EFWD$K_IF1]) )
		return	$LOG(STS$K_ERROR, "Missing IF1");

	if ( 0 > if_nametoindex($ASCPTR(&q_if[E2EFWD$K_IF1])) )
		return	$LOG(STS$K_ERROR, "Cannot translate IF1: <%.*s> to index", 0, $ASC(&q_if[E2EFWD$K_IF1]), errno);

	if ( !$ASCLEN(&q_if[E2EFWD$K_IF2]) )
		return	$LOG(STS$K_ERROR, "Missing IF2");

	if ( 0 > if_nametoindex($ASCPTR(&q_if[E2EFWD$K_IF2])) )
		return	$LOG(STS$K_ERROR, "Cannot translate IF2: <%.*s> to index", 1, $ASC(&q_if[E2EFWD$K_IF2]), errno);


	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: Intialize network interface in the AF_PACKET mode, set promocious flag to accep all HA destination
 *	and send from any HA sources.
 *
 *   INPUTS:
 *	a_if_name:	name of the network interface
 *
 *   OUTPUTS:
 *	a_if_sd:	socket descriptior hass ben assotiated with the network device
 *	a_if_sd:	HA address of the NIC
 *	a_if_sk:	Link Layer socket
 *
 *   RETURNS:
 *   condition code
 */
static int	s_init_eth (
		const char	*a_if_name,
			int	*a_if_sd,
			char	*a_if_ha,
	struct sockaddr_ll	*a_if_sk
			)

{
int	l_rc, l_opt;
struct ifreq l_ifreq = {0};
struct packet_mreq l_mreq = {.mr_type = PACKET_MR_PROMISC};
struct fanout_args l_fanout_args = {0};


	$LOG(STS$K_INFO, "Initialize <%s> ...", a_if_name);

	*a_if_sk = (struct sockaddr_ll)   {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL), .sll_halen = ETH_ALEN};

	a_if_sk->sll_ifindex = if_nametoindex(a_if_name);


	if ( 0 > (*a_if_sd = socket(PF_PACKET, SOCK_RAW,  htons(ETH_P_ALL))) )
		return	$LOG(STS$K_ERROR, "socket()->%d, errno", *a_if_sd, errno);


	if ( 0 > (l_rc = bind(*a_if_sd , (struct sockaddr *) a_if_sk , sizeof(struct sockaddr_ll))) )
		return $LOG(STS$K_ERROR, "bind(#%d, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);



	strncpy(l_ifreq.ifr_name, a_if_name, IFNAMSIZ - 1);
	if ( 0 > (l_rc = ioctl(*a_if_sd, SIOCGIFHWADDR, &l_ifreq)) )
		return	$LOG(STS$K_ERROR, "ioctl()->%d, errno: %d", l_rc, errno);

	memcpy(a_if_ha, l_ifreq.ifr_ifru.ifru_hwaddr.sa_data, ETH_ALEN);

	l_mreq.mr_ifindex = a_if_sk->sll_ifindex;
	l_mreq.mr_type = PACKET_MR_PROMISC;

	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &l_mreq, sizeof(l_mreq))) )
	     return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_ADD_MEMBERSHIP, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);

	l_opt = 1;
	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_QDISC_BYPASS, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_QDISC_BYPASS, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);


	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_LOSS, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_LOSS, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);


	if ( g_fanout > 0 )
		{
		l_fanout_args.type_flags = g_fanout;
		l_fanout_args.id = *a_if_sd;


		if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_FANOUT, &l_fanout_args, sizeof(l_fanout_args))) )
		     return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_FANOUT, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);

		$LOG(STS$K_INFO, "Fanout is set for sd: %d [type: %d, id: %d]", *a_if_sd, l_fanout_args.type_flags, l_fanout_args.id);
		}

	return	STS$K_SUCCESS;
}




struct e2e_fwd_ring_t {
	struct iovec *rd;
	uint8_t *map;
	struct tpacket_req3 req;
};



static int	s_init_eth_ring (
		const char	*a_if_name,
			int	*a_if_sd,
			char	*a_if_ha,
	struct sockaddr_ll	*a_if_sk,
	struct e2e_fwd_ring_t	*a_ring
			)

{
int	l_rc, l_opt;
struct ifreq l_ifreq = {0};
struct packet_mreq l_mreq = {.mr_type = PACKET_MR_PROMISC};
struct fanout_args l_fanout_args = {0};
unsigned int l_blocksiz = (1 << 22), l_framesiz = (1 << 11), l_blocknum = 64;

	$LOG(STS$K_INFO, "Initialize <%s> ...", a_if_name);

	*a_if_sk = (struct sockaddr_ll)   {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL), .sll_halen = ETH_ALEN};

	a_if_sk->sll_ifindex = if_nametoindex(a_if_name);


	if ( 0 > (*a_if_sd = socket(PF_PACKET, SOCK_RAW,  htons(ETH_P_ALL))) )
		return	$LOG(STS$K_ERROR, "socket()->%d, errno", *a_if_sd, errno);



	l_opt = TPACKET_V3;
	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_VERSION, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_VERSION)->%d, errno: %d", *a_if_sd, l_rc, errno);


	memset(&a_ring->req, 0, sizeof(a_ring->req));

	a_ring->req.tp_block_size = l_blocksiz;
	a_ring->req.tp_frame_size = l_framesiz;
	a_ring->req.tp_block_nr = l_blocknum;
	a_ring->req.tp_frame_nr = (l_blocksiz * l_blocknum) / l_framesiz;
	a_ring->req.tp_retire_blk_tov = 60;
	a_ring->req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;

	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_RX_RING, &a_ring->req, sizeof(a_ring->req))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_RX_RING)->%d, errno: %d", *a_if_sd, l_rc, errno);







	if ( 0 > (l_rc = bind(*a_if_sd , (struct sockaddr *) a_if_sk , sizeof(struct sockaddr_ll))) )
		return $LOG(STS$K_ERROR, "bind(#%d, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);



	strncpy(l_ifreq.ifr_name, a_if_name, IFNAMSIZ - 1);
	if ( 0 > (l_rc = ioctl(*a_if_sd, SIOCGIFHWADDR, &l_ifreq)) )
		return	$LOG(STS$K_ERROR, "ioctl()->%d, errno", l_rc, errno);
	memcpy(a_if_ha, l_ifreq.ifr_ifru.ifru_hwaddr.sa_data, ETH_ALEN);

	l_mreq.mr_ifindex = a_if_sk->sll_ifindex;
	l_mreq.mr_type = PACKET_MR_PROMISC;

	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &l_mreq, sizeof(l_mreq))) )
	     return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_ADD_MEMBERSHIP, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);

	l_opt = 1;
	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_QDISC_BYPASS, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_QDISC_BYPASS, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);

#if 0
	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_LOSS, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_LOSS, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);

#endif


	l_fanout_args.type_flags = g_fanout;
	l_fanout_args.id = *a_if_sd;



	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_FANOUT, &l_fanout_args, sizeof(l_fanout_args))) )
	     return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_FANOUT, <%s>)->%d, errno: %d", *a_if_sd, a_if_name, l_rc, errno);

	$LOG(STS$K_INFO, "Fanout is set for sd: %d [type: %d, id: %d]", *a_if_sd, l_fanout_args.type_flags, l_fanout_args.id);

	return	STS$K_SUCCESS;
}




static atomic_uint s_cpu_num;

static int s_bind_to_cpu(int *a_cpu_num)
{
cpu_set_t set;
int	l_rc, l_cpu_num;
pid_t	l_tid;

	l_cpu_num = atomic_fetch_add(&s_cpu_num, 1);
	l_cpu_num %= g_number_of_processors;

	CPU_ZERO( &set );
	CPU_SET( l_cpu_num, &set );
	l_tid = gettid();


	if (l_rc = sched_setaffinity( l_tid, sizeof( cpu_set_t ), &set ) )
		return	$LOG(STS$K_ERROR, "sched_setaffinity(TID: %d, CPU: %d)", l_tid, l_cpu_num);


	*a_cpu_num = l_cpu_num;


	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Processor for FANOUT=NONE mode. Is supposed to be used on "simplest AF_PACKET" usage.
 *	Do initialization ofg own pait I/O descriptors for the the same NIC pair.
 *	Bind thread to core.
 *
 *
 *   INPUTS:
 *	a_th_idx:	Thread index
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	NONE
 */
static int	s_e2e_fwd_th (int a_th_idx)
{
int	l_if1_sd, l_if2_sd, l_rc, l_len, l_cpu_num;
struct sockaddr_ll  l_if1_sk = {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL)},
	l_if2_sk = {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL)};
struct pollfd l_pfd[E2EFWD$K_IFMAX] = {-1};
char	l_buf[2*8192];
char	l_if_ha[E2EFWD$K_IFMAX][ETH_ALEN];
struct e2e_fwd_stats_t  *l_e2e_fwd_stats;

	assert( a_th_idx < E2EFWD$K_MAXPROCS );
	l_e2e_fwd_stats = &g_e2e_fwd_stats[a_th_idx];

	l_rc = s_bind_to_cpu (&l_cpu_num);

	if ( !(1 & (l_rc = s_init_eth ($ASCPTR(&q_if[E2EFWD$K_IF1]), &l_if1_sd, l_if_ha[E2EFWD$K_IF1], &l_if1_sk))) )
		return	$LOG(STS$K_ERROR, "Abort thread execution");

	if ( !(1 & (l_rc = s_init_eth ($ASCPTR(&q_if[E2EFWD$K_IF2]), &l_if2_sd, l_if_ha[E2EFWD$K_IF2], &l_if2_sk))) )
		return	$LOG(STS$K_ERROR, "Abort thread execution");


	l_pfd[E2EFWD$K_IF1].fd = l_if1_sd;
	l_pfd[E2EFWD$K_IF2].fd = l_if2_sd;
	l_pfd[E2EFWD$K_IF1].events = l_pfd[E2EFWD$K_IF2].events = POLLIN;

	$LOG(STS$K_INFO, "CPU#%d --- Starting packet processing [%.*s\\#%d, %.*s\\#%d] ...",
	     l_cpu_num,
	     $ASC(&q_if[E2EFWD$K_IF1]), l_if1_sd,
	     $ASC(&q_if[E2EFWD$K_IF2]), l_if2_sd
	     );

	while ( !s_exit_flag )
		{
		if ( !(l_rc = poll(l_pfd, E2EFWD$K_IFMAX, 3*1024)) )
			continue;

		assert( l_rc >= 0 );

		if ( l_pfd[E2EFWD$K_IF1].revents & POLLIN )
			{
			l_rc  = recv(l_pfd[E2EFWD$K_IF1].fd , l_buf, sizeof(l_buf), 0);
			assert( l_rc > 0 );

			l_len = l_rc;
			l_e2e_fwd_stats->count[E2EFWD$K_IF1][E2EFWD$K_STAT_RX_PKTS] += 1;
			l_e2e_fwd_stats->count[E2EFWD$K_IF1][E2EFWD$K_STAT_RX_BYTES] += l_len;

			if ( g_trace )
				$DUMPHEX(l_buf, l_len);

			if ( 0 > (l_rc = send(l_pfd[E2EFWD$K_IF2].fd, l_buf, l_len,  0))  )
				{
				l_e2e_fwd_stats->count[E2EFWD$K_IF2][E2EFWD$K_STAT_TX_ERRS] += 1;

				$IFTRACE(g_trace, "send(#%d, <%.*s>)->%d, errno: %d", l_pfd[E2EFWD$K_IF2].fd, $ASC(&q_if[E2EFWD$K_IF2]), l_rc, errno);
				}
			else	{
				l_e2e_fwd_stats->count[E2EFWD$K_IF2][E2EFWD$K_STAT_TX_PKTS] += 1;
				l_e2e_fwd_stats->count[E2EFWD$K_IF2][E2EFWD$K_STAT_TX_BYTES] += l_len;
				}
			}

		if ( l_pfd[E2EFWD$K_IF2].revents & POLLIN )
			{
			l_rc  = recv(l_pfd[E2EFWD$K_IF2].fd , l_buf, sizeof(l_buf), 0);
			assert( l_rc > 0 );

			l_len = l_rc;
			l_e2e_fwd_stats->count[E2EFWD$K_IF2][E2EFWD$K_STAT_RX_PKTS] += 1;
			l_e2e_fwd_stats->count[E2EFWD$K_IF2][E2EFWD$K_STAT_RX_BYTES] += l_len;

			if ( g_trace )
				$DUMPHEX(l_buf, l_len);

			if ( 0 > (l_rc = send(l_pfd[E2EFWD$K_IF1].fd, l_buf, l_len,  0))  )
				{
				l_e2e_fwd_stats->count[E2EFWD$K_IF1][E2EFWD$K_STAT_TX_ERRS] += 1;

				$IFTRACE(g_trace, "send(#%d, <%.*s>)->%d, errno: %d", l_pfd[E2EFWD$K_IF1].fd, $ASC(&q_if[E2EFWD$K_IF1]), l_rc, errno);
				}
			else	{
				l_e2e_fwd_stats->count[E2EFWD$K_IF1][E2EFWD$K_STAT_TX_PKTS] += 1;
				l_e2e_fwd_stats->count[E2EFWD$K_IF1][E2EFWD$K_STAT_TX_BYTES] += l_len;
				}
			}

		}


	return	STS$K_SUCCESS;

}

/*
 *  DESCRIPTION: Do I/O from left-to-right in one direction.
 *	Read from RX and send to TX and is NOT in back direction!
 *
 *  INPUTS:
 *	a_th_arg:
 *
 *  OUTPUTS:
 *	NONE
 *
 *  RETURNS:
 *	NONE
 */
static int	s_e2e_fwd_th_l2r (
		struct th_arg_t		*a_th_arg
				)
{
int	l_if_rx_sd, l_if_tx_sd, l_rc, l_len, l_cpu_num, l_th_idx = a_th_arg->th_idx;
struct sockaddr_ll  l_if_rx_sk = {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL)},
	l_if_tx_sk = {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL)};
struct pollfd l_pfd[E2EFWD$K_IFMAX] = {-1};
char	l_buf[2*8192];
char	l_if_ha[E2EFWD$K_IFMAX][ETH_ALEN];
struct e2e_fwd_stats_t  *l_e2e_fwd_stats;

	assert( l_th_idx < E2EFWD$K_MAXPROCS );
	l_e2e_fwd_stats = &g_e2e_fwd_stats[l_th_idx];

	l_rc = s_bind_to_cpu (&l_cpu_num);

	if ( !(1 & (l_rc = s_init_eth ($ASCPTR(a_th_arg->if_rx), &l_if_rx_sd, l_if_ha[E2EFWD$K_IF1], &l_if_rx_sk))) )
		return	$LOG(STS$K_ERROR, "Abort thread execution");

	if ( !(1 & (l_rc = s_init_eth ($ASCPTR(a_th_arg->if_tx), &l_if_tx_sd, l_if_ha[E2EFWD$K_IF2], &l_if_tx_sk))) )
		return	$LOG(STS$K_ERROR, "Abort thread execution");


	l_pfd[E2EFWD$K_IF_RX].fd = l_if_rx_sd;
	l_pfd[E2EFWD$K_IF_TX].fd = l_if_tx_sd;
	l_pfd[E2EFWD$K_IF_RX].events = POLLIN;
	l_pfd[E2EFWD$K_IF_TX].events = 0;

	$LOG(STS$K_INFO, "Starting packet processing [%.*s\\#%d -->> %.*s\\#%d] on CPU#%d ...",
	     $ASC(a_th_arg->if_rx), l_if_rx_sd,
	     $ASC(a_th_arg->if_tx), l_if_tx_sd,
	     l_cpu_num
	     );


	while ( !s_exit_flag )
		{
		if ( !(l_rc = poll(l_pfd, E2EFWD$K_IFMAX, 3*1024)) )
			continue;

		assert( l_rc >= 0 );

		if ( (l_pfd[E2EFWD$K_IF1].revents & POLLOUT) == l_pfd[E2EFWD$K_IF1].events )
			{
			l_pfd[E2EFWD$K_IF_RX].events = POLLIN;
			l_pfd[E2EFWD$K_IF_TX].events = 0;

			continue;
			}


		if ( l_pfd[E2EFWD$K_IF_RX].revents & POLLIN )
			{
			l_rc  = recv(l_pfd[E2EFWD$K_IF1].fd , l_buf, sizeof(l_buf), 0);
			assert( l_rc >= 0 );

			l_len = l_rc;
			l_e2e_fwd_stats->count[E2EFWD$K_IF_RX][E2EFWD$K_STAT_RX_PKTS] += 1;
			l_e2e_fwd_stats->count[E2EFWD$K_IF_RX][E2EFWD$K_STAT_RX_BYTES] += l_len;

			if ( g_trace )
				$DUMPHEX(l_buf, l_len);

			if ( 0 > (l_rc = send(l_pfd[E2EFWD$K_IF_TX].fd, l_buf, l_len,  0))  )
				{
				l_e2e_fwd_stats->count[E2EFWD$K_IF_TX][E2EFWD$K_STAT_TX_ERRS] += 1;

				if ( errno == ENOBUFS )
					{
					l_pfd[E2EFWD$K_IF_RX].events = 0;		/* Disable receiving on RX */
					l_pfd[E2EFWD$K_IF_TX].events = POLLOUT;		/* For TX to ready to accept new send */
					}

				$IFTRACE(g_trace, "send(#%d, <%.*s>)->%d, errno: %d", l_pfd[E2EFWD$K_IF2].fd, $ASC(&q_if[E2EFWD$K_IF2]), l_rc, errno);
				}
			else	{
				l_e2e_fwd_stats->count[E2EFWD$K_IF_TX][E2EFWD$K_STAT_TX_PKTS] += 1;
				l_e2e_fwd_stats->count[E2EFWD$K_IF_TX][E2EFWD$K_STAT_TX_BYTES] += l_len;
				}
			}
		}


	return	STS$K_SUCCESS;

}








static void s_show_stat (void)
{
struct e2e_fwd_stats_t 	l_e2e_fwd_stats= {0};

	for (int i = 0; i < g_nprocs; i++)
		{
		for (int j = 0; j < E2EFWD$K_STAT_MAX; j++)
			l_e2e_fwd_stats.count[E2EFWD$K_IF1][j] += g_e2e_fwd_stats[i].count[E2EFWD$K_IF1][j];

		for (int j = 0; j < E2EFWD$K_STAT_MAX; j++)
			l_e2e_fwd_stats.count[E2EFWD$K_IF2][j] += g_e2e_fwd_stats[i].count[E2EFWD$K_IF2][j];
		}



	$LOG(STS$K_INFO, "-------------------------------------------------------------------------------------------------");

	$LOG(STS$K_INFO, "<%.*s> --- RX: [pkts: %llu, octets: %llu, errs: %llu], TX: [pkts: %llu, octets: %llu, errs: %llu]",
		$ASC(&q_if[E2EFWD$K_IF1]),
		l_e2e_fwd_stats.count[E2EFWD$K_IF1][E2EFWD$K_STAT_RX_PKTS],
		l_e2e_fwd_stats.count[E2EFWD$K_IF1][E2EFWD$K_STAT_RX_BYTES],
		l_e2e_fwd_stats.count[E2EFWD$K_IF1][E2EFWD$K_STAT_RX_ERRS],

		l_e2e_fwd_stats.count[E2EFWD$K_IF1][E2EFWD$K_STAT_TX_PKTS],
		l_e2e_fwd_stats.count[E2EFWD$K_IF1][E2EFWD$K_STAT_TX_BYTES],
		l_e2e_fwd_stats.count[E2EFWD$K_IF1][E2EFWD$K_STAT_TX_ERRS]
		);

	$LOG(STS$K_INFO, "<%.*s> --- RX: [pkts: %llu, octets: %llu, errs: %llu], TX: [pkts: %llu, octets: %llu, errs: %llu]",
		$ASC(&q_if[E2EFWD$K_IF2]),
		l_e2e_fwd_stats.count[E2EFWD$K_IF2][E2EFWD$K_STAT_RX_PKTS],
		l_e2e_fwd_stats.count[E2EFWD$K_IF2][E2EFWD$K_STAT_RX_BYTES],
		l_e2e_fwd_stats.count[E2EFWD$K_IF2][E2EFWD$K_STAT_RX_ERRS],

		l_e2e_fwd_stats.count[E2EFWD$K_IF2][E2EFWD$K_STAT_TX_PKTS],
		l_e2e_fwd_stats.count[E2EFWD$K_IF2][E2EFWD$K_STAT_TX_BYTES],
		l_e2e_fwd_stats.count[E2EFWD$K_IF2][E2EFWD$K_STAT_TX_ERRS]
		);
}



int	main	(int argc, char **argv)
{
int	status, l_rc;
pthread_t	l_tid;
struct th_arg_t *l_th_arg;



	$LOG(STS$K_INFO, "Rev: " __IDENT__ "/"  __ARCH__NAME__   ", (built  at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")");

	/*
	 * Process command line arguments
	 */
	__util$getparams(argc, argv, optstbl);

	if ( $ASCLEN(&q_logfspec) )
		{
		__util$deflog($ASCPTR(&q_logfspec), NULL);

		$LOG(STS$K_INFO, "Rev: " __IDENT__ "/"  __ARCH__NAME__   ", (built  at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")");
		}


	__util$showparams(optstbl);

	s_init_sig_handler ();

	if ( !(1 & s_config_validate ()) )
		exit(-1);

	for (int i = 0; i < g_nprocs; i++)
		{
		if ( g_fanout >= 0 )
			{
			l_th_arg = calloc(1, sizeof(struct th_arg_t));
			assert ( l_th_arg );

			l_th_arg->th_idx = i;
			l_th_arg->if_rx = &q_if[E2EFWD$K_IF1];
			l_th_arg->if_tx = &q_if[E2EFWD$K_IF2];

			if ( (l_rc = pthread_create(&l_tid, NULL, (pthread_func_t) s_e2e_fwd_th_l2r, (void *) l_th_arg)) )
				s_exit_flag = $LOG(STS$K_ERROR, "pthread_create(s_e2e_fwd_th)->%d, errno: %d", l_rc, errno);


			l_th_arg = calloc(1, sizeof(struct th_arg_t));
			assert ( l_th_arg );

			l_th_arg->th_idx = i;
			l_th_arg->if_rx = &q_if[E2EFWD$K_IF2];
			l_th_arg->if_tx = &q_if[E2EFWD$K_IF1];

			if ( (l_rc = pthread_create(&l_tid, NULL, (pthread_func_t) s_e2e_fwd_th_l2r, (void *) l_th_arg)) )
				s_exit_flag = $LOG(STS$K_ERROR, "pthread_create(s_e2e_fwd_th)->%d, errno: %d", l_rc, errno);
			}

		else	{
			if ( (l_rc = pthread_create(&l_tid, NULL, (pthread_func_t) s_e2e_fwd_th, (void *) i)) )
				$LOG(STS$K_ERROR, "pthread_create(s_e2e_fwd_th)->%d, errno: %d", l_rc, errno);
			}

		}


	while ( !l_rc && !s_exit_flag )
		{
		for ( status = 3; (status = sleep(status)); );			/* Hibernate ... */

		/* If logfile size has been set - rewind it ... */
		if ( g_logsize )
			__util$rewindlogfile(g_logsize);


		s_show_stat ();
		}

	s_show_stat ();

	$LOG(STS$K_INFO, "Exiting with exit_flag=%d!", s_exit_flag);
}
