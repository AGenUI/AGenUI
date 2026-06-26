#include "audio_player_component.h"

#include "log/a2ui_capi_log.h"
#include "../../measure/a2ui_platform_layout_bridge.h"
#include "../../utils/a2ui_color_palette.h"
#include "a2ui/utils/a2ui_parse_utils.h"
#include "../a2ui_node.h"

#include <string>

extern const std::string& a2ui_get_files_dir();

namespace a2ui {

using colors::kColorTransparent;
using colors::kColorShadow20;

namespace {

constexpr const char* kPlayIconFileName = "audio_play.svg";
constexpr const char* kPauseIconFileName = "audio_pause.svg";
constexpr const char* kLoadingRingFileName = "audio_loading_ring.svg";
constexpr float kProgressTotal = 100.0f;

std::string buildAudioIconSrc(const char* fileName) {
    const std::string& filesDir = a2ui_get_files_dir();
    if (filesDir.empty()) {
        return std::string();
    }
    return "file://" + filesDir + "/data/icons/" + fileName;
}

float clampProgress(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

} // namespace

AudioPlayerComponent::AudioPlayerComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "AudioPlayer") {
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_STACK);

    loadStyleConfig();

    {
        A2UINode node(m_nodeHandle);
        node.setWidth(m_size);
        node.setHeight(m_size);
        node.setMargin(0.0f, 0.0f, 0.0f, 0.0f);
        node.setBorderRadius(m_size * 0.5f);
        node.setClip(true);
        node.setCustomShadow(8.0f, 0.0f, 2.0f, kColorShadow20);
    }

    createUI();

    m_avPlayer = OH_AVPlayer_Create();
    if (m_avPlayer) {
        OH_AVPlayer_SetOnInfoCallback(m_avPlayer, onPlayerInfoCallback, this);
        OH_AVPlayer_SetOnErrorCallback(m_avPlayer, onPlayerErrorCallback, this);
        HM_LOGI("AudioPlayerComponent - AVPlayer created, id=%s", id.c_str());
    } else {
        HM_LOGE("AudioPlayerComponent - Failed to create AVPlayer, id=%s", id.c_str());
    }

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    applyVisualState();
    HM_LOGI("AudioPlayerComponent - Created: id=%s", id.c_str());
}

AudioPlayerComponent::~AudioPlayerComponent() {
    HM_LOGI("AudioPlayerComponent - Destroying: id=%s", m_id.c_str());
    releasePlayer();
    destroyUI();
    HM_LOGI("AudioPlayerComponent - Destroyed: id=%s", m_id.c_str());
}

void AudioPlayerComponent::onDestroy() {
    releasePlayer();
    destroyUI();
}

void AudioPlayerComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE("handle is null, id=%s", m_id.c_str());
        return;
    }

    if (properties.contains("description") && properties["description"].is_string()) {
        m_description = properties["description"].get<std::string>();
    }

    if (properties.contains("url") && properties["url"].is_string()) {
        std::string newUrl = properties["url"].get<std::string>();
        if (!newUrl.empty()) {
            setAudioUrl(newUrl);
        }
    }

    applyVisualState();
    HM_LOGI("url=%s, id=%s",
            m_audioUrl.c_str(), m_id.c_str());
}

void AudioPlayerComponent::loadStyleConfig() {
    const nlohmann::json styles = getComponentStylesFor("AudioPlayer");
    if (styles.is_null() || !styles.is_object()) {
        return;
    }

    m_size = parseStyleDimension(styles, "size", m_size);
    m_playIconSize = parseStyleDimension(styles, "play-icon-size", m_playIconSize);
    m_pauseIconSize = parseStyleDimension(styles, "pause-icon-size", m_pauseIconSize);
    m_ringWidth = parseStyleDimension(styles, "ring-width", m_ringWidth);

    if (styles.contains("play-bg-color") && styles["play-bg-color"].is_string()) {
        m_playBgColor = parseColor(styles["play-bg-color"].get<std::string>());
    }
    if (styles.contains("pause-bg-color") && styles["pause-bg-color"].is_string()) {
        m_pauseBgColor = parseColor(styles["pause-bg-color"].get<std::string>());
    }
    if (styles.contains("ring-color") && styles["ring-color"].is_string()) {
        m_ringColor = parseColor(styles["ring-color"].get<std::string>());
    }
    if (styles.contains("play-icon-color") && styles["play-icon-color"].is_string()) {
        m_playIconColor = parseColor(styles["play-icon-color"].get<std::string>());
    }
    if (styles.contains("pause-icon-color") && styles["pause-icon-color"].is_string()) {
        m_pauseIconColor = parseColor(styles["pause-icon-color"].get<std::string>());
    }
    if (styles.contains("loading-color") && styles["loading-color"].is_string()) {
        m_loadingColor = parseColor(styles["loading-color"].get<std::string>());
    }
    if (styles.contains("error-bg-color") && styles["error-bg-color"].is_string()) {
        m_errorBgColor = parseColor(styles["error-bg-color"].get<std::string>());
    }
}

