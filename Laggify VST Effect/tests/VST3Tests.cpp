// A minimal in-process VST3 host: exercise the built plugin's view contract
// and live input/automation without requiring FL Studio or an attached window.
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
    std::cout << "PASS: " << message << '\n';
}
bool isId(const TUID actual, const TUID expected)
{
    return std::memcmp(actual, expected, sizeof(TUID)) == 0;
}

// These objects live on the stack for the entire plugin lifetime; the simple
// reference counts let the plugin retain/release them as it would a real host.
struct Host final : IHostApplication
{
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (isId(iid, INLINE_UID_OF(IHostApplication)) || isId(iid, INLINE_UID_OF(FUnknown)))
        {
            *object = this;
            addRef();
            return kResultOk;
        }
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override
    {
        return ++references;
    }
    uint32 PLUGIN_API release() override
    {
        return --references;
    }
    tresult PLUGIN_API getName(String128 output) override
    {
        const char16* name = u"Laggify test host";
        int i = 0;
        for (; name[i] && i < 127; ++i)
            output[i] = name[i];
        output[i] = 0;
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID, TUID, void** object) override
    {
        *object = nullptr;
        return kNoInterface;
    }
    uint32 references = 1;
};
struct StateStream final : IBStream
{
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (isId(iid, INLINE_UID_OF(IBStream)) || isId(iid, INLINE_UID_OF(FUnknown)))
        {
            *object = this;
            addRef();
            return kResultOk;
        }
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override
    {
        return ++references;
    }
    uint32 PLUGIN_API release() override
    {
        return --references;
    }
    tresult PLUGIN_API read(void* destination, int32 count, int32* bytesRead) override
    {
        if (count < 0 || destination == nullptr)
            return kInvalidArgument;
        const auto n = std::min<std::size_t>(static_cast<std::size_t>(count), bytes.size() - position);
        if (n > 0)
            std::memcpy(destination, bytes.data() + position, n);
        position += n;
        if (bytesRead)
            *bytesRead = static_cast<int32>(n);
        return kResultOk;
    }
    tresult PLUGIN_API write(void* source, int32 count, int32* bytesWritten) override
    {
        if (count < 0 || source == nullptr)
            return kInvalidArgument;
        const auto n = static_cast<std::size_t>(count);
        bytes.resize(std::max(bytes.size(), position + n));
        if (n > 0)
            std::memcpy(bytes.data() + position, source, n);
        position += n;
        if (bytesWritten)
            *bytesWritten = count;
        return kResultOk;
    }
    tresult PLUGIN_API seek(int64 offset, int32 mode, int64* result) override
    {
        int64 base = 0;
        if (mode == kIBSeekCur)
            base = static_cast<int64>(position);
        else if (mode == kIBSeekEnd)
            base = static_cast<int64>(bytes.size());
        else if (mode != kIBSeekSet)
            return kInvalidArgument;
        if (offset < -base || offset > static_cast<int64>(bytes.size()) - base)
            return kInvalidArgument;
        position = static_cast<std::size_t>(base + offset);
        if (result)
            *result = static_cast<int64>(position);
        return kResultOk;
    }
    tresult PLUGIN_API tell(int64* result) override
    {
        if (!result)
            return kInvalidArgument;
        *result = static_cast<int64>(position);
        return kResultOk;
    }
    uint32 references = 1;
    std::vector<char> bytes;
    std::size_t position = 0;
};

