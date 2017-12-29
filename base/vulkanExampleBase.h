/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "common.hpp"

#include "vks/vks.hpp"
#include "vks/helpers.hpp"
#include "vks/filesystem.hpp"
#include "vks/ui.hpp"
#include "vks/model.hpp"
#include "vks/texture.hpp"

#include "camera.hpp"

#define GAMEPAD_BUTTON_A 0x1000
#define GAMEPAD_BUTTON_B 0x1001
#define GAMEPAD_BUTTON_X 0x1002
#define GAMEPAD_BUTTON_Y 0x1003
#define GAMEPAD_BUTTON_L1 0x1004
#define GAMEPAD_BUTTON_R1 0x1005
#define GAMEPAD_BUTTON_START 0x1006

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
//#define ENABLE_VALIDATION true

namespace vkx {
    struct UpdateOperation {
        const vk::Buffer buffer;
        const vk::DeviceSize size;
        const vk::DeviceSize offset;
        const uint32_t* data;

        template <typename T>
        UpdateOperation(const vk::Buffer& buffer, const T& data, vk::DeviceSize offset = 0) : buffer(buffer), size(sizeof(T)), offset(offset), data((uint32_t*)&data) {
            assert(0 == (sizeof(T) % 4));
            assert(0 == (offset % 4));
        }
    };

    class ExampleBase {
    protected:
        ExampleBase();
        ~ExampleBase();

    public:
        void run();
        // Called if the window is resized and some resources have to be recreatesd
        void windowResize(const glm::uvec2& newSize);

    private:
        // Set to true when the debug marker extension is detected
        bool enableDebugMarkers{ false };
        // fps timer (one second interval)
        float fpsTimer = 0.0f;
        // Get window title with example name, device, et.
        std::string getWindowTitle();

    protected:
        bool enableVsync{ false };
        // Command buffers used for rendering
        std::vector<vk::CommandBuffer> primaryCmdBuffers;
        std::vector<vk::CommandBuffer> drawCmdBuffers;
        bool primaryCmdBuffersDirty{ true };
        std::vector<vk::ClearValue> clearValues;
        vk::RenderPassBeginInfo renderPassBeginInfo;


        vk::Viewport viewport() {
            return vks::util::viewport(size);
        }

        vk::Rect2D scissor() {
            return vks::util::rect2D(size);
        }

        vk::ShaderModule loadShaderModule(const std::string& filename) const {
            vk::ShaderModule result;
            vks::util::withBinaryFileContexts(filename, [&] (size_t size, const void* data) {
                result = context.device.createShaderModule(
                    vk::ShaderModuleCreateInfo{ {}, size, (const uint32_t*)data }
                );
            });
            vk::ShaderModuleCreateInfo x;
            return result;
        }

        // Load a SPIR-V shader
        vk::PipelineShaderStageCreateInfo loadShader(const std::string& fileName, vk::ShaderStageFlagBits stage) const {
            vk::PipelineShaderStageCreateInfo shaderStage;
            shaderStage.stage = stage;
            shaderStage.module = loadShaderModule(fileName);
            shaderStage.pName = "main"; // todo : make param
            return shaderStage;
        }

        virtual void setupRenderPassBeginInfo() {
            clearValues.clear();
            clearValues.push_back(vks::util::clearColor(glm::vec4(0.1, 0.1, 0.1, 1.0)));
            clearValues.push_back(vk::ClearDepthStencilValue{ 1.0f, 0 });

            renderPassBeginInfo = vk::RenderPassBeginInfo();
            renderPassBeginInfo.renderPass = renderPass;
            renderPassBeginInfo.renderArea.extent = size;
            renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
            renderPassBeginInfo.pClearValues = clearValues.data();
        }

