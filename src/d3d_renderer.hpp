#pragma once
#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "gameboy.hpp"
#include "hq2x_shader.h"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

class GpuRenderer {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<IDXGISwapChain> swap;
    Ptr<ID3D11RenderTargetView> target;Ptr<ID3D11Texture2D> texture,previousTexture;Ptr<ID3D11ShaderResourceView> image,previousImage;
    Ptr<ID3D11VertexShader> vs;Ptr<ID3D11PixelShader> ps;Ptr<ID3D11SamplerState> sampler;Ptr<ID3D11Buffer> constants;
    HWND view=nullptr;unsigned width=0,height=0,sourceW=0,sourceH=0;bool software=false,history=false;u64 lastFrame=0;
    std::string error;
    bool compile(){
        static const char shader[]=R"(
cbuffer Settings:register(b0){float4 settings;float4 extras;};
Texture2D frame:register(t0);Texture2D previousFrame:register(t1);SamplerState pointSample:register(s0);
)";
        const std::string code=std::string(shader)+hq2x_shader+R"(
float3 scalePixel(float2 uv){
 float2 t=1/settings.xy;float3 e=frame.Sample(pointSample,uv).rgb;
 float3 a=frame.Sample(pointSample,uv+float2(-t.x,-t.y)).rgb,b=frame.Sample(pointSample,uv+float2(0,-t.y)).rgb,c=frame.Sample(pointSample,uv+float2(t.x,-t.y)).rgb;
 float3 d=frame.Sample(pointSample,uv+float2(-t.x,0)).rgb,f=frame.Sample(pointSample,uv+float2(t.x,0)).rgb;
 float3 g=frame.Sample(pointSample,uv+float2(-t.x,t.y)).rgb,h=frame.Sample(pointSample,uv+float2(0,t.y)).rgb,i=frame.Sample(pointSample,uv+t).rgb;
 if(all(b==h)||all(d==f))return e;
 if(settings.w<1.5){int2 p=(int2)floor(frac(uv*settings.xy)*2);
  if(p.y==0)return p.x==0?(all(d==b)?d:e):(all(b==f)?f:e);
  return p.x==0?(all(d==h)?d:e):(all(h==f)?f:e);}
 int2 p=(int2)floor(frac(uv*settings.xy)*3);
 if(p.y==0){if(p.x==0)return all(d==b)?d:e;if(p.x==2)return all(b==f)?f:e;return ((all(d==b)&&any(e!=c))||(all(b==f)&&any(e!=a)))?b:e;}
 if(p.y==1){if(p.x==0)return ((all(d==b)&&any(e!=g))||(all(d==h)&&any(e!=a)))?d:e;if(p.x==2)return ((all(b==f)&&any(e!=i))||(all(h==f)&&any(e!=c)))?f:e;return e;}
 if(p.x==0)return all(d==h)?d:e;if(p.x==2)return all(h==f)?f:e;return ((all(d==h)&&any(e!=i))||(all(h==f)&&any(e!=g)))?h:e;
}
struct V {float4 position:SV_Position;float2 uv:TEXCOORD;};
V vertex(uint id:SV_VertexID){V o;float2 p=float2((id<<1)&2,id&2);o.uv=p;o.position=float4(p*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 pixel(V v):SV_Target{
 float2 uv=v.uv,p=uv*2-1;float effect=settings.z;
 if(effect>1.5){uv=(p*(1+0.075*dot(p,p)))*0.5+0.5;if(any(uv<0)||any(uv>1))return float4(0,0,0,1);}
 float3 c=(settings.w>2.5&&settings.w<3.5)?hq2x(uv,settings.xy).rgb:((settings.w>0.5&&settings.w<2.5)?scalePixel(uv):frame.Sample(pointSample,uv).rgb);
 c=lerp(c,previousFrame.Sample(pointSample,uv).rgb,extras.y);
 if(extras.x>1.5)c=pow(saturate(c),1.15);else if(extras.x>0.5)c=float3(dot(c,float3(26,4,2)),dot(c,float3(2,24,6)),dot(c,float3(4,4,24)))/32;
 if(effect>1.5){float2 texel=1/settings.xy;float3 glow=0;
  glow+=frame.Sample(pointSample,uv+float2(texel.x,0)).rgb;
  glow+=frame.Sample(pointSample,uv-float2(texel.x,0)).rgb;
  glow+=frame.Sample(pointSample,uv+float2(0,texel.y)).rgb;
  glow+=frame.Sample(pointSample,uv-float2(0,texel.y)).rgb;
  c=c*0.86+glow*0.06;c*=0.82+0.18*cos(uv.y*settings.y*6.2831853);
  c*=saturate(1-0.23*dot(p,p));
 }else if(effect>0.5){float2 local=frac(uv*settings.xy);c*=0.80+0.20*step(0.12,local.x)*step(0.12,local.y);}
 return float4(saturate(c),1);
})";
        Ptr<ID3DBlob> vertex,pixel,errors;
        HRESULT hr=D3DCompile(code.data(),code.size(),nullptr,nullptr,nullptr,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,vertex.GetAddressOf(),errors.GetAddressOf());
        if(SUCCEEDED(hr))hr=D3DCompile(code.data(),code.size(),nullptr,nullptr,nullptr,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,pixel.GetAddressOf(),errors.ReleaseAndGetAddressOf());
        if(FAILED(hr)){error=errors?std::string((char*)errors->GetBufferPointer(),errors->GetBufferSize()):"Shader compilation failed.";return false;}
        hr=device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,vs.GetAddressOf());
        if(SUCCEEDED(hr))hr=device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,ps.GetAddressOf());
        D3D11_SAMPLER_DESC sampling{};sampling.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sampling.AddressU=sampling.AddressV=sampling.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampling.MaxLOD=D3D11_FLOAT32_MAX;
        if(SUCCEEDED(hr))hr=device->CreateSamplerState(&sampling,sampler.GetAddressOf());D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=32;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(SUCCEEDED(hr))hr=device->CreateBuffer(&buffer,nullptr,constants.GetAddressOf());return SUCCEEDED(hr);
    }
    bool resize(unsigned w,unsigned h){if(width==w&&height==h&&target)return true;context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();
        HRESULT hr=swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0);Ptr<ID3D11Texture2D> back;if(SUCCEEDED(hr))hr=swap->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)back.GetAddressOf());
        if(SUCCEEDED(hr))hr=device->CreateRenderTargetView(back.Get(),nullptr,target.GetAddressOf());if(FAILED(hr)){error="Direct3D resize failed.";return false;}width=w;height=h;return true;}
    bool upload(std::span<const u32> pixels,unsigned w,unsigned h,u64 stamp){
        if(sourceW!=w||sourceH!=h||!texture){image.Reset();previousImage.Reset();texture.Reset();previousTexture.Reset();history=false;D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            HRESULT hr=device->CreateTexture2D(&desc,nullptr,texture.GetAddressOf());if(SUCCEEDED(hr))hr=device->CreateShaderResourceView(texture.Get(),nullptr,image.GetAddressOf());if(SUCCEEDED(hr))hr=device->CreateTexture2D(&desc,nullptr,previousTexture.GetAddressOf());if(SUCCEEDED(hr))hr=device->CreateShaderResourceView(previousTexture.Get(),nullptr,previousImage.GetAddressOf());if(FAILED(hr))return false;sourceW=w;sourceH=h;}
        if(history&&lastFrame!=stamp)context->CopyResource(previousTexture.Get(),texture.Get());
        context->UpdateSubresource(texture.Get(),0,nullptr,pixels.data(),w*4,0);if(!history)context->UpdateSubresource(previousTexture.Get(),0,nullptr,pixels.data(),w*4,0);history=true;lastFrame=stamp;return true;}
