#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <bit>
#include <cstring>
#include <stdexcept>

const bool DEBUGGING = true;



/**
 * Represents a single user chat message or event (like file, typing, etc.).
 */
struct ChatMessage {
    std::string sender;       // username
    std::string text;         // message text (or /command)
    std::string room;         // the room/channel to which this message belongs
    bool isFile = false;      // if true, we interpret fileData as an attached file
    std::string fileName;     // if isFile is true, the filename
    std::vector<std::uint8_t> fileData; // the actual file bytes (if isFile==true)
    
    // A flag to indicate that this is a "user is typing" or "system" message
    bool isTyping = false;
};

/**
 * @brief Serialize and deserialize the ChatMessage struct.
 * 
 * The serialization format is as follows:
 *
 * 
 * Basically:
 * Here we define a mini library for serialization/deserialization
*/

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
     * @brief Swap the bytes of an integral value.
     *  Example bytes: 0x12345678
     * After swap: 0x78563412
     * Needed because MacOS WONT LET ME USE C++23 FEATURES WHY APPLE WHY
     * 
     * @tparam T the type of the value to swap
     * @param value the value to swap
     * @return T the swapped value
     */
    template<typename T>
    T byteswap(T value) {
        static_assert(std::is_integral_v<T>, "Integral type required");
        auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
        std::reverse(bytes.begin(), bytes.end());
        return std::bit_cast<T>(bytes);
        //       T result = 0;
        //       for(size_t i = 0; i < sizeof(T); ++i)
        //          result = (result << 8) | ((value >> (i * 8)) & 0xFF);
    }
    
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
            value : byteswap(value);
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
            value : byteswap(value);
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
    // auto serialize = [](const ChatMessage& msg) -> ByteBuffer {
    //     return [sender_bytes = serialize_string(msg.sender), text_bytes = serialize_string(msg.text), &msg]() mutable {
    //         sender_bytes.insert(sender_bytes.end(), text_bytes.begin(), text_bytes.end());
    //         return sender_bytes;
    //     }();
    // };

    /**
     * The "full" ChatMessage serialization. 
     * We'll do it in steps:
     *   1) bool isFile
     *   2) bool isTyping
     *   3) sender, text, room, fileName
     *   4) fileData (if isFile==true)
     */
    inline ByteBuffer serialize(const ChatMessage& msg) {
        ByteBuffer out;
        // Serialize booleans as a single byte each (0 or 1)
        out.push_back(msg.isFile ? 1 : 0);
        out.push_back(msg.isTyping ? 1 : 0);
        // sender, text, room, fileName
        auto s1 = serialize_string(msg.sender);
        auto s2 = serialize_string(msg.text);
        auto s3 = serialize_string(msg.room);
        auto s4 = serialize_string(msg.fileName);
        out.insert(out.end(), s1.begin(), s1.end());
        out.insert(out.end(), s2.begin(), s2.end());
        out.insert(out.end(), s3.begin(), s3.end());
        out.insert(out.end(), s4.begin(), s4.end());
        // fileData if isFile
        if(msg.isFile) {
            // We store fileData length + raw bytes
            Int32 len = (Int32)msg.fileData.size();
            auto lenBytes = serialize_value<Int32>(len);
            out.insert(out.end(), lenBytes.begin(), lenBytes.end());
            out.insert(out.end(), msg.fileData.begin(), msg.fileData.end());
        }
        return out;
    }

    /**
     * @brief Deserialize a chat message from a byte buffer.
     *  The message is deserialized as two strings: the sender and the text.
     * 
     * @param data The byte buffer to deserialize the message from.
     * @return ChatMessage The deserialized message.
     */
    // auto deserialize = [](ByteView& data) -> ChatMessage {return {deserialize_string(data), deserialize_string(data)};};
    inline ChatMessage deserialize(ByteView& data) {
        ChatMessage msg;
        if(data.size() < 2) throw std::runtime_error("Not enough data for booleans");
        msg.isFile   = (data[0] != 0);
        msg.isTyping = (data[1] != 0);
        data = data.subspan(2);

        msg.sender   = deserialize_string(data);
        msg.text     = deserialize_string(data);
        msg.room     = deserialize_string(data);
        msg.fileName = deserialize_string(data);

        if(msg.isFile) {
            // read Int32 length + that many bytes
            Int32 len = deserialize_value<Int32>(data);
            if(data.size() < (size_t)len) throw std::runtime_error("File data truncated");
            msg.fileData.insert(msg.fileData.end(), data.begin(), data.begin()+len);
            data = data.subspan(len);
        }
        return msg;
    }
};