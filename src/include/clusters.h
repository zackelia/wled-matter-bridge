#include <stdint.h>

// TODO: I don't know why this warning is hitting
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <jthread.hpp>
#pragma GCC diagnostic pop

#include <lib/support/CodeUtils.h>

class IdentifyInterface
{
public:
    virtual ~IdentifyInterface() = default;

    void Identify(uint16_t time)
    {
        if (remaining_time > 0)
        {
            // An identify command is already going, don't start another at the same time.
            return;
        }
        remaining_time   = time;
        animation_thread = std::jthread([=] { this->AnimateIdentify(); });
    }
    virtual void AnimateIdentify() = 0;

    uint16_t IdentifyTime() { return remaining_time; }

protected:
    uint16_t remaining_time = 0;
    std::jthread animation_thread;
};

class ColorControlInterface
{
public:
    virtual ~ColorControlInterface() = default;

    virtual uint8_t ColorMode() { return mColorMode; }
    virtual void SetColorMode(uint8_t mode) { mColorMode = mode; }

    virtual uint16_t Capabilities() = 0;

    virtual uint16_t Mireds()                = 0;
    virtual void SetMireds(uint16_t aMireds) = 0;

protected:
    uint8_t mColorMode = 255;
    uint16_t mMireds   = 0;
};
