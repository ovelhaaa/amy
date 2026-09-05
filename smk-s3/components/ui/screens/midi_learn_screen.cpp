#include "midi_learn_screen.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace smk {

MidiLearnScreen::MidiLearnScreen() {}

void MidiLearnScreen::setMidiLearn(MidiLearn* learn) {
    midi_learn_ = learn;
}

void MidiLearnScreen::triggerFeedback(const char* msg, uint16_t color) {
    if (msg) snprintf(feedback_msg_, sizeof(feedback_msg_), "%s", msg);
    feedback_color_ = color;
    feedback_timer_ms_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void MidiLearnScreen::update() {
    blink_phase_++;
}

void MidiLearnScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();

    uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    bool feedback_active = (now_ms - feedback_timer_ms_ < 1500) && (feedback_msg_[0] != '\0');

    bool is_learning = midi_learn_ && midi_learn_->isLearning();
    bool is_complete = midi_learn_ && midi_learn_->isComplete();
    uint8_t cur_step = midi_learn_ ? midi_learn_->currentStepNumber() : 0;
    uint8_t total_steps = midi_learn_ ? midi_learn_->totalSteps() : 39;

    if (dw <= 160) {
        // ── 160x128 ST7735S Display Layout ──

        // 1. Top Header Box (Dark Gray)
        display.fillRect(0, 0, 160, 18, DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, 3, 5, "MIDI LEARN", DisplayDriver::kColorCyan, DisplayDriver::kColorDarkGray, FontType::Font5x7);

        // Step Counter / State Badge
        char step_str[24];
        if (is_learning) {
            snprintf(step_str, sizeof(step_str), "[%02u/%02u]", cur_step, total_steps);
            FontRenderer::drawString(display, 95, 5, step_str, DisplayDriver::kColorAmber, DisplayDriver::kColorDarkGray, FontType::Font5x7);
        } else if (is_complete) {
            FontRenderer::drawString(display, 80, 5, "[SALVO!]", DisplayDriver::kColorGreen, DisplayDriver::kColorDarkGray, FontType::Font5x7);
        } else {
            FontRenderer::drawString(display, 85, 5, "[PRONTO]", DisplayDriver::kColorYellow, DisplayDriver::kColorDarkGray, FontType::Font5x7);
        }

        display.drawHLine(0, 18, 160, DisplayDriver::kColorMidGray);

        if (is_learning) {
            // 2. Central Prompt Box (y=22..62, w=156, h=40)
            display.fillRect(2, 22, 156, 40, DisplayDriver::kColorBlack);
            display.drawRect(2, 22, 156, 40, DisplayDriver::kColorCyan);

            const char* step_name = midi_learn_->currentStepName();
            const char* step_hint = midi_learn_->currentStepHint();

            FontRenderer::drawString(display, 6, 26, step_name ? step_name : "", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
            FontRenderer::drawString(display, 6, 42, step_hint ? step_hint : "", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

            // 3. Progress Bar (y=66, w=156, h=8)
            display.drawRect(2, 66, 156, 8, DisplayDriver::kColorMidGray);
            float progress = (total_steps > 0) ? ((float)cur_step / (float)total_steps) : 0.0f;
            int16_t fill_w = static_cast<int16_t>(152 * progress);
            if (fill_w > 0) {
                display.fillRect(4, 68, fill_w, 4, DisplayDriver::kColorGreen);
            }

            // 4. Last Capture Info Box (y=78..98)
            display.fillRect(2, 78, 156, 20, DisplayDriver::kColorDarkGray);
            display.drawRect(2, 78, 156, 20, DisplayDriver::kColorMidGray);

            if (feedback_active) {
                FontRenderer::drawString(display, 6, 84, feedback_msg_, feedback_color_, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            } else if (midi_learn_->lastCapturedNumber() != 0xFFFF) {
                char cap_buf[48];
                const char* type_str = (midi_learn_->lastCapturedType() == 1) ? "CC" : (midi_learn_->lastCapturedType() == 2 ? "BEND" : "NOTE");
                snprintf(cap_buf, sizeof(cap_buf), "ULTIMO: %s#%u CH%u V:%ld",
                         type_str, midi_learn_->lastCapturedNumber(),
                         midi_learn_->lastCapturedChannel() + 1, (long)midi_learn_->lastCapturedValue());
                FontRenderer::drawString(display, 6, 84, cap_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            } else {
                FontRenderer::drawString(display, 6, 84, "Aguardando toque/movimento...", DisplayDriver::kColorLightGray, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            }

            // 5. Bottom Navigation Bar (y=104..126)
            display.drawHLine(0, 102, 160, DisplayDriver::kColorMidGray);
            FontRenderer::drawString(display, 3, 106, "STOP: Pular Passo", DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);
            FontRenderer::drawString(display, 3, 117, "REC: Cancelar Assistente", DisplayDriver::kColorRed, DisplayDriver::kColorBlack, FontType::Font5x7);

        } else if (is_complete) {
            // Completion Screen
            display.fillRect(2, 24, 156, 46, DisplayDriver::kColorDarkGray);
            display.drawRect(2, 24, 156, 46, DisplayDriver::kColorGreen);

            FontRenderer::drawString(display, 8, 30, "MAPEAMENTO CONCLUIDO!", DisplayDriver::kColorGreen, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            FontRenderer::drawString(display, 8, 44, "Salvo na Flash SPIFFS", DisplayDriver::kColorWhite, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            FontRenderer::drawString(display, 8, 56, "Perfil: smk25_custom", DisplayDriver::kColorCyan, DisplayDriver::kColorDarkGray, FontType::Font5x7);

            display.drawHLine(0, 80, 160, DisplayDriver::kColorMidGray);
            FontRenderer::drawString(display, 4, 90, "PLAY: Iniciar Novamente", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
            FontRenderer::drawString(display, 4, 108, "PAD 5/6: Voltar ao Synth", DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);

        } else {
            // Idle / Start Screen
            display.fillRect(2, 24, 156, 52, DisplayDriver::kColorDarkGray);
            display.drawRect(2, 24, 156, 52, DisplayDriver::kColorYellow);

            FontRenderer::drawString(display, 8, 30, "ASSISTENTE MIDI LEARN", DisplayDriver::kColorYellow, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            FontRenderer::drawString(display, 8, 44, "Mapeia Teclas, Pitch, Mod,", DisplayDriver::kColorWhite, DisplayDriver::kColorDarkGray, FontType::Font5x7);
            FontRenderer::drawString(display, 8, 56, "16 Knobs (A/B), Pads e Botoes", DisplayDriver::kColorLightGray, DisplayDriver::kColorDarkGray, FontType::Font5x7);

            display.drawHLine(0, 84, 160, DisplayDriver::kColorMidGray);
            bool blink = (blink_phase_ / 15) % 2 == 0;
            uint16_t play_col = blink ? DisplayDriver::kColorGreen : DisplayDriver::kColorYellow;
            FontRenderer::drawString(display, 4, 94, ">> PRESSIONE PLAY <<", play_col, DisplayDriver::kColorBlack, FontType::Font5x7);
            FontRenderer::drawString(display, 4, 110, "PLAY inicia o assistente", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
        }
        return;
    }

    // ── 284x76 Widescreen Display Layout ──
    char header_str[48];
    if (is_learning) {
        snprintf(header_str, sizeof(header_str), "MIDI LEARN WIZARD [%02u/%02u]", cur_step, total_steps);
    } else if (is_complete) {
        snprintf(header_str, sizeof(header_str), "MIDI LEARN [CONCLUIDO & SALVO]");
    } else {
        snprintf(header_str, sizeof(header_str), "MIDI LEARN [PRONTO P/ INICIAR]");
    }
    FontRenderer::drawString(display, 2, 2, header_str, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

    if (is_learning) {
        const char* step_name = midi_learn_->currentStepName();
        const char* step_hint = midi_learn_->currentStepHint();

        FontRenderer::drawString(display, 4, 16, step_name ? step_name : "", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
        FontRenderer::drawString(display, 4, 28, step_hint ? step_hint : "", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Progress bar
        display.drawRect(4, 42, 160, 8, DisplayDriver::kColorMidGray);
        float progress = (total_steps > 0) ? ((float)cur_step / (float)total_steps) : 0.0f;
        int16_t fill_w = static_cast<int16_t>(156 * progress);
        if (fill_w > 0) {
            display.fillRect(6, 44, fill_w, 4, DisplayDriver::kColorGreen);
        }

        // Right box: Capture feedback
        display.drawRect(172, 14, 108, 36, DisplayDriver::kColorMidGray);
        if (feedback_active) {
            FontRenderer::drawString(display, 176, 24, feedback_msg_, feedback_color_, DisplayDriver::kColorBlack, FontType::Font5x7);
        } else if (midi_learn_->lastCapturedNumber() != 0xFFFF) {
            char cap_buf[32];
            snprintf(cap_buf, sizeof(cap_buf), "ID:%u VAL:%ld", midi_learn_->lastCapturedNumber(), (long)midi_learn_->lastCapturedValue());
            FontRenderer::drawString(display, 176, 20, "CAPTURADO:", DisplayDriver::kColorGreen, DisplayDriver::kColorBlack, FontType::Font5x7);
            FontRenderer::drawString(display, 176, 32, cap_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
        } else {
            FontRenderer::drawString(display, 176, 26, "Aguardando...", DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
        }

        display.drawHLine(0, 56, dw, DisplayDriver::kColorMidGray);
        FontRenderer::drawString(display, 2, 62, "STOP: Pular Passo  |  REC: Cancelar Assistente", DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);
    } else if (is_complete) {
        FontRenderer::drawString(display, 4, 18, "MAPEAMENTO CONCLUIDO COM SUCESSO!", DisplayDriver::kColorGreen, DisplayDriver::kColorBlack, FontType::Font5x7);
        FontRenderer::drawString(display, 4, 32, "Perfil 'smk25_custom' gravado na memoria Flash SPIFFS.", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
        display.drawHLine(0, 56, dw, DisplayDriver::kColorMidGray);
        FontRenderer::drawString(display, 2, 62, "PLAY: Reiniciar  |  Pad B3: Home  |  B5/B6: Paginas", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
    } else {
        FontRenderer::drawString(display, 4, 18, "PLAY inicia o mapeamento MIDI.", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
        FontRenderer::drawString(display, 4, 32, "Teclas, Pitch, Mod, Knobs A/B, Pads A/B e transporte.", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
        display.drawHLine(0, 56, dw, DisplayDriver::kColorMidGray);
        FontRenderer::drawString(display, 2, 62, "PLAY: Iniciar  |  Pad B3: Home  |  B5/B6: Paginas", DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    }
}

} // namespace smk
