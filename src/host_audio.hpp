#pragma once
#ifndef _WIN32
#error HostAudio is a Windows audio backend; use audio_buffer.hpp for portable buffers and WAV export.
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmsystem.h>
#include <future>
#include <mutex>
#include <thread>
#include "audio_buffer.hpp"
#include "gameboy.hpp"
#ifdef _MSC_VER
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"uuid.lib")
#pragma comment(lib,"avrt.lib")
#pragma comment(lib,"winmm.lib")
#endif

namespace host_audio {
template<class Interface> class ComObject {
    Interface* pointer=nullptr;
public:
    ComObject()=default;ComObject(const ComObject&)=delete;ComObject& operator=(const ComObject&)=delete;
    ~ComObject(){if(pointer)pointer->Release();}
    Interface** put(){if(pointer)pointer->Release();pointer=nullptr;return &pointer;}
    Interface* operator->()const{return pointer;}
};
class EventHandle {
    HANDLE handle=nullptr;
public:
    explicit EventHandle(bool manual=false):handle(CreateEventW(nullptr,manual?TRUE:FALSE,FALSE,nullptr)){}
    ~EventHandle(){if(handle)CloseHandle(handle);}
    EventHandle(const EventHandle&)=delete;EventHandle& operator=(const EventHandle&)=delete;
    HANDLE get()const{return handle;}
    explicit operator bool()const{return handle!=nullptr;}
};
} // namespace host_audio