        virtual void buildCommandBuffers() final {
            if (drawCmdBuffers.empty()) {
                throw std::runtime_error("Draw command buffers have not been populated.");
            }
            context.trashCommandBuffers(primaryCmdBuffers);

            // FIXME find a better way to ensure that the draw and text buffers are no longer in use before 
            // executing them within this command buffer.
            context.queue.waitIdle();

            // Destroy command buffers if already present
            if (primaryCmdBuffers.empty()) {
                // Create one command buffer per image in the swap chain

                // Command buffers store a reference to the
                // frame buffer inside their render pass info
                // so for static usage without having to rebuild
                // them each frame, we use one per frame buffer
                vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
                cmdBufAllocateInfo.commandPool = cmdPool;
                cmdBufAllocateInfo.commandBufferCount = swapChain.imageCount;
                primaryCmdBuffers = device.allocateCommandBuffers(cmdBufAllocateInfo);
            }

            vk::CommandBufferBeginInfo cmdBufInfo { vk::CommandBufferUsageFlagBits::eSimultaneousUse };
            for (size_t i = 0; i < swapChain.imageCount; ++i) {
                const auto& cmdBuffer = primaryCmdBuffers[i];
                cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
                cmdBuffer.begin(cmdBufInfo);

                // Let child classes execute operations outside the renderpass, like buffer barriers or query pool operations
                updatePrimaryCommandBuffer(cmdBuffer);

                renderPassBeginInfo.framebuffer = framebuffers[i];
                cmdBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eSecondaryCommandBuffers);
                if (!drawCmdBuffers.empty()) {
                    cmdBuffer.executeCommands(drawCmdBuffers[i]);
                }
                cmdBuffer.endRenderPass();
                cmdBuffer.end();
            }
            primaryCmdBuffersDirty = false;
        }
    protected:
        // Last frame time, measured using a high performance timer (if available)
        float frameTimer{ 1.0f };
        // Frame counter to display fps
        uint32_t frameCounter{ 0 };
        uint32_t lastFPS{ 0 };
        std::list<UpdateOperation> pendingUpdates;

        // Color buffer format
        vk::Format colorformat{ vk::Format::eB8G8R8A8Unorm };

        // Depth buffer format...  selected during Vulkan initialization
        vk::Format depthFormat{ vk::Format::eUndefined };

        // vk::Pipeline stage flags for the submit info structure
        vk::PipelineStageFlags submitPipelineStages = vk::PipelineStageFlagBits::eBottomOfPipe;
        // Contains command buffers and semaphores to be presented to the queue
        vk::SubmitInfo submitInfo;
        // Global render pass for frame buffer writes
        vk::RenderPass renderPass;

        // List of available frame buffers (same as number of swap chain images)
        std::vector<vk::Framebuffer> framebuffers;
        // Active frame buffer index
        uint32_t currentBuffer = 0;
        // Descriptor set pool
        vk::DescriptorPool descriptorPool;

        vks::Context context;
        const vk::PhysicalDevice& physicalDevice { context.physicalDevice };
        const vk::Device& device { context.device };
        const vk::Queue& queue { context.queue };
        vks::ui::UIOverlay ui { context };

        // Wraps the swap chain to present images (framebuffers) to the windowing system
        vks::SwapChain swapChain;

        // Synchronization semaphores
        struct {
            // Swap chain image presentation
            vk::Semaphore acquireComplete;
            // Command buffer submission and execution
            vk::Semaphore renderComplete;
            // UI buffer submission and execution
            vk::Semaphore overlayComplete;
            vk::Semaphore transferComplete;
        } semaphores;


        // Returns the base asset path (for shaders, models, textures) depending on the os
        const std::string& getAssetPath();

    protected:
        /** @brief Example settings that can be changed e.g. by command line arguments */
        struct Settings {
            /** @brief Activates validation layers (and message output) when set to true */
            bool validation = false;
            /** @brief Set to true if fullscreen mode has been requested via command line */
            bool fullscreen = false;
            /** @brief Set to true if v-sync will be forced for the swapchain */
            bool vsync = false;
            /** @brief Enable UI overlay */
            bool overlay = true;
        } settings;

        struct {
            bool left = false;
            bool right = false;
            bool middle = false;
        } mouseButtons;

        struct {
            bool active = false;
        } benchmark;

        // Command buffer pool
        vk::CommandPool cmdPool;

        bool prepared = false;
        vk::Extent2D size{ 1280, 720 };

        vk::ClearColorValue defaultClearColor = vks::util::clearColor(glm::vec4({ 0.025f, 0.025f, 0.025f, 1.0f }));

