/*
 For more information, please see: http://software.sci.utah.edu
 
 The MIT License
 
 Copyright (c) 2009 Scientific Computing and Imaging Institute,
 University of Utah.
 
 
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 */

#include <Core/Interface/Interface.h>

#include <Interface/Application/ControllerContext.h>

namespace Seg3D
{

ControllerContext::ControllerContext( ControllerInterface* controller ) :
  controller_( controller )
{
}

ControllerContext::~ControllerContext()
{
}

void ControllerContext::report_error( const std::string& error )
{
  ControllerInterface::PostActionMessage( controller_, error );
}

void ControllerContext::report_warning( const std::string& warning )
{
  ControllerInterface::PostActionMessage( controller_, warning );
}

void ControllerContext::report_message( const std::string& message )
{
  ControllerInterface::PostActionMessage( controller_, message );
}

void ControllerContext::report_need_resource( const Core::NotifierHandle& notifier )
{
  std::string message = std::string( "'" ) + notifier->get_name() + std::string(
      "' is currently unavailable" );
  ControllerInterface::PostActionMessage( controller_, message );
}

void ControllerContext::report_done()
{
  if ( is_success() ) ControllerInterface::PostActionMessage( controller_, "" );
}

Core::ActionSource ControllerContext::source() const
{
  return Core::ActionSource::COMMANDLINE_E;
}

} //end namespace Seg3D