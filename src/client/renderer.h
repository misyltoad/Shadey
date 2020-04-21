#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>

namespace shadey {

  enum RendererVertexType {
    RendererVertexType_Quad,
    RendererVertexType_Triangle,
    RendererVertexType_Count,
  };

  struct RendererOptions {
    VkClearValue clearColor;
    RendererVertexType vertexType;
    uint32_t resolution[2];
  };

  class Renderer {

  public:

    Renderer();

    ~Renderer();

    std::string init(bool hlsl, std::string glslFrag);

    static void fixCode(bool hlsl, std::string& code);

    static RendererOptions getRendererOptions(const std::string& code);

  private:

    VkInstance       m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDevice     = VK_NULL_HANDLE;
    uint32_t         m_graphicsFamily = UINT32_MAX;
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkQueue          m_queue          = VK_NULL_HANDLE;
    VkImage          m_image          = VK_NULL_HANDLE;
    VkDeviceMemory   m_imageMemory    = VK_NULL_HANDLE;
    VkImageView      m_imageView      = VK_NULL_HANDLE;
    VkBuffer         m_buffer         = VK_NULL_HANDLE;
    VkDeviceMemory   m_bufferMemory   = VK_NULL_HANDLE;
    void*            m_bufferMemPtr   = nullptr;
    VkShaderModule   m_vertModule     = VK_NULL_HANDLE;
    VkShaderModule   m_fragModule     = VK_NULL_HANDLE;
    VkPipelineLayout m_layout         = VK_NULL_HANDLE;
    VkRenderPass     m_renderpass     = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
    VkFramebuffer    m_framebuffer    = VK_NULL_HANDLE;
    VkCommandPool    m_commandPool    = VK_NULL_HANDLE;
    VkCommandBuffer  m_commandBuffer  = VK_NULL_HANDLE;
  };

}