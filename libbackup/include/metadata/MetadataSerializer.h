#ifndef BACKUP_METADATASERIALIZER_H
#define BACKUP_METADATASERIALIZER_H

#include "core/FileInfo.h"
#include <vector>
#include <string>
#include <cstdint>

/**
 * 将 std::vector<FileInfo> 序列化为平坦字节流，或反序列化回来。
 *
 * 为什么需要序列化？
 * FileInfo 内部有 std::string、std::vector<uint8_t> 等 C++ 对象，
 * 它们的数据存储在堆上，对象本体只存指针。直接 write(&obj, sizeof(obj))
 * 只会把指针值写出去，读回来时指针变成悬垂指针，程序崩溃。
 * 序列化逐字段将实际数据写入字节数组，反序列化时按相同格式解析并重建对象。
 */
class MetadataSerializer {
public:
    /// 序列化：FileInfo 列表 → 字节流
    std::vector<uint8_t> serialize(const std::vector<FileInfo>& files);

    /// 反序列化：字节流 → FileInfo 列表
    std::vector<FileInfo> deserialize(const std::vector<uint8_t>& data);

private:
    void writeUint16(std::vector<uint8_t>& buf, uint16_t val);
    void writeUint32(std::vector<uint8_t>& buf, uint32_t val);
    void writeUint64(std::vector<uint8_t>& buf, uint64_t val);
    void writeString(std::vector<uint8_t>& buf, const std::string& s);
    void writeBytes(std::vector<uint8_t>& buf, const uint8_t* data, size_t len);

    uint16_t readUint16(const uint8_t* data, size_t& offset);
    uint32_t readUint32(const uint8_t* data, size_t& offset);
    uint64_t readUint64(const uint8_t* data, size_t& offset);
    std::string readString(const uint8_t* data, size_t& offset);
    std::vector<uint8_t> readBytes(const uint8_t* data, size_t& offset, size_t len);
};

#endif // BACKUP_METADATASERIALIZER_H
