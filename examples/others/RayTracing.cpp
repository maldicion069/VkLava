#include <lava/lava.h>
using namespace lava;

#include <routes.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <routes.h>

class MyApp : public VulkanApp
{
public:
  // Resources for graphic part
  struct
  {
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout;
    std::shared_ptr<DescritorSet> descriptorSetPreCompute;
    std::shared_ptr<DescritorSet> descriptorSet;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<PipelineLayout> pipelineLayout;
  } graphics;

  // Resources for compute part
  struct
  {
    struct
    {
      std::shared_ptr<Buffer> spheres;
    } storageBuffers;
    std::shared_ptr<Buffer> uniformBuffer;
    std::shared_ptr<Queue> queue;
    std::shared_ptr<CommandPool> commandPool;
    std::shared_ptr<CommandBuffer> commandBuffer;
    std::shared_ptr<Fence> fence;
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout;
    std::shared_ptr<DescritorSet> descriptorSet;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<PipelineLayout> pipelineLayout;
    struct UBOCompute
    {
      // float globalTime;
      glm::vec2 windowSize;
    } ubo;
  } compute;

  // SSBO sphere declaration 
  struct Sphere
  {                 // Shader uses std140 layout (so we only use vec4 instead of vec3)
    glm::vec3 pos;                
    float radius;
    glm::vec3 diffuse;
    float specular;
    uint32_t id;                // Id used to identify sphere for raytracing
    glm::ivec3 _pad;
  };

  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<PipelineLayout> _pipelineLayout;
  std::shared_ptr<vk::ImageView> _textureImageView;
  std::shared_ptr<Sampler> _textureSampler;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<Texture2D> tex;

  void updateUniformBuffers( void )
  {
    // TODO compute.ubo.
  }
  
  // Prepare a texture target that is used to store compute shader calculations
  void createTextureTarget( std::shared_ptr<Texture> tex, 
    uint32_t width, uint32_t height )
  {
    // TODO: CHECK IF SUPPORT STORAGE IMAGE

    t
    // Prepare blit target texture
    tex->width = width;
    tex->height = height;
  }

