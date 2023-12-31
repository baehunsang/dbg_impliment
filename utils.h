
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/ioctl.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nf_tables.h>

#include <libmnl/libmnl.h>
#include <libnftnl/rule.h>
#include <libnftnl/expr.h>
#include <libnftnl/table.h>
#include <libnftnl/chain.h>

void pin_cpu(int cpu)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &set)) {
		printf("error\n");
		exit(-1);
	}
}

bool create_table(uint32_t protocol, char * table_name, bool delete){
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	uint32_t portid, seq, table_seq, chain_seq, family;
	struct nftnl_table *t;
	struct mnl_nlmsg_batch *batch;
	int ret, batching;

	t = nftnl_table_alloc();
	if (t == NULL) {
		perror("nftnl_table_alloc");
		return false;
	}

	nftnl_table_set_u32(t, NFTNL_TABLE_FAMILY, protocol);  
	nftnl_table_set_str(t, NFTNL_TABLE_NAME, table_name);

	batching = nftnl_batch_is_supported();
	if (batching < 0) {
		perror("cannot talk to nfnetlink");
		return false;
	}

	seq = time(NULL);
	batch = mnl_nlmsg_batch_start(buf, sizeof(buf));

	if (batching) {
		nftnl_batch_begin(mnl_nlmsg_batch_current(batch), seq++);
		mnl_nlmsg_batch_next(batch);
	}

	table_seq = seq;


	nlh = nftnl_table_nlmsg_build_hdr(mnl_nlmsg_batch_current(batch),
			delete?NFT_MSG_DELTABLE:NFT_MSG_NEWTABLE, NFPROTO_IPV4,
			NLM_F_ACK, seq++);
	nftnl_table_nlmsg_build_payload(nlh, t);
	nftnl_table_free(t);
	mnl_nlmsg_batch_next(batch);

	if (batching) {
		nftnl_batch_end(mnl_nlmsg_batch_current(batch), seq++);
		mnl_nlmsg_batch_next(batch);
	}
	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return false;
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		return false;
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, mnl_nlmsg_batch_head(batch),
				mnl_nlmsg_batch_size(batch)) < 0) {
		perror("mnl_socket_send");
		return false;
	}

	mnl_nlmsg_batch_stop(batch);

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, table_seq, portid, NULL, NULL);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		return false;
	}
	mnl_socket_close(nl);

	return true;
}




bool create_chain(char * table_name, char * chain_name, uint32_t hook_num){  // NF_INET_LOCAL_IN
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	uint32_t portid, seq, chain_seq;
	int ret, family;
	struct nftnl_chain *t;
	struct mnl_nlmsg_batch *batch;
	int batching;

	t = nftnl_chain_alloc();
	if (t == NULL)
		return false;
	nftnl_chain_set_str(t, NFTNL_CHAIN_TABLE, table_name);
	nftnl_chain_set_str(t, NFTNL_CHAIN_NAME, chain_name);

	if(hook_num != 0)
		nftnl_chain_set_u32(t, NFTNL_CHAIN_HOOKNUM, hook_num);
	nftnl_chain_set_u32(t, NFTNL_CHAIN_PRIO, 0);

	batching = nftnl_batch_is_supported();
	if (batching < 0) {
		perror("cannot talk to nfnetlink");
		return false;
	}

	seq = time(NULL);
	batch = mnl_nlmsg_batch_start(buf, sizeof(buf));

	if (batching) {
		nftnl_batch_begin(mnl_nlmsg_batch_current(batch), seq++);
		mnl_nlmsg_batch_next(batch);
	}

	chain_seq = seq;
	nlh = nftnl_chain_nlmsg_build_hdr(mnl_nlmsg_batch_current(batch),
			NFT_MSG_NEWCHAIN, NFPROTO_IPV4,
			NLM_F_ACK, seq++);
	nftnl_chain_nlmsg_build_payload(nlh, t);
	nftnl_chain_free(t);
	mnl_nlmsg_batch_next(batch);

	if (batching) {
		nftnl_batch_end(mnl_nlmsg_batch_current(batch), seq++);
		mnl_nlmsg_batch_next(batch);
	}

	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return false;
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		return false;
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, mnl_nlmsg_batch_head(batch),
				mnl_nlmsg_batch_size(batch)) < 0) {
		perror("mnl_socket_send");
		return false;
	}

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, chain_seq, portid, NULL, NULL);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		return false;
	}
	mnl_socket_close(nl);

	return true;

}

