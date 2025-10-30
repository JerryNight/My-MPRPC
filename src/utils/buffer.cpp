#include "buffer.h"
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace rpc {

// 从文件描述符读取数据: 使用readv实现，栈上临时缓冲区配合，避免频繁扩容
ssize_t Buffer::readFromFd(int fd, int* saved_errno) {
    char extra_buf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    
    // 第一块缓冲区指向Buffer的可写区域
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    // 第二块缓冲区指向栈上临时缓冲区
    vec[1].iov_base = extra_buf;
    vec[1].iov_len = sizeof(extra_buf);
    
    // 如果Buffer可写空间较大，不使用临时缓冲区
    const int iovcnt = (writable < sizeof(extra_buf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    
    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 数据全部写入Buffer
        writer_index_ += n;
    } else {
        // Buffer写满，额外数据在extra_buf中
        writer_index_ = buffer_.size();
        append(extra_buf, n - writable);
    }
    return n;
}

}

