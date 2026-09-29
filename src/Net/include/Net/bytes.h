/**
 * @file bytes.h
 * @brief Numbers in and out of fixed byte buffers, least significant first
 */

#ifndef NET_INCLUDE_NET_BYTES_H_
#define NET_INCLUDE_NET_BYTES_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wolfenstein::net {

// Writes numbers into a fixed buffer, least significant byte first, as the
// protocol has them. What does not fit is not written, and the writer is
// failed from then on: a message that does not fit is not sent.
class ByteWriter
{
  public:
	explicit ByteWriter(std::span<std::uint8_t> buffer) : buffer_(buffer) {}

	void U8(std::uint8_t value) {
		if (Reserve(1)) {
			buffer_[size_++] = value;
		}
	}
	void U16(std::uint16_t value) {
		U8(static_cast<std::uint8_t>(value));
		U8(static_cast<std::uint8_t>(value >> 8U));
	}
	void U32(std::uint32_t value) {
		U16(static_cast<std::uint16_t>(value));
		U16(static_cast<std::uint16_t>(value >> 16U));
	}
	void I8(std::int8_t value) { U8(static_cast<std::uint8_t>(value)); }
	void I16(std::int16_t value) { U16(static_cast<std::uint16_t>(value)); }
	void Bytes(std::span<const std::uint8_t> bytes) {
		if (Reserve(bytes.size())) {
			std::ranges::copy(
				bytes, buffer_.begin() + static_cast<std::ptrdiff_t>(size_));
			size_ += bytes.size();
		}
	}

	bool Ok() const { return ok_; }
	std::size_t Size() const { return size_; }
	std::span<const std::uint8_t> Written() const {
		return buffer_.first(size_);
	}

  private:
	bool Reserve(std::size_t count) {
		ok_ = ok_ && buffer_.size() - size_ >= count;
		return ok_;
	}

	std::span<std::uint8_t> buffer_;
	std::size_t size_ = 0;
	bool ok_ = true;
};

// Reads them back. Reading past the end gives 0s and fails the reader: a
// message cut short is dropped whole.
class ByteReader
{
  public:
	explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

	std::uint8_t U8() {
		if (!Take(1)) {
			return 0;
		}
		return data_[at_++];
	}
	std::uint16_t U16() {
		const std::uint16_t low = U8();
		const std::uint16_t high = U8();
		return static_cast<std::uint16_t>(low | (high << 8U));
	}
	std::uint32_t U32() {
		const std::uint32_t low = U16();
		const std::uint32_t high = U16();
		return low | (high << 16U);
	}
	std::int8_t I8() { return static_cast<std::int8_t>(U8()); }
	std::int16_t I16() { return static_cast<std::int16_t>(U16()); }
	void Bytes(std::span<std::uint8_t> out) {
		if (Take(out.size())) {
			std::ranges::copy(data_.subspan(at_, out.size()), out.begin());
			at_ += out.size();
		}
	}

	// What was read makes no sense: nothing more is read
	void Fail() { ok_ = false; }
	bool Ok() const { return ok_; }
	std::size_t Remaining() const { return data_.size() - at_; }

  private:
	bool Take(std::size_t count) {
		ok_ = ok_ && data_.size() - at_ >= count;
		return ok_;
	}

	std::span<const std::uint8_t> data_;
	std::size_t at_ = 0;
	bool ok_ = true;
};

// Text of at most N bytes, kept in place (a player's name, a level's file
// name): copied and sent without allocating
template <std::size_t N>
class FixedString
{
  public:
	static constexpr std::size_t kCapacity = N;

	FixedString() = default;
	// `text`, cut to N bytes
	explicit FixedString(std::string_view text)
		: size_(static_cast<std::uint8_t>(std::min(text.size(), N))) {
		std::ranges::copy(text.substr(0, size_), chars_.begin());
	}
	std::string_view View() const { return {chars_.data(), size_}; }

	// As a length and its bytes
	void Write(ByteWriter& out) const {
		out.U8(size_);
		out.Bytes(std::span(
			reinterpret_cast<const std::uint8_t*>(chars_.data()), size_));
	}
	static FixedString Read(ByteReader& in) {
		FixedString text;
		const std::uint8_t size = in.U8();
		if (size > N) {
			in.Fail();	// longer than any this side would send
			return text;
		}
		text.size_ = size;
		in.Bytes(std::span(reinterpret_cast<std::uint8_t*>(text.chars_.data()),
						   size));
		return text;
	}

	friend bool operator==(const FixedString& a, const FixedString& b) {
		return a.View() == b.View();
	}

  private:
	std::array<char, N> chars_{};
	std::uint8_t size_ = 0;
	static_assert(N < 256, "a length is one byte");
};

}  // namespace wolfenstein::net

#endif	// NET_INCLUDE_NET_BYTES_H_