  MyApp(char const* title, uint32_t width, uint32_t height)
    : VulkanApp( title, width, height )
  {
    // PREPARING STORAGE BUFFER
    {
      // Spheres
      std::vector<Sphere> spheres;
      spheres.push_back(newSphere(glm::vec3(1.75f, -0.5f, 0.0f), 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 32.0f));
      spheres.push_back(newSphere(glm::vec3(0.0f, 1.0f, -0.5f), 1.0f, glm::vec3(0.65f, 0.77f, 0.97f), 32.0f));
      spheres.push_back(newSphere(glm::vec3(-1.75f, -0.75f, -0.5f), 1.25f, glm::vec3(0.9f, 0.76f, 0.46f), 32.0f));
      vk::DeviceSize storageBufferSize = spheres.size() * sizeof(Sphere);

      // Stage
      std::shared_ptr<Buffer> stagingBuffer;

      stagingBuffer = _device->createBuffer( storageBufferSize, 
        vk::BufferUsageFlagBits::eTransferSrc, 
        vk::SharingMode::eExclusive, nullptr,
        vk::MemoryPropertyFlagBits::eHostVisible | 
          vk::MemoryPropertyFlagBits::eHostCoherent );

      compute.storageBuffers.spheres = _device->createBuffer( storageBufferSize, 
        vk::BufferUsageFlagBits::eVertexBuffer |vk::BufferUsageFlagBits::eStorageBuffer, 
        vk::SharingMode::eExclusive, nullptr,
        vk::MemoryPropertyFlagBits::eDeviceLocal );

      // Copy to staging buffer
      // create a command pool for command buffer allocation
      std::shared_ptr<CommandPool> commandPool = _device->createCommandPool( 
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex );
      std::shared_ptr<CommandBuffer> copyCmd = commandPool->allocateCommandBuffer( );

      copyCmd->beginSimple( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
      vk::BufferCopy copyRegion;
      copyRegion.size = storageBufferSize;
      copyCmd->copyBuffer( stagingBuffer.buffer, 
        compute.storageBuffers.spheres.bUffer, { copyRegion } );

      // Send command buffer
      copyCmd->end( );

      queue->submitAndWait( copyCmd );

      stagingBuffer.reset( );
    }

    // PREPARE UNIFORM BUFFERS
    {
      // Compute shader parameter uniform buffer block
      compute.uniformBuffer = _device->createBuffer( sizeof( compute.ubo ), 
        vk::BufferUsageFlagBits::eUniformBuffer, 
        vk::SharingMode::eExclusive, nullptr,
        vk::MemoryPropertyFlagBits::eHostVisible | 
          vk::MemoryPropertyFlagBits::eHostCoherent );

      updateUniformBuffers( );
    }


    std::shared_ptr<CommandPool> commandPool = _device->createCommandPool(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex );
    tex = std::make_shared<Texture2D>( _device, LAVA_EXAMPLES_RESOURCES_ROUTE + 
      std::string( "/uv_checker.png" ), commandPool, _graphicsQueue );

    // init descriptor and pipeline layouts
    std::vector<DescriptorSetLayoutBinding> dslbs;
    DescriptorSetLayoutBinding mvpDescriptor( 0, vk::DescriptorType::eCombinedImageSampler, 
      vk::ShaderStageFlagBits::eFragment );
    dslbs.push_back( mvpDescriptor );
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = _device->createDescriptorSetLayout( dslbs );

    _pipelineLayout = _device->createPipelineLayout( descriptorSetLayout, nullptr );


    std::shared_ptr<DescriptorPool> descriptorPool =
      _device->createDescriptorPool( {}, 1, { { vk::DescriptorType::eCombinedImageSampler, 1 } } );

    // Init descriptor set
    _descriptorSet = _device->allocateDescriptorSet( descriptorPool, descriptorSetLayout );
    std::vector<WriteDescriptorSet> wdss;

    WriteDescriptorSet w( _descriptorSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, 1, 
      DescriptorImageInfo( 
        vk::ImageLayout::eGeneral, 
        std::make_shared<vk::ImageView>( tex->view ), 
        std::make_shared<vk::Sampler>( tex->sampler )
      ), nullptr
    );
    wdss.push_back( w );
    _device->updateDescriptorSets( wdss, {} );


    // init shaders
    std::shared_ptr<ShaderModule> vertexShaderModule =_device->createShaderModule( 
      LAVA_EXAMPLES_RESOURCES_ROUTE + std::string("/fullquad_vert.spv"), 
      vk::ShaderStageFlagBits::eVertex );
    std::shared_ptr<ShaderModule> fragmentShaderModule = _device->createShaderModule( 
      LAVA_EXAMPLES_RESOURCES_ROUTE + std::string( "/fullquad_frag.spv" ), 
      vk::ShaderStageFlagBits::eFragment );

    // init pipeline
    std::shared_ptr<PipelineCache> pipelineCache = 
      _device->createPipelineCache( 0, nullptr );
    PipelineShaderStageCreateInfo vertexStage( 
      vk::ShaderStageFlagBits::eVertex, vertexShaderModule );
    PipelineShaderStageCreateInfo fragmentStage( 
      vk::ShaderStageFlagBits::eFragment, fragmentShaderModule );
    PipelineVertexInputStateCreateInfo vertexInput( {}, {} );
    vk::PipelineInputAssemblyStateCreateInfo assembly( {}, 
      vk::PrimitiveTopology::eTriangleStrip, VK_FALSE );
    PipelineViewportStateCreateInfo viewport( { {} }, { {} } );
    vk::PipelineRasterizationStateCreateInfo rasterization( {}, false, false, 
      vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, 
      vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f );
    PipelineMultisampleStateCreateInfo multisample( 
      vk::SampleCountFlagBits::e1, false, 0.0f, nullptr, false, false );
    vk::StencilOpState stencilOpState( vk::StencilOp::eKeep, 
      vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 
      0, 0, 0 );
    vk::PipelineDepthStencilStateCreateInfo depthStencil( {}, true, true, 
      vk::CompareOp::eLessOrEqual, false, false, stencilOpState, 
      stencilOpState, 0.0f, 0.0f );
    vk::PipelineColorBlendAttachmentState colorBlendAttachment( false, 
      vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, 
      vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG 
      | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA );
    PipelineColorBlendStateCreateInfo colorBlend( false, vk::LogicOp::eNoOp, 
      colorBlendAttachment, { 1.0f, 1.0f, 1.0f, 1.0f } );
    PipelineDynamicStateCreateInfo dynamic( { vk::DynamicState::eViewport, 
      vk::DynamicState::eScissor } );


    _pipeline = _device->createGraphicsPipeline( pipelineCache, {}, 
    { vertexStage, fragmentStage }, vertexInput, assembly, nullptr, 
      viewport, rasterization, multisample, depthStencil, colorBlend, dynamic,
      _pipelineLayout, _renderPass );
  }
  void doPaint( void ) override
  {
    // create a command pool for command buffer allocation
    std::shared_ptr<CommandPool> commandPool = _device->createCommandPool( 
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex );
    std::shared_ptr<CommandBuffer> commandBuffer = commandPool->allocateCommandBuffer( );

    commandBuffer->begin( );

    std::array<float, 4> ccv = { 0.2f, 0.3f, 0.3f, 1.0f };
    commandBuffer->beginRenderPass( _renderPass, 
      _defaultFramebuffer->getFramebuffer( ), vk::Rect2D( { 0, 0 }, 
        _defaultFramebuffer->getExtent( ) ),
    { vk::ClearValue( ccv ), vk::ClearValue( 
      vk::ClearDepthStencilValue( 1.0f, 0 ) ) }, vk::SubpassContents::eInline );
    commandBuffer->bindGraphicsPipeline( _pipeline );
    commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics,
      _pipelineLayout, 0, { _descriptorSet }, nullptr );

    commandBuffer->setViewport( 0, vk::Viewport( 0.0f, 0.0f, 
      ( float ) _defaultFramebuffer->getExtent( ).width, 
      ( float ) _defaultFramebuffer->getExtent( ).height, 0.0f, 1.0f ) );
    commandBuffer->setScissor( 0, vk::Rect2D( { 0, 0 }, 
      _defaultFramebuffer->getExtent( ) ) );
    commandBuffer->draw( 4, 1, 0, 0 );
    commandBuffer->endRenderPass( );

    commandBuffer->end( );

    _graphicsQueue->submit( SubmitInfo{
      { _defaultFramebuffer->getPresentSemaphore( ) },
      { vk::PipelineStageFlagBits::eColorAttachmentOutput },
      commandBuffer,
      _renderComplete
    } );
  }
  void keyEvent(int key, int scancode, int action, int mods)
  {
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
      switch (action)
      {
      case GLFW_PRESS:
        glfwSetWindowShouldClose(getWindow()->getWindow( ), GLFW_TRUE);
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
    //if (glfwInit())
    //{
    VulkanApp* app = new MyApp( "FullQuadTexture", 800, 600 );

    app->getWindow( )->setErrorCallback( glfwErrorCallback );

    while ( app->isRunning( ) )
    {
      app->waitEvents( );
      app->paint( );
    }

    delete app;
    //}
  }
  catch ( std::system_error err )
  {
    std::cout << "System Error: " << err.what( ) << std::endl;
  }
  system( "PAUSE" );
  return 0;
}