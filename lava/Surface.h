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

#ifndef __LAVA_SURFACE__
#define __LAVA_SURFACE__

#include "includes.hpp"
#include "noncopyable.hpp"

namespace lava
{
  class Instance;
  class Surface : private NonCopyable<Surface>
  {
  public:
    Surface( const std::shared_ptr< Instance >& instance, 
      const vk::SurfaceKHR& surface );
    Surface( const Surface& ) = delete;

    Surface& operator=( const Surface& ) = delete;
    ~Surface( void );

    inline operator vk::SurfaceKHR( void ) const
    {
      return _surface;
    }
  private:
    std::shared_ptr< Instance > _instance;
    vk::SurfaceKHR _surface;
  };
}

#endif /* __LAVA_SURFACE__ */