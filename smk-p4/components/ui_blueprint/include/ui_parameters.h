#pragma once
#include "ui_theme.h"
#include <cstddef>
#include <cstdint>

namespace smk::p4::ui {

enum class ParameterValueType : uint8_t {
    Float  = 0,
    Int    = 1,
    Bool   = 2,
    Enum   = 3,
    String = 4
};

enum class ParameterScale : uint8_t {
    Linear      = 0,
    Logarithmic = 1
};

enum ParameterCapability : uint16_t {
    kCapNone             = 0,
    kCapControllable     = 1u << 0,
    kCapAutomatable      = 1u << 1,
    kCapMidiMappable     = 1u << 2,
    kCapMacroTargetable  = 1u << 3,
    kCapPersistedInPatch = 1u << 4,
    kCapReadOnly         = 1u << 5,
    kCapLayerScoped      = 1u << 6
};

struct ParameterValue {
    ParameterValueType type = ParameterValueType::Float;
    union {
        float       f_val;
        int32_t     i_val;
        bool        b_val;
        int32_t     enum_idx;
        const char* s_val;
    };

    static constexpr ParameterValue makeFloat(float v) {
        ParameterValue pv{};
        pv.type = ParameterValueType::Float;
        pv.f_val = v;
        return pv;
    }
    static constexpr ParameterValue makeInt(int32_t v) {
        ParameterValue pv{};
        pv.type = ParameterValueType::Int;
        pv.i_val = v;
        return pv;
    }
    static constexpr ParameterValue makeBool(bool v) {
        ParameterValue pv{};
        pv.type = ParameterValueType::Bool;
        pv.b_val = v;
        return pv;
    }
    static constexpr ParameterValue makeEnum(int32_t idx) {
        ParameterValue pv{};
        pv.type = ParameterValueType::Enum;
        pv.enum_idx = idx;
        return pv;
    }
    static constexpr ParameterValue makeString(const char* s) {
        ParameterValue pv{};
        pv.type = ParameterValueType::String;
        pv.s_val = s;
        return pv;
    }

    float asFloat() const {
        switch (type) {
            case ParameterValueType::Float: return f_val;
            case ParameterValueType::Int:   return static_cast<float>(i_val);
            case ParameterValueType::Bool:  return b_val ? 1.0f : 0.0f;
            case ParameterValueType::Enum:  return static_cast<float>(enum_idx);
            default:                        return 0.0f;
        }
    }

    int32_t asInt() const {
        switch (type) {
            case ParameterValueType::Int:   return i_val;
            case ParameterValueType::Enum:  return enum_idx;
            case ParameterValueType::Bool:  return b_val ? 1 : 0;
            case ParameterValueType::Float: return static_cast<int32_t>(f_val);
            default:                        return 0;
        }
    }

    bool asBool() const {
        switch (type) {
            case ParameterValueType::Bool:  return b_val;
            case ParameterValueType::Int:   return i_val != 0;
            case ParameterValueType::Enum:  return enum_idx != 0;
            case ParameterValueType::Float: return f_val != 0.0f;
            default:                        return false;
        }
    }
};

struct ParameterDescriptor {
    const char*        id;               // Stable canonical ID (never tied to UI label)
    const char*        ns;               // Namespace ("perf", "osc", "fm", "filter", etc.)
    const char*        name;             // Short human-readable UI name
    ParameterValueType type;             // Float, Int, Bool, Enum, String
    float              minimum;          // Minimum numeric value (or 0 for Bool/Enum)
    float              maximum;          // Maximum numeric value (or count-1 for Enum)
    float              default_value;    // Default numeric value
    float              step;             // Increment step
    const char*        unit;             // Unit string ("Hz", "ms", "%", "dB", "st", "ct", "BPM", "")
    ParameterScale     scale;            // Linear or Logarithmic
    const char*        category;         // High-level domain ("Performance", "Synthesis", "Harmony", etc.)
    const char*        group;            // Sub-group ("OSC", "FM", "FILTER", "SCALE", "CHORD", etc.)
    const char* const* enum_values;      // Array of enum option labels (nullptr if not Enum)
    uint8_t            enum_count;       // Number of enum option labels
    uint16_t           capabilities;     // Bitmask of ParameterCapability
    FeatureStatus      status;           // Implementation status in P4 standalone firmware
    const char*        depends_on_id;    // Optional dependency parameter ID (or nullptr)
    int32_t            depends_on_value; // Required integer/bool/enum value if depends_on_id != nullptr (-1 = any non-zero)
    const char*        evidence_ref;     // Repository file proving current status
    const char*        description;      // Concise functional description
};

class ParameterRegistry {
public:
    static const ParameterDescriptor* all(size_t& out_count);
    static size_t count();
    static const ParameterDescriptor* findById(const char* id);
    static int indexOf(const char* id);
    static bool hasDuplicateIds();
    static void formatValue(const ParameterDescriptor& desc, const ParameterValue& val, char* buf, size_t cap);
};

} // namespace smk::p4::ui
