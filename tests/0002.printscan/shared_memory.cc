#include <cassert>
#include <cstddef>

#include <fast_io.h>

int main()
{
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
	constexpr ::std::size_t capacity{4096};
	constexpr char8_t name[]{u8"fast_io_shared_memory_test"};
	constexpr char8_t native_name[]{u8"fast_io_native_shared_memory_test"};

	::fast_io::win32_shared_memory win32_mapping{name, capacity};
	win32_mapping[0] = ::std::byte{0x5A};

#if !defined(_WIN32_WINDOWS)
	::fast_io::nt_shared_memory nt_mapping{name, 1, ::fast_io::ipc_mode::in};
	assert(nt_mapping.size() == capacity);
	assert(nt_mapping[0] == ::std::byte{0x5A});

	::fast_io::native_shared_memory native_mapping{native_name, capacity};
	native_mapping[0] = ::std::byte{0x3C};
	::fast_io::native_shared_memory native_reader{native_name, 1, ::fast_io::ipc_mode::in};
	assert(native_reader.size() == capacity);
	assert(native_reader[0] == ::std::byte{0x3C});

	assert(native_mapping.size() == capacity);
	assert(native_mapping[0] == ::std::byte{0x3C});

	auto duplicated_mapping{native_mapping};
	assert(duplicated_mapping.size() == capacity);
	assert(duplicated_mapping[0] == ::std::byte{0x3C});
#endif
#endif
}
