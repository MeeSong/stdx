﻿#pragma once
#include <stdx/async/task.h>
#include <stdx/async/spin_lock.h>
#include <stdx/io.h>
#include <stdx/env.h>
#ifdef WIN32
#include <WinSock2.h>
#pragma comment(lib,"Ws2_32.lib ")
#endif 
namespace stdx
{
#ifdef WIN32
#define _ThrowWSAError 	auto _ERROR_CODE = WSAGetLastError(); \
						if(_ERROR_CODE != WSA_IO_PENDING)\
						{\
							std::string _ERROR_STR("windows WSA error:");\
							_ERROR_STR.append(std::to_string(_ERROR_CODE));\
							throw std::system_error(std::error_code(_ERROR_CODE,std::system_category()),_ERROR_STR.c_str());\
						}\

	struct _WSAStarter
	{
		WSAData wsa;
		_WSAStarter()
			:wsa()
		{
			if (WSAStartup(MAKEWORD(2, 2), &wsa))
			{
				_ThrowWinError
			}
		}
		~_WSAStarter()
		{
			if (WSACleanup())
			{
				_ThrowWinError
			}
		}
	};
	_WSAStarter _wsastarter;
	struct protocol
	{
		enum
		{
			ip = IPPROTO_IP,
			tcp = IPPROTO_TCP,
			udp = IPPROTO_UDP
		};
	};
	struct socket_type
	{
		enum
		{
			raw = SOCK_RAW,
			stream = SOCK_STREAM,
			dgram = SOCK_DGRAM
		};
	};
	struct addr_family
	{
		enum
		{
			ip = AF_INET,
			ipv6 = AF_INET6
		};
	};

	class network_addr
	{
	public:
		network_addr()=default;
		network_addr(unsigned long ip,const uint16 &port)
		{
			m_handle.sin_family = addr_family::ip;
			m_handle.sin_addr.S_un.S_addr = ip;
			m_handle.sin_port = htons(port);
		}
		network_addr(const char *ip, const uint16 &port)
			:network_addr(inet_addr(ip),port)
		{}
		network_addr(const network_addr &other)
		{
			m_handle = other.m_handle;
		}
		~network_addr() = default;
		operator SOCKADDR_IN* ()
		{
			return &m_handle;
		}

		operator sockaddr*()
		{
			return (sockaddr*)&m_handle;
		}

		network_addr &operator=(const network_addr &other)
		{
			m_handle = other.m_handle;
			return *this;
		}
		const static int addr_len = sizeof(sockaddr);
		network_addr &port(const uint16 &port)
		{
			m_handle.sin_port = htons(port);
			return *this;
		}
		uint16 port() const
		{
			return ntohs(m_handle.sin_port);
		}
		const char *ip() const
		{
			return inet_ntoa(m_handle.sin_addr);
		}
		network_addr &ip(const char *ip)
		{
			m_handle.sin_addr.S_un.S_addr = inet_addr(ip);
			return *this;
		}
	private:
		SOCKADDR_IN m_handle;
	};
	struct network_io_context
	{
		network_io_context()
		{
			std::memset(&m_ol, 0, sizeof(OVERLAPPED));
		}
		~network_io_context() = default;
		WSAOVERLAPPED m_ol;
		SOCKET this_socket;
		network_addr addr;
		WSABUF buffer;
		DWORD size;
		SOCKET target_socket;
		std::function <void(network_io_context*,std::exception_ptr)> *callback;
	};

	struct network_send_event
	{
		network_send_event() = default;
		~network_send_event() = default;
		network_send_event(const network_send_event &other)
			:sock(other.sock)
			, size(other.size)
		{}
		network_send_event(network_send_event &&other)
			:sock(std::move(other.sock))
			, size(std::move(other.size))
		{}
		network_send_event &operator=(const network_send_event &other)
		{
			sock = other.sock;
			size = other.size;
			return *this;
		}
		network_send_event(network_io_context *ptr)
			:sock(ptr->this_socket)
			, size(ptr->size)
		{}
		SOCKET sock;
		size_t size;
	};

