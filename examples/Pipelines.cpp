#include <lava/lava.h>
using namespace lava;

#include <routes.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <routes.h>


class MyApp : public VulkanApp
{
  // Same uniform buffer layout as shader
  struct UBOVS
  {
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model;
    glm::vec3 lightPos = glm::vec3(0.0f, 2.0f, 1.0f);
  } uboVS;

  struct {
    std::shared_ptr< Pipeline > phong;
    std::shared_ptr< Pipeline > wireframe;
    std::shared_ptr< Pipeline > toon;
  } pipelines;

public:
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<PipelineLayout> _pipelineLayout;
  std::shared_ptr<DescriptorSet> _descriptorSet;

  //uint32_t numIndices;

  std::shared_ptr<lava::extras::Geometry> geometry;

  MyApp( char const* title, uint32_t width, uint32_t height )
    : VulkanApp( title, width, height )
  {
    geometry = std::make_shared<lava::extras::Geometry>( _device, 
      LAVA_EXAMPLES_RESOURCES_ROUTE + std::string( "/monkey.obj_" ) );

    // MVP buffer
    {
      uint32_t mvpBufferSize = sizeof( UBOVS );
      _uniformBufferMVP = _device->createBuffer( mvpBufferSize, 
        vk::BufferUsageFlagBits::eUniformBuffer, 
        vk::SharingMode::eExclusive, nullptr,
        vk::MemoryPropertyFlagBits::eHostVisible | 
          vk::MemoryPropertyFlagBits::eHostCoherent );
    }

    // Init descriptor and pipeline layouts
    std::vector<DescriptorSetLayoutBinding> dslbs;
    DescriptorSetLayoutBinding mvpDescriptor( 0, vk::DescriptorType::eUniformBuffer, 
      vk::ShaderStageFlagBits::eVertex );
    dslbs.push_back( mvpDescriptor );
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = _device->createDescriptorSetLayout( dslbs );

    _pipelineLayout = _device->createPipelineLayout( descriptorSetLayout, nullptr );

    std::array<vk::DescriptorPoolSize, 1> poolSize;
    poolSize[ 0 ] = vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, 1 );
    std::shared_ptr<DescriptorPool> descriptorPool = _device->createDescriptorPool( {}, 1, poolSize );

    // Init descriptor set
    _descriptorSet = _device->allocateDescriptorSet( descriptorPool, descriptorSetLayout );
    std::vector<WriteDescriptorSet> wdss;
    DescriptorBufferInfo buffInfo( _uniformBufferMVP, 0, sizeof( glm::mat4 ) );
    WriteDescriptorSet w( _descriptorSet, 0, 0, 
      vk::DescriptorType::eUniformBuffer, 1, nullptr, buffInfo );
    wdss.push_back( w );
    _device->updateDescriptorSets( wdss, {} );

    // init shaders
    std::shared_ptr<ShaderModule> vertexShaderModule = _device->createShaderModule( 
      LAVA_EXAMPLES_RESOURCES_ROUTE + std::string( "/mesh_vert.spv" ), vk::ShaderStageFlagBits::eVertex );
    std::shared_ptr<ShaderModule> fragmentShaderModule = _device->createShaderModule( 
      LAVA_EXAMPLES_RESOURCES_ROUTE + std::string( "/mesh_frag.spv" ), vk::ShaderStageFlagBits::eFragment );

    // init pipeline
    std::shared_ptr<PipelineCache> pipelineCache = _device->createPipelineCache( 0, nullptr );
    PipelineShaderStageCreateInfo vertexStage( vk::ShaderStageFlagBits::eVertex, vertexShaderModule );
    PipelineShaderStageCreateInfo fragmentStage( vk::ShaderStageFlagBits::eFragment, fragmentShaderModule );

