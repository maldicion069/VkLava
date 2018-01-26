#include <lava/lava.h>
using namespace lava;

#include <routes.h>

class MyApp : public VulkanApp
{
public:
  struct
  {
    std::shared_ptr<Buffer> storageBufferA;
    std::shared_ptr<Buffer> storageBufferB;
    std::shared_ptr<Buffer> storageBufferOutput;
    std::shared_ptr<Buffer> uniformBuffer;
    std::shared_ptr<Queue> queue;
    std::shared_ptr<CommandPool> commandPool;
    std::shared_ptr<CommandBuffer> commandBuffer;
    std::shared_ptr<Fence> fence;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<PipelineLayout> pipelineLayout;
    std::shared_ptr<DescriptorSet> descriptorSet;
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout;

    struct
    {
      int BufferSize;
    } ubo;
  } compute;

  std::shared_ptr<DescriptorPool> descriptorPool;

  MyApp(char const* title, uint32_t width, uint32_t height)
    : VulkanApp( title, width, height )
  {
    const int arraySize = 10;
    float A[ arraySize ]; // Input array A
    float B[ arraySize ]; // Input array B
    float O[ arraySize ]; // Output array
    for ( size_t i = 0; i < arraySize; ++i )
    {
      A[ i ] = ( float ) i;
      B[ i ] = arraySize - i - 1.0f;
      O[ i ] = 0;
    }

    compute.storageBufferA = _device->createBuffer( arraySize * sizeof( float ),
      vk::BufferUsageFlagBits::eStorageBuffer,
      vk::SharingMode::eExclusive, nullptr,
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent );

    compute.storageBufferB = _device->createBuffer( arraySize * sizeof( float ),
      vk::BufferUsageFlagBits::eStorageBuffer,
      vk::SharingMode::eExclusive, nullptr,
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent );

    compute.storageBufferOutput = _device->createBuffer( arraySize * sizeof( float ),
      vk::BufferUsageFlagBits::eStorageBuffer | vk ::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
      vk::SharingMode::eExclusive, nullptr,
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent );

    compute.uniformBuffer = _device->createBuffer( sizeof( compute.ubo ),
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::SharingMode::eExclusive, nullptr,
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent );


    std::array<vk::DescriptorPoolSize, 2> poolSize;
    // Compute UBO
    poolSize[ 0 ] = vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, 1 );
    // Storage buffers
    poolSize[ 1 ] = vk::DescriptorPoolSize( vk::DescriptorType::eStorageBuffer, 3 );

    descriptorPool = _device->createDescriptorPool( {}, 1, poolSize );

    // Search for a compute queue in the array of 
    //    queue families, try to find one that support
    std::vector<uint32_t> queueFamilyIndices =
      _physicalDevice->getComputeQueueFamilyIndices( _surface );
    assert( !queueFamilyIndices.empty( ) );
    uint32_t _queueComputeFamilyIndex = queueFamilyIndices[ 0 ];

    compute.queue = _device->getQueue( _queueComputeFamilyIndex, 0 );

    std::cout << "Compute queue created" << std::endl;

    std::vector<DescriptorSetLayoutBinding> dslbs =
    {
      // Binding 0: Storage buffer (InputBufferA)
      DescriptorSetLayoutBinding(
        0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute
      ),
      // Binding 1: Storage buffer (InputBufferB)
      DescriptorSetLayoutBinding(
        1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute
      ),
      // Binding 2: Storage buffer (OutputBuffer)
      DescriptorSetLayoutBinding(
        2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute
      ),
      // Binding 3: Uniform buffer block (BufferSize)
      DescriptorSetLayoutBinding(
        3, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute
      ),
    };
    compute.descriptorSetLayout = _device->createDescriptorSetLayout( dslbs );
    compute.pipelineLayout = _device->createPipelineLayout( compute.descriptorSetLayout );

    compute.descriptorSet = _device->allocateDescriptorSet( descriptorPool, compute.descriptorSetLayout );

