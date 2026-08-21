#include "a2ui_default_registry.h"
#include "a2ui_component_registry.h"
#include "a2ui_component_creator.h"
#include "../a2ui_component_types.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

ComponentRegistry& getDefaultFactoryRegistry() {
    static ComponentRegistry* p = [] {
        auto* r = new ComponentRegistry();
        r->setOwnsFactories(true);

        auto makeCreator = [](const std::string& type) -> A2UIComponentCreator* {
            auto* creator = new A2UIComponentCreator();
            creator->setType(type);
            return creator;
        };

        // Base components
        r->registerFactory(ComponentType::kText,         makeCreator(ComponentType::kText));
        // NOTE: "AmapText" is intentionally NOT aliased to the native Text component.
        // The amap host registers an ArkTS hybrid component under this type name to
        // provide SpanText rich-text rendering (text/spans dual mode). With no native
        // factory registered, ComponentRegistry falls back to the hybrid registry.
        r->registerFactory(ComponentType::kButton,       makeCreator(ComponentType::kButton));
        r->registerFactory(ComponentType::kImage,        makeCreator(ComponentType::kImage));
        r->registerFactory(ComponentType::kIcon,         makeCreator(ComponentType::kIcon));
        r->registerFactory(ComponentType::kDivider,      makeCreator(ComponentType::kDivider));
        r->registerFactory(ComponentType::kVideo,        makeCreator(ComponentType::kVideo));
        r->registerFactory(ComponentType::kAudioPlayer,  makeCreator(ComponentType::kAudioPlayer));
        r->registerFactory(ComponentType::kModal,        makeCreator(ComponentType::kModal));

        // Layout components
        r->registerFactory(ComponentType::kColumn,       makeCreator(ComponentType::kColumn));
        r->registerFactory(ComponentType::kRow,          makeCreator(ComponentType::kRow));
        r->registerFactory(ComponentType::kCard,         makeCreator(ComponentType::kCard));
        r->registerFactory(ComponentType::kTabs,         makeCreator(ComponentType::kTabs));
        r->registerFactory(ComponentType::kList,         makeCreator(ComponentType::kList));

        // Interactive components
        r->registerFactory(ComponentType::kTextField,    makeCreator(ComponentType::kTextField));
        r->registerFactory(ComponentType::kCheckBox,     makeCreator(ComponentType::kCheckBox));
        r->registerFactory(ComponentType::kSlider,       makeCreator(ComponentType::kSlider));
        r->registerFactory(ComponentType::kChoicePicker, makeCreator(ComponentType::kChoicePicker));
        r->registerFactory(ComponentType::kDateTimeInput,makeCreator(ComponentType::kDateTimeInput));

        // Extended components
        r->registerFactory(ComponentType::kRichText,     makeCreator(ComponentType::kRichText));
        r->registerFactory(ComponentType::kTable,        makeCreator(ComponentType::kTable));
        r->registerFactory(ComponentType::kCarousel,     makeCreator(ComponentType::kCarousel));
        r->registerFactory(ComponentType::kWeb,          makeCreator(ComponentType::kWeb));

        HM_LOGI("Default factory registry initialized with %d factories",
                 r->getRegisteredFactoryCount());
        return r;
    }();
    return *p;
}

} // namespace a2ui
