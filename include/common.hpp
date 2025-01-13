#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <bit>
#if __cplusplus > 202002L
#error "C++23 or later"
#elif __cplusplus > 201703L
#error "C++20"
#elif __cplusplus > 201402L
#error "C++17"
#else
#error "C++14 or earlier"
#endif

struct ChatMessage { std::string sender, text; };

namespace ChatSerDes {
    // uint8_t as a byte
    using Byte = std::uint8_t;
    // vector of bytes as a byte buffer
    using ByteBuffer = std::vector<Byte>;
    // span of bytes as a byte view as it is a view of the buffer
    using ByteView = std::span<const Byte>;
    // uint32_t as a 32-bit integer
    using Int32 = std::uint32_t;
    
    /**
     * @brief Serialize a value into a byte buffer.
     *      The value is converted to little-endian if the native endianness is big-endian.
     * 
     * @tparam T The type of the value to serialize.
     * @param value The value to serialize.
     * @return ByteBuffer The serialized value.
     */
    template<typename T>
    ByteBuffer serialize_value(const T& value) {
        ByteBuffer bytes(sizeof(T));
        auto le_value = std::endian::native == std::endian::little ? 
            value : std::byteswap(value);
        std::memcpy(bytes.data(), &le_value, sizeof(T));
        return bytes;
    }

    // ByteBuffer serialize(const std::string& str) {
    //     ByteBuffer bytes = serialize(static_cast<Int32>(str.size()));
    //     bytes.insert(bytes.end(), str.begin(), str.end());
    //     return bytes;
    // }

    /**
     * @brief Serialize a string into a byte buffer.
     *    The string is serialized as a 32-bit integer representing its size followed by its characters.
     * 
     * @param str The string to serialize.
     * @return ByteBuffer The serialized string.
     */
    auto serialize_string = [](const std::string& str) -> ByteBuffer {
        return [size_bytes = serialize_value(static_cast<Int32>(str.size())), &str]() mutable {
            size_bytes.insert(size_bytes.end(), str.begin(), str.end());
            return size_bytes;
        }();
    };

    // ByteBuffer serialize(const ChatMessage& msg) {
    //     auto sender_bytes = serialize(msg.sender);
    //     auto text_bytes = serialize(msg.text);
    //     sender_bytes.insert(sender_bytes.end(), text_bytes.begin(), text_bytes.end());
    //     return sender_bytes;
    // }

    /**
     * @brief Serialize a chat message into a byte buffer.
     *    The message is serialized as two strings: the sender and the text.
     * 
     * @param msg The message to serialize.
     * @return ByteBuffer The serialized message.
     */
    auto serialize = [](const ChatMessage& msg) -> ByteBuffer {
        return [sender_bytes = serialize_string(msg.sender), text_bytes = serialize_string(msg.text), &msg]() mutable {
            sender_bytes.insert(sender_bytes.end(), text_bytes.begin(), text_bytes.end());
            return sender_bytes;
        }();
    };

    /**
     * @brief Deserialize a value from a byte buffer.
     *    The value is converted from little-endian to the native endianness if it is big-endian.
     * 
     * @tparam T The type of the value to deserialize.
     * @param data The byte buffer to deserialize the value from.
     * @return T The deserialized value.
     */
    template<typename T>
    T deserialize_value(ByteView& data) {
        if (data.size() < sizeof(T)) throw std::runtime_error("Insufficient data");
        T value;
        std::memcpy(&value, data.data(), sizeof(T));
        value = std::endian::native == std::endian::little ? 
            value : std::byteswap(value);
        data = data.subspan(sizeof(T));
        return value;
    }

    /**
     * @brief Deserialize a string from a byte buffer
     *   The string is deserialized from a 32-bit integer representing its size followed by its characters.
     * 
     * @param data The byte buffer to deserialize the string from.
     * @return std::string The deserialized string.
     */
    auto deserialize_string = [](ByteView& data) {
        auto size = deserialize_value<Int32>(data);
        if (data.size() < size) throw std::runtime_error("Insufficient data");
        auto str = std::string(reinterpret_cast<const char*>(data.data()), size);
        data = data.subspan(size);
        return str;
    };

    /**
     * @brief Deserialize a chat message from a byte buffer.
     *  The message is deserialized as two strings: the sender and the text.
     * 
     * @param data The byte buffer to deserialize the message from.
     * @return ChatMessage The deserialized message.
     */
    auto deserialize = [](ByteView& data) -> ChatMessage {return {deserialize_string(data), deserialize_string(data)};};
};