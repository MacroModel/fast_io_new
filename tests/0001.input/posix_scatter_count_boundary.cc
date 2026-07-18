#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

#include <fast_io.h>

#if defined(__linux__)

namespace
{

inline constexpr ::std::size_t scatter_count{2051u};
inline constexpr ::std::size_t linux_iov_max{1024u};

inline ::std::vector<::fast_io::io_scatter_t> make_one_byte_scatters(::std::vector<::std::byte> &storage)
{
	::std::vector<::fast_io::io_scatter_t> scatters;
	scatters.reserve(storage.size());
	for (auto &element : storage)
	{
		scatters.push_back({__builtin_addressof(element), 1u});
	}
	return scatters;
}

inline void expect_equal(::std::vector<::std::byte> const &actual, ::std::vector<::std::byte> const &expected)
{
	assert(::std::equal(actual.cbegin(), actual.cend(), expected.cbegin(), expected.cend()));
}

} // namespace

#endif

int main()
{
#if defined(__linux__)
	// More than two Linux IOV_MAX-sized batches ensure that both the 1024 boundary and the final short descriptor batch
	// are exercised. One-byte entries make the expected some-status unambiguous and keep the test's data volume small.
	::std::vector<::std::byte> expected(scatter_count);
	for (::std::size_t i{}; i != expected.size(); ++i)
	{
		expected[i] = static_cast<::std::byte>(i & 0xffu);
	}

	::fast_io::posix_file file(::fast_io::io_temp);
	::fast_io::operations::write_all_bytes(file, expected.data(), expected.data() + expected.size());

	::std::vector<::std::byte> actual(scatter_count);
	auto scatters{make_one_byte_scatters(actual)};

	::fast_io::operations::input_stream_seek_bytes(file, 0, ::fast_io::seekdir::beg);
	auto const read_some_status{
		::fast_io::operations::scatter_read_some_bytes(file, scatters.data(), scatters.size())};
	assert(read_some_status.position == linux_iov_max);
	assert(read_some_status.position_in_scatter == 0u);

	::std::fill(actual.begin(), actual.end(), ::std::byte{});
	::fast_io::operations::input_stream_seek_bytes(file, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::scatter_read_all_bytes(file, scatters.data(), scatters.size());
	expect_equal(actual, expected);

	::std::fill(actual.begin(), actual.end(), ::std::byte{});
	auto const pread_some_status{
		::fast_io::operations::scatter_pread_some_bytes(file, scatters.data(), scatters.size(), 0)};
	assert(pread_some_status.position == linux_iov_max);
	assert(pread_some_status.position_in_scatter == 0u);

	::std::fill(actual.begin(), actual.end(), ::std::byte{});
	::fast_io::operations::scatter_pread_all_bytes(file, scatters.data(), scatters.size(), 0);
	expect_equal(actual, expected);
#endif
}
