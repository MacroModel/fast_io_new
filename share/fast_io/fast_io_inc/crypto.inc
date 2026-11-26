export namespace fast_io
{
    using ::fast_io::aes_ctx;
    using ::fast_io::aes128;
    using ::fast_io::aes192;
    using ::fast_io::aes256;
    namespace transcoders
    {
        using ::fast_io::transcoders::aes_ctr;
    }
    using ::fast_io::rapidhash64;
    using ::fast_io::native_swiss_hasher;
    using ::fast_io::crc32_context;
    using ::fast_io::crc32c_context;
    using ::fast_io::md5_context;
    using ::fast_io::sha1_context;
    using ::fast_io::sha224_context;
    using ::fast_io::sha256_context;
    using ::fast_io::sha384_context;
    using ::fast_io::sha512_context;
    using ::fast_io::sha512_224_context;
    using ::fast_io::sha512_256_context;
    namespace details
    {
        using ::fast_io::details::support_hardware_crc32;
        using ::fast_io::details::support_hardware_crc32c;
    }
}
