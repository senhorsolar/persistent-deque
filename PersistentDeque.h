#include <bit>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "CircularBuffer.h"


template <typename T, std::size_t BufferSizeBytes=4096>
class PersistentDeque
{
public:
    static constexpr std::size_t RequestedBufferCapacity =
        (sizeof(T) > BufferSizeBytes) ? 1 : (BufferSizeBytes / sizeof(T));

    static constexpr std::size_t TunedMinBufferCapacity =
        (std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>) ? 32 : 1;

    static constexpr std::size_t BufferCapacity =
        (RequestedBufferCapacity < TunedMinBufferCapacity)
        ? TunedMinBufferCapacity
        : RequestedBufferCapacity;

    static constexpr bool BufferCapacityIsPowerOfTwo =
        (BufferCapacity != 0) && ((BufferCapacity & (BufferCapacity - 1)) == 0);

    using Buffer = CircularBuffer<T, BufferCapacity>;
    using Map = DynamicCircularBuffer<std::shared_ptr<Buffer>>;

    class const_iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() = default;

        reference operator*() const;
        pointer operator->() const;

        const_iterator& operator++();
        const_iterator operator++(int);
        const_iterator& operator--();
        const_iterator operator--(int);

        const_iterator& operator+=(difference_type n);
        const_iterator& operator-=(difference_type n);
        const_iterator operator+(difference_type n) const;
        const_iterator operator-(difference_type n) const;
        difference_type operator-(const const_iterator& other) const;
        reference operator[](difference_type n) const;

        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;
        bool operator<(const const_iterator& other) const;
        bool operator<=(const const_iterator& other) const;
        bool operator>(const const_iterator& other) const;
        bool operator>=(const const_iterator& other) const;

        friend const_iterator operator+(difference_type n, const const_iterator& it) {
            return it + n;
        }

    private:
        friend class PersistentDeque;

        explicit const_iterator(const PersistentDeque* owner, std::size_t logical_idx);

        void MoveToLogicalIndex(std::size_t logical_idx);

        const PersistentDeque* m_owner = nullptr;
        std::size_t m_logical_idx = 0;
        std::size_t m_buffer_idx = 0;
        std::size_t m_idx_in_buffer = 0;
    };

    PersistentDeque() = default;

    void PushFront(const T& value);
    void PushFront(T&& value);
    void PushBack(const T& value);
    void PushBack(T&& value);

    template <typename... Args>
    void EmplaceFront(Args&&... args) {
        PushFront(T(std::forward<Args>(args)...));
    }

    template <typename... Args>
    void EmplaceBack(Args&&... args) {
        PushBack(T(std::forward<Args>(args)...));
    }

    T PopFront();
    T PopBack();

    // Remove non-const ref accessor since it makes it hard to ensure persistence
    // T& At(std::size_t i);
    // inline T& operator[](std::size_t i);

    const T& At(std::size_t i) const;
    inline const T& operator[](std::size_t i) const;

    inline bool IsEmpty() const { return m_size == 0; }
    inline std::size_t Size() const { return m_size; }

    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

private:
    inline std::size_t BackIndex() const {
        return m_buffers.Size() - 1;
    }

    inline std::shared_ptr<Buffer>& FrontBlockRef() {
        return m_buffers[0];
    }

    inline const std::shared_ptr<Buffer>& FrontBlockRef() const {
        return m_buffers[0];
    }

    inline std::shared_ptr<Buffer>& BackBlockRef() {
        return m_buffers[BackIndex()];
    }

    inline const std::shared_ptr<Buffer>& BackBlockRef() const {
        return m_buffers[BackIndex()];
    }

    template <typename U>
    void PushFrontImpl(U&& value);

    template <typename U>
    void PushBackImpl(U&& value);

    static void EnsureUnique(std::shared_ptr<Buffer>& block);

    // Maps logical index to (buffer idx, idx in buffer)
    std::pair<std::size_t, std::size_t> Locate(std::size_t i) const;

    Map m_buffers;
    std::size_t m_size = 0;
};

/////////////////////////
// const_iterator impl //
/////////////////////////

