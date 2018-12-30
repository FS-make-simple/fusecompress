## Using extended attributes ##

Short answer: Pointless.

I tried using xattr to store/retrieve the header to save us the `open`/`read`/`close` cycle for every `stat()`, but there was _no_ difference in performance at all. Tested with ext3 and reiser3 as backing filesystems.

## `MAX_DATABASE_LEN` ##

Caching more meta data could make things faster. Simply increasing this value will not, however, due to the O(n) search implemented in `direct_open()`. May be fixed by using a different algorithm. Potential problem: Lots of files may stack up in the background compression queue. (Reducing MAX\_DATABASE\_LEN does not help performance either, BTW.)