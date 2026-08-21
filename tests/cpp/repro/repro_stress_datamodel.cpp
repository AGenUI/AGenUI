// Reproducer: 通过 updateDataModel 发送极长路径 + 特殊字符的值，
// 测试 Surface::updateDataModel / Surface::appendDataModel 在处理
// 病理输入时的行为。
//
// 场景:
//   发送 updateDataModel 协议，其中：
//   - path 为极长的路径（100KB+ 的嵌套路径）
//   - value 为包含大量转义序列的字符串
//   验证引擎不会因此崩溃或死锁。
//
// 预期结果:
//   引擎正常处理，打印 PASS 后退出 (exit 0)。
//   如果引擎崩溃或死锁，则探测到问题。

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "nlohmann/json.hpp"
#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"

static void sendAndSync(agenui::ISurfaceManager* sm, const std::string& data) {
    sm->beginTextStream();
    sm->receiveTextChunk(data);
    sm->endTextStream();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

int main() {
    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "initAGenUIEngine failed\n");
        return 2;
    }

    agenui::ISurfaceManager* sm = engine->createSurfaceManager();
    if (!sm) {
        std::fprintf(stderr, "createSurfaceManager failed\n");
        agenui::destroyAGenUIEngine();
        return 2;
    }

    sm->beginTextStream();
    sm->endTextStream();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Step 1: Create surface
    sendAndSync(sm,
        R"({"version":"v0.9",)"
        R"("createSurface":{"surfaceId":"stress_surface","catalogId":"test",)"
        R"("theme":{},"sendDataModel":false,"animated":true}})");

    // Step 2: 极长 path（100KB 级别）
    {
        std::string longPath = "/";
        for (int i = 0; i < 5000; i++) {
            longPath += "a/b/c/d/e/f/g/h/i/j/";
        }

        std::string payload =
            R"({"updateDataModel":{)"
            R"("surfaceId":"stress_surface",)"
            R"("path":")" + longPath + R"(",)"
            R"("value":"long_path_test"})"
            R"(})";

        std::printf("Sending updateDataModel with path length=%zu bytes\n", longPath.size());

        sm->beginTextStream();
        sm->receiveTextChunk(payload);
        sm->endTextStream();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Step 3: 极大 value（100KB+ 字符串）
    {
        std::string largeValue;
        for (int i = 0; i < 100000; i++) {
            largeValue += "ABCDEFGHIJ";
        }

        // 用 nlohmann::json 构建 payload 确保 value 被正确转义
        nlohmann::json udm;
        udm["surfaceId"] = "stress_surface";
        udm["path"] = "/test";
        udm["value"] = largeValue;

        nlohmann::json root;
        root["updateDataModel"] = udm;
        std::string payload = root.dump();

        std::printf("Sending updateDataModel with value length=%zu bytes\n", largeValue.size());

        sm->beginTextStream();
        sm->receiveTextChunk(payload);
        sm->endTextStream();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Step 4: 空 path + 空 value
    {
        sm->beginTextStream();
        sm->receiveTextChunk(
            R"({"updateDataModel":{"surfaceId":"stress_surface",)"
            R"("path":"","value":""}})"
        );
        sm->endTextStream();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Step 5: appendDataModel 极长 value
    {
        std::string largeValue(50000, 'X');

        nlohmann::json adm;
        adm["surfaceId"] = "stress_surface";
        adm["path"] = "/large";
        adm["value"] = largeValue;

        nlohmann::json root;
        root["appendDataModel"] = adm;
        std::string payload = root.dump();

        std::printf("Sending appendDataModel with value length=%zu bytes\n", largeValue.size());

        sm->beginTextStream();
        sm->receiveTextChunk(payload);
        sm->endTextStream();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::printf("repro_stress_datamodel: PASS (no crash during stress)\n");

    engine->destroySurfaceManager(sm);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    agenui::destroyAGenUIEngine();
    return 0;
}