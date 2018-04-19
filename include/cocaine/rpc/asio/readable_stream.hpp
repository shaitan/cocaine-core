/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_IO_BUFFERED_READABLE_STREAM_HPP
#define COCAINE_IO_BUFFERED_READABLE_STREAM_HPP

#include "cocaine/errors.hpp"
#include "cocaine/memory.hpp"

#include <functional>

#include <asio/io_service.hpp>
#include <asio/basic_stream_socket.hpp>

#include <cstring>

namespace cocaine { namespace io {

template<class Protocol, class Decoder>
class readable_stream:
    public std::enable_shared_from_this<readable_stream<Protocol, Decoder>>
{
    COCAINE_DECLARE_NONCOPYABLE(readable_stream)

    static const size_t kInitialBufferSize = 65536;

    typedef typename Protocol::socket socket_type;

    typedef Decoder decoder_type;
    typedef typename decoder_type::message_type message_type;

    const std::shared_ptr<socket_type> m_socket;

    typedef std::function<void(const std::error_code&)> handler_type;

    std::vector<char, uninitialized<char>> m_ring;
    std::vector<char, uninitialized<char>>::size_type m_rd_offset, m_rx_offset;

    decoder_type m_decoder;

public:
    explicit
    readable_stream(const std::shared_ptr<socket_type>& socket):
        m_socket(socket)
    {
        m_ring.resize(kInitialBufferSize);
        m_rd_offset = m_rx_offset = 0;
    }

    void
    read(message_type& message, handler_type handle) {
        std::error_code ec;

        const size_t
            bytes_pending = m_rd_offset - m_rx_offset,
            bytes_decoded = m_decoder.decode(m_ring.data() + m_rx_offset, bytes_pending, message, ec);

        if(ec != error::insufficient_bytes) {
            if(!ec) {
                m_rx_offset += bytes_decoded;
            }

            return m_socket->get_io_service().post(std::bind(handle, ec));
        }

        if(m_rx_offset) {
            // Compactify the ring before the asynchronous read operation.
            std::memmove(m_ring.data(), m_ring.data() + m_rx_offset, bytes_pending);

            m_rd_offset = bytes_pending;
            m_rx_offset = 0;
        }

        if(bytes_pending * 2 >= m_ring.size()) {
            // The total size of unprocessed data in larger than half the size of the ring, so grow
            // the ring in order to accommodate more data.
            m_ring.resize(m_ring.size() * 2);
        }

        namespace ph = std::placeholders;

        m_socket->async_read_some(
            asio::buffer(m_ring.data() + m_rd_offset, m_ring.size() - m_rd_offset),
            std::bind(&readable_stream::fill, this->shared_from_this(), std::ref(message), handle, ph::_1, ph::_2)
        );
    }

    auto
    pressure() const -> size_t {
        return m_ring.size();
    }

private:
    void
    fill(message_type& message, handler_type handle, const std::error_code& ec, size_t bytes_read) {
        if(ec) {
            if(ec == asio::error::operation_aborted) {
                return;
            }

            return m_socket->get_io_service().post(std::bind(handle, ec));
        }

        m_rd_offset += bytes_read;

        read(std::ref(message), handle);
    }
};

}} // namespace cocaine::io

#endif
