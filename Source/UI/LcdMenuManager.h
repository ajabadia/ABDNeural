#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace NEURONiK::UI
{

/**
 * Manages the hierarchical menu structure and state machine for the 16x2 LCD.
 * Standard states: IDLE, NAVIGATION, EDIT.
 */
class LcdMenuManager
{
public:
    enum class State {
        Idle,       // Showing Patch / Bank info
        Navigation, // Browsing categories or parameters
        Edit        // Modifying a specific parameter value
    };

    enum class ItemType {
        Parameter,
        MidiCC,
        Action
    };

    struct MenuItem {
        juce::String label;
        juce::String paramID; 
        ItemType type = ItemType::Parameter;
        std::vector<MenuItem> subItems;
    };

    LcdMenuManager() {
        setupMenu(0);
    }

    void setupMenu(int engineType) {
        bool isNeuronik = (engineType == 0);

        // GLOBAL
        MenuItem global { "GLOBAL", "", ItemType::Parameter, {
            { "MASTER VOL", "masterLevel" },
            { "ENGINE SELECT", "engineType" },
            { "MASTER BPM", "masterBPM" },
            { "MIDI CH", "midiChannel" },
            { "VEL CURVE", "velocityCurve" }
        }};

        // RESONATOR / NEURAL PAD
        std::vector<MenuItem> resSubItems;
        resSubItems.push_back({ "UNISON DETUNE", "unisonDetune" });
        
        if (isNeuronik) {
            resSubItems.push_back({ "MORPH X", "morphX" });
            resSubItems.push_back({ "MORPH Y", "morphY" });
            resSubItems.push_back({ "INHARMONICITY", "oscInharmonicity" });
            resSubItems.push_back({ "ROUGHNESS", "oscRoughness" });
            resSubItems.push_back({ "ODD/EVEN BAL", "resonatorParity" });
            resSubItems.push_back({ "SPECTRAL SHIFT", "resonatorShift" });
            resSubItems.push_back({ "HARM ROLLOFF", "resonatorRolloff" });
        } else {
            resSubItems.push_back({ "EXCITE NOISE", "oscExciteNoise" });
            resSubItems.push_back({ "EXCITE COLOR", "excitationColor" });
            resSubItems.push_back({ "IMPULSE MIX", "impulseMix" });
            resSubItems.push_back({ "RES BANK RES", "resonatorRes" });
        }

        MenuItem resonator { "RESONATOR", "", ItemType::Parameter, resSubItems };

        // FILTER
        MenuItem filter { "FILTER", "", ItemType::Parameter, {
            { "CUTOFF", "filterCutoff" },
            { "RESONANCE", "filterRes" },
            { "ENV AMOUNT", "filterEnvAmount" }
        }};

        // FX
        MenuItem fx { "EFFECTS", "", ItemType::Parameter, {
            { "SATURATION", "fxSaturation" },
            { "CHORUS MIX", "fxChorusMix" },
            { "DELAY TIME", "fxDelayTime" },
            { "REVERB MIX", "fxReverbMix" }
        }};

        // MIDI CONTROL (Phase 21.3)
        std::vector<MenuItem> midiSubItems;
        midiSubItems.push_back({ "CC CUTOFF", "filterCutoff", ItemType::MidiCC });
        midiSubItems.push_back({ "CC RESON", "filterRes", ItemType::MidiCC });
        midiSubItems.push_back({ "CC OSC LVL", "oscLevel", ItemType::MidiCC });
        midiSubItems.push_back({ "CC ATTACK", "envAttack", ItemType::MidiCC });
        
        if (isNeuronik) {
            midiSubItems.push_back({ "CC MORPH X", "morphX", ItemType::MidiCC });
            midiSubItems.push_back({ "CC MORPH Y", "morphY", ItemType::MidiCC });
            midiSubItems.push_back({ "CC INHARM", "oscInharmonicity", ItemType::MidiCC });
            midiSubItems.push_back({ "CC ROUGH", "oscRoughness", ItemType::MidiCC });
        }

        midiSubItems.push_back({ "RESET ALL", "RESET_MIDI", ItemType::Action });

        MenuItem midi { "MIDI CONTROL", "", ItemType::MidiCC, midiSubItems };

        rootItems = { global, resonator, filter, fx, midi };
    }

    // --- Interaction ---
    
    void onMenuPress() {
        if (state == State::Edit) {
            state = State::Navigation; // Cancel edit
        } else if (state == State::Navigation) {
            if (inSubMenu) {
                inSubMenu = false; // Go up to category
            } else {
                state = State::Idle; // Exit to main screen
            }
        } else {
            state = State::Navigation; // Enter menu
            rootIdx = 0;
            inSubMenu = false;
        }
    }

    void onOkPress() {
        if (state == State::Navigation) {
            if (!inSubMenu) {
                inSubMenu = true;
                subIdx = 0;
            } else {
                auto item = getCurrentItem();
                if (item.type != ItemType::Action)
                    state = State::Edit;
                else
                    state = State::Navigation; // Action triggers immediately in Editor
            }
        } else if (state == State::Edit) {
            state = State::Navigation; // Confirm and return
        }
    }

    void onEncoderRotate(int delta) {
        if (state == State::Navigation) {
            if (inSubMenu) {
                int size = (int)rootItems[rootIdx].subItems.size();
                subIdx = (subIdx + delta + size) % size;
            } else {
                int size = (int)rootItems.size();
                rootIdx = (rootIdx + delta + size) % size;
            }
        }
        // In Edit state, the rotation is handled by the editor via MIDI/Parameter mapping
        // In Idle state, we could use it for preset selection
    }

    // --- Getters ---

    State getState() const { return state; }
    bool isEditing() const { return state == State::Edit; }
    bool isInSubMenu() const { return inSubMenu; }

    juce::String getLine1() const {
        if (state == State::Idle) return ""; 
        if (inSubMenu) return rootItems[rootIdx].label;
        return "MAIN MENU";
    }

    juce::String getLine2() const {
        if (state == State::Idle) return "";
        if (inSubMenu) return rootItems[rootIdx].subItems[subIdx].label;
        return rootItems[rootIdx].label;
    }
    
    juce::String getCurrentParamID() const {
        if (inSubMenu) return rootItems[rootIdx].subItems[subIdx].paramID;
        return "";
    }

    ItemType getCurrentItemType() const {
        if (inSubMenu) return rootItems[rootIdx].subItems[subIdx].type;
        return ItemType::Parameter;
    }

    MenuItem getCurrentItem() const {
        if (inSubMenu) return rootItems[rootIdx].subItems[subIdx];
        return rootItems[rootIdx];
    }

private:
    std::vector<MenuItem> rootItems;
    int rootIdx = 0;
    int subIdx = 0;
    bool inSubMenu = false;
    State state = State::Idle;
};

} // namespace NEURONiK::UI
