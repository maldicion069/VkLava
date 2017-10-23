#include <lava/lava.h>
using namespace lava;

#include <routes.h>

struct
{
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
} uboVS;

struct
{
  bool enable_hdr = true;
  float exposure = 1.0f;
} uboFS;

class MyApp : public VulkanApp
{
public:
  struct
  {
    std::shared_ptr<Pipeline> solid;
    std::shared_ptr<Pipeline> postprocess;
  } pipelines;

  struct
  {
    std::shared_ptr<PipelineLayout> solid;
    std::shared_ptr<PipelineLayout> postprocess;
  } pipelineLayouts;

  struct
  {
    std::shared_ptr<DescriptorSet> solid;
    std::shared_ptr<DescriptorSet> postprocess;
  } descriptorSets;

  std::shared_ptr<Buffer> uniformBufferMVP;
  std::shared_ptr<Buffer> uniformBufferHDR;

  struct
  {
    std::shared_ptr<DescriptorSetLayout> solid;
    std::shared_ptr<DescriptorSetLayout> postprocess;
  } descriptorSetLayout;

  std::shared_ptr<lava::extras::Geometry> geometry;

  std::shared_ptr<lava::extras::CustomFBO> fbo;

  MyApp( char const* title, uint32_t width, uint32_t height )
    : VulkanApp( title, width, height )
  {
    geometry = std::make_shared<lava::extras::Geometry>( _device,
      LAVA_EXAMPLES_MESHES_ROUTE + std::string( "monkey.obj_" ) );

    fbo = std::make_shared<lava::extras::CustomFBO>( _device, width, height );

    fbo->addColorAttachmentt( vk::Format::eR16G16B16A16Sfloat );
    fbo->addDepthAttachment( this->_depthFormat );

    fbo->build( );

    // MVP buffer
    {
      uint32_t mvpBufferSize = sizeof( uboVS );
      uniformBufferMVP = _device->createBuffer( mvpBufferSize,
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive, nullptr,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent );
    }

    // HDR buffer
    {
      uint32_t hdrBufferSize = sizeof( uboFS );
      uniformBufferHDR = _device->createBuffer( hdrBufferSize,
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive, nullptr,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent );
    }

    updateUniformBuffers( );

    // Init descriptor and pipeline layouts
    std::vector<DescriptorSetLayoutBinding> dslbs1 =
    {
      // Binding 0 : Vertex shader uniform buffer
      DescriptorSetLayoutBinding(
        0,
        vk::DescriptorType::eUniformBuffer,
        vk::ShaderStageFlagBits::eVertex
      )
    };
    descriptorSetLayout.solid = _device->createDescriptorSetLayout( dslbs1 );

    std::vector<DescriptorSetLayoutBinding> dslbs2 =
    {// Binding 0 : Color texture target
      DescriptorSetLayoutBinding(
        0,
        vk::DescriptorType::eCombinedImageSampler,
        vk::ShaderStageFlagBits::eFragment
      ),
      // Binding 1 : fragment shader uniform buffer (ubo HDR)
      DescriptorSetLayoutBinding(
        1,
        vk::DescriptorType::eUniformBuffer,
        vk::ShaderStageFlagBits::eFragment
      )
    };
    descriptorSetLayout.postprocess = _device->createDescriptorSetLayout( dslbs2 );

    pipelineLayouts.solid = _device->createPipelineLayout( descriptorSetLayout.solid );
    pipelineLayouts.postprocess = _device->createPipelineLayout( descriptorSetLayout.postprocess );

    // init shaders
    std::shared_ptr<ShaderModule> vertexShaderModule = _device->createShaderModule(
      LAVA_EXAMPLES_SPV_ROUTE + std::string( "mesh_vert.spv" ),
      vk::ShaderStageFlagBits::eVertex );
    std::shared_ptr<ShaderModule> fragmentShaderModule = _device->createShaderModule(
      LAVA_EXAMPLES_SPV_ROUTE + std::string( "mesh_frag.spv" ),
      vk::ShaderStageFlagBits::eFragment );

    // init pipeline
    std::shared_ptr<PipelineCache> pipelineCache = _device->createPipelineCache( 0 );
    PipelineShaderStageCreateInfo vertexStage(
      vk::ShaderStageFlagBits::eVertex, vertexShaderModule );
    PipelineShaderStageCreateInfo fragmentStage(
      vk::ShaderStageFlagBits::eFragment, fragmentShaderModule );

    PipelineVertexInputStateCreateInfo vertexInput(
      vk::VertexInputBindingDescription( 0, sizeof( lava::extras::Vertex ),
        vk::VertexInputRate::eVertex ),
        {
          vk::VertexInputAttributeDescription( 0, 0,
          vk::Format::eR32G32B32Sfloat, offsetof( lava::extras::Vertex, position ) ),
          vk::VertexInputAttributeDescription( 1, 0,
          vk::Format::eR32G32B32Sfloat, offsetof( lava::extras::Vertex, normal ) ),
          vk::VertexInputAttributeDescription( 2, 0,
          vk::Format::eR32G32Sfloat, offsetof( lava::extras::Vertex, texCoord ) )
        }
    );
    vk::PipelineInputAssemblyStateCreateInfo assembly( {},
      vk::PrimitiveTopology::eTriangleList, VK_FALSE );
    PipelineViewportStateCreateInfo viewport( { {} }, { {} } );
    vk::PipelineRasterizationStateCreateInfo rasterization( {}, true,
      false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
      vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f );
    PipelineMultisampleStateCreateInfo multisample( vk::SampleCountFlagBits::e1,
      false, 0.0f, nullptr, false, false );
    vk::StencilOpState stencilOpState( vk::StencilOp::eKeep,
      vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 0, 0 );
    vk::PipelineDepthStencilStateCreateInfo depthStencil( {}, true, true,
      vk::CompareOp::eLessOrEqual, false, false, stencilOpState,
      stencilOpState, 0.0f, 0.0f );
    vk::PipelineColorBlendAttachmentState colorBlendAttachment( false,
      vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
      vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA );
    PipelineColorBlendStateCreateInfo colorBlend( false, vk::LogicOp::eNoOp,
      colorBlendAttachment, { 1.0f, 1.0f, 1.0f, 1.0f } );
    PipelineDynamicStateCreateInfo dynamic( { vk::DynamicState::eViewport,
      vk::DynamicState::eScissor } );


    pipelines.solid = _device->createGraphicsPipeline( pipelineCache, {},
    { vertexStage, fragmentStage }, vertexInput, assembly, nullptr,
      viewport, rasterization, multisample, depthStencil, colorBlend, dynamic,
      pipelineLayouts.solid, fbo->renderPass );

    std::array<vk::DescriptorPoolSize, 1> poolSize1;
    // Binding 0: MVP buffer
    poolSize1[ 0 ] = vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, 1 );
    std::shared_ptr<DescriptorPool> descriptorPool1 =
      _device->createDescriptorPool( {}, 1, poolSize1 );