	struct network_recv_event 
	{
		network_recv_event() = default;
		~network_recv_event() = default;
		network_recv_event(const network_recv_event &other)
			:sock(other.sock)
			,buffer(other.buffer)
			,size(other.size)
		{}
		network_recv_event(network_recv_event &&other)
			:sock(std::move(other.sock))
			,buffer(other.buffer)
			,size(other.size)
		{}
		network_recv_event &operator=(const network_recv_event &other)
		{
			sock = other.sock;
			buffer = other.buffer;
			size = other.size;
			return *this;
		}
		network_recv_event(network_io_context *ptr)
			:sock(ptr->target_socket)
			,buffer(ptr->buffer.len, ptr->buffer.buf)
			,size(ptr->size)
		{}
		SOCKET sock;
		stdx::buffer buffer;
		size_t size;
	};

	//struct network_accept_event
	//{
	//	network_accept_event() = default;
	//	~network_accept_event() = default;
	//	network_accept_event(const network_accept_event &other)
	//		:accept(other.accept)
	//		,buffer(other.buffer)
	//		,size(other.size)
	//		,addr(other.addr)
	//	{}
	//	network_accept_event &operator=(const network_accept_event &other)
	//	{
	//		accept = other.accept;
	//		buffer = other.buffer;
	//		size = other.size;
	//		addr = other.addr;
	//		return *this;
	//	}
	//	network_accept_event(network_io_context *ptr)
	//		:accept(ptr->target_socket)
	//		,buffer(ptr->buffer.len-((sizeof(sockaddr)+16)*2),(ptr->buffer.buf+(sizeof(sockaddr)+16)*2))
	//		,size(ptr->size)
	//		,addr(ptr->addr)
	//	{}
	//	SOCKET accept;
	//	stdx::buffer buffer;
	//	size_t size;
	//	network_addr addr;
	//};