struct Queue final : IParamValueQueue
{
    ParamID id = 0;
    ParamValue value = 0;
    uint32 references = 1;
    tresult PLUGIN_API queryInterface(const TUID iid, void** out) override
    {
        if (isId(iid, INLINE_UID_OF(IParamValueQueue)) || isId(iid, INLINE_UID_OF(FUnknown)))
        {
            *out = this;
            addRef();
            return kResultOk;
        }
        *out = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override
    {
        return ++references;
    }
    uint32 PLUGIN_API release() override
    {
        return --references;
    }
    ParamID PLUGIN_API getParameterId() override
    {
        return id;
    }
    int32 PLUGIN_API getPointCount() override
    {
        return 1;
    }
    tresult PLUGIN_API getPoint(int32 index, int32& offset, ParamValue& v) override
    {
        if (index != 0)
            return kInvalidArgument;
        offset = 0;
        v = value;
        return kResultOk;
    }
    tresult PLUGIN_API addPoint(int32, ParamValue, int32&) override
    {
        return kNotImplemented;
    }
};
struct Changes final : IParameterChanges
{
    Queue queues[2];
    uint32 references = 1;
    tresult PLUGIN_API queryInterface(const TUID iid, void** out) override
    {
        if (isId(iid, INLINE_UID_OF(IParameterChanges)) || isId(iid, INLINE_UID_OF(FUnknown)))
        {
            *out = this;
            addRef();
            return kResultOk;
        }
        *out = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override
    {
        return ++references;
    }
    uint32 PLUGIN_API release() override
    {
        return --references;
    }
    int32 PLUGIN_API getParameterCount() override
    {
        return 2;
    }
    IParamValueQueue* PLUGIN_API getParameterData(int32 i) override
    {
        return i >= 0 && i < 2 ? &queues[i] : nullptr;
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override
    {
        return nullptr;
    }
};
bool titleIs(const String128 title, const char16* target)
{
    int i = 0;
    for (; i < 127 && title[i] && target[i]; ++i)
        if (title[i] != target[i])
            return false;
    return i < 128 && title[i] == target[i];
}
struct Frame final : IPlugFrame
{
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (isId(iid, INLINE_UID_OF(IPlugFrame)) || isId(iid, INLINE_UID_OF(FUnknown)))
        {
            *object = this;
            addRef();
            return kResultOk;
        }
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override
    {
        return ++references;
    }
    uint32 PLUGIN_API release() override
    {
        return --references;
    }
    tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* rectangle) override
    {
        ++requests;
        return view->onSize(rectangle);
    }
    uint32 references = 1;
    int requests = 0;
};
} // namespace

int main(int argc, char** argv)
{
    check(argc == 2, "Module path passed");
#if defined(_WIN32)
    auto library = LoadLibraryA(argv[1]);
    if (!library)
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
    check(library != nullptr, "Load VST3 module");
    auto entry = reinterpret_cast<bool (*)()>(GetProcAddress(library, "InitDll"));
    auto exitModule = reinterpret_cast<bool (*)()>(GetProcAddress(library, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(library, "GetPluginFactory"));
    check(entry && exitModule && getFactory && entry(), "VST3 module entry");
#else
    auto* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library)
        std::cerr << dlerror() << '\n';
    check(library != nullptr, "Load VST3 module");
    auto entry = reinterpret_cast<bool (*)(void*)>(dlsym(library, "ModuleEntry"));
    auto exitModule = reinterpret_cast<bool (*)()>(dlsym(library, "ModuleExit"));
    auto getFactory = reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(dlsym(library, "GetPluginFactory"));
    check(entry && exitModule && getFactory && entry(library), "VST3 module entry");
#endif
    auto* factory = getFactory();
    check(factory != nullptr, "VST3 factory");
    PClassInfo info{};
    int classIndex = -1;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk && !std::strcmp(info.category, "Audio Module Class"))
        {
            classIndex = i;
            break;
        }
    check(classIndex >= 0, "Audio class found");
    IPluginFactory2* factory2 = nullptr;
    check(factory->queryInterface(INLINE_UID_OF(IPluginFactory2), reinterpret_cast<void**>(&factory2)) == kResultOk &&
              factory2,
          "Query extended plugin factory");
    PClassInfo2 extended{};
    check(factory2->getClassInfo2(classIndex, &extended) == kResultOk &&
              std::strstr(extended.subCategories, "Fx") != nullptr &&
              std::strstr(extended.subCategories, "Instrument") == nullptr,
          "VST3 identifies itself as an effect, not an instrument");
    factory2->release();

