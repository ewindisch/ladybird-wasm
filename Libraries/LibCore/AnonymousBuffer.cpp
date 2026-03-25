/*
 * Copyright (c) 2021, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Platform.h>
#include <AK/Try.h>
#include <LibCore/AnonymousBuffer.h>
#include <LibCore/System.h>
#include <fcntl.h>
#ifndef AK_OS_EMSCRIPTEN
#    include <sys/mman.h>
#endif

namespace Core {

#ifdef AK_OS_EMSCRIPTEN
// On Emscripten there is no OS shared memory. Use malloc-backed buffers.
// The fd field is set to -1 to signal malloc ownership in the destructor.
ErrorOr<AnonymousBuffer> AnonymousBuffer::create_with_size(size_t size)
{
    auto impl = TRY(AnonymousBufferImpl::create(-1, size));
    return AnonymousBuffer(move(impl));
}

ErrorOr<NonnullRefPtr<AnonymousBufferImpl>> AnonymousBufferImpl::create(int, size_t size)
{
    void* data = calloc(1, size);
    if (!data)
        return Error::from_errno(ENOMEM);
    return AK::adopt_nonnull_ref_or_enomem(new (nothrow) AnonymousBufferImpl(-1, size, data));
}

AnonymousBufferImpl::~AnonymousBufferImpl()
{
    if (m_fd == -1) {
        free(m_data);
    }
}
#else
ErrorOr<AnonymousBuffer> AnonymousBuffer::create_with_size(size_t size)
{
    auto fd = TRY(Core::System::anon_create(size, O_CLOEXEC));
    return create_from_anon_fd(fd, size);
}

ErrorOr<NonnullRefPtr<AnonymousBufferImpl>> AnonymousBufferImpl::create(int fd, size_t size)
{
    auto* data = mmap(nullptr, round_up_to_power_of_two(size, PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
        return Error::from_errno(errno);
    return AK::adopt_nonnull_ref_or_enomem(new (nothrow) AnonymousBufferImpl(fd, size, data));
}

AnonymousBufferImpl::~AnonymousBufferImpl()
{
    if (m_fd != -1) {
        auto rc = close(m_fd);
        VERIFY(rc == 0);
    }
    auto rc = munmap(m_data, round_up_to_power_of_two(m_size, PAGE_SIZE));
    VERIFY(rc == 0);
}
#endif

ErrorOr<AnonymousBuffer> AnonymousBuffer::create_from_anon_fd(int fd, size_t size)
{
    auto impl = TRY(AnonymousBufferImpl::create(fd, size));
    return AnonymousBuffer(move(impl));
}

AnonymousBufferImpl::AnonymousBufferImpl(int fd, size_t size, void* data)
    : m_fd(fd)
    , m_size(size)
    , m_data(data)
{
}

}
