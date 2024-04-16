#include <../third_party/glfw/include/GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>
#include <webgpu/webgpu.h>
#include <iostream>

#define UNUSED(x) (void)(x)

const uint32_t kWidth = 640;
const uint32_t kHeight = 480;

wgpu::SwapChain swapChain;
wgpu::RenderPipeline pipeline;
wgpu::Instance instance;
wgpu::Device device;
wgpu::Buffer buffer1;
wgpu::Buffer buffer2;
wgpu::Buffer vertexBuffer;
wgpu::Buffer indexBuffer;
wgpu::BufferDescriptor bufferDesc;
wgpu::VertexBufferLayout vertexBufferLayout;
std::vector<wgpu::VertexAttribute> vertexAttribs(2);

std::vector<float> vertexData = {
    // x,   y,     r,   g,   b
    -0.5, -0.5,   1.0, 0.0, 0.0,
    +0.5, -0.5,   0.0, 1.0, 0.0,
    +0.5, +0.5,   0.0, 0.0, 1.0,
    -0.5, +0.5,   1.0, 1.0, 0.0
};

// This is a list of indices referencing positions in the pointData
std::vector<uint16_t> indexData = {
    0, 1, 2, // Triangle #0
    0, 2, 3  // Triangle #1
};

int indexCount = static_cast<int>(indexData.size());

// We now divide the vector size by 5 fields.
int vertexCount = static_cast<int>(vertexData.size() / 5);

void setDefault(wgpu::Limits &limits) {
    limits.maxTextureDimension1D = 0;
    limits.maxTextureDimension2D = 0;
    limits.maxTextureDimension3D = 0;
    // [...] Set everything to 0 to mean "no limit"
}

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

            wgpu::DeviceDescriptor deviceDesc;
            wgpu::SupportedLimits supportedLimits;
            // Don't forget to = Default
            wgpu::RequiredLimits requiredLimits{};
            setDefault(requiredLimits.limits);
            // We use at most 1 vertex attribute for now
            requiredLimits.limits.maxVertexAttributes = 2;
            // We should also tell that we use 1 vertex buffers
            requiredLimits.limits.maxVertexBuffers = 1;
            // Maximum size of a buffer is 6 vertices of 2 float each
            requiredLimits.limits.maxBufferSize = 6 * 5 * sizeof(float);
            // Maximum stride between 2 consecutive vertices in the vertex buffer
            requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);
            // This must be set even if we do not use storage buffers for now
            requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
            // This must be set even if we do not use uniform buffers for now
            requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
            deviceDesc.requiredLimits = &requiredLimits;

                adapter.RequestDevice(
                        &deviceDesc,
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
    struct VertexInput {
        @location(0) position: vec2f,
        @location(1) color: vec3f,
    };

    /**
     * A structure with fields labeled with builtins and locations can also be used
     * as *output* of the vertex shader, which is also the input of the fragment
     * shader.
     */
    struct VertexOutput {
        @builtin(position) position: vec4f,
        // The location here does not refer to a vertex attribute, it just means
        // that this field must be handled by the rasterizer.
        // (It can also refer to another field of another struct that would be used
        // as input to the fragment shader.)
        @location(0) color: vec3f,
    };

    @vertex
    fn vs_main(in: VertexInput) -> VertexOutput {
        var out: VertexOutput;
        let ratio = 640.0 / 480.0; // The width and height of the target surface
        out.position = vec4f(in.position.x, in.position.y * ratio, 0.0, 1.0);
        out.color = in.color; // forward to the fragment shader
        return out;
    }

    @fragment
    fn fs_main(in: VertexOutput) -> @location(0) vec4f {
        return vec4f(in.color, 1.0);
    }
)";

