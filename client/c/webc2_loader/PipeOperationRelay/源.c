#include <Windows.h>
#include <stdio.h>

#define PAYLOAD_MAX_SIZE 512 * 1024
#define BUFFER_MAX_SIZE 1024 * 1024

//�ţ�������˼��������Զ���Ĺܵ���beacon�ܵ��ŽӵĽṹ��
struct BRIDGE
{
	HANDLE client;
	HANDLE server;
};

//��beacon��ȡ����
DWORD read_frame(HANDLE my_handle, char* buffer, DWORD max) {

	DWORD size = 0, temp = 0, total = 0;
	/* read the 4-byte length */
	ReadFile(my_handle, (char*)& size, 4, &temp, NULL);
	printf("read_frame length: %d\n", size);
	/* read the whole thing in */
	while (total < size) {
		ReadFile(my_handle, buffer + total, size - total, &temp,
			NULL);
		total += temp;
	}
	return size;
}

//��beaconд������
void write_frame(HANDLE my_handle, char* buffer, DWORD length) {
	printf("write_frame length: %d\n", length);
	DWORD wrote = 0;
	WriteFile(my_handle, (void*)& length, 4, &wrote, NULL);
	printf("write %d bytes.\n", wrote);
	WriteFile(my_handle, buffer, length, &wrote, NULL);
	printf("write %d bytes.\n", wrote);
}

//�ӿ�������ȡ����
DWORD read_client(HANDLE my_handle, char* buffer) {
	DWORD size = 0;
	DWORD readed = 0;
	ReadFile(my_handle, &size, 4, NULL, NULL);
	printf("read_client length: %d\n", size);
	ReadFile(my_handle, buffer, size, &readed, NULL);
	printf("final data from client: %d\n", readed);
	return readed;
}

//�������д������
void write_client(HANDLE my_handle, char* buffer, DWORD length) {
	DWORD wrote = 0;
	WriteFile(my_handle, buffer, length, &wrote, NULL);
	printf("write client total %d data %d\n", wrote, length);
}

//�ͻ��˶��ܵ��������д�ܵ��߼�
DWORD WINAPI ReadOnlyPipeProcess(LPVOID lpvParam) {
	//�������ܵ��ľ��ȡ����
	struct BRIDGE* bridge = (struct BRIDGE*)lpvParam;
	HANDLE hpipe = bridge->client;
	HANDLE beacon = bridge->server;
	
	DWORD length = 0;
	char* buffer = VirtualAlloc(0, BUFFER_MAX_SIZE, MEM_COMMIT, PAGE_READWRITE);
	if (buffer == NULL)
	{
		exit(-1);
	}
	//�ٴ�У��ܵ�
	if ((hpipe == INVALID_HANDLE_VALUE) || (beacon == INVALID_HANDLE_VALUE))
	{
		return FALSE;
	}
	while (TRUE)
	{
		if (ConnectNamedPipe(hpipe, NULL))
		{
			printf("client want read.\n");
			length = read_frame(beacon, buffer, BUFFER_MAX_SIZE);
			printf("read from beacon: %d\n", length);
			//�����δ��ͣ���һ�γ��ȣ��ٷ����ݡ�
			write_client(hpipe,(char *) &length, 4);
			FlushFileBuffers(hpipe);
			write_client(hpipe, buffer, length);
			FlushFileBuffers(hpipe);
			DisconnectNamedPipe(hpipe);
			//��ջ�����
			ZeroMemory(buffer, BUFFER_MAX_SIZE);
			length = 0;
		}

	}

	return 1;
}

//�ͻ���д�ܵ�������˶��ܵ��߼�
DWORD WINAPI WriteOnlyPipeProcess(LPVOID lpvParam) {
	//ȡ�������ܵ�
	struct BRIDGE* bridge = (struct BRIDGE*)lpvParam;
	HANDLE hpipe = bridge->client;
	HANDLE beacon = bridge->server;
	
	DWORD length = 0;
	char* buffer = VirtualAlloc(0, BUFFER_MAX_SIZE, MEM_COMMIT, PAGE_READWRITE);
	if (buffer == NULL)
	{
		exit(-1);
	}
	if ((hpipe == INVALID_HANDLE_VALUE) || (beacon == INVALID_HANDLE_VALUE))
	{
		return FALSE;
	}
	while (TRUE)
	{
		if (ConnectNamedPipe(hpipe, NULL))
		{
			//һ���Զ���һ����д
			printf("client want write.\n");
			length = read_client(hpipe, buffer);
			printf("read from client: %d\n", length);
			write_frame(beacon, buffer, length);
			DisconnectNamedPipe(hpipe);
			//��ջ�����
			ZeroMemory(buffer, BUFFER_MAX_SIZE);
			length = 0;
		}

	}

	return 2;
}

int main(int argc, char* argv[]) {

	//�����ͻ��˶��ܵ�
	HANDLE hPipeRead = CreateNamedPipe("\\\\.\\pipe\\hlread", PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, BUFFER_MAX_SIZE, BUFFER_MAX_SIZE, 0, NULL);
	//�����ͻ���д�ܵ�
	HANDLE hPipeWrite = CreateNamedPipe("\\\\.\\pipe\\hlwrite", PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, BUFFER_MAX_SIZE, BUFFER_MAX_SIZE, 0, NULL);
	//��beacon��������
	HANDLE hfileServer = CreateFileA("\\\\.\\pipe\\hltest", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, NULL);


	//���ܵ��������Ƿ����ɹ�
	if ((hPipeRead == INVALID_HANDLE_VALUE) || (hPipeWrite == INVALID_HANDLE_VALUE) || (hfileServer == INVALID_HANDLE_VALUE))
	{
		if (hPipeRead == INVALID_HANDLE_VALUE)
		{
			printf("error during create readpipe.");
		}
		if (hPipeWrite == INVALID_HANDLE_VALUE)
		{
			printf("error during create writepipe.");
		}
		if (hfileServer == INVALID_HANDLE_VALUE)
		{
			printf("error during connect to beacon.");
		}
		exit(-1);
	}
	else
	{	
		//һ������
		printf("all pipes are ok.\n");
	}

	
	//����ͻ��˶��ܵ���beacon����
	struct BRIDGE readbridge;
	readbridge.client = hPipeRead;
	readbridge.server = hfileServer;
	//�����ͻ��˶��ܵ��߼�
	HANDLE hTPipeRead = CreateThread(NULL, 0, ReadOnlyPipeProcess, (LPVOID)& readbridge, 0, NULL);
	
	//����ͻ���д�ܵ���beacon����
	struct BRIDGE writebridge;
	writebridge.client = hPipeWrite;
	writebridge.server = hfileServer;
	//�����ͻ���д�ܵ��߼�
	HANDLE hTPipeWrite = CreateThread(NULL, 0, WriteOnlyPipeProcess, (LPVOID)& writebridge, 0, NULL);

	//����û��ʲô���壬ֱ��д����ѭ��Ҳ��
	HANDLE waitHandles[] = { hPipeRead,hPipeWrite };
	while (TRUE)
	{
		WaitForMultipleObjects(2, waitHandles, TRUE, INFINITE);
	}

	return 0;

}