    IComponent* component = nullptr;
    check(factory->createInstance(info.cid, INLINE_UID_OF(IComponent), reinterpret_cast<void**>(&component)) ==
                  kResultOk &&
              component,
          "Create component");
    Host host;
    check(component->initialize(&host) == kResultOk, "Initialize component");
    check(component->getBusCount(kAudio, kInput) == 1 && component->getBusCount(kAudio, kOutput) == 1,
          "Effect has one audio input and one audio output bus");
    BusInfo inputInfo{}, outputInfo{};
    check(component->getBusInfo(kAudio, kInput, 0, inputInfo) == kResultOk &&
              component->getBusInfo(kAudio, kOutput, 0, outputInfo) == kResultOk && inputInfo.channelCount == 2 &&
              outputInfo.channelCount == 2,
          "Default effect buses are stereo");
    IAudioProcessor* audio = nullptr;
    check(component->queryInterface(INLINE_UID_OF(IAudioProcessor), reinterpret_cast<void**>(&audio)) == kResultOk &&
              audio,
          "Query VST3 audio processor");
    SpeakerArrangement ins = SpeakerArr::kStereo, outs = SpeakerArr::kStereo;
    check(audio->setBusArrangements(&ins, 1, &outs, 1) == kResultTrue, "Set stereo input/output arrangement");
    ProcessSetup setup{kRealtime, kSample32, 2048, 44100.0};
    check(audio->setupProcessing(setup) == kResultOk, "Configure audio processing");
    check(component->activateBus(kAudio, kInput, 0, true) == kResultOk &&
              component->activateBus(kAudio, kOutput, 0, true) == kResultOk,
          "Activate input/output buses");
    check(component->setActive(true) == kResultOk && audio->setProcessing(true) == kResultOk, "Start processing");
    TUID controllerId;
    check(component->getControllerClassId(controllerId) == kResultOk, "Get controller class");
    IEditController* controller = nullptr;
    check(factory->createInstance(controllerId, INLINE_UID_OF(IEditController),
                                  reinterpret_cast<void**>(&controller)) == kResultOk &&
              controller,
          "Create controller");
    check(controller->initialize(&host) == kResultOk, "Initialize controller");
    IConnectionPoint *componentConnection = nullptr, *controllerConnection = nullptr;
    check(component->queryInterface(INLINE_UID_OF(IConnectionPoint), reinterpret_cast<void**>(&componentConnection)) ==
                  kResultOk &&
              controller->queryInterface(INLINE_UID_OF(IConnectionPoint),
                                         reinterpret_cast<void**>(&controllerConnection)) == kResultOk &&
              componentConnection != nullptr && controllerConnection != nullptr,
          "Query component connections");
    check(componentConnection->connect(controllerConnection) == kResultOk &&
              controllerConnection->connect(componentConnection) == kResultOk,
          "Connect component and controller");

