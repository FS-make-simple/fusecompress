The fusecompress file header is 12 bytes long and looks like this:

```
typedef struct
{
        char id[3];             // ID of FuseCompress format
        unsigned char type;     // ID of used compression module
        off_t size;             // Uncompressed size of the file in bytes

} __attribute__((packed)) header_t;
```

## `id` ##
`id` contains three magic bytes (0x1f, 0x5d, 0x89) fusecompress uses to identify compressed files.

## `type` ##
`type` defines what compressor has been used:

  * 0x00: "null" compressor; this is essentially an uncompressed file with a fusecompress header
  * 0x01: Bzip2
  * 0x02: gzip (zlib)
  * 0x03: LZO
  * 0x04: LZMA

## `size` ##
Size is a little-endian 64-bit integer containing the uncompressed size of the file.