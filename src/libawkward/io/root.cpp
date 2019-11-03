// BSD 3-Clause License; see https://github.com/jpivarski/awkward-1.0/blob/master/LICENSE

#include <cstring>

#include "awkward/Content.h"
#include "awkward/Identity.h"
#include "awkward/array/ListOffsetArray.h"
#include "awkward/fillable/GrowableBuffer.h"

#include "awkward/io/root.h"

namespace awkward {
  void FromROOT_nestedvector_fill(std::vector<GrowableBuffer<int64_t>>& levels, GrowableBuffer<int64_t>& bytepos_tocopy, int64_t& bytepos, const NumpyArray& rawdata, int64_t whichlevel, int64_t itemsize) {
    if (whichlevel == levels.size()) {
      bytepos_tocopy.append(bytepos);
      bytepos += itemsize;
    }

    else {
      uint32_t bigendian = *reinterpret_cast<uint32_t*>(rawdata.byteptr((ssize_t)bytepos));

      // FIXME: check native endianness
      uint32_t length = ((bigendian >> 24) & 0xff)     |  // move byte 3 to byte 0
                        ((bigendian <<  8) & 0xff0000) |  // move byte 1 to byte 2
                        ((bigendian >>  8) & 0xff00)   |  // move byte 2 to byte 1
                        ((bigendian << 24) & 0xff000000); // byte 0 to byte 3

      bytepos += sizeof(int32_t);
      for (uint32_t i = 0;  i < length;  i++) {
        FromROOT_nestedvector_fill(levels, bytepos_tocopy, bytepos, rawdata, whichlevel + 1, itemsize);
      }
      int64_t previous = levels[(unsigned int)whichlevel].getitem_at_unsafe(levels[(unsigned int)whichlevel].length() - 1);
      levels[(unsigned int)whichlevel].append(previous + length);
    }
  }

  const std::shared_ptr<Content> FromROOT_nestedvector(const Index64& byteoffsets, const NumpyArray& rawdata, int64_t depth, int64_t itemsize, std::string format, const FillableOptions& options) {
    assert(depth > 0);
    assert(rawdata.ndim() == 1);

    Index64 level0(byteoffsets.length());
    level0.setitem_at_unsafe(0, 0);

    std::vector<GrowableBuffer<int64_t>> levels;
    for (int64_t i = 0;  i < depth;  i++) {
      levels.push_back(GrowableBuffer<int64_t>(options));
      levels[(size_t)i].append(0);
    }

    GrowableBuffer<int64_t> bytepos_tocopy(options);

    for (int64_t i = 0;  i < byteoffsets.length() - 1;  i++) {
      int64_t bytepos = byteoffsets.getitem_at_unsafe(i);
      FromROOT_nestedvector_fill(levels, bytepos_tocopy, bytepos, rawdata, 0, itemsize);
      level0.setitem_at_unsafe(i + 1, levels[0].length());
    }

    std::shared_ptr<void> ptr(new uint8_t[(size_t)(bytepos_tocopy.length()*itemsize)], awkward::util::array_deleter<uint8_t>());
    ssize_t offset = rawdata.byteoffset();
    uint8_t* toptr = reinterpret_cast<uint8_t*>(ptr.get());
    uint8_t* fromptr = reinterpret_cast<uint8_t*>(rawdata.ptr().get());
    for (int64_t i = 0;  i < bytepos_tocopy.length();  i++) {
      ssize_t bytepos = (ssize_t)bytepos_tocopy.getitem_at_unsafe(i);
      std::memcpy(&toptr[(ssize_t)(i*itemsize)], &fromptr[offset + bytepos], (size_t)itemsize);
    }

    std::vector<ssize_t> shape = { (ssize_t)bytepos_tocopy.length() };
    std::vector<ssize_t> strides = { (ssize_t)itemsize };
    std::shared_ptr<Content> out(new NumpyArray(Identity::none(), ptr, shape, strides, 0, (ssize_t)itemsize, format));

    for (int64_t i = depth - 1;  i >= 0;  i--) {
      out = std::shared_ptr<Content>(new ListOffsetArray64(Identity::none(), levels[(size_t)i].toindex(), out));
    }
    return out;
  }

}