        // Defines a frame rate independent timer value clamped from -1.0...1.0
        // For use in animations, rotations, etc.
        float timer = 0.0f;
        // Multiplier for speeding up (or slowing down) the global timer
        float timerSpeed = 0.25f;

        bool paused = false;

        // Use to adjust mouse rotation speed
        float rotationSpeed = 1.0f;
        // Use to adjust mouse zoom speed
        float zoomSpeed = 1.0f;

        Camera camera;
        glm::vec2 mousePos;

        std::string title = "Vulkan Example";
        std::string name = "vulkanExample";
        vks::Image depthStencil;

        // Gamepad state (only one pad supported)

        struct GamePadState {
            struct Axes {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                float rz = 0.0f;
            } axes;
        } gamePadState;

        // OS specific
#if defined(__ANDROID__)
        // true if application has focused, false if moved to background
        bool focused = false;
        static int32_t handle_input_event(android_app* app, AInputEvent *event);
        int32_t onInput(AInputEvent* event);
        static void handle_app_cmd(android_app* app, int32_t cmd);
        void onAppCmd(int32_t cmd);
#else
        GLFWwindow* window;
#endif
        void updateOverlay();

        virtual void OnUpdateUIOverlay() {}
        virtual void OnSetupUIOverlay(vks::ui::UIOverlayCreateInfo& uiCreateInfo) {}

        // Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
        virtual void initVulkan();
        virtual void setupWindow();
        // A default draw implementation
        virtual void draw() {
            // Get next image in the swap chain (back/front buffer)
            prepareFrame();
            // Execute the compiled command buffer for the current swap chain image
            drawCurrentCommandBuffer();
            // Push the rendered frame to the surface
            submitFrame();
        }
        // Pure virtual render function (override in derived class)
        virtual void render() {
            if (!prepared) {
                return;
            }
            draw();
        }
        virtual void update(float deltaTime) {
            frameTimer = deltaTime;
            ++frameCounter;
            // Convert to clamped timer value
            if (!paused) {
                timer += timerSpeed * frameTimer;
                if (timer > 1.0) {
                    timer -= 1.0f;
                }
            }
            fpsTimer += (float)frameTimer;
            if (fpsTimer > 1.0f) {
#if !defined(__ANDROID__)
                std::string windowTitle = getWindowTitle();
                glfwSetWindowTitle(window, windowTitle.c_str());
#endif
                lastFPS = frameCounter;
                fpsTimer = 0.0f;
                frameCounter = 0;
            }

            updateOverlay();

            // Check gamepad state
            const float deadZone = 0.0015f;
            // todo : check if gamepad is present
            // todo : time based and relative axis positions
            bool updateView = false;
            // Rotate
            if (std::abs(gamePadState.axes.x) > deadZone) {
                camera.yawPitch.x += gamePadState.axes.x * 0.5f * rotationSpeed;
                updateView = true;
            }
            if (std::abs(gamePadState.axes.y) > deadZone) {
                camera.yawPitch.x += gamePadState.axes.y * 0.5f * rotationSpeed;
                updateView = true;
            }
            // Zoom
            if (std::abs(gamePadState.axes.rz) > deadZone) {
                camera.dolly(gamePadState.axes.rz * 0.01f * zoomSpeed);
                updateView = true;
            }
            if (updateView) {
                viewChanged();
            }

        }

        // Called when view change occurs
        // Can be overriden in derived class to e.g. update uniform buffers
        // Containing view dependant matrices
        virtual void viewChanged() {}

        // Called when the window has been resized
        // Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
        virtual void windowResized();

        // Setup default depth and stencil views
        void setupDepthStencil();
        // Create framebuffers for all requested swap chain images
        // Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
        virtual void setupFrameBuffer();

        // Setup a default render pass
        // Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
        virtual void setupRenderPass();

        void setupUi();