    ParamID enabledId = 0, volumeId = 0, bypassId = 0;
    const std::array<const char16*, 7> extraNames{u"Time Mix", u"Volume Mix", u"Attack", u"Release", u"Tension",
                                                   u"Hold", u"Scratch"};
    std::array<ParamID, 7> extraIds{};
    std::array<bool, 7> gotExtras{};
    bool gotEnabled = false, gotVolume = false, gotBypass = false;
    for (int32 i = 0; i < controller->getParameterCount(); ++i)
    {
        ParameterInfo parameter{};
        if (controller->getParameterInfo(i, parameter) != kResultOk)
            continue;
        if (titleIs(parameter.title, u"Enabled"))
        {
            enabledId = parameter.id;
            gotEnabled = (parameter.flags & ParameterInfo::kCanAutomate) != 0;
        }
        if (titleIs(parameter.title, u"Volume 1"))
        {
            volumeId = parameter.id;
            gotVolume = (parameter.flags & ParameterInfo::kCanAutomate) != 0;
        }
        if ((parameter.flags & ParameterInfo::kIsBypass) != 0)
        {
            bypassId = parameter.id;
            gotBypass = true;
        }
        for (std::size_t j = 0; j < extraNames.size(); ++j)
            if (titleIs(parameter.title, extraNames[j]))
            {
                extraIds[j] = parameter.id;
                gotExtras[j] = (parameter.flags & ParameterInfo::kCanAutomate) != 0;
            }
    }
    check(gotEnabled && gotVolume && gotBypass, "Power, host bypass and curve points are exposed as VST3 parameters");
    check(std::all_of(gotExtras.begin(), gotExtras.end(), [](bool b) { return b; }),
          "Time/volume mixes, ATT, REL, TNS, HOLD and scratch are all host-automatable");
    constexpr int samples = 2048;
    float inLeft[samples], inRight[samples], outLeft[samples]{}, outRight[samples]{};
    for (int i = 0; i < samples; ++i)
    {
        inLeft[i] = 0.4f;
        inRight[i] = -0.3f;
    }
    Sample32* inChannels[] = {inLeft, inRight};
    Sample32* outChannels[] = {outLeft, outRight};
    AudioBusBuffers input, output;
    input.numChannels = output.numChannels = 2;
    input.channelBuffers32 = inChannels;
    output.channelBuffers32 = outChannels;
    ProcessContext context{};
    context.sampleRate = 44100.0;
    context.tempo = 120.0;
    context.projectTimeMusic = 0;
    context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid | ProcessContext::kProjectTimeMusicValid;
    ProcessData data;
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample32;
    data.numSamples = samples;
    data.numInputs = data.numOutputs = 1;
    data.inputs = &input;
    data.outputs = &output;
    data.processContext = &context;
    check(audio->process(data) == kResultOk, "Process live input in the default bypassed state");
    bool dry = true;
    for (int i = 0; i < samples; ++i)
        dry &= inLeft[i] == outLeft[i] && inRight[i] == outRight[i];
    check(dry, "Default VST3 effect passes both input channels through unchanged");
    Changes changes;
    changes.queues[0].id = enabledId;
    changes.queues[0].value = 1.0;
    changes.queues[1].id = volumeId;
    changes.queues[1].value = 0.0;
    data.inputParameterChanges = &changes;
    check(audio->process(data) == kResultOk, "Process VST3 host automation");
    check(std::abs(outLeft[samples - 1]) < 0.01f && std::abs(outRight[samples - 1]) < 0.01f,
          "Automated volume point audibly gates live stereo input");
    changes.queues[0].id = extraIds[1]; // Volume Mix
    changes.queues[0].value = 0.0;
    changes.queues[1].id = enabledId;
    changes.queues[1].value = 1.0;
    check(audio->process(data) == kResultOk && std::abs(outLeft[samples - 1] - .4f) < .01f &&
              std::abs(outRight[samples - 1] + .3f) < .01f,
          "VST3 automation independently removes the volume envelope");
    StateStream controlsState;
    check(component->getState(&controlsState) == kResultOk && !controlsState.bytes.empty(),
          "Save VST3 state containing the new controls");
    changes.queues[0].value = 1.0;
    check(audio->process(data) == kResultOk && std::abs(outLeft[samples - 1]) < .01f,
          "Restoring volume mix automation audibly re-enables gating");
    controlsState.position = 0;
    check(component->setState(&controlsState) == kResultOk, "Restore VST3 state containing the new controls");
    data.inputParameterChanges = nullptr;
    check(audio->process(data) == kResultOk && std::abs(outLeft[samples - 1] - .4f) < .01f,
          "Restored VST3 volume mix changes the actual audio");
    data.inputParameterChanges = &changes;
    changes.queues[0].id = extraIds[5]; // Hold
    changes.queues[0].value = 1.0;
    check(audio->process(data) == kResultOk && audio->getTailSamples() == kInfiniteTail,
          "Automating HOLD reports its infinite loop tail to the VST3 host");
    changes.queues[0].value = 0.0;
    check(audio->process(data) == kResultOk && audio->getTailSamples() != kInfiniteTail,
          "Releasing HOLD restores a finite tail");
    changes.queues[0].id = extraIds[1];
    changes.queues[0].value = 1.0;
    check(audio->process(data) == kResultOk && std::abs(outLeft[samples - 1]) < .01f,
          "Volume gate resumes after HOLD");
    changes.queues[0].id = bypassId;
    changes.queues[0].value = 1.0;
    changes.queues[1].id = enabledId;
    changes.queues[1].value = 1.0;
    check(audio->process(data) == kResultOk, "Apply VST3 host bypass");
    dry = true;
    for (int i = 0; i < samples; ++i)
        dry &= inLeft[i] == outLeft[i] && inRight[i] == outRight[i];
    check(dry, "Host bypass passes both input channels unchanged");
    changes.queues[0].value = 0.0;
    check(audio->process(data) == kResultOk && std::abs(outLeft[samples - 1]) < 0.01f,
          "Releasing host bypass resumes the effect");
    data.inputParameterChanges = nullptr;
    check(audio->setProcessing(false) == kResultOk && component->setActive(false) == kResultOk, "Stop processing");

