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

#include "Geometry.h"
#include "ModelImporter.h"

#include "../Device.h"

namespace lava
{
  namespace extras
  {
    Transform::Transform( void )
    {
      SetAsIdentity();
    }

    Transform::Transform( const glm::vec3& position_, const glm::quat& rotation_, 
      const glm::vec3& scale_ ) 
    : position(position_)
      , rotation(rotation_)
      , scale(scale_)
    {
    }

    Transform::Transform( const glm::vec3& position_, 
      const glm::quat& rotation_) 
    : position(position_)
      , rotation(rotation_)
      , scale(glm::vec3(1.0f))
    {
    }

    Transform::Transform(const glm::vec3& position_) 
    : position(position_)
      , rotation(glm::quat(glm::vec3(0.0f)))
      , scale(glm::vec3(1.0f))
    {
    }

    Transform::~Transform( void )
    {
    }

    void Transform::Translate(glm::vec3 deltaPosition)
    {
      position += deltaPosition;
    }

    void Transform::Rotate(glm::quat deltaRotation)
    {
      rotation *= deltaRotation;
    }

    void Transform::Rotate(glm::vec3 deltaEulerRotationRad)
    {
      glm::quat rotationQuat(deltaEulerRotationRad);
      rotation *= rotationQuat;
    }

    void Transform::Scale(glm::vec3 deltaScale)
    {
      scale *= deltaScale;
    }

    void Transform::SetAsIdentity( void )
    {
      position = glm::vec3(0.0f);
      rotation = glm::quat(glm::vec3(0.0f));
      scale = glm::vec3(1.0f);
    }

    glm::mat4 Transform::GetModelMatrix( void )
    {
      glm::mat4 matTrans = glm::translate(glm::mat4(1.0f), position);
      glm::mat4 matRot = glm::mat4(rotation);
      glm::mat4 matScale = glm::scale(glm::mat4(1.0f), scale);
      glm::mat4 matModel = matTrans * matRot * matScale;

      return matModel;
    }

    Transform Transform::Identity( void )
    {
      Transform result;

      result.SetAsIdentity();

      return result;
    }
  }
}