// Main-thread facade. pump/flush/recording methods use the emulated APU only on
// the caller thread. The render worker owns all COM/device handles and sees PCM
// frames through the SPSC queue; it cannot modify emulation timing or state.
class HostAudio {
    using Frame=host_audio::StereoFrame;
    enum class Backend { None,WASAPI,WaveOut };
    host_audio::StereoRing<8192> ring;
    host_audio::WavWriter writer;
    std::thread worker;
    std::atomic<Backend> backend{Backend::None};
    std::atomic<bool> playing{false},stopping{false},discontinuity{false};
    std::atomic<int> volume{100};
    std::atomic<std::uint64_t> lostFrames{0},missingFrames{0},flushRequest{0},flushDone{0};
    HANDLE stopEvent=nullptr,controlEvent=nullptr,flushEvent=nullptr;
    mutable std::mutex errorMutex;
    std::string error;
    void setError(std::string message){std::lock_guard lock(errorMutex);error=std::move(message);}
    void setHresultError(const char* operation,HRESULT result){
        std::ostringstream message;message<<operation<<" failed (HRESULT 0x"<<std::hex<<(std::uint32_t)result<<").";setError(message.str());
    }
    static WAVEFORMATEX format() {
        WAVEFORMATEX value{};value.wFormatTag=WAVE_FORMAT_PCM;value.nChannels=2;value.nSamplesPerSec=44100;
        value.wBitsPerSample=16;value.nBlockAlign=4;value.nAvgBytesPerSec=176400;return value;
    }
    bool consumeFlush(host_audio::SmoothRenderer& renderer) {
        const auto requested=flushRequest.load(std::memory_order_acquire);
        if(requested==flushDone.load(std::memory_order_relaxed))return false;
        ring.discardConsumer();renderer.restart();return true;
    }
    void acknowledgeFlush(){flushDone.store(flushRequest.load(std::memory_order_acquire),std::memory_order_release);SetEvent(flushEvent);}
    void render(host_audio::SmoothRenderer& smoother,Frame* data,std::size_t count) {
        if(discontinuity.exchange(false,std::memory_order_acq_rel))smoother.restart();
        const auto missing=smoother.render(ring,std::span<Frame>(data,count),playing.load(std::memory_order_acquire),volume.load(std::memory_order_relaxed));
        missingFrames.fetch_add(missing,std::memory_order_relaxed);
    }
    void renderPcm(host_audio::SmoothRenderer& smoother,BYTE* data,std::size_t count) {
        std::array<Frame,512> frames{};
        for(std::size_t offset=0;offset<count;){const auto take=std::min<std::size_t>(frames.size(),count-offset);
            render(smoother,frames.data(),take);std::memcpy(data+offset*4,frames.data(),take*4);offset+=take;}
    }
    // Microsoft shared-mode auto-conversion inserts the channel mapper and SRC
    // between this 44.1 kHz PCM stream and the actual endpoint mix format.
    bool runWasapi(unsigned latency,std::promise<bool>& ready,bool& announced) {
        host_audio::EventHandle deviceEvent;
        host_audio::ComObject<IMMDeviceEnumerator> enumerator;
        host_audio::ComObject<IMMDevice> device;
        host_audio::ComObject<IAudioClient> client;
        host_audio::ComObject<IAudioRenderClient> renderClient;
        if(!deviceEvent)return false;
        HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)enumerator.put());
        if(SUCCEEDED(hr))hr=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,device.put());
        if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)client.put());
        const auto pcm=format();
        if(SUCCEEDED(hr))hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            (REFERENCE_TIME)latency*10000,0,&pcm,nullptr);
        if(SUCCEEDED(hr))hr=client->SetEventHandle(deviceEvent.get());
        UINT32 capacity=0;
        if(SUCCEEDED(hr))hr=client->GetBufferSize(&capacity);
        if(SUCCEEDED(hr))hr=client->GetService(__uuidof(IAudioRenderClient),(void**)renderClient.put());
        BYTE* initial=nullptr;
        if(SUCCEEDED(hr))hr=renderClient->GetBuffer(capacity,&initial);
        if(SUCCEEDED(hr))hr=renderClient->ReleaseBuffer(capacity,AUDCLNT_BUFFERFLAGS_SILENT);
        if(SUCCEEDED(hr))hr=client->Start();
        if(FAILED(hr)){setHresultError("WASAPI initialization",hr);return false;}
        backend.store(Backend::WASAPI,std::memory_order_release);ready.set_value(true);announced=true;
        host_audio::SmoothRenderer smoother;
        HANDLE events[]={stopEvent,controlEvent,deviceEvent.get()};
        while(!stopping.load(std::memory_order_acquire)) {
            const DWORD event=WaitForMultipleObjects(3,events,FALSE,2000);
            if(event==WAIT_OBJECT_0)break;
            if(event==WAIT_FAILED){setError("WASAPI event wait failed.");break;}
            if(consumeFlush(smoother)) {
                hr=client->Stop();if(SUCCEEDED(hr))hr=client->Reset();
                if(SUCCEEDED(hr))hr=renderClient->GetBuffer(capacity,&initial);
                if(SUCCEEDED(hr)){renderPcm(smoother,initial,capacity);hr=renderClient->ReleaseBuffer(capacity,0);}
                if(SUCCEEDED(hr))hr=client->Start();acknowledgeFlush();
                if(FAILED(hr)){setHresultError("WASAPI flush",hr);break;}
            }
            UINT32 padding=0;hr=client->GetCurrentPadding(&padding);
            if(FAILED(hr)){setHresultError("WASAPI padding",hr);break;}
            if(padding>capacity){setError("WASAPI reported invalid buffer padding.");break;}
            const UINT32 available=capacity-padding;if(!available)continue;
            BYTE* data=nullptr;hr=renderClient->GetBuffer(available,&data);
            if(SUCCEEDED(hr)){renderPcm(smoother,data,available);hr=renderClient->ReleaseBuffer(available,0);}
            if(FAILED(hr)){setHresultError("WASAPI render",hr);break;}
        }
        client->Stop();return true;
    }
    bool runWaveOut(unsigned latency,std::promise<bool>& ready,bool& announced) {
        host_audio::EventHandle deviceEvent(true);if(!deviceEvent)return false;
        HWAVEOUT handle=nullptr;const auto pcm=format();
        MMRESULT result=waveOutOpen(&handle,WAVE_MAPPER,&pcm,(DWORD_PTR)deviceEvent.get(),0,CALLBACK_EVENT);
        if(result!=MMSYSERR_NOERROR){setError("No audio device: WASAPI and waveOut initialization failed.");return false;}
        constexpr std::size_t blocks=3;
        const std::size_t frames=std::clamp<std::size_t>((std::size_t)latency*44100/(blocks*1000),256,4096);
        std::array<std::vector<Frame>,blocks> data;
        std::array<WAVEHDR,blocks> headers{};std::array<bool,blocks> queued{};
        struct Cleanup {
            HWAVEOUT handle;std::array<WAVEHDR,blocks>& headers;std::size_t prepared=0;
            ~Cleanup(){waveOutReset(handle);for(std::size_t n=0;n<prepared;++n)waveOutUnprepareHeader(handle,&headers[n],sizeof(WAVEHDR));waveOutClose(handle);}
        } cleanup{handle,headers};
        bool success=true;
        for(std::size_t n=0;n<blocks;++n){
            data[n].resize(frames);headers[n].lpData=(LPSTR)data[n].data();headers[n].dwBufferLength=(DWORD)(frames*4);
            result=waveOutPrepareHeader(handle,&headers[n],sizeof(WAVEHDR));
            if(result!=MMSYSERR_NOERROR){success=false;setError("Could not prepare waveOut buffer.");break;}++cleanup.prepared;
        }
        host_audio::SmoothRenderer smoother;
        if(success){backend.store(Backend::WaveOut,std::memory_order_release);setError("");ready.set_value(true);announced=true;}
        HANDLE events[]={stopEvent,controlEvent,deviceEvent.get()};
        while(success&&!stopping.load(std::memory_order_acquire)) {
            // Clear before inspecting all headers, so a new completion cannot be lost.
            ResetEvent(deviceEvent.get());
            if(consumeFlush(smoother)){waveOutReset(handle);queued.fill(false);acknowledgeFlush();}
            for(std::size_t n=0;n<blocks;++n)if(!queued[n]||(headers[n].dwFlags&WHDR_DONE)) {
                render(smoother,data[n].data(),frames);
                result=waveOutWrite(handle,&headers[n],sizeof(WAVEHDR));
                if(result!=MMSYSERR_NOERROR){success=false;setError("Could not submit waveOut audio buffer.");break;}queued[n]=true;
            }
            if(!success)break;
            const DWORD event=WaitForMultipleObjects(3,events,FALSE,2000);
            if(event==WAIT_OBJECT_0)break;
            if(event==WAIT_FAILED){setError("waveOut event wait failed.");break;}
        }
        return announced;
    }
    void workerMain(unsigned latency,std::promise<bool> ready) {
        bool announced=false;
        // A fresh worker apartment owns creation, use and destruction of every COM object.
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        DWORD taskIndex=0;HANDLE task=AvSetMmThreadCharacteristicsW(L"Audio",&taskIndex);
        try {
            if(FAILED(initialized)||!runWasapi(latency,ready,announced))runWaveOut(latency,ready,announced);
            if(!announced)ready.set_value(false);
        } catch(const std::exception& failure){setError(std::string("Audio worker: ")+failure.what());if(!announced)ready.set_value(false);}
        catch(...){setError("Audio worker failed.");if(!announced)ready.set_value(false);}
        backend.store(Backend::None,std::memory_order_release);acknowledgeFlush();
        if(task)AvRevertMmThreadCharacteristics(task);
        if(SUCCEEDED(initialized))CoUninitialize();
    }