    std::vector<WriteDescriptorSet> wdss =
    {
      // Binding 0: Storage buffer (InputBufferA)
      lava::WriteDescriptorSet(
        compute.descriptorSet, 0, 0, vk::DescriptorType::eStorageBuffer,
        1, nullptr, DescriptorBufferInfo( compute.storageBufferA, 0,
          sizeof( compute.ubo ) )
      ),
      // Binding 3: Storage buffer (InputBufferB)
      lava::WriteDescriptorSet(
        compute.descriptorSet, 1, 0, vk::DescriptorType::eStorageBuffer,
        1, nullptr, DescriptorBufferInfo( compute.storageBufferB, 0,
          sizeof( compute.ubo ) )
      ),
      // Binding 3: Storage buffer (OutputBuffer)
      lava::WriteDescriptorSet(
        compute.descriptorSet, 2, 0, vk::DescriptorType::eStorageBuffer,
        1, nullptr, DescriptorBufferInfo( compute.storageBufferOutput, 0,
          sizeof( compute.ubo ) )
      ),
      // Binding 3: Uniform buffer block
      lava::WriteDescriptorSet(
        compute.descriptorSet, 3, 0, vk::DescriptorType::eUniformBuffer,
        1, nullptr, DescriptorBufferInfo( compute.uniformBuffer, 0,
          sizeof( compute.ubo ) )
      )
    };

    _device->updateDescriptorSets( wdss, {} );

    // Create compute shader pipelines
    std::shared_ptr<ShaderModule> computeShaderModule =
      _device->createShaderModule(
        LAVA_EXAMPLES_SPV_ROUTE + std::string( "arraySum_comp.spv" ),
        vk::ShaderStageFlagBits::eCompute
      );

    PipelineShaderStageCreateInfo computeStage(
      vk::ShaderStageFlagBits::eCompute, computeShaderModule );

    std::cout << "CREATE PIPELINE" << std::endl;

    compute.pipeline = _device->createComputePipeline(
      pipelineCache, {}, computeStage, compute.pipelineLayout );

    // Fence for compute CB sync
    compute.fence = _device->createFence( true );

    // Separate command pool as queue family for compute may be different than graphics
    compute.commandPool = _device->createCommandPool( {}, compute.queue->getQueueFamilyIndex( ) );

    // Create a command buffer for compute operations
    compute.commandBuffer = compute.commandPool->allocateCommandBuffer( );

    // Barrier to ensure that input buffer transfer is finished before compute shader reads from it
    vk::BufferMemoryBarrier bmb;
    bmb.buffer = static_cast< vk::Buffer >( *compute.storageBufferOutput );
    bmb.size = VK_WHOLE_SIZE;
    bmb.srcAccessMask = vk::AccessFlagBits::eHostWrite;
    bmb.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    compute.commandBuffer->beginSimple( );
    compute.commandBuffer->pipelineBarrier( 
      vk::PipelineStageFlagBits::eHost, 
      vk::PipelineStageFlagBits::eComputeShader, {}, {}, { bmb }, {} );

    compute.commandBuffer->bindComputePipeline( compute.pipeline );
    compute.commandBuffer->bindDescriptorSets( 
      vk::PipelineBindPoint::eCompute, compute.pipelineLayout, 0, 
      compute.descriptorSet, { } );

    compute.commandBuffer->dispatch( arraySize, 1, 1 );

    // Barrier to ensure that shader writes are finished before buffer is read back from GPU
    bmb.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    bmb.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    bmb.buffer = static_cast< vk::Buffer >( *compute.storageBufferOutput );
    bmb.size = VK_WHOLE_SIZE;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    compute.commandBuffer->pipelineBarrier(
      vk::PipelineStageFlagBits::eComputeShader,
      vk::PipelineStageFlagBits::eTransfer, {}, {}, { bmb }, {} );

    // Read back to host visible buffer
    vk::BufferCopy copyRegion;
    copyRegion.size = arraySize * sizeof( float );