bool delete_chain(char * table_name, char * chain_name){ 
	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	uint32_t portid, seq, chain_seq;
	int ret, family;
	struct nftnl_chain *t;
	struct mnl_nlmsg_batch *batch;
	int batching;

	t = nftnl_chain_alloc();
	if (t == NULL)
		return false;
	nftnl_chain_set_str(t, NFTNL_CHAIN_TABLE, table_name);
	nftnl_chain_set_str(t, NFTNL_CHAIN_NAME, chain_name);

	seq = time(NULL);
	batch = mnl_nlmsg_batch_start(buf, sizeof(buf));
    nftnl_batch_begin(mnl_nlmsg_batch_current(batch), seq++);
	mnl_nlmsg_batch_next(batch);

	chain_seq = seq;
    nlh = nftnl_chain_nlmsg_build_hdr(mnl_nlmsg_batch_current(batch),
					NFT_MSG_DELCHAIN, NFPROTO_IPV4,
					NLM_F_ACK, seq++);

	nftnl_chain_nlmsg_build_payload(nlh, t);
	nftnl_chain_free(t);
	mnl_nlmsg_batch_next(batch);

	
    nftnl_batch_end(mnl_nlmsg_batch_current(batch), seq++);
    mnl_nlmsg_batch_next(batch);


	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return false;
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		return false;
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, mnl_nlmsg_batch_head(batch),
				mnl_nlmsg_batch_size(batch)) < 0) {
		perror("mnl_socket_send");
		return false;
	}

    mnl_nlmsg_batch_stop(batch);

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, chain_seq, portid, NULL, NULL);
		if (ret <= 0)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (ret == -1) {
		perror("error");
		return false;
	}
	mnl_socket_close(nl);

	return true;

}

bool create_rule(struct nftnl_rule * r)
{
	struct mnl_socket *nl;
	struct nlmsghdr *nlh;
	struct mnl_nlmsg_batch *batch;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	uint32_t seq = time(NULL);
	int ret;

	nl = mnl_socket_open(NETLINK_NETFILTER);
	if (nl == NULL) {
		perror("mnl_socket_open");
		return false;
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		return false;
	}

	batch = mnl_nlmsg_batch_start(buf, sizeof(buf));

	nftnl_batch_begin(mnl_nlmsg_batch_current(batch), seq++);
	mnl_nlmsg_batch_next(batch);

	nlh = nftnl_rule_nlmsg_build_hdr(mnl_nlmsg_batch_current(batch),
			NFT_MSG_NEWRULE,
			nftnl_rule_get_u32(r, NFTNL_RULE_FAMILY),
			NLM_F_APPEND|NLM_F_CREATE|NLM_F_ACK, seq++);

	nftnl_rule_nlmsg_build_payload(nlh, r);
	nftnl_rule_free(r);
	mnl_nlmsg_batch_next(batch);

	nftnl_batch_end(mnl_nlmsg_batch_current(batch), seq++);
	mnl_nlmsg_batch_next(batch);

	ret = mnl_socket_sendto(nl, mnl_nlmsg_batch_head(batch),
			mnl_nlmsg_batch_size(batch));
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return false;
	}

	mnl_nlmsg_batch_stop(batch);

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));

	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return false;
	}

	ret = mnl_cb_run(buf, ret, 0, mnl_socket_get_portid(nl), NULL, NULL);
	if (ret < 0) {
		perror("mnl_cb_run");
		return false;
	}

	mnl_socket_close(nl);

	return true;
}


static void add_meta(struct nftnl_rule *r, uint32_t key, uint32_t dreg)
{
	struct nftnl_expr *e;
	e = nftnl_expr_alloc("meta");
	if (e == NULL) {
		perror("expr payload oom");
		exit(EXIT_FAILURE);
	}
	nftnl_expr_set_u32(e, NFTNL_EXPR_META_KEY, key);
	nftnl_expr_set_u32(e, NFTNL_EXPR_META_DREG, dreg);

	nftnl_rule_add_expr(r, e);
}

