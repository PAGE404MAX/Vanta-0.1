// A minimal in-process VST3 host: exercise the built plugin's view contract
// without requiring FL Studio or an attached native window.
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/vst/ivstcomponent.h>
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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    std::cout << "PASS: " << message << '\n';
}
bool isId(const TUID actual, const TUID expected) { return std::memcmp(actual, expected, sizeof(TUID)) == 0; }

// These objects live on the stack for the entire plugin lifetime; the simple
// reference counts let the plugin retain/release them as it would a real host.
struct Host final : IHostApplication
{
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (isId(iid, INLINE_UID_OF(IHostApplication)) || isId(iid, INLINE_UID_OF(FUnknown)))
        { *object = this; addRef(); return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++references; }
    uint32 PLUGIN_API release() override { return --references; }
    tresult PLUGIN_API getName(String128 output) override
    {
        const char16* name = u"VANTA test host";
        int i = 0; for (; name[i] && i < 127; ++i) output[i] = name[i];
        output[i] = 0; return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID, TUID, void** object) override { *object = nullptr; return kNoInterface; }
    uint32 references = 1;
};
struct StateStream final : IBStream
{
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (isId(iid, INLINE_UID_OF(IBStream)) || isId(iid, INLINE_UID_OF(FUnknown)))
        { *object = this; addRef(); return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++references; }
    uint32 PLUGIN_API release() override { return --references; }
    tresult PLUGIN_API read(void* destination, int32 count, int32* bytesRead) override
    {
        if (count < 0 || destination == nullptr) return kInvalidArgument;
        const auto n = std::min<std::size_t>(static_cast<std::size_t>(count), bytes.size() - position);
        if (n > 0) std::memcpy(destination, bytes.data() + position, n);
        position += n; if (bytesRead) *bytesRead = static_cast<int32>(n);
        return kResultOk;
    }
    tresult PLUGIN_API write(void* source, int32 count, int32* bytesWritten) override
    {
        if (count < 0 || source == nullptr) return kInvalidArgument;
        const auto n = static_cast<std::size_t>(count);
        bytes.resize(std::max(bytes.size(), position + n));
        if (n > 0) std::memcpy(bytes.data() + position, source, n);
        position += n; if (bytesWritten) *bytesWritten = count;
        return kResultOk;
    }
    tresult PLUGIN_API seek(int64 offset, int32 mode, int64* result) override
    {
        int64 base = 0;
        if (mode == kIBSeekCur) base = static_cast<int64>(position);
        else if (mode == kIBSeekEnd) base = static_cast<int64>(bytes.size());
        else if (mode != kIBSeekSet) return kInvalidArgument;
        if (offset < -base || offset > static_cast<int64>(bytes.size()) - base) return kInvalidArgument;
        position = static_cast<std::size_t>(base + offset);
        if (result) *result = static_cast<int64>(position);
        return kResultOk;
    }
    tresult PLUGIN_API tell(int64* result) override
    {
        if (!result) return kInvalidArgument;
        *result = static_cast<int64>(position); return kResultOk;
    }
    uint32 references = 1;
    std::vector<char> bytes;
    std::size_t position = 0;
};
struct Frame final : IPlugFrame
{
    tresult PLUGIN_API queryInterface(const TUID iid, void** object) override
    {
        if (isId(iid, INLINE_UID_OF(IPlugFrame)) || isId(iid, INLINE_UID_OF(FUnknown)))
        { *object = this; addRef(); return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return ++references; }
    uint32 PLUGIN_API release() override { return --references; }
    tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* rectangle) override
    {
        ++requests; return view->onSize(rectangle);
    }
    uint32 references = 1;
    int requests = 0;
};
}

