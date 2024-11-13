#define	__MODULE__	"E2E-FWD"
#define	__IDENT__	"X.00-01"
#define	__REV__		"00.01.00"


/*
**++
**
**  FACILITY:  Ethernet-To-Ethernet forwarder
**
**  DESCRIPTION: A simple application to performs fowarding Ethernet packets "as-is" from one <eth>
**	to a yet another one <eth>. This commpact application is for debug\\test purpose only.
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
**--
*/

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
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<poll.h>
#include	<net/if.h>
#include	<netpacket/packet.h>
#include	<net/ethernet.h> /* the L2 protocols */
#include	<sys/ioctl.h>




#define		__FAC__	"E2E-FWD"
#define		__TFAC__ __FAC__ ": "		/* Special prefix for $TRACE			*/
#include	"utility_routines.h"


#ifndef	__ARCH__NAME__
#define	__ARCH__NAME__	"VAX"
#endif




/* Global configuration parameters */
static ASC	q_logfspec = {0},
	q_confspec = {0},
	q_if1 = {0}, q_if2 = {0}
	;


static int	s_exit_flag = 0,							/* Global flag 'all must to be stop'	*/
	g_trace = 0,									/* A flag to produce extensible logging	*/
	g_logsize = 0
	;


static	const	int s_one = 1, s_off = 0;


static const OPTS optstbl [] =
{
	{$ASCINI("config"),	&q_confspec, ASC$K_SZ,	OPTS$K_CONF},

	{$ASCINI("trace"),	&g_trace, 0,		OPTS$K_OPT},
	{$ASCINI("logfile"),	&q_logfspec, ASC$K_SZ,	OPTS$K_STR},
	{$ASCINI("logsize"),	&g_logsize, 0,		OPTS$K_INT},

	{$ASCINI("if1"),	&q_if1,  ASC$K_SZ,	OPTS$K_STR},
	{$ASCINI("if2"),	&q_if2,  ASC$K_SZ,	OPTS$K_STR},

	OPTS_NULL		/* End-of-List marker*/
};



typedef void *(* pthread_func_t) (void *);


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

	if ( !$ASCLEN(&q_if1) )
		return	$LOG(STS$K_ERROR, "Missing IF1");

	if ( 0 > if_nametoindex($ASCPTR(&q_if1)) )
		return	$LOG(STS$K_ERROR, "Cannot translate IF1: <%.*s> to index", 0, $ASC(&q_if1), errno);

	if ( !$ASCLEN(&q_if2) )
		return	$LOG(STS$K_ERROR, "Missing IF2");

	if ( 0 > if_nametoindex($ASCPTR(&q_if2)) )
		return	$LOG(STS$K_ERROR, "Cannot translate IF2: <%.*s> to index", 1, $ASC(&q_if2), errno);


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


	$LOG(STS$K_INFO, "Initialize <%s> ...", a_if_name);

	*a_if_sk = (struct sockaddr_ll)   {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL), .sll_halen = ETH_ALEN};

	a_if_sk->sll_ifindex = if_nametoindex(a_if_name);


	if ( 0 > (*a_if_sd = socket(PF_PACKET, SOCK_RAW,  htons(ETH_P_ALL))) )
		return	$LOG(STS$K_ERROR, "socket()->%d, errno", *a_if_sd, errno);

	if ( 0 > (l_rc = bind(*a_if_sd , (struct sockaddr *) a_if_sk , sizeof(struct sockaddr_ll))) )
		return $LOG(STS$K_ERROR, "bind(#%d, <%s>)->%d, errno=%d", *a_if_sd, a_if_name, l_rc, errno);


	strncpy(l_ifreq.ifr_name, a_if_name, IFNAMSIZ - 1);
	if ( 0 > (l_rc = ioctl(*a_if_sd, SIOCGIFHWADDR, &l_ifreq)) )
		return	$LOG(STS$K_ERROR, "ioctl()->%d, errno", l_rc, errno);
	memcpy(a_if_ha, l_ifreq.ifr_ifru.ifru_hwaddr.sa_data, ETH_ALEN);

	l_mreq.mr_ifindex = a_if_sk->sll_ifindex;
	l_mreq.mr_type = PACKET_MR_PROMISC;

	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &l_mreq, sizeof(l_mreq))) )
	     return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_ADD_MEMBERSHIP, <%s>)->%d, errno=%d", *a_if_sd, a_if_name, errno);

	l_opt = 1;
	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_QDISC_BYPASS, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_QDISC_BYPASS, <%s>)->%d, errno=%d", *a_if_sd, a_if_name, errno);

	if ( 0 > (l_rc = setsockopt(*a_if_sd, SOL_PACKET, PACKET_LOSS, &l_opt, sizeof(l_opt))) )
		return $LOG(STS$K_ERROR, "setsockopt(#%d, PACKET_LOSS, <%s>)->%d, errno=%d", *a_if_sd, a_if_name, errno);


	return	STS$K_SUCCESS;
}




