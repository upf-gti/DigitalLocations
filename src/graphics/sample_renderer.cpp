#include "sample_renderer.h"

#ifdef XR_SUPPORT
#include "dawnxr/dawnxr_internal.h"
#endif

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

#include "spdlog/spdlog.h"

SampleRenderer::SampleRenderer() : Renderer()
{

}

int SampleRenderer::initialize(GLFWwindow* window, bool use_mirror_screen)
{
    Renderer::initialize(window, use_mirror_screen);

    clear_color = glm::vec4(0.22f, 0.22f, 0.22f, 1.0);

    init_camera_bind_group();

    orbit_camera = new OrbitCamera();
    orbit_camera->set_perspective(glm::radians(45.0f), webgpu_context.render_width / static_cast<float>(webgpu_context.render_height), z_near, z_far);
    orbit_camera->look_at(glm::vec3(0.0f, 0.1f, 0.4f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    orbit_camera->set_mouse_sensitivity(0.004f);
    orbit_camera->set_speed(0.75f);

    flyover_camera = new FlyoverCamera();
    flyover_camera->set_perspective(glm::radians(45.0f), webgpu_context.render_width / static_cast<float>(webgpu_context.render_height), z_near, z_far);
    flyover_camera->look_at(glm::vec3(0.0f, 0.1f, 0.4f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    flyover_camera->set_mouse_sensitivity(0.004f);
    flyover_camera->set_speed(0.75f);

    camera = flyover_camera;

    std::vector<Uniform*> uniforms = { &camera_uniform };
    render_bind_group_camera = webgpu_context.create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/mesh_pbr.wgsl"), 1);

    return 0;
}

void SampleRenderer::clean()
{
    Renderer::clean();

    delete orbit_camera;
    delete flyover_camera;
}

void SampleRenderer::update(float delta_time)
{
    if (const auto& io = ImGui::GetIO(); !io.WantCaptureMouse && !io.WantCaptureKeyboard) {
        camera->update(delta_time);
    }
}

void SampleRenderer::render()
{
    prepare_instancing();

    render_screen();

    clear_renderables();
}

void SampleRenderer::render_opaque(WGPURenderPassEncoder render_pass)
{
    Renderer::instance->render_opaque(render_pass, render_bind_group_camera);
}

void SampleRenderer::render_transparent(WGPURenderPassEncoder render_pass)
{
    Renderer::instance->render_transparent(render_pass, render_bind_group_camera);
}

void SampleRenderer::render_screen()
{
    camera_data.eye = camera->get_eye();
    camera_data.mvp = camera->get_view_projection();
    camera_data.dummy = 0.f;

    wgpuQueueWriteBuffer(webgpu_context.device_queue, std::get<WGPUBuffer>(camera_uniform.data), 0, &(camera_data), sizeof(sCameraData));

    WGPUTextureView swapchain_view = wgpuSwapChainGetCurrentTextureView(webgpu_context.screen_swapchain);

    ImGui::Render();

    {
        // Create the command encoder
        WGPUCommandEncoderDescriptor encoder_desc = {};
        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(webgpu_context.device, &encoder_desc);

        // Prepare the color attachment
        WGPURenderPassColorAttachment render_pass_color_attachment = {};
        render_pass_color_attachment.view = swapchain_view;
        render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
        render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        glm::vec4 clear_color = SampleRenderer::instance->get_clear_color();
        render_pass_color_attachment.clearValue = WGPUColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);

        // Prepate the depth attachment
        WGPURenderPassDepthStencilAttachment render_pass_depth_attachment = {};
        render_pass_depth_attachment.view = eye_depth_texture_view[EYE_LEFT];
        render_pass_depth_attachment.depthClearValue = 1.0f;
        render_pass_depth_attachment.depthLoadOp = WGPULoadOp_Clear;
        render_pass_depth_attachment.depthStoreOp = WGPUStoreOp_Store;
        render_pass_depth_attachment.depthReadOnly = false;
        render_pass_depth_attachment.stencilClearValue = 0; // Stencil config necesary, even if unused
        render_pass_depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
        render_pass_depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
        render_pass_depth_attachment.stencilReadOnly = true;

        WGPURenderPassDescriptor render_pass_descr = {};
        render_pass_descr.colorAttachmentCount = 1;
        render_pass_descr.colorAttachments = &render_pass_color_attachment;
        render_pass_descr.depthStencilAttachment = &render_pass_depth_attachment;

        // Create & fill the render pass (encoder)
        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(command_encoder, &render_pass_descr);

        render_opaque(render_pass);

        render_transparent(render_pass);

        wgpuRenderPassEncoderEnd(render_pass);

        wgpuRenderPassEncoderRelease(render_pass);

        // render imgui
        {
            WGPURenderPassColorAttachment color_attachments = {};
            color_attachments.loadOp = WGPULoadOp_Load;
            color_attachments.storeOp = WGPUStoreOp_Store;
            color_attachments.clearValue = { 0.0, 0.0, 0.0, 0.0 };
            color_attachments.view = swapchain_view;
            color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

            WGPURenderPassDescriptor render_pass_desc = {};
            render_pass_desc.colorAttachmentCount = 1;
            render_pass_desc.colorAttachments = &color_attachments;
            render_pass_desc.depthStencilAttachment = nullptr;

            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(command_encoder, &render_pass_desc);

            ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);

            wgpuRenderPassEncoderEnd(pass);
            wgpuRenderPassEncoderRelease(pass);
        }

        WGPUCommandBufferDescriptor cmd_buff_descriptor = {};
        cmd_buff_descriptor.nextInChain = NULL;
        cmd_buff_descriptor.label = "Command buffer";

        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(command_encoder, &cmd_buff_descriptor);

        wgpuQueueSubmit(webgpu_context.device_queue, 1, &commands);

        wgpuCommandBufferRelease(commands);
        wgpuCommandEncoderRelease(command_encoder);
    }

    wgpuTextureViewRelease(swapchain_view);

#ifndef __EMSCRIPTEN__
    wgpuSwapChainPresent(webgpu_context.screen_swapchain);
#endif
}

void SampleRenderer::init_camera_bind_group()
{
    camera_uniform.data = webgpu_context.create_buffer(sizeof(sCameraData), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, nullptr, "camera_buffer");
    camera_uniform.binding = 0;
    camera_uniform.buffer_size = sizeof(sCameraData);
}

void SampleRenderer::resize_window(int width, int height)
{
    Renderer::resize_window(width, height);
}

void SampleRenderer::set_camera_type(int camera_type)
{
    if (camera_type == CAMERA_FLYOVER) {
        flyover_camera->look_at(orbit_camera->get_eye(), orbit_camera->get_center(), orbit_camera->get_up());
        camera = flyover_camera;
    }
    else if (camera_type == CAMERA_ORBIT) {
        orbit_camera->look_at(flyover_camera->get_eye(), flyover_camera->get_center(), flyover_camera->get_up());
        camera = orbit_camera;
    }
}
