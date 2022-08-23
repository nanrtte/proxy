﻿//
// socks_server.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "socks/logging.hpp"
#include "url/url_view.hpp"

#include <memory>
#include <string>
#include <array>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace net = boost::asio;

namespace socks {

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

	struct socks_server_option
	{
		std::string usrdid_;
		std::string passwd_;

		std::string bind_addr_;

		// 多层代理模式, 下一个代理服务器.
		// 例如: socks5://user:passwd@proxy.server.com:1080
		// 默认使用hostname模式, dns解析在远程执行.
		std::string next_proxy_;
	};

	class socks_server_base {
	public:
		virtual ~socks_server_base() {}
		virtual void remove_client(size_t id) = 0;
		virtual const socks_server_option& option() = 0;
	};

	class socks_session
		: public std::enable_shared_from_this<socks_session>
	{
		socks_session(const socks_session&) = delete;
		socks_session& operator=(const socks_session&) = delete;

	public:
		socks_session(tcp::socket&& socket,
			size_t id, std::weak_ptr<socks_server_base> server);
		~socks_session();

	public:
		void start();
		void close();

	private:
		net::awaitable<void> start_socks_proxy();
		net::awaitable<void> socks_connect_v5();
		net::awaitable<void> socks_connect_v4();
		net::awaitable<bool> socks_auth();
		net::awaitable<void> transfer(tcp::socket& from, tcp::socket& to);

		net::awaitable<void> connect_host(
			std::string target_host, uint16_t target_port,
			boost::system::error_code& ec, bool resolve = false);

	private:
		tcp::socket m_local_socket;
		tcp::socket m_remote_socket;
		size_t m_connection_id;
		std::array<char, 2048> m_local_buffer{};
		std::weak_ptr<socks_server_base> m_socks_server;
		socks_server_option m_option;
		std::unique_ptr<urls::url_view> m_next_proxy;
		bool m_abort{ false };
	};

	using socks_session_ptr = std::shared_ptr<socks_session>;
	using socks_session_weak_ptr = std::weak_ptr<socks_session>;


	//////////////////////////////////////////////////////////////////////////

	class socks_server
		: public socks_server_base
		, public std::enable_shared_from_this<socks_server>
	{
		socks_server(const socks_server&) = delete;
		socks_server& operator=(const socks_server&) = delete;

		friend class socks_session;

	public:
		socks_server(net::any_io_executor& executor,
			const tcp::endpoint& endp, socks_server_option opt = {});
		virtual ~socks_server() = default;

	public:
		void start();
		void close();

	private:
		virtual void remove_client(size_t id) override;
		virtual const socks_server_option& option() override;

	private:
		net::awaitable<void> start_socks_listen(tcp::acceptor& a);

	private:
		net::any_io_executor m_executor;
		tcp::acceptor m_acceptor;
		socks_server_option m_option;
		std::unordered_map<size_t, socks_session_weak_ptr> m_clients;
		bool m_abort{ false };
	};

}