        void populateSubCommandBuffers(std::vector<vk::CommandBuffer>& cmdBuffers, std::function<void(const vk::CommandBuffer& commandBuffer)> f) {
            if (!cmdBuffers.empty()) {
                context.trashCommandBuffers(cmdBuffers);
            }

            vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
            cmdBufAllocateInfo.commandPool = context.getCommandPool();
            cmdBufAllocateInfo.commandBufferCount = swapChain.imageCount;
            cmdBufAllocateInfo.level = vk::CommandBufferLevel::eSecondary;
            cmdBuffers = device.allocateCommandBuffers(cmdBufAllocateInfo);

            vk::CommandBufferInheritanceInfo inheritance;
            inheritance.renderPass = renderPass;
            inheritance.subpass = 0;
            vk::CommandBufferBeginInfo beginInfo;
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
            beginInfo.pInheritanceInfo = &inheritance;
            for (uint32_t i = 0; i < swapChain.imageCount; ++i) {
                currentBuffer = i;
                inheritance.framebuffer = framebuffers[i];
                vk::CommandBuffer& cmdBuffer = cmdBuffers[i];
                cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
                cmdBuffer.begin(beginInfo);
                f(cmdBuffer);
                cmdBuffer.end();
            }
            currentBuffer = 0;
        }

        virtual void updatePrimaryCommandBuffer(const vk::CommandBuffer& cmdBuffer) {}

        virtual void updateDrawCommandBuffers() final {
            populateSubCommandBuffers(drawCmdBuffers, [&](const vk::CommandBuffer& cmdBuffer) {
                updateDrawCommandBuffer(cmdBuffer);
            });
            primaryCmdBuffersDirty = true;
        }

        // Pure virtual function to be overriden by the dervice class
        // Called in case of an event where e.g. the framebuffer has to be rebuild and thus
        // all command buffers that may reference this
        virtual void updateDrawCommandBuffer(const vk::CommandBuffer& drawCommand) = 0;

        void drawCurrentCommandBuffer(const vk::Semaphore& semaphore = vk::Semaphore()) {
            vk::Fence fence = swapChain.getSubmitFence();
            {
                uint32_t fenceIndex = currentBuffer;
                context.dumpster.push_back([fenceIndex, this] {
                    swapChain.clearSubmitFence(fenceIndex);
                });
            }

            // Command buffer(s) to be sumitted to the queue
            std::vector<vk::Semaphore> waitSemaphores{ { semaphore == vk::Semaphore() ? semaphores.acquireComplete : semaphore } };
            std::vector<vk::PipelineStageFlags> waitStages{ submitPipelineStages };
            if (semaphores.transferComplete) {
                auto transferComplete = semaphores.transferComplete;
                semaphores.transferComplete = vk::Semaphore();
                waitSemaphores.push_back(transferComplete);
                waitStages.push_back(vk::PipelineStageFlagBits::eTransfer);
                context.dumpster.push_back([transferComplete, this] {
                    device.destroySemaphore(transferComplete);
                });
            }

            context.emptyDumpster(fence);

            vk::Semaphore transferPending;
            std::vector<vk::Semaphore> signalSemaphores{ { semaphores.renderComplete } };
            if (!pendingUpdates.empty()) {
                transferPending = device.createSemaphore(vk::SemaphoreCreateInfo());
                signalSemaphores.push_back(transferPending);
            }

            {
                vk::SubmitInfo submitInfo;
                submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
                submitInfo.pWaitSemaphores = waitSemaphores.data();
                submitInfo.pWaitDstStageMask = waitStages.data();
                submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
                submitInfo.pSignalSemaphores = signalSemaphores.data();
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &primaryCmdBuffers[currentBuffer];
                // Submit to queue
                context.queue.submit(submitInfo, fence);
            }

            executePendingTransfers(transferPending);
            context.recycle();
        }

