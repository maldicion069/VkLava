/**
* Copyright (c) 2017, Lava
* All rights reserved.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**/

#ifndef __LAVARENDERER_DEFAULTFRAMEBUFFER__
#define __LAVARENDERER_DEFAULTFRAMEBUFFER__

#include <lava/lava.h>
#include <lavaRenderer/api.h>

#include <stdint.h> // UINT64_MAX
#include <memory>

namespace lava
{
  namespace glfw
  {
    class DefaultFramebuffer
    {
    public:
      LAVARENDERER_API
        DefaultFramebuffer( const std::shared_ptr<Device>& device,
          const std::shared_ptr<Surface>& surface,
          vk::Format surfaceFormat, vk::ColorSpaceKHR colorSpace,
          vk::Format depthFormat, const std::shared_ptr<RenderPass>& renderPass );

      LAVARENDERER_API
        ~DefaultFramebuffer( void )
      {
      }

      LAVARENDERER_API
        void rebuild( const std::shared_ptr<Device>& device,
          const std::shared_ptr<Surface>& surface,
          vk::Format surfaceFormat, vk::Format depthFormat,
          const std::shared_ptr<RenderPass>& renderPass );

      LAVARENDERER_API
        const vk::Extent2D& getExtent( void ) const { return _extent; }
      LAVARENDERER_API
        const std::shared_ptr<Framebuffer>& getFramebuffer( void ) const
      {
        return _framebuffers[ _swapchainIndex ];
      }
      LAVARENDERER_API // TODO: Remove API
        void acquireNextFrame( uint64_t timeout = UINT64_MAX,
          const std::shared_ptr<Fence>& fence = nullptr )
      {
        _swapchainIndex = _swapchain->acquireNextImage( timeout, fence );
      }

      const std::shared_ptr<Semaphore>& getPresentSemaphore( void ) const
      {
        return _swapchain->getPresentCompleteSemaphores( )[ _swapchainIndex ];
      }

      const std::shared_ptr<Image> getLastImage( void )
      {
        return _colorImages[ _swapchainIndex ];
      }

      uint32_t index( void ) const
      {
        return _swapchainIndex;
      }

      LAVARENDERER_API // TODO: Remove API
        void present( const std::shared_ptr<Queue>& queue,
          vk::ArrayProxy<const std::shared_ptr<Semaphore>> waitSemaphores = { } )
      {
        auto results = queue->present( waitSemaphores, _swapchain, _swapchainIndex );
        vk::Result res = results.at( 0 );
        if ( res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR )
        {
          // TODO: RESIZE
        }
        //auto str = lava::utils::translateVulkanResult( results[ 0 ] );
        //std::cout << str << std::endl;
      }
      LAVARENDERER_API
        int imagesCount( void ) const
      {
        return _framebuffers.size( );
      }

      LAVARENDERER_API
        bool supportsGrab( void ) const
      {
        return _swapChainSupportsReadBack;
      }
    private:
      void clear( vk::SwapchainKHR swapChain );

      vk::Extent2D _extent;

      bool _swapChainSupportsReadBack;

    public:
      std::shared_ptr<Swapchain> _swapchain;
    protected:
      uint32_t _swapchainIndex;
      std::vector<std::shared_ptr<Image>> _colorImages;
      std::vector<std::shared_ptr<ImageView>> _colorViews;
      std::shared_ptr<Image> _depthImage;
      std::shared_ptr<ImageView> _depthView;
      std::vector<std::shared_ptr<Framebuffer>> _framebuffers;
    };
  }
}

#endif /* __LAVARENDERER_DEFAULTFRAMEBUFFER__ */