static void add_cmp(struct nftnl_rule *r, uint32_t sreg, uint32_t op,
		const void *data, uint32_t data_len)
{
	struct nftnl_expr *e;

	e = nftnl_expr_alloc("cmp");
	if (e == NULL) {
		perror("expr cmp oom");
		exit(EXIT_FAILURE);
	}

	nftnl_expr_set_u32(e, NFTNL_EXPR_CMP_SREG, sreg);
	nftnl_expr_set_u32(e, NFTNL_EXPR_CMP_OP, op);
	nftnl_expr_set(e, NFTNL_EXPR_CMP_DATA, data, data_len);

	nftnl_rule_add_expr(r, e);
}

static void add_payload(struct nftnl_rule *r, uint32_t base, uint32_t sreg, uint32_t dreg, uint32_t offset, uint32_t len)
{
	struct nftnl_expr *e;
	e = nftnl_expr_alloc("payload");
	if (e == NULL) {
		perror("expr payload oom");
		exit(EXIT_FAILURE);
	}
	nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_BASE, base);

	if(sreg != 0)
		nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_SREG, sreg);
	if(dreg != 0)
		nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_DREG, dreg);

	nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_OFFSET, offset);
	nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_LEN, len);

	nftnl_rule_add_expr(r, e);
}

////////////////////////////////edit//////////////////////////////////////////////////////////
static void add_payload_set(struct nftnl_rule *r, uint32_t base, uint32_t offset, uint32_t len, uint32_t sreg){
	struct nftnl_expr* e;
	e = nftnl_expr_alloc("payload");

	nftnl_expr_set_u32(e, NFTA_PAYLOAD_OFFSET, offset);
	nftnl_expr_set_u32(e, NFTA_PAYLOAD_LEN, len);
	nftnl_expr_set_u32(e, NFTA_PAYLOAD_SREG, sreg);
	nftnl_expr_set_u32(e, NFTA_PAYLOAD_BASE, base);
	nftnl_expr_set_u32(e, NFTA_PAYLOAD_CSUM_TYPE, NFT_PAYLOAD_CSUM_NONE);

	nftnl_rule_add_expr(r, e);

}
/////////////////////////////////////////////////////////////////////////////

int add_verdict(struct nftnl_rule *r, int verdict, char * chain, u_int32_t dreg)
{
	struct nftnl_expr *e;

	e = nftnl_expr_alloc("immediate");
	if (e == NULL) {
		perror("expr payload oom");
		exit(EXIT_FAILURE);
	}

	nftnl_expr_set_u32(e, NFTNL_EXPR_IMM_DREG, dreg);
	nftnl_expr_set_u32(e, NFTNL_EXPR_IMM_VERDICT, verdict);

	if(chain)
		nftnl_expr_set_str(e, NFTNL_EXPR_IMM_CHAIN, chain);

	nftnl_rule_add_expr(r, e);
	return 0;
}

void write_file(const char *filename, char *text) {

    int fd = open(filename, O_RDWR);

    write(fd, text, strlen(text));
    close(fd);
}

void new_ns(void) {

    uid_t uid = getuid();
    gid_t gid = getgid();
    char buffer[0x100];

    if (unshare(CLONE_NEWUSER | CLONE_NEWNS)) {
        perror(" [-] unshare(CLONE_NEWUSER | CLONE_NEWNS)");
		exit(EXIT_FAILURE);
	}

    if (unshare(CLONE_NEWNET)){
        perror(" [-] unshare(CLONE_NEWNET)");
		exit(EXIT_FAILURE);
	}
       
    write_file("/proc/self/setgroups", "deny");
    snprintf(buffer, sizeof(buffer), "0 %d 1", uid);
    write_file("/proc/self/uid_map", buffer);
    snprintf(buffer, sizeof(buffer), "0 %d 1", gid);
    write_file("/proc/self/gid_map", buffer);
}

#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 16
#endif

void hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
	{
		/* print offset */
		if(i % HEXDUMP_COLS == 0)
		{
			printf("0x%06x: ", i);
		}

		/* print hex data */
		if(i < len)
		{
			printf("%02x ", 0xFF & ((char*)mem)[i]);
		}
		else /* end of block, just aligning for ASCII dump */
		{
			printf("   ");
		}

		/* print ASCII dump */
		if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
		{
			for(j = i - (HEXDUMP_COLS - 1); j <= i; j++)
			{
				if(j >= len) /* end of block, not really printing */
				{
					putchar(' ');
				}
				else if(isprint(((char*)mem)[j])) /* printable char */
				{
					putchar(0xFF & ((char*)mem)[j]);        
				}
				else /* other char */
				{
					putchar('.');
				}
			}
			putchar('\n');
		}
	}
}