void CreateRenderPipeline() {

    // Position attribute
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = wgpu::VertexFormat::Float32x2;
    vertexAttribs[0].offset = 0;

    // Color attribute
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = wgpu::VertexFormat::Float32x3; // different type!
    vertexAttribs[1].offset = 2 * sizeof(float); // non null offset!

    vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
    vertexBufferLayout.attributes = vertexAttribs.data();

    vertexBufferLayout.arrayStride = 5 * sizeof(float);
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

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
        .vertex = {.module = shaderModule ,
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout
                    },
        .primitive = {.topology = wgpu::PrimitiveTopology::TriangleList,
                        .frontFace = wgpu::FrontFace::CCW,
                        .cullMode = wgpu::CullMode::None
                    },
        .fragment = &fragmentState,
        };
    pipeline = device.CreateRenderPipeline(&descriptor);
}

void InitGraphics(wgpu::Surface surface) {
    SetupSwapChain(surface);
    CreateRenderPipeline();
}

void GetBuffer() {
//    wgpu::BufferDescriptor bufferDesc;
//    bufferDesc.label = "Some GPU-side data buffer";
//    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
//    bufferDesc.size = 16;
//    bufferDesc.mappedAtCreation = false;
//    buffer1 = device.CreateBuffer(&bufferDesc);

    wgpu::BufferDescriptor bufferDesc2;
    bufferDesc2.label = "Some GPU-side data buffer 2";
    bufferDesc2.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    bufferDesc2.size = 16;
    bufferDesc2.mappedAtCreation = false;
    buffer2 = device.CreateBuffer(&bufferDesc2);

    // Create vertex buffer
    bufferDesc.size = vertexData.size() * sizeof(float);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    vertexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    bufferDesc.size = indexData.size() * sizeof(uint16_t);
    //bufferDesc.size = (bufferDesc.size + 3) & ~3;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
    bufferDesc.mappedAtCreation = false;
    indexBuffer = device.CreateBuffer(&bufferDesc);
    device.GetQueue().WriteBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);

}

void Render() {
    // Main Render Block.

    GetBuffer();

    // Create some CPU-side data buffer (of size 16 bytes)
    std::vector<uint8_t> numbers(16);
    for (uint8_t i = 0; i < 16; ++i) numbers[i] = i;

    // Copy this from `numbers` (RAM) to `buffer1` (VRAM)
    //device.GetQueue().WriteBuffer(buffer1, 0, numbers.data(), numbers.size());
  


    //auto onBuffer2Mapped = [](WGPUBufferMapAsyncStatus status, void* pUserData) {
    //    auto* buffer = static_cast<wgpu::Buffer*>(pUserData);
    //    // std::cout << "Buffer 2 mapped with status " << status << std::endl;
    //    if (static_cast<wgpu::BufferMapAsyncStatus>(status) != wgpu::BufferMapAsyncStatus::Success) return;

    //    // Get a pointer to wherever the driver mapped the GPU memory to the RAM
    //    // uint8_t* bufferData = (uint8_t*)buffer->GetConstMappedRange(0, 16);

    //    // [...] (Do stuff with bufferData)

    //    // Then do not forget to unmap the memory
    //    buffer->Unmap();
    //};

    //buffer2.MapAsync(wgpu::MapMode::Read, 0, 16, onBuffer2Mapped, &buffer2);

    wgpu::RenderPassColorAttachment attachment{
    .view = swapChain.GetCurrentTextureView(),
    .loadOp = wgpu::LoadOp::Clear,
    .storeOp = wgpu::StoreOp::Store,
    .clearValue = wgpu::Color{ 0, 0, 0, 1.0 } };

    wgpu::RenderPassDescriptor renderpass{ .colorAttachmentCount = 1,
                                          .colorAttachments = &attachment };

    wgpu::CommandEncoderDescriptor commandEncoderDesc;
    commandEncoderDesc.label = "Command Encoder";
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&commandEncoderDesc);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(pipeline);
    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexData.size() * sizeof(float));
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexData.size() * sizeof(uint16_t));
    /*pass.Draw(vertexCount, 1, 0, 0);*/
    pass.DrawIndexed(indexCount, 1, 0, 0, 0);
    pass.End();
    wgpu::CommandBufferDescriptor cmdBufferDescriptor;
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer commands = encoder.Finish(&cmdBufferDescriptor);
    device.GetQueue().Submit(1,&commands);
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
        device.Tick();
        device.Tick();
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

    vertexBuffer.Destroy();
}