static int	s_e2e_fwd_th (void *a_arg)
{
int	l_if1_sd, l_if2_sd, l_rc, l_len;
struct sockaddr_ll  l_if1_sk = {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL)},
	l_if2_sk = {.sll_family = PF_PACKET, .sll_protocol = htons(ETH_P_ALL)};
struct pollfd l_pfd[2] = {-1};
char	l_buf[2*8192];
char	l_if1_ha[ETH_ALEN], l_if2_ha[ETH_ALEN];


	$LOG(STS$K_INFO, "Starting packet processing ...");


	if ( !(1 & (l_rc = s_init_eth ($ASCPTR(&q_if1), &l_if1_sd, l_if1_ha, &l_if1_sk))) )
		return	$LOG(STS$K_ERROR, "Abort thread execution");

	if ( !(1 & (l_rc = s_init_eth ($ASCPTR(&q_if2), &l_if2_sd, l_if2_ha, &l_if2_sk))) )
		return	$LOG(STS$K_ERROR, "Abort thread execution");


	l_pfd[0].fd = l_if1_sd;
	l_pfd[1].fd = l_if2_sd;

	l_pfd[0].events = l_pfd[1].events = POLLIN;


	while ( !s_exit_flag )
		{
		if ( !(l_rc = poll(l_pfd, 2, 3*1024)) )
			continue;

		assert( l_rc >= 0 );

		if ( l_pfd[0].revents & POLLIN )
			{
			l_rc  = recv(l_pfd[0].fd , l_buf, sizeof(l_buf), 0);
			assert( l_rc >= 0 );

			l_len = l_rc;


			if ( g_trace )
				$DUMPHEX(l_buf, l_len);

			if ( 0 > (l_rc = send(l_pfd[1].fd, l_buf, l_len,  0))  )
				$LOG(STS$K_ERROR, "send(#%d, <%.*s>)->%d, errno=%d", l_pfd[1].fd, $ASC(&q_if2), l_rc, errno);
			}

		if ( l_pfd[1].revents & POLLIN )
			{
			l_rc  = recv(l_pfd[1].fd , l_buf, sizeof(l_buf), 0);
			assert( l_rc >= 0 );

			l_len = l_rc;

			if ( g_trace )
				$DUMPHEX(l_buf, l_len);

			if ( 0 > (l_rc = send(l_pfd[0].fd, l_buf, l_len,  0))  )
				$LOG(STS$K_ERROR, "send(#%d, <%.*s>)->%d, errno=%d", l_pfd[0].fd, $ASC(&q_if1), l_rc, errno);
			}

		}


	return	STS$K_SUCCESS;

}


int	main	(int argc, char **argv)
{
int	status, l_rc;
pthread_t	l_tid;



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

	if ( g_trace )
		__util$showparams(optstbl);


	s_init_sig_handler ();

	if ( !(1 & s_config_validate ()) )
		exit(-1);


	if ( (l_rc = pthread_create(&l_tid, NULL, (pthread_func_t) s_e2e_fwd_th, NULL)) )
		$LOG(STS$K_ERROR, "pthread_create(sw_emul$if1_to_if2_n_if4_th)->%d, errno=%d", l_rc, errno);


	while ( !l_rc && !s_exit_flag )
		{
		for ( status = 13; (status = sleep(status)); );			/* Hibernate ... */

		/* If logfile size has been set - rewind it ... */
		if ( g_logsize )
			__util$rewindlogfile(g_logsize);

		}

	$LOG(STS$K_INFO, "Exiting with exit_flag=%d!", s_exit_flag);
}