public:
    HostAudio()=default;HostAudio(const HostAudio&)=delete;HostAudio& operator=(const HostAudio&)=delete;
    ~HostAudio(){close();}
    bool open(unsigned latencyMs=40) {
        stopDevice();setError("");stopping.store(false,std::memory_order_release);playing.store(false,std::memory_order_release);
        lostFrames.store(0);missingFrames.store(0);flushRequest.store(0);flushDone.store(0);discontinuity.store(false);ring.resetStopped();
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);controlEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);flushEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!stopEvent||!controlEvent||!flushEvent){setError("Could not create audio synchronization events.");stopDevice();return false;}
        std::promise<bool> startup;auto result=startup.get_future();
        try{worker=std::thread([this,latency=std::clamp(latencyMs,20u,200u),ready=std::move(startup)]() mutable {workerMain(latency,std::move(ready));});}
        catch(const std::exception& failure){setError(std::string("Could not start audio worker: ")+failure.what());stopDevice();return false;}
        const bool started=result.get();if(!started)stopDevice();return started;
    }
    void pump(APU& apu,bool play) {
        playing.store(play,std::memory_order_release);
        std::array<Frame,512> frames{};
        std::array<std::int16_t,1024> samples{};
        while(apu.available()) {
            const std::size_t count=std::min<std::size_t>(frames.size(),apu.available());
            apu.samples(samples.data(),(int)count);
            for(std::size_t n=0;n<count;++n)frames[n]={samples[n*2],samples[n*2+1]};
            const auto input=std::span<const Frame>(frames).first(count);
            if(writer.recording()&&!writer.append(input)){const auto message=writer.lastError();writer.close();setError(message);}
            if(play&&backend.load(std::memory_order_acquire)!=Backend::None){
                const auto accepted=ring.push(input);lostFrames.fetch_add(count-accepted,std::memory_order_relaxed);
                if(accepted<count)discontinuity.store(true,std::memory_order_release);
            }
        }
    }
    void flush(APU& apu) {
        apu.clearQueue();playing.store(false,std::memory_order_release);
        if(!worker.joinable()||backend.load(std::memory_order_acquire)==Backend::None)return;
        const auto request=flushRequest.fetch_add(1,std::memory_order_acq_rel)+1;SetEvent(controlEvent);
        // The consumer clears its own index; the producer never races it by resetting the ring.
        while(flushDone.load(std::memory_order_acquire)<request){if(WaitForSingleObject(flushEvent,100)!=WAIT_OBJECT_0)break;}
    }
    void close() {
        stopRecording();stopDevice();
    }
private:
    void stopDevice() {
        playing.store(false,std::memory_order_release);stopping.store(true,std::memory_order_release);
        if(stopEvent)SetEvent(stopEvent);if(worker.joinable())worker.join();
        backend.store(Backend::None,std::memory_order_release);
        for(HANDLE* event:{&stopEvent,&controlEvent,&flushEvent})if(*event){CloseHandle(*event);*event=nullptr;}
        ring.resetStopped();
    }
public:
    void setVolume(int percent){volume.store(std::clamp(percent,0,100),std::memory_order_relaxed);}
    bool startRecording(const std::filesystem::path& path) {
        if(!writer.open(path)){setError(writer.lastError());return false;}setError("");return true;
    }
    bool stopRecording() {
        if(!writer.recording())return true;
        const bool result=writer.close();if(!result)setError(writer.lastError());return result;
    }
    bool recording()const{return writer.recording();}
    std::string backendName()const {
        switch(backend.load(std::memory_order_acquire)){case Backend::WASAPI:return "WASAPI";case Backend::WaveOut:return "waveOut fallback";default:return "No device";}
    }
    std::string lastError()const{std::lock_guard lock(errorMutex);return error;}
    std::uint64_t overflows()const{return lostFrames.load(std::memory_order_relaxed);}
    std::uint64_t underflows()const{return missingFrames.load(std::memory_order_relaxed);}
};
