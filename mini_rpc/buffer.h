#include <vector>
#include <string>
#include <assert.h>
#include <algorithm>
#include <cstring>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

class Buffer {
public:
    static const size_t INITIAL_SIZE = 1024;
    static const size_t HEADER_PREPEND = 8;

    explicit Buffer(size_t initial_size = INITIAL_SIZE)
        :buffer_(initial_size + HEADER_PREPEND),
         read_index_(0),
         write_index_(0) {}
    
    ~Buffer() = default;

    // 可读数据大小
    size_t readableBytes() const {
        return write_index_ - read_index_;
    }

    // 可写数据大小
    size_t writableBytes() const {
        return buffer_.size() - write_index_;
    }

    // 查看数据
    char* peek(){
        return begin() + read_index_;
    }

    // 追加数据
    void append(const char* data, int len){
        ensureWritableBytes(len);
        std::copy(data, data + len, begin() + write_index_);
        write_index_ += len;
    }

    // 消费数据
    void retrieve(size_t len) {
        assert(len <= readableBytes());
        if (len < readableBytes()) {
            read_index_ += len;
        } else {
            retrieveAll();
        }
    }

    // 消费所有数据
    void retrieveAll() {
        read_index_ = HEADER_PREPEND;
        write_index_ = HEADER_PREPEND;
    }

    // 读取fd缓冲区数据
    ssize_t readFd(int fd) {
        // 临时栈缓冲区
        char extra_buf[65536];
        struct iovec iov[2];
        iov[0].iov_base = begin() + write_index_;
        iov[0].iov_len = writableBytes();
        iov[1].iov_base = extra_buf;
        iov[1].iov_len = sizeof(extra_buf);

        // 读数据
        int iovcnt = (writableBytes() < sizeof(extra_buf) ? 2 : 1);
        ssize_t n = readv(fd, iov, iovcnt);
        if (n < 0) {
            // readv error
        } else if (n <= iov[0].iov_len) {
            // buffer足够了
            write_index_ += n;
        } else {
            // 使用到临时缓冲区了
            write_index_ = buffer_.size(); // 写指针移动到最后
            append(extra_buf, n - iov[0].iov_len);
        }
        return n;
    }

    // 读取一个4字节整数
    uint32_t readInt32() {
        assert(readableBytes() >= sizeof(uint32_t));
        uint32_t length;
        memcpy(&length, peek(), 4);
        retrieve(sizeof(uint32_t));
        return ntohl(length);
    }

    // 读取指定大小的数据
    std::string retrieveAsString(size_t len) {
        assert(readableBytes() >= len);
        std::string str(peek(), len);
        retrieve(len);
        return str;
    }

private:
    // 获取buffer可读首地址
    char* begin() {
        return buffer_.data();
    }

    // buffer扩容
    void ensureWritableBytes(size_t len) {
        if (len > writableBytes()) {
            if (len > writableBytes() + read_index_ - HEADER_PREPEND) {
                // 扩容
                buffer_.resize(write_index_ + len);
            } else {
                // 移动
                size_t readable = readableBytes();
                std::copy(begin() + read_index_, begin() + write_index_, begin() + HEADER_PREPEND);
                read_index_ = HEADER_PREPEND;
                write_index_ = read_index_ + readable;
            }
        }
    }

private:
    // 存储数据
    std::vector<char> buffer_;

    // 读指针、写指针
    size_t read_index_;
    size_t write_index_;

};