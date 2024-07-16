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

#ifndef DECK_ASSISTANT_UTIL_BLOB_H
#define DECK_ASSISTANT_UTIL_BLOB_H

#include <string>
#include <string_view>

namespace util
{

class Blob;

class BlobView
{
public:
	BlobView();
	BlobView(char const* initial);
	BlobView(unsigned char const* initial, std::size_t initial_size);
	BlobView(std::string_view const& initial);
	BlobView(std::string const& initial);
	BlobView(BlobView&& other);
	BlobView(BlobView const& other);

	inline unsigned char const* data() const { return m_data; }
	inline std::size_t size() const { return m_end - m_data; }

	std::string_view to_bin() const;
	std::string to_hex() const;
	std::string to_base64() const;

#if (defined HAVE_GNUTLS || defined HAVE_OPENSSL)
	Blob sha1() const;
	Blob sha256() const;
#endif

	BlobView& operator=(BlobView&& other);
	BlobView& operator=(BlobView const& other);

	std::strong_ordering operator<=>(BlobView const& blob) const;
	bool operator!=(BlobView const& blob) const;

protected:
	unsigned char* m_data;
	unsigned char* m_end;
};

class Blob : public BlobView
{
public:
	Blob(std::size_t reserve_size = 0);
	Blob(Blob&& other);
	Blob(Blob const&) = delete;
	~Blob();

	void clear();
	void release();
	inline unsigned char* data() { return m_data; }
	inline std::size_t capacity() const { return m_capacity - m_data; }
	void reserve(std::size_t reserve_size);

	static Blob from_literal(std::string_view const& initial);
	static Blob from_random(std::size_t len);
	static Blob from_hex(std::string_view const& initial, bool& ok);
	static Blob from_base64(std::string_view const& initial, bool& ok);

	Blob& operator=(Blob&&);
	Blob& operator=(Blob const&) = delete;

	Blob& operator+=(BlobView const& blob);

	using BlobView::operator<=>;
	using BlobView::operator!=;

	// Some helpers for catch2
	inline std::strong_ordering operator<=>(Blob const& blob) const { return BlobView::operator<=>(blob); }
	inline bool operator==(Blob const& blob) const { return BlobView::operator<=>(blob) == std::strong_ordering::equal; }
	inline bool operator!=(Blob const& blob) const { return BlobView::operator!=(blob); }

protected:
	unsigned char* m_capacity;
};

} // namespace util

#endif // DECK_ASSISTANT_UTIL_BLOB_H
