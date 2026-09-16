#pragma once

namespace NEURONiK::DSP::Runtime
{
    enum class EventType
    {
        NoteOn,
        NoteOff,
        PitchBend,
        ChannelPressure,
        PolyAftertouch,
        Timbre
    };

    struct Event
    {
        EventType type = EventType::NoteOn;
        int channel = 1;
        int note = 0;
        int value14 = 8192;
        float value = 0.0f;
        int sampleOffset = 0;
    };
}
