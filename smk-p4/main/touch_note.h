#pragma once
#include "app_config.h"
#include "smk_client.h"

namespace smk::p4 {
// Single-finger gesture state. Unsent presses are cancelled on release;
// accepted presses always retain an Off latch, even while awaiting an ACK.
class TouchNote {
public:
    void press(uint8_t note,uint32_t now_ms) {
        releaseAll();
        if(note==0 || note>=128) return;
        desired_=note; started_ms_=now_ms;
    }
    void release(uint8_t note) { if(note==desired_ || note==sent_) releaseAll(); }
    void releaseAll() {
        desired_=link::kNoNote;
        if(sent_!=link::kNoNote) { off_=sent_; sent_=link::kNoNote; }
    }
    void disconnect() { desired_=sent_=off_=link::kNoNote; }
    void service(link::Client& client,uint32_t now_ms,bool playable) {
        if(!client.connected()) { disconnect(); return; }
        if(!playable || (desired_!=link::kNoNote && uint32_t(now_ms-started_ms_)>=config::kTouchMaxHoldMs)) releaseAll();
        if(off_!=link::kNoNote) { client.releaseNote(off_); off_=link::kNoNote; }
        if(playable && desired_!=link::kNoNote && sent_!=desired_) {
            const uint8_t payload[]={desired_,config::kTouchVelocity};
            if(client.request(link::Type::AuditionNoteOn,payload,sizeof(payload),now_ms)) {
                sent_=desired_; client.heldNote(sent_);
            }
        }
    }
    uint8_t desired() const { return desired_; }
private:
    uint32_t started_ms_=0;
    uint8_t desired_=link::kNoNote,sent_=link::kNoNote,off_=link::kNoNote;
};
}
