/**
 * Copyright (c) 2017 - 2018, Lava
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

#include "CommandBuffer.h"

#include <lava/Buffer.h>
#include <lava/Image.h>
#include <lava/Device.h>
#include <lava/Event.h>
#include <lava/Framebuffer.h>
#include <lava/Pipeline.h>
#include <lava/PhysicalDevice.h>
#include <lava/RenderPass.h>
#include <lava/QueryPool.h>

namespace lava
{
  ImageMemoryBarrier::ImageMemoryBarrier(
    vk::AccessFlags srcAccessMask_, vk::AccessFlags dstAccessMask_,
    vk::ImageLayout oldLayout_, vk::ImageLayout newLayout_,
    uint32_t srcQueueFamilyIndex_, uint32_t dstQueueFamilyIndex_,
    const std::shared_ptr<Image>& image_,
    const vk::ImageSubresourceRange& subresourceRange_ )
    : srcAccessMask( srcAccessMask_ )
    , dstAccessMask( dstAccessMask_ )
    , oldLayout( oldLayout_ )
    , newLayout( newLayout_ )
    , srcQueueFamilyIndex( srcQueueFamilyIndex_ )
    , dstQueueFamilyIndex( dstQueueFamilyIndex_ )
    , image( image_ )
    , subresourceRange( subresourceRange_ )
  {
  }

  ImageMemoryBarrier::ImageMemoryBarrier( ImageMemoryBarrier const& rhs )
    : ImageMemoryBarrier( rhs.srcAccessMask, rhs.dstAccessMask,
      rhs.oldLayout, rhs.newLayout,
      rhs.srcQueueFamilyIndex, rhs.dstQueueFamilyIndex,
      rhs.image, rhs.subresourceRange )
  {
  }

  ImageMemoryBarrier & ImageMemoryBarrier::operator=(
    ImageMemoryBarrier const& rhs )
  {
    srcAccessMask = rhs.srcAccessMask;
    dstAccessMask = rhs.dstAccessMask;
    oldLayout = rhs.oldLayout;
    newLayout = rhs.newLayout;
    srcQueueFamilyIndex = rhs.srcQueueFamilyIndex;
    dstQueueFamilyIndex = rhs.dstQueueFamilyIndex;
    image = rhs.image;
    subresourceRange = rhs.subresourceRange;

    return *this;
  }
  CommandPool::CommandPool( const std::shared_ptr<Device>& device,
    vk::CommandPoolCreateFlags flags, uint32_t familyIndex )
    : VulkanResource( device )
    , _familyIndex( familyIndex )
  {
    vk::CommandPoolCreateInfo cci( flags, familyIndex );
    _commandPool = static_cast< vk::Device >( *_device )
      .createCommandPool( cci );
  }
  CommandPool::~CommandPool( void )
  {
    static_cast< vk::Device >( *_device ).destroyCommandPool( _commandPool );
  }
  std::shared_ptr<CommandBuffer> CommandPool::allocateCommandBuffer(
    vk::CommandBufferLevel level )
  {
    std::shared_ptr<CommandBuffer> commandBuffer =
      std::make_shared<CommandBuffer>( shared_from_this( ), level );
    _commandBuffers.push_back( commandBuffer.get( ) );
    return( commandBuffer );
  }

  bool CommandPool::supportsCompute( void ) const
  {
    return !!( _device->getPhysicalDevice( )
      ->getQueueFamilyProperties( )[ _familyIndex ]
      .queueFlags & vk::QueueFlagBits::eCompute );
  }

  bool CommandPool::supportsGraphics( void ) const
  {
    return !!( _device->getPhysicalDevice( )->
      getQueueFamilyProperties( )[ _familyIndex ]
      .queueFlags & vk::QueueFlagBits::eGraphics );
  }

  bool CommandPool::supportsTransfer( void ) const
  {
    return !!( _device->getPhysicalDevice( )->
      getQueueFamilyProperties( )[ _familyIndex ]
      .queueFlags & vk::QueueFlagBits::eTransfer );
  }
  CommandBuffer::CommandBuffer( const std::shared_ptr<CommandPool>& cmdPool,
    vk::CommandBufferLevel level )
    : _commandPool( cmdPool )
    , _level( level )
    , _state( State::Ready )
  {
    vk::CommandBufferAllocateInfo info( *_commandPool, level, 1 );
    std::vector<vk::CommandBuffer> commandBuffers =
      static_cast< vk::Device >( *_commandPool->getDevice( ) )
      .allocateCommandBuffers( info );
    assert( !commandBuffers.empty( ) );
    _commandBuffer = commandBuffers[ 0 ];
  }

  CommandBuffer::~CommandBuffer( void )
  {
    if ( _state == State::Submitted )
    {
      // TODO: Wait for finish
    }
    else if ( _state != State::Ready )
    {

    }
    static_cast<vk::Device>( *_commandPool->getDevice( ) )
      .freeCommandBuffers( *_commandPool, _commandBuffer );
  }

  /*void CommandBuffer::begin( vk::CommandBufferUsageFlags flags, 
    vk::CommandBufferInheritanceInfo * inheritInfo )
  {
    assert( !_isRecording );
    _renderPass = inheritInfo->renderPass;
    _framebuffer = inheritInfo->framebuffer;
    vk::CommandBufferBeginInfo cbbi( flags, inheritInfo );

    _commandBuffer.begin( cbbi );
    _isRecording = true;
  }*/

  void CommandBuffer::begin( vk::CommandBufferUsageFlags flags,
    const std::shared_ptr<RenderPass>& renderPass, uint32_t subpass,
    const std::shared_ptr<Framebuffer>& framebuffer,
    vk::Bool32 occlusionQueryEnable, vk::QueryControlFlags queryFlags,
    vk::QueryPipelineStatisticFlags pipelineStatistics )
  {
    assert( _state == State::Ready );

    _renderPass = renderPass;
    _framebuffer = framebuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo;
    vk::CommandBufferBeginInfo beginInfo( flags, &inheritanceInfo );

    inheritanceInfo.renderPass = renderPass ?
      *renderPass : vk::RenderPass( );
    inheritanceInfo.subpass = subpass;
    inheritanceInfo.framebuffer = framebuffer ?
      *framebuffer : vk::Framebuffer( );
    inheritanceInfo.occlusionQueryEnable = occlusionQueryEnable;
    inheritanceInfo.queryFlags = queryFlags;
    inheritanceInfo.pipelineStatistics = pipelineStatistics;

    _commandBuffer.begin( beginInfo );

    _state = State::Recording;
  }


  void CommandBuffer::end( void )
  {
    assert( _state == State::Recording );

    _commandBuffer.end( );

    _state = State::RecordingDone;
  }

  void CommandBuffer::resetEvent( const std::shared_ptr<Event>& ev, 
    vk::PipelineStageFlags stageMask )
  {
    _commandBuffer.resetEvent( *ev, stageMask );
  }

  void CommandBuffer::setEvent( const std::shared_ptr<Event>& ev, 
    vk::PipelineStageFlags stageMask )
  {
    _commandBuffer.setEvent( *ev, stageMask );
  }

  void CommandBuffer::waitEvents(
    vk::ArrayProxy<const std::shared_ptr<Event>> events,
    vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
    vk::ArrayProxy<const vk::MemoryBarrier> memoryBarriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier> bufferMemoryBarriers,
    vk::ArrayProxy<const vk::ImageMemoryBarrier> imageMemoryBarriers
  )
  {
    std::vector<vk::Event> evts;
    for ( const auto& e : events )
    {
      evts.push_back( *e );
    }

    _commandBuffer.waitEvents(
      evts, srcStageMask, dstStageMask, memoryBarriers,
      bufferMemoryBarriers, imageMemoryBarriers
    );
  }

  void CommandBuffer::beginRenderPass( const std::shared_ptr<RenderPass>& rp,
    const std::shared_ptr<Framebuffer>& framebuffer, const vk::Rect2D& area,
    vk::ArrayProxy<const vk::ClearValue> clearValues, vk::SubpassContents cnts )
  {
    assert( _state == State::Recording );

    _renderPass = rp;
    _framebuffer = framebuffer;

    vk::RenderPassBeginInfo renderPassBeginInfo;

    renderPassBeginInfo.renderPass = *rp;
    renderPassBeginInfo.framebuffer = *framebuffer;
    renderPassBeginInfo.renderArea = area;
    renderPassBeginInfo.clearValueCount = clearValues.size( );
    renderPassBeginInfo.pClearValues =
      reinterpret_cast< vk::ClearValue const* >( clearValues.data( ) );

    _commandBuffer.beginRenderPass( renderPassBeginInfo, cnts );

    _state = State::RecordingRenderPass;
  }

  void CommandBuffer::fillBuffer( const std::shared_ptr<lava::Buffer>& dstBuffer,
    vk::DeviceSize dstOffset, vk::DeviceSize fillSize, uint32_t data )
  {
    _commandBuffer.fillBuffer( *dstBuffer, dstOffset, fillSize, data );
  }

  void CommandBuffer::nextSubpass( vk::SubpassContents contents )
  {
    assert( _level == vk::CommandBufferLevel::ePrimary );
    _commandBuffer.nextSubpass( contents );
  }

  void CommandBuffer::endRenderPass( void )
  {
    assert( _state == State::RecordingRenderPass );

    _renderPass.reset( );
    _framebuffer.reset( );

    _commandBuffer.endRenderPass( );

    _state = State::Recording;
  }

  void CommandBuffer::executeCommands(
    const std::vector<std::shared_ptr<lava::CommandBuffer>>& secondaryCmds )
  {
    std::vector< vk::CommandBuffer > v;
    v.reserve( secondaryCmds.size( ) );
    for ( const auto& cmd : secondaryCmds )
    {
      v.push_back( *cmd );
    }
    _commandBuffer.executeCommands( v );
  }


  void CommandBuffer::clearAttachments(
    vk::ArrayProxy<const vk::ClearAttachment> attachments,
    vk::ArrayProxy<const vk::ClearRect> rects )
  {
    _commandBuffer.clearAttachments( attachments, rects );
  }

  void CommandBuffer::clearColorImage( const std::shared_ptr<Image>& img,
    vk::ImageLayout imageLayout, const vk::ClearColorValue& color,
    vk::ArrayProxy<const vk::ImageSubresourceRange> ranges )
  {
    _commandBuffer.clearColorImage( *img, imageLayout, color, ranges );
  }

  void CommandBuffer::clearDepthStencilImage( const std::shared_ptr<Image>& img,
    vk::ImageLayout imageLayout, float depth, uint32_t stencil,
    vk::ArrayProxy<const vk::ImageSubresourceRange> ranges )
  {
    vk::ClearDepthStencilValue depthStencil{ depth, stencil };
    _commandBuffer.clearDepthStencilImage( *img, imageLayout, depthStencil,
      ranges );
  }

  void CommandBuffer::blitImage( const std::shared_ptr<Image>& srcImage,
    vk::ImageLayout srcImageLayout, const std::shared_ptr<Image>& dstImage,
    vk::ImageLayout dstImageLayout, vk::ArrayProxy<const vk::ImageBlit> regions,
    vk::Filter filter )
  {
    _commandBuffer.blitImage( *srcImage, srcImageLayout, *dstImage,
      dstImageLayout, regions, filter );
  }

  void CommandBuffer::beginQuery(
    const std::shared_ptr<lava::QueryPool>& queryPool, uint32_t slot,
    vk::QueryControlFlags flags )
  {
    _commandBuffer.beginQuery( *queryPool, slot, flags );
  }

  void CommandBuffer::copyQueryPoolResults(
    const std::shared_ptr<lava::QueryPool>& queryPool, uint32_t startQuery,
    uint32_t queryCount, const std::shared_ptr<lava::Buffer>& dstBuffer,
    vk::DeviceSize dstOffset, vk::DeviceSize dstStride,
    vk::QueryResultFlags flags )
  {
    _commandBuffer.copyQueryPoolResults( *queryPool, startQuery, queryCount,
      *dstBuffer, dstOffset, dstStride, flags );
  }

  void CommandBuffer::endQuery(
    const std::shared_ptr<lava::QueryPool>& queryPool, uint32_t slot )
  {
    _commandBuffer.endQuery( *queryPool, slot );
  }

  void CommandBuffer::resetQueryPool(
    const std::shared_ptr<lava::QueryPool>& queryPool, uint32_t startQuery,
    uint32_t queryCount )
  {
    _commandBuffer.resetQueryPool( *queryPool, startQuery, queryCount );
  }

  void CommandBuffer::writeTimestamp( vk::PipelineStageFlagBits pipelineStage,
    const std::shared_ptr<lava::QueryPool>& queryPool, uint32_t entry )
  {
    _commandBuffer.writeTimestamp( pipelineStage, *queryPool, entry );
  }

  void CommandBuffer::setViewportScissors( uint32_t width, uint32_t height )
  {
    setScissor( 0, vk::Rect2D( { 0, 0 }, { width, height } ) );
    setViewport( 0, vk::Viewport( 0.0f, 0.0f, ( float ) width, ( float ) height,
      0.0f, 1.0f ) );
  }
  void CommandBuffer::setViewportScissors( const vk::Extent2D& dimensions )
  {
    setViewportScissors( dimensions.width, dimensions.height );
  }
  void CommandBuffer::setScissor( uint32_t first,
    vk::ArrayProxy<const vk::Rect2D> scissors )
  {
    _commandBuffer.setScissor( first, scissors );
  }
  void CommandBuffer::setViewport( uint32_t first,
    vk::ArrayProxy<const vk::Viewport> viewports )
  {
    _commandBuffer.setViewport( first, viewports );
  }
  void CommandBuffer::setDepthBias( float depthBias, float depthBiasClamp,
    float slopeScaledDepthBias )
  {
    _commandBuffer.setDepthBias( depthBias, depthBiasClamp,
      slopeScaledDepthBias );
  }
  void CommandBuffer::setDepthBounds( float minDepthBounds,
    float maxDepthBounds )
  {
    _commandBuffer.setDepthBounds( minDepthBounds, maxDepthBounds );
  }

  void CommandBuffer::setLineWidth( float lineWidth )
  {
    _commandBuffer.setLineWidth( lineWidth );
  }

  void CommandBuffer::setBlendConstants( const float blendConst[ 4 ] )
  {
    _commandBuffer.setBlendConstants( blendConst );
  }

  void CommandBuffer::setStencilCompareMask( vk::StencilFaceFlags faceMask,
    uint32_t stencilCompareMask )
  {
    _commandBuffer.setStencilCompareMask( faceMask, stencilCompareMask );
  }

  void CommandBuffer::setStencilReference( vk::StencilFaceFlags faceMask,
    uint32_t stencilReference )
  {
    _commandBuffer.setStencilReference( faceMask, stencilReference );
  }

  void CommandBuffer::setStencilWriteMask( vk::StencilFaceFlags faceMask,
    uint32_t stencilWriteMask )
  {
    _commandBuffer.setStencilWriteMask( faceMask, stencilWriteMask );
  }

  void CommandBuffer::dispatch( uint32_t x, uint32_t y, uint32_t z )
  {
    _commandBuffer.dispatch( x, y, z );
  }
  void CommandBuffer::draw( uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInst )
  {
    _commandBuffer.draw( vertexCount, instanceCount, firstVertex, firstInst );
  }

  void CommandBuffer::drawIndirect( const std::shared_ptr<Buffer>& buffer,
    vk::DeviceSize offset, uint32_t count, uint32_t stride )
  {
    _commandBuffer.drawIndirect( *buffer, offset, count, stride );
  }

  void CommandBuffer::drawIndexed( uint32_t indexCount, uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset, uint32_t firstInstance )
  {
    _commandBuffer.drawIndexed( indexCount, instanceCount, firstIndex,
      vertexOffset, firstInstance );
  }

  void CommandBuffer::drawIndexedIndirect( const std::shared_ptr<Buffer>& buffer,
    vk::DeviceSize offset, uint32_t count, uint32_t stride )
  {
    _commandBuffer.drawIndexedIndirect( *buffer, offset, count, stride );
  }

  void CommandBuffer::reset( vk::CommandBufferResetFlagBits flags )
  {
    _commandBuffer.reset( flags );
    _state = State::Ready; // TODO ?
  }

  void CommandBuffer::bindVertexBuffer( uint32_t startBinding,
    const std::shared_ptr<Buffer>& buffer, vk::DeviceSize offset )
  {
    _commandBuffer.bindVertexBuffers( startBinding,
      static_cast< vk::Buffer >( *buffer ), offset );
  }

  void CommandBuffer::bindIndexBuffer( const std::shared_ptr<Buffer>& buffer,
    vk::DeviceSize offset, vk::IndexType indexType )
  {
    _commandBuffer.bindIndexBuffer( *buffer, offset, indexType );
  }

  void CommandBuffer::bindIndexBuffer(
    const std::shared_ptr<IndexBuffer>& buffer, vk::DeviceSize offset )
  {
    _commandBuffer.bindIndexBuffer( *buffer, offset, buffer->getIndexType( ) );
  }

  void CommandBuffer::bindVertexBuffers( uint32_t startBinding,
    vk::ArrayProxy<const std::shared_ptr<Buffer>> buffers,
    vk::ArrayProxy<const vk::DeviceSize> offsets )
  {
    assert( buffers.size( ) == offsets.size( ) );

    _bindVertexBuffers.clear( );
    for ( auto & buffer : buffers )
    {
      _bindVertexBuffers.push_back( *buffer );
    }

    _commandBuffer.bindVertexBuffers( startBinding, _bindVertexBuffers,
      offsets );
  }


  void CommandBuffer::copyBuffer( const std::shared_ptr<Buffer>& srcBuffer, 
    const std::shared_ptr<Buffer>& dstBuffer, 
    vk::ArrayProxy<const vk::BufferCopy> regions )
  {
    _commandBuffer.copyBuffer( *srcBuffer, *dstBuffer, regions );
  }

  void CommandBuffer::copyBufferToImage( const std::shared_ptr<Buffer>& srcBuffer, 
    const std::shared_ptr<Image>& dstImage, vk::ImageLayout dstImageLayout, 
    vk::ArrayProxy<const vk::BufferImageCopy> regions )
  {
    _commandBuffer.copyBufferToImage( *srcBuffer, *dstImage, dstImageLayout,
      regions );
  }

  void CommandBuffer::copyImage( const std::shared_ptr<Image>& srcImage,
    vk::ImageLayout srcImageLayout, const std::shared_ptr<Image>& dstImage,
    vk::ImageLayout dstImageLayout, vk::ArrayProxy<const vk::ImageCopy> regions )
  {
    _commandBuffer.copyImage( *srcImage, srcImageLayout, *dstImage,
      dstImageLayout, regions );
  }

  void CommandBuffer::copyImageToBuffer( const std::shared_ptr<Image>& srcImage,
    vk::ImageLayout srcImageLayout, const std::shared_ptr<Buffer>& dstBuffer,
    vk::ArrayProxy<const vk::BufferImageCopy> regions )
  {
    _commandBuffer.copyImageToBuffer( *srcImage, srcImageLayout, *dstBuffer,
      regions );
  }

  void CommandBuffer::pipelineBarrier( vk::PipelineStageFlags srcStageMask,
    vk::PipelineStageFlags destStageMask, vk::DependencyFlags depFlags,
    vk::ArrayProxy<const vk::MemoryBarrier> barriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier> bufferMemoryBarriers,
    vk::ArrayProxy<const ImageMemoryBarrier> imageMemoryBarriers )
  {
    std::vector<vk::ImageMemoryBarrier> imbs;
    imbs.reserve( imageMemoryBarriers.size( ) );
    for ( auto const& imb : imageMemoryBarriers )
    {
      imbs.push_back( vk::ImageMemoryBarrier(
        imb.srcAccessMask, imb.dstAccessMask,
        imb.oldLayout, imb.newLayout,
        imb.srcQueueFamilyIndex, imb.dstQueueFamilyIndex,
        imb.image ? static_cast<vk::Image>( *imb.image ) : nullptr,
        imb.subresourceRange ) );
    }

    _commandBuffer.pipelineBarrier( srcStageMask, destStageMask, depFlags,
      barriers, bufferMemoryBarriers, imbs );
  }

  void CommandBuffer::bindDescriptorSets(
    vk::PipelineBindPoint pipelineBindPoint,
    const std::shared_ptr<PipelineLayout>& pipelineLayout, uint32_t firstSet,
    vk::ArrayProxy<const std::shared_ptr<DescriptorSet>> descriptorSets,
    vk::ArrayProxy<const uint32_t> dynamicOffsets )
  {
    _bindDescriptorSets.clear( );
    for ( auto& descriptor : descriptorSets )
    {
      _bindDescriptorSets.push_back( *descriptor );
    }

    _commandBuffer.bindDescriptorSets( pipelineBindPoint, *pipelineLayout,
      firstSet, _bindDescriptorSets, dynamicOffsets );
  }

  void CommandBuffer::bindPipeline( vk::PipelineBindPoint bindingPoint,
    const std::shared_ptr<Pipeline>& pipeline )
  {
    _commandBuffer.bindPipeline( bindingPoint, *pipeline );
  }
  void CommandBuffer::bindGraphicsPipeline( const std::shared_ptr<Pipeline>& p )
  {
    _commandBuffer.bindPipeline( vk::PipelineBindPoint::eGraphics, *p );
  }
  void CommandBuffer::bindComputePipeline( const std::shared_ptr<Pipeline>& p )
  {
    _commandBuffer.bindPipeline( vk::PipelineBindPoint::eCompute, *p );
  }

  void CommandBuffer::pushDescriptorSetKHR( vk::PipelineBindPoint bindpoint,
    std::shared_ptr<PipelineLayout> pipLayout, uint32_t set,
    vk::ArrayProxy<WriteDescriptorSet> descriptorWrites )
  {
    if ( !vkCmdPushDescriptorSetKHR )
    {
      VkDevice device = static_cast< VkDevice >
        ( static_cast<vk::Device>( *_commandPool->getDevice( ) ) );
      vkCmdPushDescriptorSetKHR =
        ( PFN_vkCmdPushDescriptorSetKHR ) vkGetDeviceProcAddr(
          device, "vkCmdPushDescriptorSetKHR" );
    }
    if ( !vkCmdPushDescriptorSetKHR )
    {
      throw;
    }
    std::vector<std::unique_ptr<vk::DescriptorImageInfo>> diis;
    diis.reserve( descriptorWrites.size( ) );

    std::vector<std::unique_ptr<vk::DescriptorBufferInfo>> dbis;
    dbis.reserve( descriptorWrites.size( ) );

    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve( descriptorWrites.size( ) );
    for ( const auto& w : descriptorWrites )
    {
      diis.push_back( std::unique_ptr<vk::DescriptorImageInfo>(
        w.imageInfo ? new vk::DescriptorImageInfo(
          w.imageInfo->sampler ?
          static_cast<vk::Sampler>( *w.imageInfo->sampler ) : nullptr,
          w.imageInfo->imageView ?
          static_cast<vk::ImageView>( *w.imageInfo->imageView ) : nullptr,
          w.imageInfo->imageLayout ) : nullptr ) );
      dbis.push_back( std::unique_ptr<vk::DescriptorBufferInfo>(
        w.bufferInfo ? new vk::DescriptorBufferInfo( w.bufferInfo->buffer ?
          static_cast<vk::Buffer>( *w.bufferInfo->buffer ) : nullptr,
          w.bufferInfo->offset, w.bufferInfo->range ) : nullptr ) );
      vk::WriteDescriptorSet write(
        nullptr,
        w.dstBinding,
        w.dstArrayElement,
        w.descriptorCount,
        w.descriptorType,
        diis.back( ).get( ),
        dbis.back( ).get( )
      );

      if ( w.texelBufferView )
      {
        auto bufferView = static_cast< vk::BufferView >( *w.texelBufferView );
        // TODO (LINUX FAILED) auto bb = static_cast< VkBufferView >( bufferView );
        write.setPTexelBufferView( &bufferView );
      }

      writes.push_back( std::move( write ) );
    }
    VkCommandBuffer m_commandBuffer = static_cast< VkCommandBuffer >( _commandBuffer );
    vk::PipelineLayout layout = *pipLayout;
    std::vector<VkWriteDescriptorSet> vkwds( descriptorWrites.size( ) );
    vkwds[ 0 ] = static_cast< VkWriteDescriptorSet >( writes.at( 0 ) );
    vkwds[ 1 ] = static_cast< VkWriteDescriptorSet >( writes.at( 1 ) );
    vkCmdPushDescriptorSetKHR( m_commandBuffer, 
      static_cast<VkPipelineBindPoint>( bindpoint ), 
      static_cast<VkPipelineLayout>( layout ), 
      set, descriptorWrites.size( ), 
      vkwds.data( ) );
  }
}