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
        const char16* name = u"Sidechain test host";
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
    check(component->getBusCount(kAudio, kInput) == 2 && component->getBusCount(kAudio, kOutput) == 1,
          "VST3 exposes separate main and external-key audio input buses");
    check(component->getBusCount(kEvent, kInput) == 1, "VST3 exposes a MIDI event input");
    BusInfo mainInfo{}, keyInfo{}, outputInfo{};
    check(component->getBusInfo(kAudio, kInput, 0, mainInfo) == kResultOk &&
              component->getBusInfo(kAudio, kInput, 1, keyInfo) == kResultOk &&
              component->getBusInfo(kAudio, kOutput, 0, outputInfo) == kResultOk && mainInfo.channelCount == 2 &&
              outputInfo.channelCount == 2 && keyInfo.busType == kAux,
          "VST3 main buses are stereo and the key is an auxiliary input");
    IAudioProcessor* audio = nullptr;
    check(component->queryInterface(INLINE_UID_OF(IAudioProcessor), reinterpret_cast<void**>(&audio)) == kResultOk &&
              audio,
          "Query VST3 processor");
    SpeakerArrangement inputs[] = {SpeakerArr::kStereo, SpeakerArr::kStereo}, outputs[] = {SpeakerArr::kStereo};
    check(audio->setBusArrangements(inputs, 2, outputs, 1) == kResultTrue,
          "Set main and sidechain stereo arrangements");
    ProcessSetup setup{kRealtime, kSample32, 2048, 44100.0};
    check(audio->setupProcessing(setup) == kResultOk, "Configure processing");
    check(component->activateBus(kAudio, kInput, 0, true) == kResultOk &&
              component->activateBus(kAudio, kInput, 1, true) == kResultOk &&
              component->activateBus(kAudio, kOutput, 0, true) == kResultOk,
          "Activate main and sidechain buses");
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
              componentConnection && controllerConnection,
          "Query component connections");
    check(componentConnection->connect(controllerConnection) == kResultOk &&
              controllerConnection->connect(componentConnection) == kResultOk,
          "Connect component and controller");
    ParamID attackId = 0, smoothId = 0, bypassId = 0;
    bool gotAttack = false, gotSmooth = false, gotBypass = false;
    for (int32 i = 0; i < controller->getParameterCount(); ++i)
    {
        ParameterInfo parameter{};
        if (controller->getParameterInfo(i, parameter) != kResultOk)
            continue;
        if (titleIs(parameter.title, u"Volume Trigger Attack"))
        {
            attackId = parameter.id;
            gotAttack = (parameter.flags & ParameterInfo::kCanAutomate) != 0;
        }
        if (titleIs(parameter.title, u"Volume Smoothing"))
        {
            smoothId = parameter.id;
            gotSmooth = (parameter.flags & ParameterInfo::kCanAutomate) != 0;
        }
        if ((parameter.flags & ParameterInfo::kIsBypass) != 0)
        {
            bypassId = parameter.id;
            gotBypass = true;
        }
    }
    check(gotAttack && gotSmooth && gotBypass && controller->getParameterCount() > 900,
          "Trigger, curve and host bypass parameters are exposed for automation");
    constexpr int n = 2048;
    float left[n], right[n], keyLeft[n]{}, keyRight[n]{}, outLeft[n]{}, outRight[n]{};
    for (int i = 0; i < n; ++i)
    {
        left[i] = .4f;
        right[i] = -.3f;
    }
    Sample32* mainChannels[] = {left, right};
    Sample32* keyChannels[] = {keyLeft, keyRight};
    Sample32* outputChannels[] = {outLeft, outRight};
    AudioBusBuffers bus[2]{}, out{};
    bus[0].numChannels = 2;
    bus[0].channelBuffers32 = mainChannels;
    bus[1].numChannels = 2;
    bus[1].channelBuffers32 = keyChannels;
    out.numChannels = 2;
    out.channelBuffers32 = outputChannels;
    ProcessContext context{};
    context.sampleRate = 44100;
    context.tempo = 120;
    context.projectTimeMusic = 0;
    context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid | ProcessContext::kProjectTimeMusicValid;
    ProcessData data{};
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample32;
    data.numSamples = n;
    data.numInputs = 2;
    data.numOutputs = 1;
    data.inputs = bus;
    data.outputs = &out;
    data.processContext = &context;
    check(audio->process(data) == kResultOk, "Process live main audio without a key");
    bool dry = true;
    for (int i = 0; i < n; ++i)
        dry &= std::abs(outLeft[i] - left[i]) < .00001f && std::abs(outRight[i] - right[i]) < .00001f;
    check(dry, "No sidechain signal leaves the default curve transparent");
    Changes changes;
    changes.queues[0].id = attackId;
    changes.queues[0].value = 0;
    changes.queues[1].id = smoothId;
    changes.queues[1].value = 0;
    data.inputParameterChanges = &changes;
    for (int i = 32; i < 320; ++i)
        keyLeft[i] = keyRight[i] = 1.0f;
    check(audio->process(data) == kResultOk, "Process actual external key audio");
    check(std::abs(outLeft[16] - .4f) < .001f && outLeft[32] < .16f && outRight[32] > -.13f,
          "External key audibly ducks both main channels at the trigger");
    check(keyLeft[32] == 1.0f && keyRight[32] == 1.0f, "External input is not written as output");
    StateStream saved;
    check(component->getState(&saved) == kResultOk && !saved.bytes.empty(), "Save VST3 state");
    changes.queues[0].id = bypassId;
    changes.queues[0].value = 1;
    changes.queues[1].id = smoothId;
    changes.queues[1].value = 0;
    check(audio->process(data) == kResultOk, "Apply host bypass");
    dry = true;
    for (int i = 0; i < n; ++i)
        dry &= outLeft[i] == left[i] && outRight[i] == right[i];
    check(dry, "VST3 host bypass is exactly dry");
    data.inputParameterChanges = nullptr;
    saved.position = 0;
    check(component->setState(&saved) == kResultOk, "Restore VST3 component state");
    check(audio->setProcessing(false) == kResultOk && component->setActive(false) == kResultOk, "Stop processing");
    audio->release();
    auto* view = controller->createView(ViewType::kEditor);
    check(view != nullptr, "Create native VST3 editor");
    ViewRect bounds{};
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left == 1300 &&
              bounds.bottom - bounds.top == 850,
          "Native editor opens at the intended size");
    check(view->canResize() == kResultTrue, "VST3 editor supports resizing");
    ViewRect small{0, 0, 200, 200};
    check(view->checkSizeConstraint(&small) == kResultTrue && small.right - small.left >= 780 &&
              small.bottom - small.top >= 510,
          "Editor enforces minimum bounds");
    ViewRect large{0, 0, 3000, 3000};
    check(view->checkSizeConstraint(&large) == kResultTrue && large.right - large.left <= 1950 &&
              large.bottom - large.top <= 1275,
          "Editor enforces maximum bounds");
    Frame frame;
    check(view->setFrame(&frame) == kResultOk, "Attach host frame");
    ViewRect scaled{0, 0, 975, 638};
    check(view->onSize(&scaled) == kResultOk, "Host can resize the editor");
    IPlugViewContentScaleSupport* dpi = nullptr;
    check(view->queryInterface(INLINE_UID_OF(IPlugViewContentScaleSupport), reinterpret_cast<void**>(&dpi)) ==
                  kResultOk &&
              dpi,
          "DPI scaling is supported");
    check(dpi->setContentScaleFactor(1.25f) == kResultOk && frame.requests > 0, "Host DPI change requests a resize");
    dpi->setContentScaleFactor(1.0f);
    dpi->release();
    view->setFrame(nullptr);
    view->release();
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
    std::cout << "All Sidechain VST3 host checks passed.\n";
}
