/*

MIT License

Copyright (c) 2021 Marc "Foddex" Oude Kotte

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <Ecs.hh>

#include <cstring>
#include <exception>
#include <vector>

class InvalidRead : public std::exception {};
class InvalidSeek : public std::exception {};

/* Memory stream that supports seeking (changing position) for both reading and writing.
 * Keeps its memory in a vector.
 */
class MemoryStream : public ecs::IStream {
public:
	MemoryStream() = default;
	MemoryStream(std::vector<uint8_t> data) : m_data(std::move(data)) {}
	const std::vector<uint8_t>& data() const { return m_data; }

	void write(const void* data, size_t count) override {
		const size_t newSize = std::max<size_t>(m_position + count, m_data.size());
		m_data.resize(newSize);
		memcpy(m_data.data() + m_position, data, count);
		m_position += count;
	}
	void read(void* data, size_t count) override {
		const size_t actual = std::min<size_t>(count, m_data.size() - m_position);
		if (actual != count)
			throw InvalidRead();
		memcpy(data, m_data.data() + m_position, count);
		m_position += count;
	}
	uint64_t position() const override {
		return m_position;
	}
	void setPosition(uint64_t pos) override {
		if (pos > m_data.size())
			throw InvalidSeek();
		m_position = pos;
	}

private:
	std::vector<uint8_t>		m_data;
	size_t						m_position = 0;
};
