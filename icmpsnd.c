#include <stdio.h>

#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>

int
main()
{
	HANDLE hIcmp;
	char *SendData = "ICMP SEND DATA";
	LPVOID ReplyBuffer;
	DWORD dwRetVal;
	DWORD buflen;
	PICMP_ECHO_REPLY pIcmpEchoReply;

	hIcmp = IcmpCreateFile();
          
	buflen = sizeof(ICMP_ECHO_REPLY) + strlen(SendData) + 1;
	ReplyBuffer = (VOID*) malloc(buflen);
	if (ReplyBuffer == NULL) {
		return 1;
	}
	memset(ReplyBuffer, 0, buflen);
	pIcmpEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;

	dwRetVal = IcmpSendEcho(hIcmp, 
				inet_addr("127.0.0.1"), 
				SendData, strlen(SendData), 
				NULL, ReplyBuffer, 
				buflen,
				1000);
	if (dwRetVal != 0) {
		printf("Received %ld messages.\n", dwRetVal);
		printf("\n");
		printf("RTT: %d\n", pIcmpEchoReply->RoundTripTime);
		printf("Data Size: %d\n", pIcmpEchoReply->DataSize);
		printf("Message: %s\n", pIcmpEchoReply->Data);
	} else {
		printf("Call to IcmpSendEcho() failed.\n");
		printf("Error: %ld\n", GetLastError());
	}

	IcmpCloseHandle(hIcmp);

	return 0;
}