void AudioPlayerComponent::createUI() {
    m_ringHandle = g_nodeAPI->createNode(ARKUI_NODE_PROGRESS);
    m_loadingHandle = g_nodeAPI->createNode(ARKUI_NODE_IMAGE);
    m_iconContainerHandle = g_nodeAPI->createNode(ARKUI_NODE_STACK);
    m_iconHandle = g_nodeAPI->createNode(ARKUI_NODE_IMAGE);

    {
        A2UIProgressNode ring(m_ringHandle);
        ring.setWidth(m_size);
        ring.setHeight(m_size);
        ring.setPosition(0.0f, 0.0f);
        ring.setTotal(kProgressTotal);
        ring.setValue(0.0f);
        ring.setColor(m_ringColor);
        ring.setType(ARKUI_PROGRESS_TYPE_SCALE_RING);
        ring.setOpacity(0.0f);
        ring.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
    }

    {
        A2UIImageNode loading(m_loadingHandle);
        loading.setWidth(m_size);
        loading.setHeight(m_size);
        loading.setPosition(0.0f, 0.0f);
        loading.setObjectFitFill();
        loading.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
        loading.setSrc(buildAudioIconSrc(kLoadingRingFileName));
        A2UINode(m_loadingHandle).setOpacity(0.0f);
        A2UINode(m_loadingHandle).setTransformCenterPercent(0.5f, 0.5f);
    }

    {
        A2UINode iconContainer(m_iconContainerHandle);
        iconContainer.setWidth(m_size);
        iconContainer.setHeight(m_size);
        iconContainer.setPosition(0.0f, 0.0f);
        iconContainer.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
    }

    {
        A2UIImageNode icon(m_iconHandle);
        icon.setWidth(m_playIconSize);
        icon.setHeight(m_playIconSize);
        icon.setObjectFitFill();
        icon.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
        icon.setSrc(buildAudioIconSrc(kPlayIconFileName));
        icon.setFillColor(m_playIconColor);
    }

    g_nodeAPI->addChild(m_nodeHandle, m_ringHandle);
    g_nodeAPI->addChild(m_nodeHandle, m_loadingHandle);
    g_nodeAPI->addChild(m_iconContainerHandle, m_iconHandle);
    g_nodeAPI->addChild(m_nodeHandle, m_iconContainerHandle);

    g_nodeAPI->addNodeEventReceiver(m_nodeHandle, onPlayPauseBtnClickEvent);
    g_nodeAPI->registerNodeEvent(m_nodeHandle, NODE_ON_CLICK, 0, this);

    HM_LOGI("UI created, id=%s", m_id.c_str());
}

void AudioPlayerComponent::destroyUI() {
    if (m_nodeHandle) {
        g_nodeAPI->unregisterNodeEvent(m_nodeHandle, NODE_ON_CLICK);
    }

    if (m_nodeHandle && m_ringHandle) {
        g_nodeAPI->removeChild(m_nodeHandle, m_ringHandle);
        g_nodeAPI->disposeNode(m_ringHandle);
        m_ringHandle = nullptr;
    }

    if (m_nodeHandle && m_loadingHandle) {
        g_nodeAPI->removeChild(m_nodeHandle, m_loadingHandle);
        g_nodeAPI->disposeNode(m_loadingHandle);
        m_loadingHandle = nullptr;
    }

    if (m_iconContainerHandle && m_iconHandle) {
        g_nodeAPI->removeChild(m_iconContainerHandle, m_iconHandle);
        g_nodeAPI->disposeNode(m_iconHandle);
        m_iconHandle = nullptr;
    }

    if (m_nodeHandle && m_iconContainerHandle) {
        g_nodeAPI->removeChild(m_nodeHandle, m_iconContainerHandle);
        g_nodeAPI->disposeNode(m_iconContainerHandle);
        m_iconContainerHandle = nullptr;
    }

    HM_LOGI("UI destroyed, id=%s", m_id.c_str());
}

void AudioPlayerComponent::updateProgressRing() {
    if (!m_ringHandle) {
        return;
    }

    float progress = 0.0f;
    if (m_duration > 0) {
        progress = clampProgress(static_cast<float>(m_currentPosition) /
                                 static_cast<float>(m_duration));
    }

    A2UIProgressNode ring(m_ringHandle);
    ring.setTotal(kProgressTotal);
    ring.setValue(progress * kProgressTotal);
}

