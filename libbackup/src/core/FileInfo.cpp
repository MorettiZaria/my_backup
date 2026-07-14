#include "core/FileInfo.h"
#include <cstring>

FileInfo::FileInfo()
    : fileType(0), permissions(0), owner(0), group(0),
      fileSize(0), atime(0), mtime(0), ctime(0), deviceId(0) {}

void FileInfo::fromStat(const struct stat& st, const std::string& relPath) {
    relativePath = relPath;
    fileType     = st.st_mode & S_IFMT;
    permissions  = st.st_mode & ~S_IFMT;
    owner        = st.st_uid;
    group        = st.st_gid;
    fileSize     = (isRegular()) ? st.st_size : 0;
#ifdef __APPLE__
    atime        = st.st_atimespec.tv_sec;
    mtime        = st.st_mtimespec.tv_sec;
    ctime        = st.st_ctimespec.tv_sec;
#else
    atime        = st.st_atim.tv_sec;
    mtime        = st.st_mtim.tv_sec;
    ctime        = st.st_ctim.tv_sec;
#endif
    deviceId     = 0;
    symlinkTarget.clear();
}

bool FileInfo::isRegular() const     { return fileType == S_IFREG; }
bool FileInfo::isDirectory() const  { return fileType == S_IFDIR; }
bool FileInfo::isSymlink() const    { return fileType == S_IFLNK; }
bool FileInfo::isFifo() const       { return fileType == S_IFIFO; }
bool FileInfo::isBlockDevice() const { return fileType == S_IFBLK; }
bool FileInfo::isCharDevice() const { return fileType == S_IFCHR; }
bool FileInfo::isSocket() const     { return fileType == S_IFSOCK; }
bool FileInfo::isDevice() const     { return isBlockDevice() || isCharDevice(); }

// ===== 序列化格式 =====
// [pathLen:2B][path:N B]
// [type:2B][perm:2B][uid:4B][gid:4B]
// [size:8B][atime:8B][mtime:8B][ctime:8B]
// [linkLen:2B][linkTarget:N B]   // 仅 S_IFLNK
// [devId:8B]                     // 仅 S_IFBLK / S_IFCHR

static void writeUint16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

static void writeUint32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

static void writeUint64(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

static void writeString(std::vector<uint8_t>& buf, const std::string& s) {
    writeUint16(buf, static_cast<uint16_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

static uint16_t readUint16(const uint8_t* data, size_t& offset) {
    uint16_t v = static_cast<uint16_t>(data[offset]) |
                 (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;
    return v;
}

static uint32_t readUint32(const uint8_t* data, size_t& offset) {
    uint32_t v = static_cast<uint32_t>(data[offset]) |
                 (static_cast<uint32_t>(data[offset + 1]) << 8) |
                 (static_cast<uint32_t>(data[offset + 2]) << 16) |
                 (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return v;
}

static uint64_t readUint64(const uint8_t* data, size_t& offset) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    return v;
}

static std::string readString(const uint8_t* data, size_t& offset) {
    uint16_t len = readUint16(data, offset);
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return s;
}

std::vector<uint8_t> FileInfo::serialize() const {
    std::vector<uint8_t> buf;

    writeString(buf, relativePath);
    writeUint16(buf, static_cast<uint16_t>(fileType));
    writeUint16(buf, static_cast<uint16_t>(permissions));
    writeUint32(buf, static_cast<uint32_t>(owner));
    writeUint32(buf, static_cast<uint32_t>(group));
    writeUint64(buf, static_cast<uint64_t>(fileSize));
    writeUint64(buf, static_cast<uint64_t>(atime));
    writeUint64(buf, static_cast<uint64_t>(mtime));
    writeUint64(buf, static_cast<uint64_t>(ctime));

    // 符号链接目标
    writeString(buf, symlinkTarget);

    // 设备号（仅设备文件）
    if (isDevice()) {
        writeUint64(buf, static_cast<uint64_t>(deviceId));
    }

    return buf;
}

FileInfo FileInfo::deserialize(const uint8_t* data, size_t& offset) {
    FileInfo info;

    info.relativePath = readString(data, offset);
    info.fileType     = static_cast<mode_t>(readUint16(data, offset));
    info.permissions  = static_cast<mode_t>(readUint16(data, offset));
    info.owner        = static_cast<uid_t>(readUint32(data, offset));
    info.group        = static_cast<gid_t>(readUint32(data, offset));
    info.fileSize     = static_cast<off_t>(readUint64(data, offset));
    info.atime        = static_cast<time_t>(readUint64(data, offset));
    info.mtime        = static_cast<time_t>(readUint64(data, offset));
    info.ctime        = static_cast<time_t>(readUint64(data, offset));
    info.symlinkTarget = readString(data, offset);

    if (info.isDevice()) {
        info.deviceId = static_cast<dev_t>(readUint64(data, offset));
    }

    return info;
}
