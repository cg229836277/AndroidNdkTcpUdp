#include <stdio.h>
#include <stdarg.h>
//errno
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

//sockaddr_un
#include <sys/un.h>

//htons,sockaddr_in
#include <netinet/in.h>
//inet_ntop
#include <arpa/inet.h>
//close,unlink
#include <unistd.h>
//offsetof
#include <stddef.h>

#ifndef __SOCKET_UTILS__
#define __SOCKET_UTILS_

//MAX log message length
#define MAX_LOG_MESSAGE_LENGTH 256
//MAX data buffer size
#define MAX_BUFFER_SIZE 80

//打印日志到java环境中
static void LogMessage(JNIEnv* env, jobject obj, const char* format, ...) {

	//cache log method ID
	static jmethodID methodID = NULL;
	if (methodID == NULL) {
		jclass clazz = env->GetObjectClass(obj);
		methodID = env->GetMethodID(clazz, "logMessage",
				"(Ljava/lang/String;)V");

		env->DeleteLocalRef(clazz);
	}

	if (methodID != NULL) {
		char buffer[MAX_BUFFER_SIZE];

		//将可变参数输出到字符数组中
		va_list ap;
		va_start(ap, format);
		vsnprintf(buffer, MAX_LOG_MESSAGE_LENGTH, format, ap);
		va_end(ap);

		//转换成java字符串
		jstring message = env->NewStringUTF(buffer);
		if (message != NULL) {
			env->CallVoidMethod(obj, methodID, message);
			env->DeleteLocalRef(message);
		}
	}
}

//通过异常类和异常信息抛出异常
static void ThrowException(JNIEnv* env, const char* className,
		const char* message) {

	jclass clazz = env->FindClass(className);
	if (clazz != NULL) {
		env->ThrowNew(clazz, message);
		env->DeleteLocalRef(clazz);
	}
}

//通过异常类和错误号抛出异常
static void ThrowErrnoException(JNIEnv* env, const char* className,
		int errnum) {

	char buffer[MAX_LOG_MESSAGE_LENGTH];

	//通过错误号获得错误消息
	if (-1 == strerror_r(errnum, buffer, MAX_LOG_MESSAGE_LENGTH)) {
		strerror_r(errno, buffer, MAX_LOG_MESSAGE_LENGTH);
	}

	ThrowException(env, className, buffer);
}

//sock用到的一些公用方法
//创建一个socket:socket()
static int NewTcpSocket(JNIEnv* env, jobject obj) {

	LogMessage(env, obj, "Constructing a new TCP socket...");
	int tcpSocket = socket(PF_INET, SOCK_STREAM, 0);

	if (-1 == tcpSocket) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	}

	return tcpSocket;
}

//绑定 bind()
static void BindSocketToPort(JNIEnv* env, jobject obj, int sd,
		unsigned short port) {
	struct sockaddr_in address;
	//清空结构体
	memset(&address, 0, sizeof(address));

	address.sin_family = PF_INET;
	//Bind to all address
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	//Convert port to network byte order
	address.sin_port = htons(port);
	//Bind socket
	LogMessage(env, obj, "Binding to port %hu.", port);
	//sockaddr方便函数传递, sockaddr_in方便用户设定, 所以需要的时候在这2者之间进行转换
	if (-1 == bind(sd, (struct sockaddr*) &address, sizeof(address))) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	}
}
//返回当前socket绑定的端口
static unsigned short GetSocketPort(JNIEnv* env, jobject obj, int sd) {
	unsigned short port = 0;
	struct sockaddr_in address;
	socklen_t addressLength = sizeof(address);
	if (-1 == getsockname(sd, (struct sockaddr*) &address, &addressLength)) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		port = ntohs(address.sin_port);
		LogMessage(env, obj, "Binding to the random port %hu.", port);
	}
	return port;
}

//监听 listen()
static void ListenOnSocket(JNIEnv*env, jobject obj, int sd, int backlog) {
	LogMessage(env, obj,
			"Listening on socket with a baklog of  %d pending connections.",
			backlog);

	//listen()用来等待参数s 的socket 连线. 参数backlog 指定同时能处理的最大连接要求,
	//如果连接数目达此上限则client 端将收到ECONNREFUSED 的错误.
	//Listen()并未开始接收连线, 只是设置socket 为listen 模式, 真正接收client 端连线的是accept().
	//通常listen()会在socket(), bind()之后调用, 接着才调用accept().

	if (-1 == listen(sd, backlog)) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	}

}

