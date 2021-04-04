#include "renderer.h"

#include <exception>
#include <stdexcept>
#include <vector>
#include <array>
#include <sstream>
#include <atomic>

#include "string_helpers.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "shader_helpers.h"

namespace shadey {

  std::string g_vertexShaders[RendererVertexType_Count] = {
R"(
#version 450

void main() {
  vec2 coord = vec2(
    float(gl_VertexIndex & 2),
    float(gl_VertexIndex & 1) * 2.0f);

  gl_Position = vec4(-1.0f + 2.0f * coord, 0.0f, 1.0f);
}
)",

R"(
#version 450

vec2 positions[3] = vec2[](
    vec2(-0.5, 0.5),
    vec2(0.5, 0.5),
    vec2(0.0, -0.5)
);

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
)"
};

  Renderer::Renderer() {
  }


  Renderer::~Renderer() {
    if (m_device != VK_NULL_HANDLE)
      vkDeviceWaitIdle(m_device);

    if (m_commandBuffer != VK_NULL_HANDLE)
      vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffer);

    if (m_commandPool != VK_NULL_HANDLE)
      vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    if (m_framebuffer != VK_NULL_HANDLE)
      vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);

    if (m_pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(m_device, m_pipeline, nullptr);

    if (m_renderpass != VK_NULL_HANDLE)
      vkDestroyRenderPass(m_device, m_renderpass, nullptr);

    if (m_layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(m_device, m_layout, nullptr);

    if (m_fragModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(m_device, m_fragModule, nullptr);

    if (m_vertModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(m_device, m_vertModule, nullptr);

    if (m_buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(m_device, m_buffer, nullptr);

    if (m_bufferMemory != VK_NULL_HANDLE)
      vkFreeMemory(m_device, m_bufferMemory, nullptr);

    if (m_imageView != VK_NULL_HANDLE)
      vkDestroyImageView(m_device, m_imageView, nullptr);

    if (m_image != VK_NULL_HANDLE)
      vkDestroyImage(m_device, m_image, nullptr);

    if (m_imageMemory != VK_NULL_HANDLE)
      vkFreeMemory(m_device, m_imageMemory, nullptr);

    if (m_device != VK_NULL_HANDLE)
      vkDestroyDevice(m_device, nullptr);

    if (m_instance != VK_NULL_HANDLE)
      vkDestroyInstance(m_instance, nullptr);
  }


  void Renderer::fixCode(bool hlsl, std::string& code) {
    if (!hlsl) {
      if (code.find("#version") == std::string::npos)
        code = "#version 330\n" + code;
    }

    if (code.find("include") != std::string::npos)
      throw std::runtime_error("Nuh uh uh!");
  }


  RendererOptions Renderer::getRendererOptions(const std::string& code) {
    RendererOptions options = {
      .clearColor = { 0.0f, 0.0f, 0.0f, 1.0f },
      .vertexType = RendererVertexType_Quad,
      .resolution = { 512, 512 }
    };

    std::istringstream iss(code);

    for (std::string line; std::getline(iss, line);) {
      if (line.starts_with("// SHADEY") || line.starts_with("//SHADEY")) {
        size_t colon  = line.find(":");
        size_t equals = line.find("=");

        if (colon == std::string::npos || equals == std::string::npos)
          continue;

        if (equals < colon)
          continue;

        if (line.length() == equals)
          continue;

        std::string param = line.substr(colon + 1, equals - colon - 1);
        trim(param);

        std::string value = line.substr(equals + 1);
        trim(value);

        if (param == "clearColor") {
          sscanf(value.c_str(), "%f %f %f %f",
            &options.clearColor.color.float32[0],
            &options.clearColor.color.float32[1],
            &options.clearColor.color.float32[2],
            &options.clearColor.color.float32[3]);
        }

        if (param == "type") {
          if (value.starts_with("tri"))
            options.vertexType = RendererVertexType_Triangle;
        }

        if (param == "resolution") {
          sscanf(value.c_str(), "%u %u",
            &options.resolution[0],
            &options.resolution[1]);

          if (options.resolution[0] == 0 || options.resolution[1] == 0)
            throw std::runtime_error("Can't have a resolution with an extent that is 0");

          if (options.resolution[0] > 16384 || options.resolution[1] > 16384)
            throw std::runtime_error("Can't have a resolution with an extent greater than 16384");

          if (options.resolution[0] * options.resolution[1] > 4096 * 2048)
            throw std::runtime_error("Can't have a resolution with an area greater than 4096 * 2048");
        }
      }
    }

    return options;
  }


  std::string Renderer::init(bool hlsl, std::string glslFrag) {
    fixCode(hlsl, glslFrag);

    auto options = getRendererOptions(glslFrag);

    // Create instance
    {
      VkApplicationInfo appInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Shadey",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "Shadey",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_2
      };

      VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo
      };

      if (vkCreateInstance(&instanceInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    // Get physical device we want
    {
      uint32_t deviceCount = 0;
      vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

      if (deviceCount == 0)
        throw std::runtime_error("Failed to find any Vulkan capable GPUs");

      std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
      vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data());

      // Pick the first one.
      m_physDevice = physicalDevices[0];
    }

    // Pick our queue family
    {
      uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &queueFamilyCount, nullptr);

      std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &queueFamilyCount, queueFamilies.data());

      for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          m_graphicsFamily = i;
          break;
        }
      }

      if (m_graphicsFamily == UINT32_MAX)
        throw std::runtime_error("No graphics queue available");
    }

    // Create our logical device
    {
      const float queuePriority = 1.0f;

      VkDeviceQueueCreateInfo queueInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_graphicsFamily,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
      };

      VkPhysicalDeviceFeatures deviceFeatures = { };

      VkDeviceCreateInfo deviceInfo = {
        .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos    = &queueInfo,
        .pEnabledFeatures     = &deviceFeatures
      };

      if (vkCreateDevice(m_physDevice, &deviceInfo, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan device");

      vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_queue);
    }

    // Create image and buffer
    {
      VkPhysicalDeviceMemoryProperties memProperties;
      vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProperties);

      uint32_t hostIndex = UINT32_MAX;
      for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (memProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
          hostIndex = i;
          break;
        }
      }

      uint32_t deviceIndex = UINT32_MAX;
      for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (memProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
          deviceIndex = i;
          break;
        }
      }

      VkImageCreateInfo imageInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extent        = { options.resolution[0], options.resolution[1], 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
      };

      if (vkCreateImage(m_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image");

      {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, m_image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = deviceIndex;

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
          throw std::runtime_error("Failed to allocate image memory");

        if (vkBindImageMemory(m_device, m_image, m_imageMemory, 0) != VK_SUCCESS)
          throw std::runtime_error("Failed to bind image memory");
      }

      VkImageViewCreateInfo imageViewInfo = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel   = 0,
          .levelCount     = 1,
          .baseArrayLayer = 0,
          .layerCount     = 1
        }
      };

      if (vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view");

      VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = 4 * sizeof(float) * options.resolution[0] * options.resolution[1],
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
      };

      if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

      {
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, m_buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = hostIndex;

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_bufferMemory) != VK_SUCCESS)
          throw std::runtime_error("Failed to allocate buffer memory");
      }

      if (vkBindBufferMemory(m_device, m_buffer, m_bufferMemory, 0) != VK_SUCCESS)
        throw std::runtime_error("Failed to bind buffer memory");

      if (vkMapMemory(m_device, m_bufferMemory, 0, VK_WHOLE_SIZE, 0, &m_bufferMemPtr) != VK_SUCCESS)
        throw std::runtime_error("Failed to map buffer memory");
    }

    // Create shaders
    {
      auto CreateModule = [&](bool hlsl, bool fragment, const std::string& glsl) {
        auto spv = compileShader(hlsl, fragment, glsl);

        VkShaderModuleCreateInfo moduleInfo = {
          .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          .codeSize = spv.size(),
          .pCode    = reinterpret_cast<const uint32_t*>(spv.data())
        };

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(m_device, &moduleInfo, nullptr, &shaderModule))
          throw std::runtime_error("Failed to create shader module");

        return shaderModule;
      };

      m_vertModule = CreateModule(false, false, g_vertexShaders[options.vertexType]);
      m_fragModule = CreateModule(hlsl, true, glslFrag);
    }

    // Create pipeline layout
    {
      VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
      };

      if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Create renderpass
    {
      VkAttachmentDescription colorAttachment = {
        .format         = VK_FORMAT_R8G8B8A8_UNORM,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      };

      VkAttachmentReference reference = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      };

      VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &reference
      };

      VkRenderPassCreateInfo renderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass
      };

      if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderpass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
    }

    // Create pipeline
    {
      VkPipelineShaderStageCreateInfo stages[2] = {
        {
          .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_VERTEX_BIT,
          .module = m_vertModule,
          .pName = "main"
        },
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = m_fragModule,
          .pName = "main"
        },
      };

      VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
      };

      VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
      };

      VkViewport viewport = {
        .x       = 0.0f,
        .y       = 0.0f,
        .width   = float(options.resolution[0]),
        .height  = float(options.resolution[1]),
        .minDepth = 0.0f,
        .maxDepth = 0.0f,
      };

      VkRect2D scissor = {
        .extent = { options.resolution[0], options.resolution[1] }
      };

      VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor
      };

      VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .cullMode    = VK_CULL_MODE_BACK_BIT,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.0f
      };

      VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading     = 1.0f
      };

      VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
      };

      VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment
      };

      VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = nullptr,
        .layout              = m_layout,
        .renderPass          = m_renderpass,
      };

      if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Create framebuffer
    {
      VkFramebufferCreateInfo framebufferInfo = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = m_renderpass,
        .attachmentCount = 1,
        .pAttachments    = &m_imageView,
        .width           = options.resolution[0],
        .height          = options.resolution[1],
        .layers          = 1
      };

      if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create framebuffer");
    }

    // Create command pool
    {
      VkCommandPoolCreateInfo commandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = m_graphicsFamily
      };

      if (vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
    }

    // Create command buffers
    {
      VkCommandBufferAllocateInfo commandBufferInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = m_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
      };

      if (vkAllocateCommandBuffers(m_device, &commandBufferInfo, &m_commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");

      VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
      };

      if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer");

      VkRenderPassBeginInfo renderPassInfo = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = m_renderpass,
        .framebuffer = m_framebuffer,
        .renderArea = {
          .offset = { 0, 0 },
          .extent = { options.resolution[0], options.resolution[1] }
        },
        .clearValueCount = 1,
        .pClearValues    = &options.clearColor
      };

      VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_image,
        .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1
        }
      };

      vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
      vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
      vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);
      vkCmdEndRenderPass(m_commandBuffer);

      VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,

        .imageSubresource = {
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel       = 0,
          .baseArrayLayer = 0,
          .layerCount     = 1,
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { options.resolution[0], options.resolution[1], 1 }
      };
      vkCmdCopyImageToBuffer(m_commandBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer, 1, &region);

      if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer");
    }
    
    // Submit
    {


      VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &m_commandBuffer,
      };

      if (vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit queue");

      vkQueueWaitIdle(m_queue);

      static std::atomic<uint32_t> index = 0;

      std::string name = "temp_" + std::to_string(index.fetch_add(1)) + std::string(".png");

      stbi_write_png(name.c_str(), options.resolution[0], options.resolution[1], 4, m_bufferMemPtr, 4 * options.resolution[0]);

      return name;
    }
  }

}