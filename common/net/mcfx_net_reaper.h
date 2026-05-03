/*
 ==============================================================================

 mcfx_net_reaper.h — pull the currently loaded REAPER project file name
 through the VST3 host-application bridge so we can advertise it in the
 mcfx_net Bonjour TXT record. No-op in every other host.

 Lifted from ambix/ambix_encoder/Source/ReaperVST3Integration.h with the
 namespace renamed and JUCE module dependencies kept identical.

 Uses JUCE's VST3ClientExtensions hook: the wrapper calls
 setIHostApplication shortly after the AudioProcessor is constructed; if
 the host is REAPER 5.02+, its IHostApplication also exposes
 IReaperHostApplication, which lets us look up REAPER API functions by
 name — here we grab `GetProjectName` and use `getReaperParent(3)` to
 fetch the per-instance project pointer.

 GPL v2 or later.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wshadow-field-in-constructor",
                                     "-Wnon-virtual-dtor",
                                     "-Wzero-as-null-pointer-constant",
                                     "-Wunused-parameter")

#include <pluginterfaces/base/ftypes.h>
#include <pluginterfaces/base/funknown.h>

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

namespace mcfx { namespace net { namespace reaper
{
    using namespace Steinberg;
    using uint32 = Steinberg::uint32;

    // Minimal IReaperHostApplication declaration. Mirrors the JUCE demo
    // reaper_vst3_interfaces.h — vendored here so we don't pull in the
    // whole demo include tree.
    class IReaperHostApplication : public FUnknown
    {
    public:
        virtual void* PLUGIN_API getReaperApi (CStringA funcname) = 0;
        virtual void* PLUGIN_API getReaperParent (uint32 w) = 0;
        virtual void* PLUGIN_API reaperExtended (uint32 call, void* p1, void* p2, void* p3) = 0;

        static const FUID iid;
    };

    DECLARE_CLASS_IID (IReaperHostApplication,
                       0x79655E36, 0x77EE4267, 0xA573FEF7, 0x4912C27C)
}}}

namespace mcfx { namespace net {

/** VST3ClientExtensions subclass that, when loaded in REAPER, lets us ask
    REAPER which project *this* plugin instance belongs to. Returns "" in
    every other host or for unsaved projects. Thread-safe — getProjectName
    can be called from any thread. */
class ReaperVST3Integration final : public juce::VST3ClientExtensions
{
public:
    ReaperVST3Integration() = default;

    ~ReaperVST3Integration() override
    {
        if (auto* h = reaperHost.exchange (nullptr, std::memory_order_acq_rel))
            h->release();
    }

    void setIHostApplication (Steinberg::FUnknown* ptr) override
    {
        if (ptr == nullptr) return;

        void* obj = nullptr;
        if (ptr->queryInterface (mcfx::net::reaper::IReaperHostApplication::iid, &obj) != Steinberg::kResultOk
            || obj == nullptr)
            return;

        auto* host = static_cast<mcfx::net::reaper::IReaperHostApplication*> (obj);
        if (auto* old = reaperHost.exchange (host, std::memory_order_acq_rel))
            old->release();

        if (auto* fn = host->getReaperApi ("GetProjectName"))
        {
            using GetProjectNameFn = void (*) (void*, char*, int);
            getProjectNameFn.store (reinterpret_cast<GetProjectNameFn> (fn),
                                    std::memory_order_release);
        }

        if (auto* fn = host->getReaperApi ("EnumProjects"))
        {
            using EnumProjectsFn = void* (*) (int, char*, int);
            enumProjectsFn.store (reinterpret_cast<EnumProjectsFn> (fn),
                                  std::memory_order_release);
        }
    }

    /** Returns the base filename (no extension, no path) of the project
        this plugin instance lives in, or "" if we're not in REAPER or the
        project is unsaved. */
    juce::String getProjectName() const
    {
        auto* host = reaperHost.load (std::memory_order_acquire);

        // Preferred path: per-instance project handle.
        if (host != nullptr)
        {
            if (auto* proj = host->getReaperParent (3))
            {
                if (auto* getName = getProjectNameFn.load (std::memory_order_acquire))
                {
                    char buf[2048] = {};
                    getName (proj, buf, (int) sizeof (buf) - 1);
                    buf[sizeof (buf) - 1] = '\0';
                    if (buf[0] == '\0') return {};
                    return juce::File (juce::String (juce::CharPointer_UTF8 (buf)))
                             .getFileNameWithoutExtension();
                }
            }
        }

        // Fallback: active project. Wrong for non-active-tab instances on
        // very old REAPER builds, but better than nothing.
        if (auto* enumP = enumProjectsFn.load (std::memory_order_acquire))
        {
            char buf[2048] = {};
            enumP (-1, buf, (int) sizeof (buf) - 1);
            buf[sizeof (buf) - 1] = '\0';
            if (buf[0] == '\0') return {};
            return juce::File (juce::String (juce::CharPointer_UTF8 (buf)))
                     .getFileNameWithoutExtension();
        }

        return {};
    }

private:
    using EnumProjectsFn   = void* (*) (int, char*, int);
    using GetProjectNameFn = void  (*) (void*, char*, int);

    std::atomic<mcfx::net::reaper::IReaperHostApplication*> reaperHost { nullptr };
    std::atomic<EnumProjectsFn>   enumProjectsFn   { nullptr };
    std::atomic<GetProjectNameFn> getProjectNameFn { nullptr };
};

}} // namespace mcfx::net