	class _NetworkIOService
	{
	public:
		using iocp_t = stdx::iocp<network_io_context>;
		_NetworkIOService()
			:m_iocp()
		{}
		_NetworkIOService(const iocp_t &iocp)
			:m_iocp(iocp)
		{}
		delete_copy(_NetworkIOService);
		~_NetworkIOService() = default;
		SOCKET create_socket(const int &addr_family, const int &sock_type, const int &protocol)
		{
			SOCKET sock = ::socket(addr_family,sock_type,protocol);
			if (sock == INVALID_SOCKET)
			{
				_ThrowWSAError
			}
			m_iocp.bind(sock);
			return sock;
		}
		SOCKET create_wsasocket(const int &addr_family,const int &sock_type,const int &protocol)
		{
			SOCKET sock = WSASocket(addr_family, sock_type,protocol, NULL, 0,WSA_FLAG_OVERLAPPED);
			if (sock == INVALID_SOCKET)
			{
				_ThrowWSAError
			}
			m_iocp.bind(sock);
			return sock;
		}
		//发送数据
		void send(SOCKET sock,const char* data,const size_t &size,std::function<void(network_send_event,std::exception_ptr)> &&callback)
		{
			auto *context_ptr = new network_io_context;
			context_ptr->this_socket = sock;
			char *buffer = (char*)std::calloc(sizeof(char), size);
			std::strncpy(buffer, data, size);
			context_ptr->buffer.buf = buffer;
			context_ptr->buffer.len = size;
			auto *call = new std::function <void(network_io_context*, std::exception_ptr)>;
			*call = [callback](network_io_context *context_ptr, std::exception_ptr error)
			{
				if (error)
				{
					std::free(context_ptr->buffer.buf);
					delete context_ptr;
					callback(network_send_event(), error);
					return;
				}
				network_send_event context(context_ptr);
				std::free(context_ptr->buffer.buf);
				delete context_ptr;
				callback(context, nullptr);
			};
			context_ptr->callback = call;
			if (WSASend(sock, &(context_ptr->buffer), 1, &(context_ptr->size), NULL, &(context_ptr->m_ol), NULL) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
			stdx::threadpool::run([](iocp_t iocp) 
			{
				auto *context_ptr = iocp.get();
				std::exception_ptr error(nullptr);
				try
				{
					DWORD flag = 0;
					if (!WSAGetOverlappedResult(context_ptr->this_socket, &(context_ptr->m_ol),&(context_ptr->size), false, &flag))
					{
						//在这里出错
						_ThrowWSAError
					}
				}
				catch (const std::exception&)
				{
					error = std::current_exception();
				}
				auto *call = context_ptr->callback;
				try
				{
					(*call)(context_ptr, error);
				}
				catch (const std::exception&)
				{
				}
				delete call;
			},m_iocp);
		}

		//接收数据
		void recv(SOCKET sock,const size_t &size,std::function<void(network_recv_event,std::exception_ptr)> &&callback) 
		{
			auto *context_ptr = new network_io_context;
			context_ptr->this_socket = sock;
			char *buf = (char*)std::calloc(sizeof(char), size);
			context_ptr->buffer.buf = buf;
			context_ptr->buffer.len = size;
			auto *call = new std::function <void(network_io_context*,std::exception_ptr)>;
			*call = [callback](network_io_context *context_ptr,std::exception_ptr error)
			{
				if (error)
				{
					std::free(context_ptr->buffer.buf);
					delete context_ptr;
					callback(network_recv_event(),error);
					return;
				}
				network_recv_event context(context_ptr);
				delete context_ptr;
				callback(context,std::exception_ptr(nullptr));
			};
			context_ptr->callback = call;

			if (WSARecv(sock, &(context_ptr->buffer), 1, &(context_ptr->size),&(_NetworkIOService::recv_flag), &(context_ptr->m_ol), NULL) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
			stdx::threadpool::run([](iocp_t iocp)
			{
				auto *context_ptr = iocp.get();
				std::exception_ptr error(nullptr);
				try
				{
					DWORD flag = 0;
					if (!WSAGetOverlappedResult(context_ptr->this_socket, &(context_ptr->m_ol), &(context_ptr->size), false,&flag))
					{
						//在这里出错
						_ThrowWSAError
					}
				}
				catch (const std::exception&)
				{
					error = std::current_exception();
				}
				auto *call = context_ptr->callback;
				try
				{
					(*call)(context_ptr, error);
				}
				catch (const std::exception&)
				{
				}
				delete call;
			}, m_iocp);
		}

		void connect(SOCKET sock,stdx::network_addr &addr)
		{
			if (WSAConnect(sock, addr, network_addr::addr_len, NULL, NULL, NULL, NULL) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
		}

		SOCKET accept(SOCKET sock,network_addr &addr)
		{
			int size = network_addr::addr_len;
			SOCKET s = WSAAccept(sock,addr,&size, NULL, NULL);
			if (s == INVALID_SOCKET)
			{
				_ThrowWSAError
			}
			m_iocp.bind(s);
			return s;
		}

		void listen(SOCKET sock,int backlog)
		{
			if (::listen(sock, backlog) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
		}

		void bind(SOCKET sock,network_addr &addr)
		{
			if (::bind(sock, addr,network_addr::addr_len)==SOCKET_ERROR)
			{
				_ThrowWSAError
			}
		}

		void send_to(SOCKET sock,const network_addr &addr,const char *data,const size_t &size,std::function<void(stdx::network_send_event,std::exception_ptr)> &&callback) 
		{
			stdx::network_io_context *context_ptr = new stdx::network_io_context;
			context_ptr->addr = addr;
			context_ptr->this_socket = sock;
			char *buf = (char*)std::calloc(sizeof(char),size);
			context_ptr->buffer.buf = buf;
			context_ptr->buffer.len = size;
			auto *call = new std::function <void(network_io_context*, std::exception_ptr)>;
			*call = [callback](network_io_context *context_ptr, std::exception_ptr error)
			{
				if (error)
				{
					std::free(context_ptr->buffer.buf);
					delete context_ptr;
					callback(network_send_event(), error);
					return;
				}
				network_send_event context(context_ptr);
				std::free(context_ptr->buffer.buf);
				delete context_ptr;
				callback(context, nullptr);
			};
			context_ptr->callback = call;
			if (WSASendTo(sock, &(context_ptr->buffer), 1, &(context_ptr->size),NULL, (context_ptr->addr),network_addr::addr_len, &(context_ptr->m_ol), NULL) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
			stdx::threadpool::run([](iocp_t iocp)
			{
				auto *context_ptr = iocp.get();
				std::exception_ptr error(nullptr);
				try
				{
					DWORD flag = 0;
					if (!WSAGetOverlappedResult(context_ptr->this_socket, &(context_ptr->m_ol), &(context_ptr->size), false, &flag))
					{
						//在这里出错
						_ThrowWSAError
					}
				}
				catch (const std::exception&)
				{
					error = std::current_exception();
				}
				auto *call = context_ptr->callback;
				try
				{
					(*call)(context_ptr, error);
				}
				catch (const std::exception&)
				{
				}
				delete call;
			}, m_iocp);
		}

		void recv_from(SOCKET sock,const network_addr &addr, const size_t &size, std::function<void(network_recv_event, std::exception_ptr)> &&callback) 
		{
			auto *context_ptr = new network_io_context;
			context_ptr->this_socket = sock;
			char *buf = (char*)std::calloc(sizeof(char), size);
			context_ptr->buffer.buf = buf;
			context_ptr->buffer.len = size;
			auto *call = new std::function <void(network_io_context*, std::exception_ptr)>;
			*call = [callback](network_io_context *context_ptr, std::exception_ptr error)
			{
				if (error)
				{
					std::free(context_ptr->buffer.buf);
					delete context_ptr;
					callback(network_recv_event(), error);
					return;
				}
				network_recv_event context(context_ptr);
				delete context_ptr;
				callback(context, std::exception_ptr(nullptr));
			};
			context_ptr->callback = call;
			if (WSARecvFrom(sock, &(context_ptr->buffer), 1, &(context_ptr->size),&(_NetworkIOService::recv_flag),context_ptr->addr,(LPINT)&(network_addr::addr_len), &(context_ptr->m_ol), NULL) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
			stdx::threadpool::run([](iocp_t iocp)
			{
				auto *context_ptr = iocp.get();
				std::exception_ptr error(nullptr);
				try
				{
					DWORD flag = 0;
					if (!WSAGetOverlappedResult(context_ptr->this_socket, &(context_ptr->m_ol), &(context_ptr->size), false, &flag))
					{
						//在这里出错
						_ThrowWSAError
					}
				}
				catch (const std::exception&)
				{
					error = std::current_exception();
				}
				auto *call = context_ptr->callback;
				try
				{
					(*call)(context_ptr, error);
				}
				catch (const std::exception&)
				{
				}
				delete call;
			}, m_iocp);
		}

		void close(SOCKET sock)
		{
			if (::closesocket(sock) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
		}
		
		network_addr &&get_local_addr(SOCKET sock) const
		{
			network_addr addr;
			int len = network_addr::addr_len;
			if (getsockname(sock, addr, &len) == SOCKET_ERROR)
			{
				_ThrowWSAError
			}
			return std::move(addr);
		}

		network_addr &&get_remote_addr(SOCKET sock) const
		{

		}
		//void _GetAcceptEx(SOCKET s, LPFN_ACCEPTEX *ptr)
		//{
		//	GUID id = WSAID_ACCEPTEX;
		//	DWORD buf;
		//	if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &id, sizeof(id), ptr, sizeof(LPFN_ACCEPTEX), &buf, NULL, NULL)==SOCKET_ERROR)
		//	{
		//		_ThrowWSAError
		//	}
		//}
		//void _GetAcceptExSockaddr(SOCKET s, LPFN_GETACCEPTEXSOCKADDRS *ptr)
		//{
		//	GUID id = WSAID_GETACCEPTEXSOCKADDRS;
		//	DWORD buf;
		//	if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &id, sizeof(id), ptr, sizeof(LPFN_GETACCEPTEXSOCKADDRS), &buf, NULL, NULL)==SOCKET_ERROR)
		//	{
		//		_ThrowWSAError
		//	}
		//}
		//network_addr _GetSocketAddrEx(SOCKET sock,void *buffer,const size_t &size)
		//{
		//	if (!get_addr_ex)
		//	{
		//		_GetAcceptExSockaddr(sock,&get_addr_ex);
		//	}
		//	network_addr local;
		//	network_addr remote;
		//	auto local_ptr = (sockaddr*)local;
		//	auto remote_ptr = (sockaddr*)remote;
		//	DWORD len = sizeof(sockaddr);
		//	get_addr_ex(buffer, size, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,&local_ptr,(int*)&len,&remote_ptr,(int*)&len);
		//	return remote;
		//}
		//void _AcceptEx(SOCKET sock,const size_t &buffer_size,std::function<void(network_accept_event,std::exception_ptr)> &&callback,DWORD addr_family= stdx::addr_family::ip,DWORD socket_type=stdx::socket_type::stream,DWORD protocol = stdx::protocol::tcp)
		//{
		//	if (!accept_ex)
		//	{
		//		_GetAcceptEx(sock, &accept_ex);
		//	}
		//	network_io_context *context = new network_io_context;
		//	context->target_socket = WSASocket(addr_family, socket_type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
		//	context->buffer.len = buffer_size+ ((sizeof(sockaddr_in) + 16) * 2);
		//	context->buffer.buf = (char*)std::calloc(sizeof(char), context->buffer.len);
		//	context->this_socket = sock;
		//	auto *call = new std::function <void(network_io_context*, std::exception_ptr)>;
		//	*call = [callback,this,buffer_size](network_io_context *context_ptr, std::exception_ptr error)
		//	{
		//		if (error)
		//		{
		//			std::free(context_ptr->buffer.buf);
		//			delete context_ptr;
		//			callback(network_accept_event(), error);
		//			return;
		//		}
		//		context_ptr->addr = _GetSocketAddrEx(context_ptr->this_socket,(void*)context_ptr->buffer.buf,buffer_size);
		//		network_accept_event context(context_ptr);
		//		delete context_ptr;
		//		callback(context, nullptr);
		//	};
		//	if (!accept_ex(sock,context->target_socket,context->buffer.buf,buffer_size, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,&(context->size),&(context->m_ol)))
		//	{
		//		_ThrowWSAError
		//	}
		//	stdx::threadpool::run([](iocp_t iocp)
		//	{
		//		auto *context_ptr = iocp.get();
		//		std::exception_ptr error(nullptr);
		//		try
		//		{
		//			DWORD flag = 0;
		//			if (!WSAGetOverlappedResult(context_ptr->this_socket, &(context_ptr->m_ol), &(context_ptr->size), false, &flag))
		//			{
		//				//在这里出错
		//				_ThrowWSAError
		//			}
		//		}
		//		catch (const std::exception&)
		//		{
		//			error = std::current_exception();
		//		}
		//		auto *call = context_ptr->callback;
		//		try
		//		{
		//			(*call)(context_ptr, error);
		//		}
		//		catch (const std::exception&)
		//		{
		//		}
		//		delete call;
		//	}, m_iocp);
		//}
	private:
		iocp_t m_iocp;
		static DWORD recv_flag;
		//static LPFN_ACCEPTEX accept_ex;
		//static LPFN_GETACCEPTEXSOCKADDRS get_addr_ex;
	};
	DWORD _NetworkIOService::recv_flag = 0;

	class network_io_service
	{
		using iocp_t = _NetworkIOService::iocp_t;
		using impl_t = std::shared_ptr<_NetworkIOService>;
	public:
		network_io_service()
			:m_impl(std::make_shared<_NetworkIOService>())
		{}

		network_io_service(const iocp_t &iocp)
			:m_impl(std::make_shared<_NetworkIOService>(iocp))
		{}

		network_io_service(const network_io_service &other)
			:m_impl(other.m_impl)
		{}

		network_io_service(network_io_service &&other)
			:m_impl(std::move(other.m_impl))
		{}

		network_io_service &operator=(const network_io_service &other)
		{
			m_impl = other.m_impl;
			return *this;
		}

		~network_io_service() = default;

		SOCKET create_socket(const int &addr_family,const int &sock_type,const int &protocol)
		{
			return m_impl->create_wsasocket(addr_family, sock_type, protocol);
		}

		void send(SOCKET sock, const char* data, const size_t &size, std::function<void(network_send_event, std::exception_ptr)> &&callback) 
		{
			m_impl->send(sock, data, size, std::move(callback));
		}

		void recv(SOCKET sock, const size_t &size, std::function<void(network_recv_event, std::exception_ptr)> &&callback) 
		{
			m_impl->recv(sock, size, std::move(callback));
		}

		void connect(SOCKET sock, stdx::network_addr &addr)
		{
			m_impl->connect(sock, addr);
		}

		SOCKET accept(SOCKET sock, network_addr &addr)
		{
			return m_impl->accept(sock, addr);
		}

		void listen(SOCKET sock, int backlog)
		{
			m_impl->listen(sock, backlog);
		}

		void bind(SOCKET sock, network_addr &addr)
		{
			m_impl->bind(sock, addr);
		}

		void send_to(SOCKET sock, const network_addr &addr, const char *data, const size_t &size, std::function<void(stdx::network_send_event, std::exception_ptr)> &&callback)
		{
			m_impl->send_to(sock, addr, data, size, std::move(callback));
		}

		void recv_from(SOCKET sock, const network_addr &addr, const size_t &size, std::function<void(network_recv_event, std::exception_ptr)> &&callback)
		{
			m_impl->recv_from(sock, addr, size, std::move(callback));
		}

		void close(SOCKET sock)
		{
			m_impl->close(sock);
		}

		network_addr &&get_local_addr(SOCKET sock) const
		{
			return std::move(m_impl->get_local_addr(sock));
		}

		network_addr &&get_remote_addr(SOCKET sock) const
		{
			return std::move(m_impl->get_remote_addr(sock));
		}

		operator bool() const
		{
			return (bool)m_impl;
		}
	private:
		impl_t m_impl;
	};
	class _Socket
	{
		using io_service_t = network_io_service;
	public:
		_Socket(const io_service_t &io_service,const int &addr_family,const int &sock_type,const int &protocol)
			:m_io_service(io_service)
			,m_handle(m_io_service.create_socket(addr_family,sock_type,protocol))
		{}

		explicit _Socket(const io_service_t &io_service,SOCKET s)
			:m_io_service(io_service)
			,m_handle(s)
		{}

		delete_copy(_Socket);

		~_Socket() = default;

		stdx::task<stdx::network_send_event> send(const char *data, const size_t &size)
		{
			if (!m_io_service)
			{
				throw std::logic_error("this io service has been free");
			}
			stdx::promise_ptr<network_send_event> promise = stdx::make_promise_ptr<network_send_event>();
			stdx::task<network_send_event> task([promise]() 
			{
				return promise->get_future().get();
			});
			m_io_service.send(m_handle, data, size, [promise,task](stdx::network_send_event context,std::exception_ptr error) mutable
			{
				if (error)
				{
					promise->set_exception(error);
				}
				else
				{
					promise->set_value(context);
				}
				task.run_on_this_thread();
			});
			return task;
		}

		stdx::task<stdx::network_send_event> send_to(const network_addr &addr,const char *data, const size_t &size)
		{
			if (!m_io_service)
			{
				throw std::logic_error("this io service has been free");
			}
			stdx::promise_ptr<network_send_event> promise = stdx::make_promise_ptr<network_send_event>();
			stdx::task<network_send_event> task([promise]()
			{
				return promise->get_future().get();
			});
			m_io_service.send_to(m_handle,addr, data, size, [promise, task](stdx::network_send_event context, std::exception_ptr error) mutable
			{
				if (error)
				{
					promise->set_exception(error);
				}
				else
				{
					promise->set_value(context);
				}
				task.run_on_this_thread();
			});
			return task;
		}

		stdx::task<stdx::network_recv_event> recv(const size_t &size)
		{
			if (!m_io_service)
			{
				throw std::logic_error("this io service has been free");
			}
			stdx::promise_ptr<network_recv_event> promise = stdx::make_promise_ptr<network_recv_event>();
			stdx::task<network_recv_event> task([promise]()
			{
				return promise->get_future().get();
			});
			m_io_service.recv(m_handle, size, [promise, task](stdx::network_recv_event context, std::exception_ptr error) mutable
			{
				if (error)
				{
					promise->set_exception(error);
				}
				else
				{
					promise->set_value(context);
				}
				task.run_on_this_thread();
			});
			return task;
		}

		stdx::task<stdx::network_recv_event> recv_from(const network_addr &addr,const size_t &size)
		{
			if (!m_io_service)
			{
				throw std::logic_error("this io service has been free");
			}
			stdx::promise_ptr<network_recv_event> promise = stdx::make_promise_ptr<network_recv_event>();
			stdx::task<network_recv_event> task([promise]()
			{
				return promise->get_future().get();
			});
			m_io_service.recv_from(m_handle,addr, size, [promise, task](stdx::network_recv_event context, std::exception_ptr error) mutable
			{
				if (error)
				{
					promise->set_exception(error);
				}
				else
				{
					promise->set_value(context);
				}
				task.run_on_this_thread();
			});
			return task;
		}

		void bind(network_addr &addr)
		{
			m_io_service.bind(m_handle, addr);
		}

		void listen(int backlog)
		{
			m_io_service.listen(m_handle, backlog);
		}

		SOCKET accept(network_addr &addr)
		{
			return m_io_service.accept(m_handle,addr);
		}

		void close()
		{
			m_io_service.close(m_handle);
		}

		void connect(network_addr &addr)
		{
			m_io_service.connect(m_handle, addr);
		}

		io_service_t io_service() const
		{
			return m_io_service;
		}

		network_addr &&local_addr() const
		{
			return std::move(m_io_service.get_local_addr(m_handle));
		}

		network_addr &&remote_addr() const
		{
			return std::move(m_io_service.get_remote_addr(m_handle));
		}
	private:
		io_service_t m_io_service;
		SOCKET m_handle;
	};
	class socket
	{
		using impl_t = std::shared_ptr<_Socket>;
		using self_t = socket;
		using io_service_t = network_io_service;
	public:
		socket(const io_service_t &io_service, const int &addr_family, const int &sock_type, const int &protocol)
			:m_impl(std::make_shared<_Socket>(io_service,addr_family,sock_type,protocol))
		{}
		socket(const self_t &other)
			:m_impl(other.m_impl)
		{}
		socket(self_t &&other)
			:m_impl(std::move(other.m_impl))
		{}
		self_t &operator=(const self_t &other)
		{
			m_impl = other.m_impl;
			return *this;
		}

		void bind(network_addr &addr)
		{
			m_impl->bind(addr);
		}

		void listen(int backlog)
		{
			m_impl->listen(backlog);
		}

		self_t &&accept()
		{
			network_addr addr;
			SOCKET s = m_impl->accept(addr);
			return std::move(socket(m_impl->io_service(),s));
		}

		void close()
		{
			m_impl->close();
		}

		void connect(network_addr &addr)
		{
			m_impl->connect(addr);
		}

		network_addr &&local_addr() const
		{
			return std::move(m_impl->local_addr());
		}

		network_addr &&remote_addr() const
		{
			return std::move(m_impl->remote_addr());
		}

		stdx::task<network_send_event> send(const char *data, const size_t &size)
		{
			return m_impl->send(data, size);
		}

		stdx::task<network_send_event> send_to(const network_addr &addr,const char *data, const size_t &size)
		{
			return m_impl->send_to(addr,data, size);
		}

		stdx::task<network_recv_event> recv(const size_t &size)
		{
			return m_impl->recv(size);
		}

		stdx::task<network_recv_event> recv_from(const network_addr &addr,const size_t &size)
		{
			return m_impl->recv_from(addr,size);
		}

	private:
		impl_t m_impl;
		socket(const io_service_t &io_service, SOCKET s)
			:m_impl(std::make_shared<_Socket>(io_service,s))
		{}
	};
#endif //Win32
#ifdef LINUX

#endif //LINUX


}
