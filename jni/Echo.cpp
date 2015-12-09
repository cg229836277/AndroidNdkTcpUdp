#include <jni.h>

#include "com_apress_echo_EchoServerActivity.h"
#include "com_apress_echo_EchoClientActivity.h"

#include "SocketUtils.h"

//服务端：启动监听
//流程:socket()->listen()->accept()->recv()->send()_close()
void JNICALL Java_com_apress_echo_EchoServerActivity_nativeStartTcpServer(
		JNIEnv *env, jobject obj, jint port) {
	int serverSocket = NewTcpSocket(env, obj);

	if (NULL == env->ExceptionOccurred()) {
		//绑定
		BindSocketToPort(env, obj, serverSocket, (unsigned short) port);
		if (NULL != env->ExceptionOccurred()) {
			goto exit;
		}

		//如果端口是0，打印出当前随机分配的端口
		if (0 == port) {
			GetSocketPort(env, obj, serverSocket);
			if (NULL != env->ExceptionOccurred()) {
				goto exit;
			}
		}

		//监听 链接4
		ListenOnSocket(env, obj, serverSocket, 4);
		if (NULL != env->ExceptionOccurred()) {
			goto exit;
		}

		//
		int clientSocket = AcceptOnSocket(env, obj, serverSocket);
		if (NULL != env->ExceptionOccurred()) {
			goto exit;
		}

		char buffer[MAX_BUFFER_SIZE];
		ssize_t recvSize;
		ssize_t sentSize;

		while (1) {
			//接收
			recvSize = ReceiveFromSocket(env, obj, clientSocket, buffer,
			MAX_BUFFER_SIZE);

			if ((0 == recvSize) || (NULL != env->ExceptionOccurred())) {
				break;
			}

			//发送
			sentSize = SendToSocket(env, obj, clientSocket, buffer,
					(size_t) recvSize);
			if ((0 == sentSize) || (NULL != env->ExceptionOccurred())) {
				break;
			}
		}

		//close the client socket
		close(clientSocket);

	}

	exit: if (serverSocket > 0) {
		close(serverSocket);
	}
}

//客户端：连接
void JNICALL Java_com_apress_echo_EchoClientActivity_nativeStartTcpClient(
		JNIEnv *env, jobject obj, jstring ip, jint port, jstring message) {

	int clientSocket = NewTcpSocket(env, obj);
	if (NULL == env->ExceptionOccurred()) {
		const char* ipAddress = env->GetStringUTFChars(ip, NULL);

		if (NULL == ipAddress) {
			goto exit;
		}
		ConnectToAddress(env, obj, clientSocket, ipAddress,
				(unsigned short) port);
		//释放ip
		env->ReleaseStringUTFChars(ip, ipAddress);

		//connect exception check
		if (NULL != env->ExceptionOccurred()) {
			goto exit;
		}

		const char* messageText = env->GetStringUTFChars(message, NULL);
		if (NULL == messageText) {
			goto exit;
		}

		//这里的size不用release??
		jsize messageSize = env->GetStringUTFLength(message);
		SendToSocket(env, obj, clientSocket, messageText, messageSize);

		//
		env->ReleaseStringUTFChars(message, messageText);

		if (NULL != env->ExceptionOccurred()) {
			goto exit;
		}

		char buffer[MAX_BUFFER_SIZE];

		ReceiveFromSocket(env, obj, clientSocket, buffer, MAX_BUFFER_SIZE);
	}

	exit: if (clientSocket > -1) {
		close(clientSocket);
	}
}

//启动udp服务端
void JNICALL Java_com_apress_echo_EchoServerActivity_nativeStartUdpServer(JNIEnv* env, jobject obj, jint port) {
	// Construct a new UDP socket.
	int serverSocket = NewUdpSocket(env, obj);
	if (NULL == env->ExceptionOccurred()) {
		// Bind socket to a port number
		BindSocketToPort(env, obj, serverSocket, (unsigned short) port);
		if (NULL != env->ExceptionOccurred())
			goto exit;
		// If random port number is requested
		if (0 == port) {
			// Get the port number socket is currently binded
			GetSocketPort(env, obj, serverSocket);
			if (NULL != env->ExceptionOccurred())
				goto exit;
		}
		// Client address
		struct sockaddr_in address;
		memset(&address, 0, sizeof(address));
		char buffer[MAX_BUFFER_SIZE];
		ssize_t recvSize;
		ssize_t sentSize;
		// Receive from the socket
		recvSize = ReceiveDatagramFromSocket(env, obj, serverSocket, &address,
				buffer, MAX_BUFFER_SIZE);
		if ((0 == recvSize) || (NULL != env->ExceptionOccurred()))
			goto exit;
		// Send to the socket
		sentSize = SendDatagramToSocket(env, obj, serverSocket, &address,
				buffer, (size_t) recvSize);
	}
	exit: if (serverSocket > 0) {
		close(serverSocket);
	}
}

JNIEXPORT void JNICALL Java_com_apress_echo_EchoClientActivity_nativeStartUdpClient(
		JNIEnv* env,jobject obj,jstring ip,jint port,jstring message){
	// Construct a new UDP socket.
	int clientSocket = NewUdpSocket(env, obj);
	if (NULL == env->ExceptionOccurred()) {
		struct sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = PF_INET;
		// Get IP address as C string
		const char* ipAddress = env->GetStringUTFChars(ip, NULL);
		if (NULL == ipAddress)
			goto exit;
		// Convert IP address string to Internet address
		int result = inet_aton(ipAddress, &(address.sin_addr));
		// Release the IP address
		env->ReleaseStringUTFChars(ip, ipAddress);
		// If conversion is failed
		if (0 == result)
		{
			// Throw an exception with error number
			ThrowErrnoException(env, "java/io/IOException", errno);
			goto exit;
		}
		// Convert port to network byte order
		address.sin_port = htons(port);
		// Get message as C string
		const char* messageText = env->GetStringUTFChars(message, NULL);
		if (NULL == messageText)
			goto exit;
		// Get the message size
		jsize messageSize = env->GetStringUTFLength(message);
		// Send message to socket
		SendDatagramToSocket(env, obj, clientSocket, &address, messageText,
				messageSize);
		// Release the message text
		env->ReleaseStringUTFChars(message, messageText);
		// If send was not successful
		if (NULL != env->ExceptionOccurred())
			goto exit;
		char buffer[MAX_BUFFER_SIZE];
		// Clear address
		memset(&address, 0, sizeof(address));
		// Receive from the socket
		ReceiveDatagramFromSocket(env, obj, clientSocket, &address, buffer,
				MAX_BUFFER_SIZE);
	}
	exit: if (clientSocket > 0) {
		close(clientSocket);
	}
}