//根据地址打印IP和端口
static void LogAddress(JNIEnv* env, jobject obj, const char* message,
		const struct sockaddr_in* address) {
	char ip[INET_ADDRSTRLEN];

	if (NULL == inet_ntop(PF_INET, &(address->sin_addr), ip, INET_ADDRSTRLEN)) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		unsigned short port = ntohs(address->sin_port);
		LogMessage(env, obj, "%s %s:%hu", message, ip, port);
	}
}

//accept()
static int AcceptOnSocket(JNIEnv* env, jobject obj, int sd) {
	struct sockaddr_in address;
	socklen_t addressLength = sizeof(address);
	LogMessage(env, obj, "Waiting for a client connection...");
	int clientSocket = accept(sd, (struct sockaddr*) &address, &addressLength);
	if (-1 == clientSocket) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		LogAddress(env, obj, "Client connection from ", &address);
	}
	return clientSocket;
}

//接收 recv()
static ssize_t ReceiveFromSocket(JNIEnv* env, jobject obj, int sd, char* buffer,
		size_t bufferSize) {
	LogMessage(env, obj, "Receiving from the socket... ");
	ssize_t recvSize = recv(sd, buffer, bufferSize - 1, 0);

	if (-1 == recvSize) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		//字符串截断
		buffer[recvSize] = NULL;

		if (recvSize > 0) {
			//接收成功，打印
			LogMessage(env, obj, "Received %d bytes:%s", bufferSize, buffer);
		} else {
			LogMessage(env, obj, "Client disconnected.");
		}
	}

	return recvSize;
}

//发送消息:send()
static ssize_t SendToSocket(JNIEnv *env, jobject obj, int sd,
		const char* buffer, size_t bufferSize) {
	LogMessage(env, obj, "Sending to the socket... ");
	ssize_t sentSize = send(sd, buffer, bufferSize, 0);

	if (-1 == sentSize) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		if (sentSize > 0) {
			LogMessage(env, obj, "Send %d bytes: %s", sentSize, buffer);
		} else {
			LogMessage(env, obj, "Client disconnected.");
		}
	}

	return sentSize;
}

//链接到服务器 connect()
static void ConnectToAddress(JNIEnv*env, jobject obj, int sd, const char*ip,
		unsigned short port) {
	LogMessage(env, obj, "Connecting to %s:%hu...", ip, port);

	struct sockaddr_in address;

	memset(&address, 0, sizeof(address));
	address.sin_family = PF_INET;

	//转换ip
	if (0 == inet_aton(ip, &(address.sin_addr))) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		address.sin_port = htons(port);
	}

	if (-1 == connect(sd, (const sockaddr*) &address, sizeof(address))) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		LogMessage(env, obj, "Connected.");
	}

}

//----------------udp

//创建udp socket
static int NewUdpSocket(JNIEnv* env, jobject obj) {

	LogMessage(env, obj, "Constructing a new UDP socket...");
	int udpSocket = socket(PF_INET, SOCK_DGRAM, 0);

	if (-1 == udpSocket) {
		ThrowErrnoException(env, "java/io/IOException", errno);
	}

	return udpSocket;
}

static ssize_t ReceiveDatagramFromSocket(JNIEnv* env,jobject obj,int sd,struct sockaddr_in* address,char* buffer,size_t bufferSize){
	socklen_t addressLength = sizeof(struct sockaddr_in);
	// Receive datagram from socket
	LogMessage(env, obj, "Receiving from the socket...");
	ssize_t recvSize = recvfrom(sd, buffer, bufferSize, 0,
			(struct sockaddr*) address, &addressLength);
	// If receive is failed
	if (-1 == recvSize) {
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else {
		// Log address
		LogAddress(env, obj, "Received from", address);
		// NULL terminate the buffer to make it a string
		buffer[recvSize] = NULL;
		// If data is received
		if (recvSize > 0) {
			LogMessage(env, obj, "Received %d bytes: %s", recvSize, buffer);
		}
	}
	return recvSize;
}

static ssize_t SendDatagramToSocket(JNIEnv* env,jobject obj,int sd,const struct sockaddr_in* address,const char* buffer,size_t bufferSize){
	// Send data buffer to the socket
	LogAddress(env, obj, "Sending to", address);
	ssize_t sentSize = sendto(sd, buffer, bufferSize, 0,
			(const sockaddr*) address, sizeof(struct sockaddr_in));
	// If send is failed
	if (-1 == sentSize) {
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	} else if (sentSize > 0) {
		LogMessage(env, obj, "Sent %d bytes: %s", sentSize, buffer);
	}
	return sentSize;
}

#endif __SOCKET_UTILS_
