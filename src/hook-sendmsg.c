
#define DEFINE_HOOK_GLOBALS 1
#define DONT_BYPASS_HOOKS   1

#include "common.h"
#include "filter.h"
#include "hook-sendmsg.h"

ssize_t (* __real_sendmsg)(int fd, const struct msghdr *msg, int flags);

static FilterReplyResult filter_parse_reply(FilterReplyResultBase * const rb,
                                            struct msghdr * const msg,
                                            int * const flags)
{
    msgpack_unpacked * const message = filter_receive_message(rb->filter);
    const msgpack_object_map * const map = &message->data.via.map;
    FilterReplyResult reply_result = filter_parse_common_reply_map(rb, map);

    if (rb->pre == false) {
        return reply_result;
    }
    const msgpack_object * const obj_flags =
        msgpack_get_map_value_for_key(map, "flags");
    if (obj_flags != NULL &&
        (obj_flags->type == MSGPACK_OBJECT_POSITIVE_INTEGER ||
         obj_flags->type == MSGPACK_OBJECT_NEGATIVE_INTEGER)) {
        const int64_t new_flags = obj_flags->via.i64;
        if (new_flags >= INT_MIN && new_flags <= INT_MAX) {
            *flags = new_flags;
            (void) flags;
        }
    }
    const msgpack_object * const obj_data =
        msgpack_get_map_value_for_key(map, "data");
    if (obj_data != NULL && obj_data->type == MSGPACK_OBJECT_RAW &&
        msg->msg_iovlen > 0) {
        struct iovec * const vecs = msg->msg_iov;
        msg->msg_iovlen = 1;
        vecs[0].iov_base = (void *) obj_data->via.raw.ptr;
        vecs[0].iov_len = obj_data->via.raw.size;
    }
    filter_overwrite_sa_with_reply_map(map, "remote_host", "remote_port",
                                       msg->msg_name, &msg->msg_namelen);
    return reply_result;
}

static FilterReplyResult filter_apply(FilterReplyResultBase * const rb,
                                      const struct sockaddr_storage * const sa_local,
                                      const socklen_t sa_local_len,
                                      struct msghdr *msg, size_t * const nbyte,
                                      int * const flags)
{
    msgpack_packer * const msgpack_packer = rb->filter->msgpack_packer;
    filter_before_apply(rb, 2U, "sendmsg", sa_local, sa_local_len,
                        msg->msg_name, msg->msg_namelen);
    msgpack_pack_mstring(msgpack_packer, "flags");
    msgpack_pack_int(msgpack_packer, *flags);
    
    assert((size_t) *rb->ret <= *nbyte);
    msgpack_pack_mstring(msgpack_packer, "data");
    msgpack_pack_raw(msgpack_packer, *nbyte);
    size_t data_remaining = *nbyte;
    size_t read_from_vec;
    struct iovec * const vecs = msg->msg_iov;
    size_t i_vecs = 0U;
    while (i_vecs < (size_t) msg->msg_iovlen &&
           data_remaining > (size_t) 0U) {
        if (data_remaining < vecs[i_vecs].iov_len) {
            read_from_vec = data_remaining;
        } else {
            read_from_vec = vecs[i_vecs].iov_len;
        }
        assert(data_remaining >= read_from_vec);
        assert(vecs[i_vecs].iov_len >= read_from_vec);
        msgpack_pack_raw_body(msgpack_packer, vecs[i_vecs].iov_base,
                              read_from_vec);
        data_remaining -= read_from_vec;
        i_vecs++;
    }
    if (filter_send_message(rb->filter) != 0) {
        return FILTER_REPLY_RESULT_ERROR;
    }
    return filter_parse_reply(rb, msg, flags);
}

int __real_sendmsg_init(void)
{
#ifdef USE_INTERPOSERS
    __real_sendmsg = sendmsg;
#else
    if (__real_sendmsg == NULL) {
        __real_sendmsg = dlsym(RTLD_NEXT, "sendmsg");
        assert(__real_sendmsg != NULL);
    }
#endif
    return 0;
}

ssize_t INTERPOSE(sendmsg)(int fd, const struct msghdr *msg, int flags)
{
    __real_sendmsg_init();
    const bool bypass_filter =
        getenv("SIXJACK_BYPASS") != NULL || is_socket(fd) == false;        
    struct sockaddr_storage sa_local, *sa_local_ = &sa_local;
    socklen_t sa_local_len;
    get_sock_info(fd, &sa_local_, &sa_local_len, NULL, NULL);
    int ret = 0;
    int ret_errno = 0;    
    bool bypass_call = false;
    struct msghdr msg_ = *msg;
    size_t nbyte = (size_t) 0U;
    struct iovec * const vecs = msg->msg_iov;
    size_t i_vecs = 0U;
    while (i_vecs < (size_t) msg->msg_iovlen) {
        assert(SIZE_MAX - nbyte >= vecs[i_vecs].iov_len);
        nbyte += vecs[i_vecs].iov_len;
        i_vecs++;
    }
    size_t new_nbyte = nbyte;
    FilterReplyResultBase rb = {
        .pre = true, .ret = &ret, .ret_errno = &ret_errno, .fd = fd
    };
    if (bypass_filter == false && (rb.filter = filter_get()) &&
        filter_apply(&rb, sa_local_, sa_local_len, &msg_,
                     &new_nbyte, &flags)
        == FILTER_REPLY_BYPASS) {
        bypass_call = true;
    }
    if (bypass_call == false) {
        ssize_t ret_ = __real_sendmsg(fd, &msg_, flags);
        ret_errno = errno;
        ret = (int) ret_;
        assert((ssize_t) ret_ == ret);
    }
    if (bypass_filter == false) {
        new_nbyte = ret;
        rb.pre = false;
        filter_apply(&rb, sa_local_, sa_local_len, &msg_,
                     &new_nbyte, &flags);
    }
    errno = ret_errno;
    
    return ret;
}
