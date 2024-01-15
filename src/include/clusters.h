#include <future>
#include <stdint.h>

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
        remaining_time = time;

        using namespace std::chrono_literals;
        if (animation_future.valid() && animation_future.wait_for(0s) != std::future_status::ready)
            return;

        animation_future = std::async(std::launch::async, [&] { this->AnimateIdentify(); });
    }
    virtual void AnimateIdentify() = 0;

    uint16_t IdentifyTime() { return remaining_time; }

protected:
    uint16_t remaining_time = 0;
    std::future<void> animation_future;
};

class ColorControlInterface
{
public:
    virtual ~ColorControlInterface() = default;

    virtual uint8_t ColorMode() { return mColorMode; }
    virtual void SetColorMode(uint8_t mode) { mColorMode = mode; }

    virtual uint16_t Capabilities() = 0;

    virtual uint16_t Mireds() { abort(); }
    virtual void SetMireds(uint16_t aMireds) { abort(); }

    virtual uint8_t Hue() { abort(); }
    virtual void SetHue(uint8_t aHue) { abort(); }

    virtual uint8_t Saturation() { abort(); }
    virtual void SetSaturation(uint8_t aSaturation) { abort(); }

protected:
    uint8_t mColorMode  = 255;
    uint16_t mMireds    = 0;
    uint8_t mHue        = 0;
    uint8_t mSaturation = 0;
};
