Func tumalloc(size):
    If HEAD = NULL:  // if first node in list doesn't exist
        ptr -> do_alloc(size)  // goto do_alloc which returns null
        return ptr
    Else: // if there is an element in HEAD list
    For each block in free_list: // for each element in free_list
        If size >= block.size:  // if free_list.size >= block.size
            header -> split(block, size+sizeof(header)) //
            remove_free_block(header)
            header.size = size
            header.magic = 0x01234567
            return header + 1
    If no block is big enough:
        ptr -> do_alloc(size)
        return ptr