    PipelineVertexInputStateCreateInfo vertexInput( 
      vk::VertexInputBindingDescription( 0, sizeof( lava::extras::Vertex ), 
        vk::VertexInputRate::eVertex ),
      {
        vk::VertexInputAttributeDescription( 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(lava::extras::Vertex, position ) ),
        vk::VertexInputAttributeDescription( 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(lava::extras::Vertex, normal ) )/*,
        vk::VertexInputAttributeDescription( 2, 0, vk::Format::eR32G32Sfloat, offsetof(lava::extras::Vertex, texCoord ) ) */
      }
    );
    vk::PipelineInputAssemblyStateCreateInfo assembly( {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE );
    PipelineViewportStateCreateInfo viewport( { {} }, { {} } );   // one dummy viewport and scissor, as dynamic state sets them
    vk::PipelineRasterizationStateCreateInfo rasterization( {}, true, 
      false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, 
      vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f );
    PipelineMultisampleStateCreateInfo multisample( vk::SampleCountFlagBits::e1, false, 0.0f, nullptr, false, false );
    vk::StencilOpState stencilOpState( vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 0, 0 );
    vk::PipelineDepthStencilStateCreateInfo depthStencil( {}, true, true, vk::CompareOp::eLessOrEqual, false, false, stencilOpState, stencilOpState, 0.0f, 0.0f );
    vk::PipelineColorBlendAttachmentState colorBlendAttachment( false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA );
    PipelineColorBlendStateCreateInfo colorBlend( false, vk::LogicOp::eNoOp, colorBlendAttachment, { 1.0f, 1.0f, 1.0f, 1.0f } );
    PipelineDynamicStateCreateInfo dynamic( { vk::DynamicState::eViewport, vk::DynamicState::eScissor } );


    _pipeline = _device->createGraphicsPipeline( pipelineCache, {}, { vertexStage, fragmentStage }, vertexInput, assembly, nullptr, viewport, rasterization, multisample, depthStencil, colorBlend, dynamic,
      _pipelineLayout, _renderPass );
  }
  void updateUniformBuffers( void )
  {
    uint32_t width = _window->getWidth( );
    uint32_t height = _window->getHeight( );

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

    UniformBufferObject ubo = {};
    ubo.model = glm::scale( glm::mat4(1.0f), glm::vec3(10.0f));
    ubo.model = glm::rotate(ubo.model, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), width / (float) height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    vk::Device device = static_cast<vk::Device>(*_device);

    uint32_t mvpBufferSize = sizeof(UniformBufferObject);
    void* data = _uniformBufferMVP->map( 0, mvpBufferSize );
    memcpy( data, &ubo, sizeof(ubo) );
    _uniformBufferMVP->unmap( );

    //std::cout<<glm::to_string(mvpc)<<std::endl;
  }

  void doPaint( void ) override
  {
    updateUniformBuffers( );

    float width = ( float ) _defaultFramebuffer->getExtent( ).width;
    float height = ( float ) _defaultFramebuffer->getExtent( ).height;

    // create a command pool for command buffer allocation
    std::shared_ptr<CommandPool> commandPool = 
      _device->createCommandPool( 
          vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex );
    std::shared_ptr<CommandBuffer> commandBuffer = commandPool->allocateCommandBuffer( );

    commandBuffer->begin( );
    std::array<float, 4> ccv = { 0.2f, 0.3f, 0.3f, 1.0f };
    commandBuffer->beginRenderPass( _renderPass, 
      _defaultFramebuffer->getFramebuffer( ), 
      vk::Rect2D( { 0, 0 }, 
      _defaultFramebuffer->getExtent( ) ),
      { vk::ClearValue( ccv ), vk::ClearValue( 
        vk::ClearDepthStencilValue( 1.0f, 0 ) )
      }, vk::SubpassContents::eInline );

    vk::Viewport viewport( 0.0f, 0.0f, width, height, 0.0f, 1.0f );
    vk::Rect2D scissor ( { 0, 0 }, _defaultFramebuffer->getExtent( ) );

    commandBuffer->setViewport( 0, viewport );
    commandBuffer->setScissor( 0, scissors );
    commandBuffer->bindDescriptorSets( vk::PipelineBindPoint::eGraphics,
      _pipelineLayout, 0, { _descriptorSet }, nullptr );

    // Left: Solid colored
    viewport.width = width / 3.0f;
    commandBuffer->bindGraphicsPipeline( pipelines.phong );
    geometry->render( commandBuffer );

    // Center: Toon
    viewport.x = width / 3.0f;
    commandBuffer->bindGraphicsPipeline( pipelines.toon );
    geometry->render( commandBuffer );

    // Right: Wireframe
    viewport.x = width / 3.0f + width / 3.0f;
    commandBuffer->bindGraphicsPipeline( pipelines.wireframe );
    geometry->render( commandBuffer );


    commandBuffer->endRenderPass( );

    commandBuffer->end( );

    _graphicsQueue->submit( SubmitInfo{
      { _defaultFramebuffer->getPresentSemaphore( ) },
      { vk::PipelineStageFlagBits::eColorAttachmentOutput },
      commandBuffer,
      _renderComplete
    } );
  }
  void keyEvent( int key, int scancode, int action, int mods )
  {
    switch ( key )
    {
    case GLFW_KEY_ESCAPE:
      switch ( action )
      {
      case GLFW_PRESS:
        glfwSetWindowShouldClose( getWindow( )->getWindow( ), GLFW_TRUE );
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

void glfwErrorCallback( int error, const char* description )
{
  fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

int main( void )
{
  try
  {
    //if (glfwInit())
    //{
    VulkanApp* app = new MyApp( "Cube Mesh", 800, 600 );

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