template <typename T, std::size_t BufferSizeBytes>
PersistentDeque<T, BufferSizeBytes>::const_iterator::const_iterator(const PersistentDeque* owner, std::size_t logical_idx)
    : m_owner(owner), m_logical_idx(logical_idx)
{
    if (m_owner == nullptr || logical_idx == m_owner->m_size) {
        m_buffer_idx = (m_owner == nullptr) ? 0 : m_owner->m_buffers.Size();
        m_idx_in_buffer = 0;
        return;
    }

    const auto [buffer_idx, idx_in_buffer] = m_owner->Locate(logical_idx);
    m_buffer_idx = buffer_idx;
    m_idx_in_buffer = idx_in_buffer;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator::reference
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator*() const
{
    return m_owner->m_buffers[m_buffer_idx]->At(m_idx_in_buffer);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator::pointer
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator->() const
{
    return &(**this);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator&
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator++()
{
    ++m_logical_idx;
    if (m_logical_idx == m_owner->m_size) {
        m_buffer_idx = m_owner->m_buffers.Size();
        m_idx_in_buffer = 0;
        return *this;
    }

    ++m_idx_in_buffer;
    if (m_idx_in_buffer == m_owner->m_buffers[m_buffer_idx]->Size()) {
        ++m_buffer_idx;
        m_idx_in_buffer = 0;
    }
    return *this;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator++(int)
{
    const_iterator tmp = *this;
    ++(*this);
    return tmp;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator&
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator--()
{
    --m_logical_idx;
    if (m_buffer_idx == m_owner->m_buffers.Size()) {
        m_buffer_idx = m_owner->m_buffers.Size() - 1;
        m_idx_in_buffer = m_owner->m_buffers[m_buffer_idx]->Size() - 1;
        return *this;
    }

    if (m_idx_in_buffer > 0) {
        --m_idx_in_buffer;
        return *this;
    }

    --m_buffer_idx;
    m_idx_in_buffer = m_owner->m_buffers[m_buffer_idx]->Size() - 1;
    return *this;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator--(int)
{
    const_iterator tmp = *this;
    --(*this);
    return tmp;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator&
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator+=(difference_type n)
{
    const difference_type new_index = static_cast<difference_type>(m_logical_idx) + n;
    if (new_index < 0 || new_index > static_cast<difference_type>(m_owner->m_size)) {
        throw std::runtime_error("PersistentDeque::const_iterator moved out of bounds");
    }

    MoveToLogicalIndex(static_cast<std::size_t>(new_index));
    return *this;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator&
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator-=(difference_type n)
{
    return (*this += -n);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator+(difference_type n) const
{
    const_iterator tmp = *this;
    tmp += n;
    return tmp;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator-(difference_type n) const
{
    const_iterator tmp = *this;
    tmp -= n;
    return tmp;
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator::difference_type
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator-(const const_iterator& other) const
{
    return static_cast<difference_type>(m_logical_idx) - static_cast<difference_type>(other.m_logical_idx);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator::reference
PersistentDeque<T, BufferSizeBytes>::const_iterator::operator[](difference_type n) const
{
    return *(*this + n);
}

template <typename T, std::size_t BufferSizeBytes>
bool PersistentDeque<T, BufferSizeBytes>::const_iterator::operator==(const const_iterator& other) const
{
    return m_owner == other.m_owner && m_logical_idx == other.m_logical_idx;
}

template <typename T, std::size_t BufferSizeBytes>
bool PersistentDeque<T, BufferSizeBytes>::const_iterator::operator!=(const const_iterator& other) const
{
    return !(*this == other);
}

template <typename T, std::size_t BufferSizeBytes>
bool PersistentDeque<T, BufferSizeBytes>::const_iterator::operator<(const const_iterator& other) const
{
    return m_owner == other.m_owner && m_logical_idx < other.m_logical_idx;
}

template <typename T, std::size_t BufferSizeBytes>
bool PersistentDeque<T, BufferSizeBytes>::const_iterator::operator<=(const const_iterator& other) const
{
    return (*this < other) || (*this == other);
}

template <typename T, std::size_t BufferSizeBytes>
bool PersistentDeque<T, BufferSizeBytes>::const_iterator::operator>(const const_iterator& other) const
{
    return other < *this;
}

template <typename T, std::size_t BufferSizeBytes>
bool PersistentDeque<T, BufferSizeBytes>::const_iterator::operator>=(const const_iterator& other) const
{
    return !(*this < other);
}

template <typename T, std::size_t BufferSizeBytes>
void PersistentDeque<T, BufferSizeBytes>::const_iterator::MoveToLogicalIndex(std::size_t logical_idx)
{
    m_logical_idx = logical_idx;
    if (m_logical_idx == m_owner->m_size) {
        m_buffer_idx = m_owner->m_buffers.Size();
        m_idx_in_buffer = 0;
        return;
    }

    const auto [buffer_idx, idx_in_buffer] = m_owner->Locate(m_logical_idx);
    m_buffer_idx = buffer_idx;
    m_idx_in_buffer = idx_in_buffer;
}

//////////////////////////
// PersistentDeque impl //
//////////////////////////

template <typename T, std::size_t BufferSizeBytes>
template <typename U>
void PersistentDeque<T, BufferSizeBytes>::PushFrontImpl(U&& value)
{
    if (!m_buffers.IsEmpty()) {
        auto& front = FrontBlockRef();
        if (!front->IsFull()) [[likely]] {
            EnsureUnique(front);
            front->PushFront(std::forward<U>(value));
            ++m_size;
            return;
        }
    }

    m_buffers.IncFront();
    auto& front = FrontBlockRef();

    // \TODO Would be nice if we didn't always have to allocate new buffer
    // here (i.e., reuse freed buffers)
    front = std::make_shared<Buffer>();

    EnsureUnique(front);
    front->PushFront(std::forward<U>(value));
    ++m_size;
}

template <typename T, std::size_t BufferSizeBytes>
template <typename U>
void PersistentDeque<T, BufferSizeBytes>::PushBackImpl(U&& value)
{
    if (!m_buffers.IsEmpty()) {
        auto& back = BackBlockRef();
        if (!back->IsFull()) [[likely]] {
            EnsureUnique(back);
            back->PushBack(std::forward<U>(value));
            ++m_size;
            return;
        }
    }

    m_buffers.IncBack();
    auto& back = BackBlockRef();

    // \TODO Would be nice if we didn't always have to allocate new buffer
    // here (i.e., reuse freed buffers)
    back = std::make_shared<Buffer>();

    EnsureUnique(back);
    back->PushBack(std::forward<U>(value));
    ++m_size;
}

template <typename T, std::size_t BufferSizeBytes>
void PersistentDeque<T, BufferSizeBytes>::EnsureUnique(std::shared_ptr<Buffer>& block)
{
    // \TODO This doesn't guarantee strict persistence, it is only a heuristic
    // that accepts some flexibility for performance. Why doesn't it guarentee
    // strict persistence? Another thread can copy inbetween this conditional
    // and the mutatation that happens after this function.
    //
    // To guarantee strict persistence we can use a CAS loop on an atomic shared
    // pointer, but that would require a slight refactor.
    if (block.use_count() > 1) {
        block = std::make_shared<Buffer>(*block);
    }
}

template <typename T, std::size_t BufferSizeBytes>
std::pair<std::size_t, std::size_t> PersistentDeque<T, BufferSizeBytes>::Locate(std::size_t i) const
{
    if (i >= m_size) {
        throw std::runtime_error(
            "Out of bounds index (" + std::to_string(i) + ") with size " + std::to_string(m_size)
        );
    }

    // In front buffer?
    const std::size_t buffer_count = m_buffers.Size();
    if (buffer_count == 1) {
        return {0, i};
    }

    const std::size_t front_size = FrontBlockRef()->Size();
    if (i < front_size) {
        return {0, i};
    }
    i -= front_size;

    // In middle buffers?
    const std::size_t middle_count = buffer_count - 2;
    const std::size_t middle_size = middle_count * BufferCapacity;
    if (i < middle_size) {
        std::size_t middle_idx;
        std::size_t idx_in_buffer;

        if constexpr (BufferCapacityIsPowerOfTwo) {
            constexpr auto nbits = std::countr_zero(BufferCapacity);
            middle_idx = i >> nbits;
            idx_in_buffer = i & (BufferCapacity - 1);
        } else {
            middle_idx = i / BufferCapacity;
            idx_in_buffer = i % BufferCapacity;
        }

        return {1 + middle_idx, idx_in_buffer};
    }

    // Must be in last buffer
    return {buffer_count - 1, i - middle_size};
}

template <typename T, std::size_t BufferSizeBytes>
void PersistentDeque<T, BufferSizeBytes>::PushFront(const T& value)
{
    PushFrontImpl(value);
}

template <typename T, std::size_t BufferSizeBytes>
void PersistentDeque<T, BufferSizeBytes>::PushFront(T&& value)
{
    PushFrontImpl(std::move(value));
}

template <typename T, std::size_t BufferSizeBytes>
void PersistentDeque<T, BufferSizeBytes>::PushBack(const T& value)
{
    PushBackImpl(value);
}

template <typename T, std::size_t BufferSizeBytes>
void PersistentDeque<T, BufferSizeBytes>::PushBack(T&& value)
{
    PushBackImpl(std::move(value));
}

template <typename T, std::size_t BufferSizeBytes>
T PersistentDeque<T, BufferSizeBytes>::PopFront()
{
    if (m_size == 0) {
        throw std::runtime_error("Error when calling PopFront(): PersistentDeque is empty");
    }

    auto& front = FrontBlockRef();
    EnsureUnique(front);
    T value = front->PopFront();

    if (front->IsEmpty()) {
        m_buffers.DecFront();
    }

    --m_size;
    return value;
}

template <typename T, std::size_t BufferSizeBytes>
T PersistentDeque<T, BufferSizeBytes>::PopBack()
{
    if (m_size == 0) {
        throw std::runtime_error("Error when calling PopBack(): PersistentDeque is empty");
    }

    auto& back = BackBlockRef();
    EnsureUnique(back);
    T value = back->PopBack();

    if (back->IsEmpty()) {
        m_buffers.DecBack();
    }

    --m_size;
    return value;
}

// template <typename T, std::size_t BufferSizeBytes>
// T& PersistentDeque<T, BufferSizeBytes>::At(std::size_t i)
// {
//     const auto [buffer_idx, idx_in_buffer] = Locate(i);
//     auto& block = m_buffers[buffer_idx];
//     // Ensure unique since we're returning by non-const ref here, allowing the
//     // user to mutate. This isn't strict since the user can store in an l-value
//     // and mutate after deque copies have been made.
//     EnsureUnique(block);
//     return block->operator[](idx_in_buffer);
// }

template <typename T, std::size_t BufferSizeBytes>
const T& PersistentDeque<T, BufferSizeBytes>::At(std::size_t i) const
{
    const auto [buffer_idx, idx_in_buffer] = Locate(i);
    const auto& block = m_buffers[buffer_idx];
    return block->operator[](idx_in_buffer);
}

// template <typename T, std::size_t BufferSizeBytes>
// T& PersistentDeque<T, BufferSizeBytes>::operator[](std::size_t i)
// {
//     return this->At(i);
// }

template <typename T, std::size_t BufferSizeBytes>
const T& PersistentDeque<T, BufferSizeBytes>::operator[](std::size_t i) const
{
    return this->At(i);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator PersistentDeque<T, BufferSizeBytes>::begin() const
{
    return const_iterator(this, 0);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator PersistentDeque<T, BufferSizeBytes>::end() const
{
    return const_iterator(this, m_size);
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator PersistentDeque<T, BufferSizeBytes>::cbegin() const
{
    return begin();
}

template <typename T, std::size_t BufferSizeBytes>
typename PersistentDeque<T, BufferSizeBytes>::const_iterator PersistentDeque<T, BufferSizeBytes>::cend() const
{
    return end();
}