    ins = outs = SpeakerArr::kMono;
    check(audio->setBusArrangements(&ins, 1, &outs, 1) == kResultTrue &&
              component->getBusInfo(kAudio, kInput, 0, inputInfo) == kResultOk && inputInfo.channelCount == 1,
          "VST3 supports a real mono insert arrangement");
    check(audio->setupProcessing(setup) == kResultOk && component->setActive(true) == kResultOk &&
              audio->setProcessing(true) == kResultOk,
          "Start mono processing");
    input.numChannels = output.numChannels = 1;
    changes.queues[0].id = enabledId;
    changes.queues[0].value = 0.0;
    changes.queues[1].id = bypassId;
    changes.queues[1].value = 0.0;
    data.inputParameterChanges = &changes;
    check(audio->process(data) == kResultOk, "Process mono input");
    dry = true;
    for (int i = 0; i < samples; ++i)
        dry &= inLeft[i] == outLeft[i];
    check(dry, "Mono input passes through when the effect is disabled");
    check(audio->setProcessing(false) == kResultOk && component->setActive(false) == kResultOk,
          "Stop mono processing");
    audio->release();

    auto* view = controller->createView(ViewType::kEditor);
    check(view != nullptr, "Create editor view");
    ViewRect bounds{};
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left == 1000 &&
              bounds.bottom - bounds.top == 820,
          "Initial VST3 view is 1000 x 820");
    check(view->canResize() == kResultTrue, "VST3 advertises host resizing");
    ViewRect tooSmall{0, 0, 200, 200};
    check(view->checkSizeConstraint(&tooSmall) == kResultTrue && tooSmall.right - tooSmall.left >= 600 &&
              tooSmall.bottom - tooSmall.top >= 492,
          "VST3 enforces minimum bounds");
    ViewRect tooLarge{0, 0, 3000, 3000};
    check(view->checkSizeConstraint(&tooLarge) == kResultTrue && tooLarge.right - tooLarge.left <= 1500 &&
              tooLarge.bottom - tooLarge.top <= 1230,
          "VST3 enforces maximum bounds");
    ViewRect uneven{0, 0, 900, 700};
    check(view->checkSizeConstraint(&uneven) == kResultTrue &&
              std::abs((uneven.right - uneven.left) * 820 - (uneven.bottom - uneven.top) * 1000) <= 1000,
          "VST3 constrains aspect ratio within one pixel");

    Frame frame;
    check(view->setFrame(&frame) == kResultOk, "Set host frame");
    ViewRect hostResize{0, 0, 750, 615};
    check(view->onSize(&hostResize) == kResultOk, "Host resizes view to 75 percent");
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left == 750 && bounds.bottom - bounds.top == 615,
          "VST3 reports the host-resized dimensions");
    StateStream saved;
    check(component->getState(&saved) == kResultOk && !saved.bytes.empty(), "Save VST3 component state at 75 percent");
    IPlugViewContentScaleSupport* dpi = nullptr;
    check(view->queryInterface(INLINE_UID_OF(IPlugViewContentScaleSupport), reinterpret_cast<void**>(&dpi)) ==
                  kResultOk &&
              dpi,
          "VST3 supports host DPI scale");
    check(dpi->setContentScaleFactor(1.25f) == kResultOk, "Apply 125 percent host DPI");
    check(frame.requests > 0, "DPI change requests host resize");
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left > 750 && bounds.bottom - bounds.top > 615,
          "VST3 reports DPI-scaled bounds");
    check(dpi->setContentScaleFactor(1.0f) == kResultOk, "Reset host DPI scale");
    ViewRect resetSize{0, 0, 1000, 820};
    check(view->onSize(&resetSize) == kResultOk, "Reset the view to 100 percent before restoring state");
    dpi->release();
    view->setFrame(nullptr);
    view->release();
    check(component->setState(&saved) == kResultOk, "Restore saved VST3 component state");
    auto* restoredView = controller->createView(ViewType::kEditor);
    check(restoredView != nullptr && restoredView->getSize(&bounds) == kResultOk && bounds.right - bounds.left == 750 &&
              bounds.bottom - bounds.top == 615,
          "Reopened VST3 view restores the saved 75 percent size");
    restoredView->release();

    componentConnection->disconnect(controllerConnection);
    controllerConnection->disconnect(componentConnection);
    componentConnection->release();
    controllerConnection->release();
    controller->terminate();
    controller->release();
    component->terminate();
    component->release();
    factory->release();
    check(exitModule(), "VST3 module exit");
#if defined(_WIN32)
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    std::cout << "All Laggify VST3 host audio and view checks passed.\n";
}