    // Init descriptor set
    descriptorSets.solid = _device->allocateDescriptorSet(
      descriptorPool1, descriptorSetLayout.solid );

    std::vector<lava::WriteDescriptorSet> wdss =
    {
      lava::WriteDescriptorSet(
        descriptorSets.solid, 0, 0, vk::DescriptorType::eUniformBuffer,
        1, nullptr, DescriptorBufferInfo( uniformBufferMVP, 0,
          sizeof( uboVS ) )
      )
    };
    _device->updateDescriptorSets( wdss, {} );


    // POSTPROCESS PIPELINE

    std::array<vk::DescriptorPoolSize, 2> poolSize2;
    // Binding 0: Texture
    poolSize2[ 0 ] = vk::DescriptorPoolSize( vk::DescriptorType::eCombinedImageSampler, 1 );
    // Binding 1: HDR buffer
    poolSize2[ 1 ] = vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, 1 );
    std::shared_ptr<DescriptorPool> descriptorPool2 =
      _device->createDescriptorPool( {}, 1, poolSize2 );

    // Init descriptor set
    descriptorSets.postprocess = _device->allocateDescriptorSet(
      descriptorPool2, descriptorSetLayout.postprocess );

    std::vector<lava::WriteDescriptorSet> wdss2 =
    {
      lava::WriteDescriptorSet(
        descriptorSets.postprocess, 0, 0,
        vk::DescriptorType::eCombinedImageSampler, 1,
        DescriptorImageInfo(
          vk::ImageLayout::eShaderReadOnlyOptimal,
          std::make_shared<vk::ImageView>( *fbo->_colorAttachments[ 0 ].view ),
          std::make_shared<vk::Sampler>( fbo->colorSampler )
        ), nullptr
      ),
      lava::WriteDescriptorSet(
        descriptorSets.solid, 1, 0, vk::DescriptorType::eUniformBuffer,
        1, nullptr, DescriptorBufferInfo( uniformBufferHDR, 0,
          sizeof( uboFS ) )
      )
    };
    _device->updateDescriptorSets( wdss2, {} );

    PipelineVertexInputStateCreateInfo emptyInputState( {}, {} );
    vk::PipelineInputAssemblyStateCreateInfo assemblyPP( {},
      vk::PrimitiveTopology::eTriangleStrip, VK_FALSE );

    // init shaders
    std::shared_ptr<ShaderModule> ppVertexShaderModule = _device->createShaderModule(
      LAVA_EXAMPLES_SPV_ROUTE + std::string( "fullquad_vert.spv" ),
      vk::ShaderStageFlagBits::eVertex );
    std::shared_ptr<ShaderModule> ppFragmentShaderModule = _device->createShaderModule(
      LAVA_EXAMPLES_SPV_ROUTE + std::string( "hdr_frag.spv" ),
      vk::ShaderStageFlagBits::eFragment );

    PipelineShaderStageCreateInfo ppVertexStage(
      vk::ShaderStageFlagBits::eVertex, ppVertexShaderModule );
    PipelineShaderStageCreateInfo ppFragmentStage(
      vk::ShaderStageFlagBits::eFragment, ppFragmentShaderModule );

    pipelines.postprocess = _device->createGraphicsPipeline( pipelineCache, {},
    { ppVertexStage, ppFragmentStage }, emptyInputState, assemblyPP, nullptr,
      viewport, rasterization, multisample, depthStencil, colorBlend, dynamic,
      pipelineLayouts.postprocess, _renderPass );

    //updateUniformBuffers( );
    //buildCmdBuffers( );
  }
  void updateUniformBuffers( void )
  {
    uint32_t width = _window->getWidth( );
    uint32_t height = _window->getHeight( );

    static auto startTime = std::chrono::high_resolution_clock::now( );

    auto currentTime = std::chrono::high_resolution_clock::now( );
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(
      currentTime - startTime ).count( ) / 1000.0f;

    uboVS.model = glm::rotate( glm::mat4( 1.0f ), time * glm::radians( 90.0f ),
      glm::vec3( 0.0f, 1.0f, 0.0f ) );
    glm::vec3 cameraPos = glm::vec3( 0.0f, 0.0f, 0.5f );
    glm::vec3 cameraFront = glm::vec3( 0.0f, 0.0f, -1.0f );
    glm::vec3 cameraUp = glm::vec3( 0.0f, 1.0f, 0.0f );

    uboVS.view = glm::lookAt( cameraPos, cameraPos + cameraFront, cameraUp );

    uboVS.proj = glm::perspective( glm::radians( 45.0f ), width / ( float ) height, 0.1f, 10.0f );
    uboVS.proj[ 1 ][ 1 ] *= -1;

    uniformBufferMVP->writeData( 0, sizeof( uboVS ), &uboVS );

    uboFS.enable_hdr = true;
    uboFS.exposure = 1.0f;

    uniformBufferHDR->writeData( 0, sizeof( uboFS ), &uboFS );
  }

  std::shared_ptr<CommandBuffer> cmdSolidBuffer;

  void buildCmdBuffers( void )
  {
    uint32_t width = _defaultFramebuffer->getExtent( ).width;
    uint32_t height = _defaultFramebuffer->getExtent( ).height;

    std::shared_ptr<CommandPool> commandPool =
      _device->createCommandPool(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex );

    cmdSolidBuffer = commandPool->allocateCommandBuffer( );

    cmdSolidBuffer->begin( );

    std::array<float, 4> ccv = { 0.2f, 0.3f, 0.3f, 1.0f };
    cmdSolidBuffer->beginRenderPass( fbo->renderPass,
      fbo->_fbo,
      vk::Rect2D( { 0, 0 },
        _defaultFramebuffer->getExtent( ) ),
        { vk::ClearValue( ccv ), vk::ClearValue(
          vk::ClearDepthStencilValue( 1.0f, 0 ) )
        }, vk::SubpassContents::eInline );

    cmdSolidBuffer->setViewportScissors( width, height );
    cmdSolidBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics,
      pipelineLayouts.solid, 0, { descriptorSets.solid }, nullptr );
    cmdSolidBuffer->bindGraphicsPipeline( pipelines.solid );
    geometry->render( cmdSolidBuffer );
    cmdSolidBuffer->endRenderPass( );

    cmdSolidBuffer->end( );


    fbo->commandBuffer = commandPool->allocateCommandBuffer( );

    fbo->commandBuffer->begin( );

    fbo->commandBuffer->beginRenderPass( _renderPass,
      _defaultFramebuffer->getFramebuffer( ),
      vk::Rect2D( { 0, 0 },
        _defaultFramebuffer->getExtent( ) ),
        { vk::ClearValue( ccv ), vk::ClearValue(
          vk::ClearDepthStencilValue( 1.0f, 0 ) )
        }, vk::SubpassContents::eInline );

    fbo->commandBuffer->setViewportScissors( width, height );
    fbo->commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics,
      pipelineLayouts.postprocess, 0, { descriptorSets.postprocess }, nullptr );
    fbo->commandBuffer->bindGraphicsPipeline( pipelines.postprocess );
    fbo->commandBuffer->draw( 4, 1, 0, 0 );
    fbo->commandBuffer->endRenderPass( );

    fbo->commandBuffer->end( );
  }

  void doPaint( void ) override
  {
    updateUniformBuffers( );
    buildCmdBuffers( );

    _graphicsQueue->submit( SubmitInfo{
      { _defaultFramebuffer->getPresentSemaphore( ) }, // wait 
      { vk::PipelineStageFlagBits::eColorAttachmentOutput },
      cmdSolidBuffer,
      { fbo->semaphore }// signal
    } );

    _graphicsQueue->submit( SubmitInfo{
      { fbo->semaphore },
      { vk::PipelineStageFlagBits::eColorAttachmentOutput },
      fbo->commandBuffer,
      _renderComplete
    } );
  }
  void keyEvent( int key, int scancode, int action, int mods )
  {
    switch ( key )
    {
    case GLFW_KEY_E:
      uboFS.enable_hdr = true;
      break;
    case GLFW_KEY_R:
      uboFS.enable_hdr = false;
      break;
    case GLFW_KEY_ESCAPE:
      getWindow( )->close( );
      break;
    default:
      break;
    }
  }
};

void glfwErrorCallback( int error, const char* description )
{
  fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

int main( int argc, char** argv )
{
  try
  {
    VulkanApp* app = new MyApp( "Postprocess (HDR)", 800, 600 );

    app->getWindow( )->setErrorCallback( glfwErrorCallback );

    while ( app->isRunning( ) )
    {
      // app->waitEvents( );
      app->paint( );
    }

    delete app;
  }
  catch ( std::system_error err )
  {
    std::cout << "System Error: " << err.what( ) << std::endl;
  }
  //system( "PAUSE" );
  return 0;
}