public:
    ~GpuRenderer(){close();}
    bool initialize(HWND parent){if(device)return true;software=false;
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"GBSGpuView09";
        cls.lpfnWndProc=[](HWND w,UINT msg,WPARAM wp,LPARAM lp)->LRESULT{if(msg==WM_ERASEBKGND)return 1;
            if(msg==WM_PAINT){PAINTSTRUCT ps{};BeginPaint(w,&ps);EndPaint(w,&ps);return 0;}return DefWindowProcW(w,msg,wp,lp);};RegisterClassW(&cls);
        view=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_CLIPSIBLINGS,0,0,160,144,parent,nullptr,cls.hInstance,nullptr);if(!view)return false;
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=view;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
        HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,2,D3D11_SDK_VERSION,&desc,swap.GetAddressOf(),device.GetAddressOf(),&level,context.GetAddressOf());
        if(FAILED(hr)){software=true;hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,2,D3D11_SDK_VERSION,&desc,swap.GetAddressOf(),device.GetAddressOf(),&level,context.GetAddressOf());}
        if(FAILED(hr)||!compile()){error="Direct3D 11 is unavailable. CPU display is active.";close();return false;}return true;}
    bool draw(HWND parent,const RECT& rect,std::span<const u32> pixels,unsigned w,unsigned h,int effect,int scaler=0,int color=0,float ghost=0,u64 stamp=0){
        if(rect.right<=rect.left||rect.bottom<=rect.top||pixels.size()!=(std::size_t)w*h)return false;
        if(!initialize(parent))return false;const unsigned rw=(unsigned)(rect.right-rect.left),rh=(unsigned)(rect.bottom-rect.top);
        SetWindowPos(view,nullptr,rect.left,rect.top,(int)rw,(int)rh,SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW);
        if(!resize(rw,rh)||!upload(pixels,w,h,stamp)){hide();return false;}
        const float params[]={(float)w,(float)h,(float)effect,(float)scaler,(float)color,ghost,0,0};context->UpdateSubresource(constants.Get(),0,nullptr,params,0,0);
        const float background[]={0,0,0,1};context->ClearRenderTargetView(target.Get(),background);ID3D11RenderTargetView* t=target.Get();context->OMSetRenderTargets(1,&t,nullptr);
        D3D11_VIEWPORT viewport{0,0,(float)rw,(float)rh,0,1};context->RSSetViewports(1,&viewport);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(nullptr);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);ID3D11ShaderResourceView* images[]={image.Get(),previousImage.Get()};ID3D11SamplerState* s=sampler.Get();ID3D11Buffer* b=constants.Get();
        context->PSSetShaderResources(0,2,images);context->PSSetSamplers(0,1,&s);context->PSSetConstantBuffers(0,1,&b);context->Draw(3,0);
        if(FAILED(swap->Present(0,0))){error="Direct3D device was lost. CPU display is active.";close();return false;}return true;}
    void resetHistory(){history=false;}
    void hide(){if(view)ShowWindow(view,SW_HIDE);}
    void close(){if(context){context->ClearState();context->Flush();}target.Reset();image.Reset();previousImage.Reset();texture.Reset();previousTexture.Reset();history=false;vs.Reset();ps.Reset();sampler.Reset();constants.Reset();swap.Reset();context.Reset();device.Reset();width=height=sourceW=sourceH=0;
        if(view)DestroyWindow(view);view=nullptr;}
    const std::string& lastError()const{return error;}bool isSoftware()const{return software;}
};
#endif
