all: ipc_server ipc_client

ipc_server: ipc.c
	gcc -DIPC_TEST_SERVER -o ipc_server ipc.c

ipc_client: ipc.c
	gcc -DIPC_TEST_CLIENT -o ipc_client ipc.c

clean:
	rm -f ipc_client ipc_server
