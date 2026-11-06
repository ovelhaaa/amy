#pragma once
#include "pcm5102_output.h"
#include "pcm_handoff.h"
#include "commands.h"
#include "output_context.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

namespace smk::p4::audio {
// Only used by the opt-in 0.8 profile. Worker can allocate/load AMY while this
// independent higher-priority task continues feeding initialized PCM to DMA.
class UnifiedOutput {
public:
    struct Metrics {
        amy::GapCounts gaps;
        uint32_t stale_discarded=0;
        uint64_t frames=0,write_total_us=0;
        uint32_t blocks=0,silence_blocks=0,barriers=0,write_max_us=0;
        uint32_t write_errors=0,timeouts=0,refill_risks=0,stack_free=0;
    };
    explicit UnifiedOutput(amy::Commands& commands):commands_(commands) {}
    ~UnifiedOutput() { stop(); }
    esp_err_t begin(int bclk,int ws,int data) {
        bclk_=bclk;ws_=ws;data_=data;return ESP_OK;
    }
    esp_err_t start() {
        metrics_=xQueueCreateStatic(1,sizeof(Metrics),metrics_storage_,&metrics_control_);
        if(!metrics_)return ESP_ERR_NO_MEM;
        running_.store(true);
        if(xTaskCreate(task,"pcm_output08",amy::kOutputStackBytes,this,
                       amy::kOutputPriority,nullptr)!=pdPASS) {
            running_.store(false);return ESP_ERR_NO_MEM;
        }
        started_=true;
        while(!initialized_.load(std::memory_order_acquire))vTaskDelay(1);
        return static_cast<esp_err_t>(error_.load());
    }
    esp_err_t write(const int16_t* pcm,size_t frames,uint32_t generation) {
        if(frames!=amy::kBlockFrames || !pcm)return ESP_ERR_INVALID_ARG;
        while(running_.load(std::memory_order_acquire)) {
            // Do not tag old rendered PCM with a newer Panic/patch generation.
            if(generation!=commands_.generation())return ESP_OK;
            if(handoff_.push(pcm,generation))return ESP_OK;
            vTaskDelay(1); // Synthesis worker pacing; never the output task.
        }
        return static_cast<esp_err_t>(error_.load());
    }
    esp_err_t stop() {
        if(started_) {
            running_.store(false,std::memory_order_release);
            while(!done_.load(std::memory_order_acquire))vTaskDelay(1);
            started_=false;
        }
        return static_cast<esp_err_t>(error_.load());
    }
    uint32_t silenceBlocks() const { return silence_blocks_.load(); }
    Metrics metrics() const { Metrics m{};if(metrics_)xQueuePeek(metrics_,&m,0);return m; }
private:
    static void task(void* self) { static_cast<UnifiedOutput*>(self)->run();vTaskDelete(nullptr); }
    void run() {
        Pcm5102Output output;
        auto error=output.begin(bclk_,ws_,data_);
        if(error==ESP_OK)error=output.start();
        error_.store(error);initialized_.store(true,std::memory_order_release);
        int16_t pcm[amy::kBlockFrames*2]{};
        int16_t last[2]{};
        uint32_t output_generation=commands_.generation();
        Metrics m{};int64_t previous_start=0;bool primed=false;
        while(error==ESP_OK && running_.load(std::memory_order_acquire)) {
            const auto start=esp_timer_get_time();
            if(previous_start && start-previous_start>int64_t(kDmaDescriptors*amy::kBlockFrames*1000000ULL/amy::kSampleRateHz))++m.refill_risks;
            previous_start=start;
            const auto generation=commands_.generation();
            const bool barrier=generation!=output_generation;
            if(barrier)++m.barriers;
            output_generation=generation;
            unsigned stale=0;
            const bool copied=!barrier && handoff_.take(pcm,generation,&stale);
            m.stale_discarded+=stale;
            if(!copied) {
                auto context=(barrier?commands_.transition_context:commands_.output_context).load(std::memory_order_acquire);
                if(!primed && !barrier && (context&7)==uint32_t(amy::OutputReason::Ready))
                    context=amy::outputContext(generation,amy::OutputReason::Boot);
                m.gaps.add(context,generation,stale!=0);
                ++silence_blocks_;
                ++m.silence_blocks;
                // One bounded block ramps the last sample to zero on a gap.
                for(unsigned f=0;f<amy::kBlockFrames;++f)
                    for(unsigned ch=0;ch<2;++ch)
                        pcm[f*2+ch]=static_cast<int16_t>(int32_t(last[ch])*
                            (amy::kBlockFrames-1-f)/amy::kBlockFrames);
            }
            if(copied)primed=true;
            last[0]=pcm[(amy::kBlockFrames-1)*2];last[1]=pcm[(amy::kBlockFrames-1)*2+1];
            const auto write_start=esp_timer_get_time();
            error=output.write(pcm,amy::kBlockFrames);
            const uint32_t write_us=static_cast<uint32_t>(esp_timer_get_time()-write_start);
            m.write_total_us+=write_us;if(write_us>m.write_max_us)m.write_max_us=write_us;
            if(error==ESP_OK) { m.frames+=amy::kBlockFrames;++m.blocks; }
            else { ++m.write_errors;if(error==ESP_ERR_TIMEOUT)++m.timeouts; }
            m.stack_free=uxTaskGetStackHighWaterMark(nullptr);
            xQueueOverwrite(metrics_,&m);
        }
        const auto stopped=output.stop();if(error==ESP_OK)error=stopped;
        error_.store(error);running_.store(false,std::memory_order_release);
        done_.store(true,std::memory_order_release);
    }
    amy::Commands& commands_;
    amy::PcmHandoff<amy::kPcmQueueBlocks> handoff_;
    int bclk_=0,ws_=0,data_=0;
    bool started_=false;
    std::atomic<bool> running_{false},initialized_{false},done_{false};
    std::atomic<int32_t> error_{ESP_OK};
    std::atomic<uint32_t> silence_blocks_{0};
    StaticQueue_t metrics_control_{};
    uint8_t metrics_storage_[sizeof(Metrics)]{};
    QueueHandle_t metrics_=nullptr;
};
}