void AudioPlayerComponent::applyVisualState() {
    if (!m_nodeHandle || !m_ringHandle || !m_loadingHandle || !m_iconContainerHandle || !m_iconHandle) {
        return;
    }

    A2UINode node(m_nodeHandle);
    A2UIProgressNode ring(m_ringHandle);
    A2UIImageNode loading(m_loadingHandle);
    A2UINode iconContainer(m_iconContainerHandle);
    A2UIImageNode icon(m_iconHandle);

    node.setWidth(m_size);
    node.setHeight(m_size);
    node.setBorderRadius(m_size * 0.5f);
    node.setBorderWidth(0.0f, 0.0f, 0.0f, 0.0f);
    node.setBorderColor(kColorTransparent);
    node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    node.setBackgroundColor(m_playBgColor);
    node.setOpacity(1.0f);

    ring.setWidth(m_size);
    ring.setHeight(m_size);
    ring.setPosition(0.0f, 0.0f);
    ring.setTotal(kProgressTotal);
    ring.setValue(0.0f);
    ring.setColor(m_ringColor);
    ring.setType(ARKUI_PROGRESS_TYPE_SCALE_RING);
    ring.setOpacity(0.0f);

    loading.setWidth(m_size);
    loading.setHeight(m_size);
    loading.setPosition(0.0f, 0.0f);
    loading.setObjectFitFill();
    loading.setSrc(buildAudioIconSrc(kLoadingRingFileName));
    A2UINode(m_loadingHandle).setTransformCenterPercent(0.5f, 0.5f);
    A2UINode(m_loadingHandle).resetRotateTransition();
    A2UINode(m_loadingHandle).setRotate(0.0f, 0.0f, 1.0f, 0.0f);
    A2UINode(m_loadingHandle).setOpacity(0.0f);

    iconContainer.setWidth(m_size);
    iconContainer.setHeight(m_size);
    iconContainer.setPosition(0.0f, 0.0f);

    float iconSize = m_playIconSize;
    float iconOffsetX = 0.0f;
    if (m_audioCurrentState == "playing") {
        iconSize = m_pauseIconSize;
        iconOffsetX = 0.0f;
    } else {
        iconOffsetX = 3.0f;
    }
    float iconLeft = (m_size - iconSize) * 0.5f + iconOffsetX;
    float iconTop = (m_size - iconSize) * 0.5f;

    icon.setWidth(iconSize);
    icon.setHeight(iconSize);
    icon.setPosition(iconLeft, iconTop);
    icon.setObjectFitFill();
    icon.resetMargin();
    icon.setSrc(buildAudioIconSrc(kPlayIconFileName));
    icon.setFillColor(m_playIconColor);
    A2UINode(m_iconHandle).setOpacity(1.0f);

    if (m_audioCurrentState == "playing") {
        node.setBackgroundColor(m_pauseBgColor);
        ring.setType(ARKUI_PROGRESS_TYPE_SCALE_RING);
        ring.setColor(m_ringColor);
        ring.setOpacity(1.0f);
        updateProgressRing();
        icon.setSrc(buildAudioIconSrc(kPauseIconFileName));
        icon.setFillColor(m_pauseIconColor);
        return;
    }

    if (m_audioCurrentState == "preparing") {
        node.setBackgroundColor(m_pauseBgColor);
        A2UINode(m_loadingHandle).setOpacity(1.0f);
        A2UINode(m_loadingHandle).setRotateTransition(
            0.0f, 0.0f, 1.0f, -360.0f, 0.0f,
            1000, ARKUI_CURVE_LINEAR, 0, -1, ARKUI_ANIMATION_PLAY_MODE_NORMAL, 1.0f);
        A2UINode(m_loadingHandle).setRotate(0.0f, 0.0f, 1.0f, -360.0f);
        A2UINode(m_iconHandle).setOpacity(0.0f);
        return;
    }

    if (m_audioCurrentState == "error") {
        node.setBackgroundColor(m_errorBgColor);
        icon.setSrc(buildAudioIconSrc(kPlayIconFileName));
        icon.setFillColor(m_playIconColor);
        return;
    }

    if (m_audioCurrentState == "destroyed") {
        node.setOpacity(0.6f);
    }
}

void AudioPlayerComponent::onPlayPauseBtnClickEvent(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) {
        return;
    }

    AudioPlayerComponent* self = static_cast<AudioPlayerComponent*>(userData);
    HM_LOGI("isPlaying=%d, state=%s, id=%s",
            self->m_isPlaying ? 1 : 0, self->m_audioCurrentState.c_str(), self->m_id.c_str());

    if (self->m_isPlaying) {
        self->pause();
        return;
    }

    if (self->m_audioCurrentState == "completed") {
        if (self->m_avPlayer) {
            OH_AVPlayer_Seek(self->m_avPlayer, 0, AV_SEEK_NEXT_SYNC);
        }
        self->m_currentPosition = 0;
        self->play();
        return;
    }

    if (self->m_audioCurrentState == "prepared" || self->m_audioCurrentState == "paused") {
        self->play();
        return;
    }

    if ((self->m_audioCurrentState.empty() || self->m_audioCurrentState == "idle") &&
        !self->m_audioUrl.empty() && self->m_avPlayer) {
        self->m_pendingPlay = true;
        self->m_audioCurrentState = "preparing";
        self->applyVisualState();
        self->handleAudioPrepare(self->m_audioUrl);
    }
}

} // namespace a2ui
