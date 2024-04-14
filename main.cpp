#include <../third_party/glfw/include/GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>
#include <iostream>

#define UNUSED(x) (void)(x)

const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

wgpu::SwapChain swapChain;
wgpu::RenderPipeline pipeline;
wgpu::Instance instance;
wgpu::Device device;

void GetDevice(void (*callback)(wgpu::Device)) {
    instance.RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
            const char* message, void* userdata) {

            UNUSED(status); UNUSED(message); UNUSED(status);
                if (status != WGPURequestAdapterStatus_Success) {
                    exit(0);
                }
                wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
                adapter.RequestDevice(
                    nullptr,
                    [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
                        const char* message, void* userdata) {
                        UNUSED(status);
                        UNUSED(message);
                            device = wgpu::Device::Acquire(cDevice);
                            device.SetUncapturedErrorCallback(
                                [](WGPUErrorType type, const char* message, void* userdata) {
                                    UNUSED(userdata);
                                    std::cout << "Error: " << type << " - message: " << message;
                                },
                                nullptr);
                            reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
                    },
                    userdata);
        },
        reinterpret_cast<void*>(callback));
}


void SetupSwapChain(wgpu::Surface surface) {
    wgpu::SwapChainDescriptor scDesc{
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = wgpu::TextureFormat::BGRA8Unorm,
        .width = kWidth,
        .height = kHeight,
        .presentMode = wgpu::PresentMode::Fifo
    };
    swapChain = device.CreateSwapChain(surface, &scDesc);
}

const char shaderCode[] = R"(
    @vertex
    fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
        var p = vec2f(0.0, 0.0);
        if (in_vertex_index == 0u) {
            p = vec2f(-0.5, -0.5);
        } else if (in_vertex_index == 1u) {
            p = vec2f(0.5, -0.5);
        } else {
            p = vec2f(0.0, 0.5);
        }
        return vec4f(p, 0.0, 1.0);
    }

    @fragment
    fn fs_main() -> @location(0) vec4f {
        return vec4f(0.0, 0.4, 1.0, 1.0);
    }
)";

void CreateRenderPipeline() {
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &wgslDesc };
    wgpu::ShaderModule shaderModule =
        device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{
        .format = wgpu::TextureFormat::BGRA8Unorm };

    wgpu::FragmentState fragmentState{ .module = shaderModule,
                                      .targetCount = 1,
                                      .targets = &colorTargetState };

    wgpu::RenderPipelineDescriptor descriptor{
        .vertex = {.module = shaderModule},
        .fragment = &fragmentState };
    pipeline = device.CreateRenderPipeline(&descriptor);
}

void InitGraphics(wgpu::Surface surface) {
    SetupSwapChain(surface);
    CreateRenderPipeline();
}

void Render() {
    // Main Render Block.
    wgpu::RenderPassColorAttachment attachment{
    .view = swapChain.GetCurrentTextureView(),
    .loadOp = wgpu::LoadOp::Clear,
    .storeOp = wgpu::StoreOp::Store,
    .clearValue = wgpu::Color{ 0, 0, 0, 1.0 } };

    wgpu::RenderPassDescriptor renderpass{ .colorAttachmentCount = 1,
                                          .colorAttachments = &attachment };

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(pipeline);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}


void Start() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(kWidth, kHeight, "WebGPU", nullptr, nullptr);

    if (!window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return;
    }

    wgpu::Surface surface =
        wgpu::glfw::CreateSurfaceForWindow(instance, window);

    InitGraphics(surface);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        Render();
        swapChain.Present();
        instance.ProcessEvents();
    }
}


int main() {
    instance = wgpu::CreateInstance();
    GetDevice([](wgpu::Device dev) {
        device = dev;
        Start();
        });
}