        void executePendingTransfers(vk::Semaphore transferPending) {
            if (!pendingUpdates.empty()) {
                vk::Fence transferFence = device.createFence(vk::FenceCreateInfo());
                semaphores.transferComplete = device.createSemaphore(vk::SemaphoreCreateInfo());
                assert(transferPending);
                assert(semaphores.transferComplete);
                // Command buffers store a reference to the
                // frame buffer inside their render pass info
                // so for static usage without having to rebuild
                // them each frame, we use one per frame buffer
                vk::CommandBuffer transferCmdBuffer;
                {
                    vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
                    cmdBufAllocateInfo.commandPool = cmdPool;
                    cmdBufAllocateInfo.commandBufferCount = 1;
                    transferCmdBuffer = device.allocateCommandBuffers(cmdBufAllocateInfo)[0];
                }


                {
                    vk::CommandBufferBeginInfo cmdBufferBeginInfo;
                    cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
                    transferCmdBuffer.begin(cmdBufferBeginInfo);
                    for (const auto& update : pendingUpdates) {
                        transferCmdBuffer.updateBuffer(update.buffer, update.offset, update.size, update.data);
                    }
                    transferCmdBuffer.end();
                }

                {
                    vk::PipelineStageFlags stageFlagBits = vk::PipelineStageFlagBits::eAllCommands;
                    vk::SubmitInfo transferSubmitInfo;
                    transferSubmitInfo.pWaitDstStageMask = &stageFlagBits;
                    transferSubmitInfo.pWaitSemaphores = &transferPending;
                    transferSubmitInfo.signalSemaphoreCount = 1;
                    transferSubmitInfo.pSignalSemaphores = &semaphores.transferComplete;
                    transferSubmitInfo.waitSemaphoreCount = 1;
                    transferSubmitInfo.commandBufferCount = 1;
                    transferSubmitInfo.pCommandBuffers = &transferCmdBuffer;
                    context.queue.submit(transferSubmitInfo, transferFence);
                }

                context.recycler.push({ transferFence, [transferPending, transferCmdBuffer, this] {
                    device.destroySemaphore(transferPending);
                    device.freeCommandBuffers(cmdPool, transferCmdBuffer);
                } });
                pendingUpdates.clear();
            }
        }

        // Prepare commonly used Vulkan functions
        virtual void prepare();

        bool platformLoopCondition();

        // Start the main render loop
        void renderLoop();

        // Prepare a submit info structure containing
        // semaphores and submit buffer info for vkQueueSubmit
        vk::SubmitInfo prepareSubmitInfo(
            const std::vector<vk::CommandBuffer>& commandBuffers,
            vk::PipelineStageFlags *pipelineStages);

        // Prepare the frame for workload submission
        // - Acquires the next image from the swap chain
        // - Submits a post present barrier
        // - Sets the default wait and signal semaphores
        void prepareFrame();

        // Submit the frames' workload
        // - Submits the text overlay (if enabled)
        // -
        void submitFrame();

        virtual const glm::mat4& getProjection() const {
            return camera.matrices.perspective;
        }

        virtual const glm::mat4& getView() const {
            return camera.matrices.view;
        }

        // Called if a key is pressed
        // Can be overriden in derived class to do custom key handling
        virtual void keyPressed(uint32_t key) {
            switch (key) {
            case GLFW_KEY_P:
                paused = !paused;
                break;

            case GLFW_KEY_F1:
                ui.visible = !ui.visible;
                break;

            case GLFW_KEY_ESCAPE:
#if defined(__ANDROID__)
#else
                glfwSetWindowShouldClose(window, 1);
#endif
                break;

            default:
                break;
            }
        }

#if defined(__ANDROID__)
#else
        // Keyboard movement handler
        virtual void mouseMoved(const glm::vec2& newPos) {
            glm::vec2 deltaPos = mousePos - newPos;
            if (deltaPos.x == 0 && deltaPos.y == 0) {
                return;
            }
            if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
                camera.dolly((deltaPos.y) * .005f * zoomSpeed);
                viewChanged();
            }
            if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
                camera.rotate(vec2(deltaPos.x, -deltaPos.y) * 0.02f * rotationSpeed);
                viewChanged();
            }
            if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) {
                camera.translate(deltaPos * -0.01f);
                viewChanged();
            }
            mousePos = newPos;
        }

        virtual void mouseScrolled(float delta) {
            camera.dolly((float)delta * 0.1f * zoomSpeed);
            viewChanged();
        }

        static void KeyboardHandler(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseHandler(GLFWwindow* window, int button, int action, int mods);
        static void MouseMoveHandler(GLFWwindow* window, double posx, double posy);
        static void MouseScrollHandler(GLFWwindow* window, double xoffset, double yoffset);
        static void FramebufferSizeHandler(GLFWwindow* window, int width, int height);
        static void CloseHandler(GLFWwindow* window);
#endif
    };
}