int main(int argc, char** argv)
{
    check(argc == 2, "Module path passed");
#if defined(_WIN32)
    auto library = LoadLibraryA(argv[1]);
    if (!library) std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
    check(library != nullptr, "Load VST3 module");
    auto entry = reinterpret_cast<bool (*)()>(GetProcAddress(library, "InitDll"));
    auto exitModule = reinterpret_cast<bool (*)()>(GetProcAddress(library, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API*)()>(GetProcAddress(library, "GetPluginFactory"));
    check(entry && exitModule && getFactory && entry(), "VST3 module entry");
#else
    auto* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::cerr << dlerror() << '\n';
    check(library != nullptr, "Load VST3 module");
    auto entry = reinterpret_cast<bool (*)(void*)>(dlsym(library, "ModuleEntry"));
    auto exitModule = reinterpret_cast<bool (*)()>(dlsym(library, "ModuleExit"));
    auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API*)()>(dlsym(library, "GetPluginFactory"));
    check(entry && exitModule && getFactory && entry(library), "VST3 module entry");
#endif
    auto* factory = getFactory(); check(factory != nullptr, "VST3 factory");
    PClassInfo info {}; bool found = false;
    for (int i = 0; i < factory->countClasses(); ++i)
        if (factory->getClassInfo(i, &info) == kResultOk && !std::strcmp(info.category, "Audio Module Class"))
        { found = true; break; }
    check(found, "Audio class found");

    IComponent* component = nullptr;
    check(factory->createInstance(info.cid, INLINE_UID_OF(IComponent), reinterpret_cast<void**>(&component)) == kResultOk && component,
          "Create component");
    Host host; check(component->initialize(&host) == kResultOk, "Initialize component");
    TUID controllerId;
    check(component->getControllerClassId(controllerId) == kResultOk, "Get controller class");
    IEditController* controller = nullptr;
    check(factory->createInstance(controllerId, INLINE_UID_OF(IEditController), reinterpret_cast<void**>(&controller)) == kResultOk && controller,
          "Create controller");
    check(controller->initialize(&host) == kResultOk, "Initialize controller");
    IConnectionPoint *componentConnection = nullptr, *controllerConnection = nullptr;
    check(component->queryInterface(INLINE_UID_OF(IConnectionPoint), reinterpret_cast<void**>(&componentConnection)) == kResultOk
          && controller->queryInterface(INLINE_UID_OF(IConnectionPoint), reinterpret_cast<void**>(&controllerConnection)) == kResultOk
          && componentConnection != nullptr && controllerConnection != nullptr, "Query component connections");
    check(componentConnection->connect(controllerConnection) == kResultOk
          && controllerConnection->connect(componentConnection) == kResultOk, "Connect component and controller");

    auto* view = controller->createView(ViewType::kEditor); check(view != nullptr, "Create editor view");
    ViewRect bounds {};
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left == 1000 && bounds.bottom - bounds.top == 720,
          "Initial VST3 view is 1000 x 720");
    check(view->canResize() == kResultTrue, "VST3 advertises host resizing");
    ViewRect tooSmall { 0, 0, 200, 200 };
    check(view->checkSizeConstraint(&tooSmall) == kResultTrue && tooSmall.right - tooSmall.left >= 600
          && tooSmall.bottom - tooSmall.top >= 432, "VST3 enforces minimum bounds");
    ViewRect tooLarge { 0, 0, 3000, 3000 };
    check(view->checkSizeConstraint(&tooLarge) == kResultTrue && tooLarge.right - tooLarge.left <= 1500
          && tooLarge.bottom - tooLarge.top <= 1080, "VST3 enforces maximum bounds");
    ViewRect uneven { 0, 0, 900, 700 };
    check(view->checkSizeConstraint(&uneven) == kResultTrue
          && (uneven.right - uneven.left) * 720 == (uneven.bottom - uneven.top) * 1000, "VST3 constrains aspect ratio");

    Frame frame; check(view->setFrame(&frame) == kResultOk, "Set host frame");
    ViewRect hostResize { 0, 0, 750, 540 };
    check(view->onSize(&hostResize) == kResultOk, "Host resizes view to 75 percent");
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left == 750 && bounds.bottom - bounds.top == 540,
          "VST3 reports the host-resized dimensions");
    StateStream saved;
    check(component->getState(&saved) == kResultOk && !saved.bytes.empty(), "Save VST3 component state at 75 percent");
    IPlugViewContentScaleSupport* dpi = nullptr;
    check(view->queryInterface(INLINE_UID_OF(IPlugViewContentScaleSupport), reinterpret_cast<void**>(&dpi)) == kResultOk && dpi,
          "VST3 supports host DPI scale");
    check(dpi->setContentScaleFactor(1.25f) == kResultOk, "Apply 125 percent host DPI");
    check(frame.requests > 0, "DPI change requests host resize");
    check(view->getSize(&bounds) == kResultOk && bounds.right - bounds.left > 750 && bounds.bottom - bounds.top > 540,
          "VST3 reports DPI-scaled bounds");
    check(dpi->setContentScaleFactor(1.0f) == kResultOk, "Reset host DPI scale");
    ViewRect resetSize { 0, 0, 1000, 720 };
    check(view->onSize(&resetSize) == kResultOk, "Reset the view to 100 percent before restoring state");
    dpi->release(); view->setFrame(nullptr); view->release();
    check(component->setState(&saved) == kResultOk, "Restore saved VST3 component state");
    auto* restoredView = controller->createView(ViewType::kEditor);
    check(restoredView != nullptr && restoredView->getSize(&bounds) == kResultOk
          && bounds.right - bounds.left == 750 && bounds.bottom - bounds.top == 540,
          "Reopened VST3 view restores the saved 75 percent size");
    restoredView->release();

    componentConnection->disconnect(controllerConnection); controllerConnection->disconnect(componentConnection);
    componentConnection->release(); controllerConnection->release();
    controller->terminate(); controller->release(); component->terminate(); component->release(); factory->release();
    check(exitModule(), "VST3 module exit");
#if defined(_WIN32)
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    std::cout << "All VST3 host-view checks passed.\n";
}
