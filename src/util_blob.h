#ifndef DECK_ASSISTANT_BLOB_H
#define DECK_ASSISTANT_BLOB_H

#include <string>
#include <string_view>

class Blob;

class BlobView
{
public:
	BlobView();
	BlobView(char const* initial);
	BlobView(std::string_view const& initial);
	BlobView(std::string const& initial);
	BlobView(BlobView&& other);
	BlobView(BlobView const& other);

	inline unsigned char const* data() const { return m_data; }
	inline std::size_t size() const { return m_end - m_data; }

	std::string_view to_bin() const;
	std::string to_hex() const;
	std::string to_base64() const;

#ifdef HAVE_GNUTLS
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
	Blob(std::size_t reserve = 0);
	Blob(Blob&& other);
	Blob(Blob const&) = delete;
	~Blob();

	void clear();
	inline unsigned char* data() { return m_data; }
	inline std::size_t capacity() const { return m_capacity - m_data; }

	static Blob from_literal(std::string_view const& initial);
	static Blob from_random(std::size_t len);
	static Blob from_hex(std::string_view const& initial);
	static Blob from_base64(std::string_view const& initial);

	Blob& operator=(Blob&&);
	Blob& operator=(Blob const&) = delete;

	Blob& operator+=(BlobView const& blob);

protected:
	unsigned char* m_capacity;
};

unsigned char hex_to_char(char const* hex);
void char_to_hex(unsigned char ch, char* hex);
void char_to_hex_uc(unsigned char ch, char* hex);

#endif // DECK_ASSISTANT_BLOB_H
