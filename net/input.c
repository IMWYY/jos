#include "ns.h"
#include "inc/lib.h"

extern union Nsipc nsipcbuf;

void sleep(uint32_t t) {
	uint32_t stop = sys_time_msec() + t;
	int ret;
	while ((ret = sys_time_msec()) < stop && ret >= 0) {
		sys_yield();
	}

	if (ret < 0) panic("input: sys_time_msec got error: %e \n", ret);
}

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	// cprintf("input curenvid: %d, nsipcbuf pagenum 0x%x, perm 0x%x\n", sys_getenvid(), 
	// 	PGNUM((uintptr_t)&nsipcbuf), uvpt[PGNUM(&nsipcbuf)] & PTE_SYSCALL);
    // cprintf("inpput nsipcbuf %08x, &buf %08x, &len %08x \n", &nsipcbuf, nsipcbuf.pkt.jp_data, &nsipcbuf.pkt.jp_len);

 	// size_t len;
  	// char buf[1518];
	while (true) {
		// in case it's a COW page, we invoke a page fault 
		// so that we can have PTE_W perm.
		nsipcbuf.pkt.jp_len = 0;

		int ret = sys_net_recv(nsipcbuf.pkt.jp_data, (size_t *)&nsipcbuf.pkt.jp_len);
		// int ret = sys_net_recv(buf, &len);
		if (ret < 0) {
			if (ret != -E_RECEIVE_RETRY) {
				cprintf("input got error: %e \n", ret);
			}
			continue;
		}
		// memcpy(nsipcbuf.pkt.jp_data, buf, len);
    	// nsipcbuf.pkt.jp_len = len;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_U | PTE_W);
		sleep(50);
	}
}
