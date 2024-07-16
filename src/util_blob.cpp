/*
 * DeckAssistant - Creating control panels using scripts.
 * Copyright (C) 2024  Esther Dalhuisen (Wake of Luna)
 *
 * DeckAssistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DeckAssistant is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "util_blob.h"
#include "util_text.h"
#include <cassert>
#include <cstring>
#include <random>

#define CLEAR_MEMORY

#if (defined HAVE_GNUTLS)
#include <gnutls/crypto.h>
#elif (defined HAVE_OPENSSL)
#include <openssl/sha.h>
#endif

using namespace util;

namespace
{

char nibble_to_base64(unsigned char ch)
{
	if (ch < 26)
		return ch + 'A';
	else if (ch < 52)
		return ch + 'a' - 26;
	else if (ch < 62)
		return ch + '0' - 52;
	else if (ch == 62)
		return '+';
	else
		return '/';
}

unsigned char base64_to_nibble(char ch, bool& ok)
{
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A';
	else if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 26;
	else if (ch >= '0' && ch <= '9')
		return ch - '0' + 52;
	else if (ch == '+')
		return 62;
	else if (ch == '/')
		return 63;
	ok = false;
	return 0;
}

inline void blob_memclear(unsigned char* from, unsigned char* to)
{
#ifdef CLEAR_MEMORY
	if (to > from)
		std::memset(from, 0, to - from);
#endif
}

} // namespace

BlobView::BlobView()
{
	m_data = nullptr;
	m_end  = nullptr;
}

BlobView::BlobView(char const* initial)
{
	if (!initial)
	{
		m_data = nullptr;
		m_end  = nullptr;
	}
	else
	{
		m_data = (unsigned char*)initial;
		m_end  = m_data + std::strlen(initial);
	}
}

BlobView::BlobView(unsigned char const* initial, std::size_t initial_size)
{
	if (!initial)
	{
		m_data = nullptr;
		m_end  = nullptr;
	}
	else
	{
		m_data = (unsigned char*)initial;
		m_end  = m_data + initial_size;
	}
}

BlobView::BlobView(std::string_view const& initial)
{
	if (initial.empty())
	{
		m_data = nullptr;
		m_end  = nullptr;
	}
	else
	{
		m_data = (unsigned char*)initial.data();
		m_end  = (unsigned char*)initial.data() + initial.size();
	}
}

BlobView::BlobView(std::string const& initial)
{
	if (initial.empty())
	{
		m_data = nullptr;
		m_end  = nullptr;
	}
	else
	{
		m_data = (unsigned char*)initial.data();
		m_end  = (unsigned char*)initial.data() + initial.size();
	}
}

BlobView::BlobView(BlobView&& other) = default;
BlobView::BlobView(BlobView const&)  = default;

std::string_view BlobView::to_bin() const
{
	return std::string_view((char const*)m_data, m_end - m_data);
}

std::string BlobView::to_hex() const
{
	std::size_t const len = size();

	std::string result(len * 2, '0');
	char* dest = result.data();

	for (std::size_t i = 0; i < len; ++i)
	{
		char_to_hex(m_data[i], dest);
		dest += 2;
	}

	return result;
}

std::string BlobView::to_base64() const
{
	std::size_t const len = size();

	std::string result(((len + 2) / 3) * 4, '?');
	char* dest = result.data();

	for (std::size_t i = 0; i < len;)
	{
		std::size_t const remaining = len - i;

		unsigned char const ch1 = m_data[i];
		++i;
		unsigned char const ch2 = (i < len) ? m_data[i] : 0;
		++i;
		unsigned char const ch3 = (i < len) ? m_data[i] : 0;
		++i;

		unsigned char const v1 = ch1 >> 2;
		unsigned char const v2 = ((ch1 & 0x03) << 4) + (ch2 >> 4);
		unsigned char const v3 = ((ch2 & 0x0f) << 2) + (ch3 >> 6);
		unsigned char const v4 = (ch3 & 0x3f);

		*dest++ = nibble_to_base64(v1);
		*dest++ = nibble_to_base64(v2);

		if (remaining == 1)
		{
			*dest++ = '=';
			*dest++ = '=';
		}
		else if (remaining == 2)
		{
			*dest++ = nibble_to_base64(v3);
			*dest++ = '=';
		}
		else
		{
			*dest++ = nibble_to_base64(v3);
			*dest++ = nibble_to_base64(v4);
		}
	}

	return result;
}

#if (defined HAVE_GNUTLS)
Blob BlobView::sha1() const
{
	Blob blob(20);

	gnutls_hash_fast(GNUTLS_DIG_SHA1, m_data, m_end - m_data, blob.data());
	blob.m_end += 20;

	return blob;
}

Blob BlobView::sha256() const
{
	Blob blob(32);

	gnutls_hash_fast(GNUTLS_DIG_SHA256, m_data, m_end - m_data, blob.data());
	blob.m_end += 32;

	return blob;
}
#elif (defined HAVE_OPENSSL)
Blob BlobView::sha1() const
{
	Blob blob(20);

	SHA1(m_data, m_end - m_data, blob.data());
	blob.m_end += 20;

	return blob;
}

Blob BlobView::sha256() const
{
	Blob blob(32);

	SHA256(m_data, m_end - m_data, blob.data());
	blob.m_end += 32;

	return blob;
}
#endif

BlobView& BlobView::operator=(BlobView&&)      = default;
BlobView& BlobView::operator=(BlobView const&) = default;

std::strong_ordering BlobView::operator<=>(BlobView const& blob) const
{
	if (m_data == blob.m_data)
		return std::strong_ordering::equal;

	std::size_t const my_size    = size();
	std::size_t const their_size = blob.size();

	std::size_t cmp_len = std::min(my_size, their_size);
	if (cmp_len > 0)
	{
		int mem_result = std::memcmp(m_data, blob.m_data, cmp_len);
		if (mem_result < 0)
			return std::strong_ordering::less;
		else if (mem_result > 0)
			return std::strong_ordering::greater;
	}

	return my_size <=> their_size;
}

bool BlobView::operator!=(BlobView const& blob) const
{
	if (size() != blob.size())
		return true;

	return (*this <=> blob) != std::strong_ordering::equal;
}

Blob::Blob(std::size_t reserve)
    : BlobView()
    , m_capacity(nullptr)
{
	if (reserve > 0)
	{
		m_data     = (unsigned char*)std::malloc(reserve);
		m_end      = m_data;
		m_capacity = m_data + reserve;
		assert(m_data && "Blob out of memory");
		blob_memclear(m_end, m_capacity);
	}
}

Blob::Blob(Blob&& other)
    : BlobView()
{
	m_data           = other.m_data;
	m_end            = other.m_end;
	m_capacity       = other.m_capacity;
	other.m_data     = nullptr;
	other.m_end      = nullptr;
	other.m_capacity = nullptr;
}

Blob::~Blob()
{
	release();
}

void Blob::clear()
{
	if (m_end > m_data)
	{
		blob_memclear(m_data, m_end);
		m_end = m_data;
	}
}

void Blob::release()
{
	if (m_data)
	{
		blob_memclear(m_data, m_end);
		std::free(m_data);
	}

	m_data     = nullptr;
	m_end      = nullptr;
	m_capacity = nullptr;
}

void Blob::reserve(std::size_t reserve_size)
{
	std::size_t const my_size = size();
	if (reserve_size < my_size)
		reserve_size = my_size;

	std::size_t const my_capacity = capacity();
	assert(my_capacity >= my_size);
	if (reserve_size == my_capacity)
		return;

	if (reserve_size == 0)
	{
		release();
	}
	else
	{
		unsigned char* buf = (unsigned char*)std::malloc(reserve_size);
		assert(buf && "Blob out of memory");

		if (my_size > 0)
			std::memcpy(buf, m_data, my_size);

		blob_memclear(m_data, m_end);
		std::free(m_data);

		m_data     = buf;
		m_end      = m_data + my_size;
		m_capacity = m_data + reserve_size;
		blob_memclear(m_end, m_capacity);
	}
}

void Blob::pop_front(std::size_t consume_size)
{
	std::size_t const my_size = size();
	if (consume_size >= my_size)
	{
		clear();
	}
	else
	{
		std::size_t new_len = my_size - consume_size;
		std::memmove(m_data, m_data + consume_size, new_len);
		blob_memclear(m_data + new_len, m_end);
		m_end = m_data + new_len;
	}
}

void Blob::added_to_tail(std::size_t added_size)
{
	assert(m_end + added_size <= m_capacity);
	m_end += added_size;
}

Blob Blob::from_literal(std::string_view const& initial)
{
	std::size_t const len = initial.size();

	Blob blob(len);
	blob.m_end += len;

	if (len > 0)
		std::memcpy(blob.m_data, initial.data(), len);

	return blob;
}

Blob Blob::from_random(std::size_t len)
{
	Blob blob(len);
	blob.m_end = blob.m_data + len;

#ifdef HAVE_GNUTLS
	if (gnutls_rnd(GNUTLS_RND_NONCE, blob.m_data, len) == 0)
		return blob;
#endif

	std::random_device rand;
	std::uniform_int_distribution<unsigned int> dist(0, 255);

	for (unsigned char* dst = blob.m_data; dst < blob.m_end; ++dst)
		*dst = dist(rand);

	return blob;
}

Blob Blob::from_hex(std::string_view const& initial, bool& ok)
{
	std::size_t const len = initial.size() >> 1;

	Blob blob(len);
	ok = true;

	char const* src = initial.data();
	for (std::size_t i = 0; i < len; ++i)
	{
		*blob.m_end = hex_to_char(src, ok);
		++blob.m_end;
		src += 2;
	}

	return blob;
}

Blob Blob::from_base64(std::string_view const& initial, bool& ok)
{
	std::size_t const len = initial.size() / 4 * 3;

	Blob blob(len);
	ok = true;

	char const* src = initial.data();
	for (std::size_t i = 0; i < len; ++i)
	{
		unsigned char v1 = base64_to_nibble(*src++, ok);
		unsigned char v2 = base64_to_nibble(*src++, ok);

		*blob.m_end = (v1 << 2) + (v2 >> 4);
		++blob.m_end;

		if (*src == '=')
			break;

		unsigned char v3 = base64_to_nibble(*src++, ok);

		*blob.m_end = (v2 << 4) + (v3 >> 2);
		++blob.m_end;

		if (*src == '=')
			break;

		unsigned char v4 = base64_to_nibble(*src++, ok);

		*blob.m_end = (v3 << 6) + v4;
		++blob.m_end;
	}

	return blob;
}

Blob& Blob::operator=(Blob&& other)
{
	release();

	m_data           = other.m_data;
	m_end            = other.m_end;
	m_capacity       = other.m_capacity;
	other.m_data     = nullptr;
	other.m_end      = nullptr;
	other.m_capacity = nullptr;

	return *this;
}

Blob& Blob::operator+=(BlobView const& blob)
{
	std::size_t const my_capacity = capacity();
	std::size_t const my_size     = size();
	std::size_t const their_size  = blob.size();

	if (my_capacity < my_size + their_size)
	{
		unsigned char* buf = (unsigned char*)std::malloc(my_size + their_size);
		assert(buf && "Blob out of memory");

		if (my_size > 0)
			std::memcpy(buf, m_data, my_size);

		blob_memclear(m_data, m_end);
		std::free(m_data);

		m_data     = buf;
		m_end      = m_data + my_size;
		m_capacity = m_end + their_size;
	}

	std::memcpy(m_end, blob.data(), their_size);
	m_end += their_size;

	return *this;
}

BlobBuffer::BlobBuffer(std::size_t reserve_size)
    : m_blob(reserve_size)
    , m_read_offset(0)
{
}

BlobBuffer::BlobBuffer(BlobBuffer&& other)
    : m_blob(std::move(other.m_blob))
    , m_read_offset(other.m_read_offset)
{
	other.m_read_offset = 0;
}

BlobBuffer::~BlobBuffer()
{
	m_read_offset = 0;
}

void BlobBuffer::clear()
{
	m_blob.clear();
	m_read_offset = 0;
}

void BlobBuffer::release()
{
	m_blob.release();
	m_read_offset = 0;
}

void BlobBuffer::reserve(std::size_t reserve_size)
{
	m_blob.reserve(reserve_size);
}

void BlobBuffer::advance(std::size_t count)
{
	std::size_t available = m_blob.size() - m_read_offset;
	if (count > available)
		count = available;

	m_read_offset += count;
}

void BlobBuffer::flush()
{
	if (m_read_offset > 0)
	{
		m_blob.pop_front(m_read_offset);
		m_read_offset = 0;
	}
}

std::size_t BlobBuffer::read(void* dest, std::size_t maxlen)
{
	std::size_t available = m_blob.size() - m_read_offset;
	if (available > maxlen)
		available = maxlen;

	if (available > 0)
	{
		std::memcpy(dest, m_blob.data() + m_read_offset, available);
		m_read_offset += available;
	}

	return available;
}

void BlobBuffer::write(void const* src, std::size_t len)
{
	if (m_read_offset > 0)
	{
		std::size_t const blob_size = m_blob.size();
		assert(blob_size >= m_read_offset);

		if (blob_size == m_read_offset || m_blob.space() < len)
		{
			m_blob.pop_front(m_read_offset);
			m_read_offset = 0;
		}
	}

	m_blob += BlobView(reinterpret_cast<unsigned char const*>(src), len);
}

void BlobBuffer::added_to_tail(std::size_t added_size)
{
	m_blob.added_to_tail(added_size);
}

BlobBuffer& BlobBuffer::operator=(BlobBuffer&& other)
{
	m_blob              = std::move(other.m_blob);
	m_read_offset       = other.m_read_offset;
	other.m_read_offset = 0;
	return *this;
}
