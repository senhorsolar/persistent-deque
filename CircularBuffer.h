#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T, std::size_t N=512>
class CircularBuffer;

template <typename T>
using DynamicCircularBuffer = CircularBuffer<T, 0>;

template <typename T, std::size_t N>
class CircularBuffer
{
public:
    static constexpr bool IsDynamic = (N == 0);
    static constexpr bool CapacityIsPowerOfTwo = (!IsDynamic) && ((N & (N - 1)) == 0);

    CircularBuffer()
    {
        if constexpr (IsDynamic) {
            m_capacity = 0;
        }
    }

    explicit CircularBuffer(std::size_t capacity)
    {
        static_assert(IsDynamic, "Can only specify runtime capacity for DynamicCircularBuffer");
        m_data.resize(capacity);
        m_capacity = capacity;
    }

    inline std::size_t Capacity() const {
        if constexpr (IsDynamic) {
            return m_capacity;
        }
        return N;
    }

    inline std::size_t Size() const {
        return m_size;
    }

    inline bool IsEmpty() const {
        return m_size == 0;
    }

    inline bool IsFull() const {
        return m_size == Capacity();
    }

    T& Front() {
        return At(0);
    }

    const T& Front() const {
        return At(0);
    }

    T& Back() {
        return At(m_size - 1);
    }

    const T& Back() const {
        return At(m_size - 1);
    }

    void PushFront(const T& value) {
        EmplaceFront(value);
    }

    void PushFront(T&& value) {
        EmplaceFront(std::move(value));
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T& EmplaceFront(Args&&... args)
    {
        IncFront();
        m_data[m_start] = T(std::forward<Args>(args)...);
        return m_data[m_start];
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        IncBack();
        const std::size_t rear = TranslateIndex(m_size - 1);
        m_data[rear] = T(std::forward<Args>(args)...);
        return m_data[rear];
    }

    T PopFront()
    {
        if (IsEmpty()) {
            throw std::runtime_error("Error when calling PopFront(): CircularBuffer is empty");
        }

        T value = std::move(m_data[m_start]);
        DecFront();
        return value;
    }

    T PopBack()
    {
        if (IsEmpty()) {
            throw std::runtime_error("Error when calling PopBack(): CircularBuffer is empty");
        }

        const std::size_t rear = TranslateIndex(m_size - 1);
        T value = std::move(m_data[rear]);
        DecBack();
        return value;
    }

    void IncFront()
    {
        EnsureCapacityForInsert();
        m_start = PrevIndex(m_start);
        ++m_size;
    }

    void IncBack()
    {
        EnsureCapacityForInsert();
        ++m_size;
    }

    void DecFront()
    {
        if (IsEmpty()) {
            throw std::runtime_error("Error when calling PopFront(): CircularBuffer is empty");
        }

        m_start = NextIndex(m_start);
        --m_size;
    }

    void DecBack()
    {
        if (IsEmpty()) {
            throw std::runtime_error("Error when calling PopBack(): CircularBuffer is empty");
        }

        --m_size;
    }

    inline T& operator[](std::size_t i) {
        return m_data[TranslateIndex(i)];
    }

    inline const T& operator[](std::size_t i) const {
        return m_data[TranslateIndex(i)];
    }

    T& At(std::size_t i)
    {
        if (i >= m_size) {
            throw std::runtime_error(
                "Out of bounds index (" + std::to_string(i) + ") with size " + std::to_string(m_size)
            );
        }
        return (*this)[i];
    }

    const T& At(std::size_t i) const
    {
        if (i >= m_size) {
            throw std::runtime_error(
                "Out of bounds index (" + std::to_string(i) + ") with size " + std::to_string(m_size)
            );
        }
        return (*this)[i];
    }

    void DebugPrint(std::ostream& os=std::cout, const std::string& prefix="CircularBuffer values") const
    {
        if (!prefix.empty()) {
            os << prefix << ":\n-- ";
        }

        for (std::size_t i = 0; i < m_size; ++i) {
            os << (*this)[i] << ((i + 1 == m_size) ? '\n' : ' ');
        }
    }

    void Resize(std::size_t capacity)
    {
        static_assert(IsDynamic, "Resize only available for DynamicCircularBuffer");

        if (capacity == m_capacity) {
            return;
        }

        // We really only resize to increase capacity, maybe shouldn't use the name 'Resize'
        if (capacity < m_size) {
            throw std::runtime_error(
                "Error when calling Resize(): new capacity " + std::to_string(capacity) +
                " is smaller than current size " + std::to_string(m_size)
            );
        }

        StorageType new_data;
        new_data.resize(capacity);
        for (std::size_t i = 0; i < m_size; ++i) {
            new_data[i] = std::move((*this)[i]);
        }

        m_data = std::move(new_data);
        m_capacity = capacity;
        m_start = 0;
    }

private:

    inline std::size_t NextIndex(std::size_t idx) const {
        return (idx + 1 == Capacity()) ? 0 : idx + 1;
    }

    inline std::size_t PrevIndex(std::size_t idx) const {
        return (idx == 0) ? (Capacity() - 1) : idx - 1;
    }

    inline std::size_t TranslateIndex(std::size_t i) const {
        std::size_t idx = m_start + i;

        if constexpr (!IsDynamic && CapacityIsPowerOfTwo) {
            return idx & (N - 1);
        } else {
            // This isn't entirely correct since it's possible that idx >=
            // 2*capacity, but modulo/division is slow. This would happen if i >=
            // capacity, but can be avoided altogether with .At(i).
            const std::size_t cap = Capacity();
            return (idx >= cap) ? idx - cap : idx;
        }
    }

    void EnsureCapacityForInsert()
    {
        if (!IsFull()) {
            return;
        }

        if constexpr (IsDynamic) {
            const std::size_t new_capacity = (m_capacity == 0) ? 1 : (2 * m_capacity + 1);
            Resize(new_capacity);
        } else {
            throw std::runtime_error("Error when inserting: fixed CircularBuffer is full");
        }
    }

    // \TODO Ideally we'd use storage that doesn't have the same restrictions as
    // vector/array (i.e., default ctor for our uses)
    using StorageType = std::conditional_t<IsDynamic, std::vector<T>, std::array<T, N>>;

    StorageType m_data{};
    std::size_t m_start = 0;
    std::size_t m_size = 0;
    std::size_t m_capacity = IsDynamic ? 0 : N;
};