    std::shared_ptr<Buffer> hostBuffer = _device->createBuffer( arraySize * sizeof( float ),
      vk::BufferUsageFlagBits::eStorageBuffer,
      vk::SharingMode::eExclusive, nullptr,
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent );

    compute.commandBuffer->copyBuffer( compute.storageBufferOutput, hostBuffer, copyRegion );

    // Barrier to ensure that buffer copy is finished before host reading from it
    bmb.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    bmb.dstAccessMask = vk::AccessFlagBits::eHostRead;
    bmb.buffer = static_cast< vk::Buffer >( *hostBuffer );
    bmb.size = VK_WHOLE_SIZE;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    compute.commandBuffer->pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eHost, {}, {}, { bmb }, {} );

    compute.commandBuffer->end( );

    // Submit compute commands
    compute.queue->submit( compute.commandBuffer, compute.fence );
    lava::Fence::waitForFences( { compute.fence }, true, std::numeric_limits<uint64_t>::max( ) );

    /*// Make device writes visible to the host
    void *mapped;
    hostBuffer->map( VK_WHOLE_SIZE, 0 );
    vk::MappedMemoryRange mappedRange;
    mappedRange.memory = hostBuffer->_memory;
    mappedRange.offset = 0;
    mappedRange.size = VK_WHOLE_SIZE;
    _device->inva
    vkInvalidateMappedMemoryRanges( device, 1, &mappedRange );

    // Copy to output
    memcpy( computeOutput.data( ), mapped, bufferSize );
    vkUnmapMemory( device, hostMemory );*/
  }
  virtual void doPaint( void ) override
  {
    std::cout << "INIT RENDER" << std::endl;
    std::shared_ptr<CommandPool> commandPool = _device->createCommandPool(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex );
    std::shared_ptr<CommandBuffer> commandBuffer = commandPool->allocateCommandBuffer( );

    float timeValue = glfwGetTime( );
    float greenValue = sin( timeValue ) / 2.0f + 0.5f;

    std::array<float, 4> ccv = { 0.0f, greenValue, 0.0f, 1.0f };
    commandBuffer->begin( );
    commandBuffer->beginRenderPass( _renderPass, _defaultFramebuffer->getFramebuffer( ),
      vk::Rect2D( { 0, 0 }, _defaultFramebuffer->getExtent( ) ), { vk::ClearValue( ccv ),
      vk::ClearValue( vk::ClearDepthStencilValue( 1.0f, 0 ) ) },
      vk::SubpassContents::eInline );
    commandBuffer->setViewport( 0, { vk::Viewport( 0.0f, 0.0f,
      ( float ) _defaultFramebuffer->getExtent( ).width,
      ( float ) _defaultFramebuffer->getExtent( ).height, 0.0f, 1.0f ) } );
    commandBuffer->setScissor( 0, { vk::Rect2D( { 0, 0 }, _defaultFramebuffer->getExtent( ) ) } );
    commandBuffer->endRenderPass( );
    commandBuffer->end( );

    _graphicsQueue->submit( SubmitInfo{
      { _defaultFramebuffer->getPresentSemaphore( ) },
      { vk::PipelineStageFlagBits::eColorAttachmentOutput },
      commandBuffer,
      _renderComplete
    } );
    std::cout << "END RENDER" << std::endl;
  }
  void keyEvent(int key, int scancode, int action, int mods)
  {
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
      switch (action)
      {
      case GLFW_PRESS:
        getWindow( )->close( );
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }
};

void glfwErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main( void )
{
  try
  {
    VulkanApp* app = new MyApp( "ArraySum", 800, 600 );

    app->getWindow( )->setErrorCallback( glfwErrorCallback );

    while ( app->isRunning( ) )
    {
      app->waitEvents( );
      app->paint( );

      app->getWindow( )->close( );
    }

    delete app;
  }
  catch ( std::system_error err )
  {
    std::cout << "System Error: " << err.what( ) << std::endl;
  }
  return 0;
}