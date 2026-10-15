#pragma once
#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "gameboy.hpp"
namespace imaging {
inline std::vector<u8> jpeg(std::span<const u32> pixels){if(pixels.size()!=160*144)return {};using Microsoft::WRL::ComPtr;
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);struct Apartment {HRESULT result;~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} apartment{initialized};
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf()))))return {};
    ComPtr<IStream> stream;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,stream.GetAddressOf())))return {};
    ComPtr<IWICBitmapEncoder> encoder;if(FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg,nullptr,encoder.GetAddressOf()))||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)))return {};
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;if(FAILED(encoder->CreateNewFrame(frame.GetAddressOf(),properties.GetAddressOf())))return {};
    PROPBAG2 option{};option.pstrName=const_cast<wchar_t*>(L"ImageQuality");VARIANT value{};value.vt=VT_R4;value.fltVal=0.92f;if(properties)properties->Write(1,&option,&value);
    WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;if(FAILED(frame->Initialize(properties.Get()))||FAILED(frame->SetSize(160,144))||FAILED(frame->SetPixelFormat(&format))||format!=GUID_WICPixelFormat24bppBGR)return {};
    std::vector<u8> bgr(pixels.size()*3);for(std::size_t i=0;i<pixels.size();++i){bgr[i*3]=(u8)pixels[i];bgr[i*3+1]=(u8)(pixels[i]>>8);bgr[i*3+2]=(u8)(pixels[i]>>16);}
    if(FAILED(frame->WritePixels(144,480,(UINT)bgr.size(),bgr.data()))||FAILED(frame->Commit())||FAILED(encoder->Commit()))return {};
    HGLOBAL memory=nullptr;if(FAILED(GetHGlobalFromStream(stream.Get(),&memory)))return {};STATSTG stat{};if(FAILED(stream->Stat(&stat,STATFLAG_NONAME)))return {};const auto size=(std::size_t)stat.cbSize.QuadPart;if(size>1024*1024)return {};
    const auto* data=static_cast<const u8*>(GlobalLock(memory));if(!data)return {};std::vector<u8> result(data,data+size);GlobalUnlock(memory);return result;
}
inline std::vector<u32> load(const std::filesystem::path& path,unsigned width,unsigned height){
    if(!width||!height||width>2048||height>2048)return {};using Microsoft::WRL::ComPtr;
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);struct Apartment {HRESULT result;~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} apartment{initialized};
    ComPtr<IWICImagingFactory> factory;HRESULT result=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf()));if(FAILED(result))return {};
    ComPtr<IWICBitmapDecoder> decoder;result=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,decoder.GetAddressOf());if(FAILED(result))return {};
    ComPtr<IWICBitmapFrameDecode> frame;if(FAILED(decoder->GetFrame(0,frame.GetAddressOf())))return {};UINT sourceW=0,sourceH=0;
    if(FAILED(frame->GetSize(&sourceW,&sourceH))||!sourceW||!sourceH||sourceW>16384||sourceH>16384||(u64)sourceW*sourceH>64*1024*1024)return {};
    ComPtr<IWICBitmapScaler> scaler;if(FAILED(factory->CreateBitmapScaler(scaler.GetAddressOf()))||FAILED(scaler->Initialize(frame.Get(),width,height,WICBitmapInterpolationModeFant)))return {};
    ComPtr<IWICFormatConverter> converter;if(FAILED(factory->CreateFormatConverter(converter.GetAddressOf()))||FAILED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    std::vector<u32> pixels((std::size_t)width*height);if(FAILED(converter->CopyPixels(nullptr,width*4,(UINT)pixels.size()*4,(BYTE*)pixels.data())))return {};return pixels;
}
inline HBITMAP thumbnail(const std::filesystem::path& path,unsigned width,unsigned height){const auto pixels=load(path,width,height);if(pixels.empty())return nullptr;
    BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=(LONG)width;info.bmiHeader.biHeight=-(LONG)height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* data=nullptr;
    HBITMAP bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&data,nullptr,0);if(bitmap&&data)std::memcpy(data,pixels.data(),pixels.size()*4);return bitmap;
}
} // namespace imaging
#endif
