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

#ifndef __LAVA_ENGINE_CAMERA__
#define __LAVA_ENGINE_CAMERA__

#include "Node.h"
#include <lavaEngine/Mathematics/Frustum.h>
#include <lavaEngine/Mathematics/Ray.h>

namespace lava
{
	namespace engine
	{
    typedef glm::vec4 Viewport;
		class Camera: public Node
		{
    public:
      static bool findCameras;
    public:
      LAVAENGINE_API
      static Camera* getMainCamera( void )
      {
        return _mainCamera;
      }
      LAVAENGINE_API
      static void setMainCamera( Camera* camera )
      {
        _mainCamera = camera;
      }
    private:
      // TODO shared_ptr??
      static Camera* _mainCamera;
    public:
      LAVAENGINE_API
      explicit Camera( void );
      LAVAENGINE_API
      Camera( const float fov, const float ar, const float n, const float f );
      LAVAENGINE_API
      virtual ~Camera( void );
    public:
      LAVAENGINE_API
      virtual void setEnabled( bool enabled ) 
      {
        Node::setEnabled( enabled );
        Camera::findCameras = true;
      }
      LAVAENGINE_API
      virtual void accept( Visitor& v ) override;
    public:
      LAVAENGINE_API
      bool isMainCamera( void ) const
      {
        return _isMainCamera;
      }
      LAVAENGINE_API
      void setIsMainCamera( bool v )
      {
        _isMainCamera = v;
      }
    protected:
      bool _isMainCamera = false;
    public:
      LAVAENGINE_API
      const glm::mat4& getProjection( void ) const;
      LAVAENGINE_API
      void setProjection( const glm::mat4& proj );
      LAVAENGINE_API
      const glm::mat4& getOrtographic( void );
      LAVAENGINE_API
      void setOrtographic( const glm::mat4 ortho );
      LAVAENGINE_API
      const glm::mat4& getView( void );
      LAVAENGINE_API
      void setView( const glm::mat4 view );
    protected:
      glm::mat4 _projectionMatrix;
      glm::mat4 _orthographicMatrix;
      glm::mat4 _viewMatrix;
    public:
      LAVAENGINE_API
      const Frustum& getFrustum( void ) const
      {
        return _frustum;
      }
      LAVAENGINE_API
      void setFrustum( const Frustum& frustum );
    protected:
      Frustum _frustum;

    public:
      LAVAENGINE_API
      const glm::vec4& getClearColor( void ) const
      {
        return _clearColor;
      }
      LAVAENGINE_API
      void setClearColor( const glm::vec4& color )
      {
        _clearColor = color;
      }
    protected:
      glm::vec4 _clearColor;  // TODO: Use color
    public:
      LAVAENGINE_API
      const Viewport& getViewport( void ) const
      {
        return _viewport;
      }
      LAVAENGINE_API
      void setViewport( const Viewport& v )
      {
        this->_viewport = v;
      }
    protected:
      Viewport _viewport;
    public:
      Ray getRay( float px, float py ) /* TODO: const*/
      {
        float x = 2.0f * px - 1.0f;
        float y = 1.0 - 2.0f * py;

        glm::vec4 rayClip( x, y, -1.0f, 1.0f );

        glm::vec4 rayEye = glm::transpose( glm::inverse( getProjection( ) ) ) 
          * rayClip;
        rayEye = glm::vec4( rayEye.x, rayEye.y, -1.0f, 0.0f );
        
        glm::vec3 rayDir = glm::normalize( 
          glm::vec3( glm::transpose( getTransform( ) ) * rayEye ) );

        return Ray( getAbsolutePosition( ), rayDir );
      }
      void computeCullingPlanes( void );
    private:
      bool _cullingEnabled = true;
      /*struct Plane
      {
        glm::vec3 normal;
        float distance;
        bool forceNormalize;
        Plane( glm::vec3 n, float d, bool f = true )
        {
          normal = n;
          distance = d;
          forceNormalize = f;
        }
      };
      std::array< Plane, 6 > _cullingPlanes;*/
		};
	}
}

#endif /* __LAVA_ENGINE_CAMERA__ */