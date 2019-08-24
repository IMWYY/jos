#include "ns.h"
#include "inc/lib.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
	// - read a packet from the network server
	// - send the packet to the device driver
	while (true) {
		int32_t req = ipc_recv(NULL, &nsipcbuf, NULL);
		if (req != NSREQ_OUTPUT) 
		{
			cprintf("output: not a NSREQ_OUTPUT request.");
			continue;
		}

		int r;
		while((r = sys_net_try_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0) 
		{
			if (r != -E_TRANSMIT_RETRY) 
			{
				cprintf("output got weird error %e \n", r);
			}
			sys_yield();
		}
	}

}
