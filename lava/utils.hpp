#ifndef __LAVA_UTILS__
#define __LAVA_UTILS__

#include "includes.hpp"

#include "CommandBuffer.h"

#include "PhysicalDevice.h"
#include "Device.h"

namespace lava
{
	class utils
	{
  public:
    static VkBool32 getSupportedDepthFormat( std::shared_ptr<PhysicalDevice> physicalDevice, 
      vk::Format& depthFormat )
    {
      // Since all depth formats may be optional, we need to find a suitable depth format to use
      // Start with the highest precision packed format
      std::vector<vk::Format> depthFormats =
      {
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD32Sfloat,
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD16UnormS8Uint,
        vk::Format::eD16Unorm
      };

      for ( auto& format : depthFormats )
      {
        vk::FormatProperties formatProps = physicalDevice->getFormatProperties( format );
        // Format must support depth stencil attachment for optimal tiling
        if ( formatProps.optimalTilingFeatures & 
          vk::FormatFeatureFlagBits::eDepthStencilAttachment )
        {
          depthFormat = format;
          return true;
        }
      }

      return false;
    }

    LAVA_API
    static void saveToImage( const std::string & filename, vk::Format colorFormat, 
      std::shared_ptr<Device> dev, std::shared_ptr<Image> currentImage, 
      uint32_t width, uint32_t height, std::shared_ptr<CommandPool> cmdPool,
      std::shared_ptr<Queue> queue );

    static unsigned char* loadImageTexture( const std::string& fileName,
      uint32_t& width, uint32_t& height, uint32_t& numChannels );
    static std::vector<char> readBinaryile( const std::string& fileName );
		static const std::string translateVulkanResult( vk::Result res );

    // Put an image memory barrier for setting an image layout on the 
    //    sub resource into the given command buffer
    LAVA_API
    static void setImageLayout( const std::shared_ptr<CommandBuffer>& cmd,
      vk::Image image,
      vk::ImageLayout oldImageLayout,
      vk::ImageLayout newImageLayout,
      vk::ImageSubresourceRange subresourceRange,
      vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
      vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands );
    // Uses a fixed sub resource layout with first mip level and layer
    LAVA_API
    static void setImageLayout( const std::shared_ptr<CommandBuffer>& cmd,
      vk::Image image,
      vk::ImageAspectFlags aspectMask,
      vk::ImageLayout oldImageLayout,
      vk::ImageLayout newImageLayout,
      vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
      vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands );
	};
}

#endif /* __LAVA_UTILS__ */