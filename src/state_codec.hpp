#pragma once
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

// Field encoding shared by the current native-state extension and BESS.
// Booleans are one byte, ints are four bytes, and every number is little-endian.
// Decoding always operates on a temporary machine before any live state commits.
namespace native_state {
struct Codec {
    std::vector<std::uint8_t> output;
    std::span<const std::uint8_t> input;
    std::size_t at=0;
    bool reading=false,ok=true;
    explicit Codec(std::span<const std::uint8_t> bytes):input(bytes),reading(true){}
    Codec()=default;
    template<class T>void field(T& value){
        constexpr unsigned size=std::is_same_v<T,bool>?1:std::is_same_v<T,int>?4:sizeof(T);
        static_assert(size<=8);
        std::uint64_t v=0;
        if(reading){
            if(at>input.size()||size>input.size()-at){ok=false;return;}
            for(unsigned i=0;i<size;++i)v|=std::uint64_t(input[at++])<<(8*i);
            if constexpr(std::is_same_v<T,bool>){if(v>1){ok=false;return;}value=v!=0;}
            else if constexpr(std::is_same_v<T,double>){value=std::bit_cast<double>(v);if(!std::isfinite(value))ok=false;}
            else if constexpr(std::is_same_v<T,int>)value=static_cast<int>(static_cast<std::int32_t>(static_cast<std::uint32_t>(v)));
            else value=static_cast<T>(v);
        }else{
            if constexpr(std::is_same_v<T,double>)v=std::bit_cast<std::uint64_t>(value);
            else v=static_cast<std::uint64_t>(value);
            for(unsigned i=0;i<size;++i)output.push_back(static_cast<std::uint8_t>(v>>(8*i)));
        }
    }
    template<class... T>void fields(T&... values){(field(values),...);}
    template<class T>void array(T& values){for(auto& value:values)field(value);}
};

// The peripheral modules own their schema and range checks alongside the
// implementation. Copy only their small extra state when saving a const core.
template<class Device>void writeAccuracy(const Device& device,Codec& codec){
    auto state=device.accuracy;Device::visitAccuracy(state,codec);
}
template<class Device>void readAccuracy(Device& device,Codec& codec){
    device.visitAccuracy(codec);
}
template<class Device>bool validAccuracy(const Device& device){
    return device.accuracyValid();
}
template<class Device>void restoreLegacy(Device& device){
    device.restoreLegacyAccuracy();
}
template<class Device>void commitAccuracy(Device& live,const Device& restored){
    live.accuracy=restored.accuracy;
}
} // namespace native_state
