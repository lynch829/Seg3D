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

// STL includes

// Boost includes 

// Application includes
#include <Application/LayerManager/LayerManager.h>
#include <Application/Viewer/Viewer.h> 
#include <Application/ViewerManager/ViewerManager.h>
#include <Application/PreferencesManager/PreferencesManager.h>
#include <Application/Session/Session.h>

// Core includes
#include <Core/Utils/ScopedCounter.h>
#include <Core/Interface/Interface.h>
#include <Core/State/StateIO.h>

namespace Seg3D
{

//////////////////////////////////////////////////////////////////////////
// Class ViewerManagerPrivate
//////////////////////////////////////////////////////////////////////////

class ViewerManagerPrivate
{
public:
	void viewer_mode_changed( size_t viewer_id );
	void viewer_visibility_changed( size_t viewer_id );
	void viewer_became_picking_target( size_t viewer_id );
	void viewer_lock_state_changed( size_t viewer_id );
	void update_picking_targets();
	void handle_layer_volume_changed( LayerHandle layer );
	void change_layout( std::string layout );

public:
	ViewerManager* vm_;

	std::vector< ViewerHandle > viewers_;
	std::vector< size_t > locked_viewers_[ 4 ];
	size_t signal_block_count_;
};

void ViewerManagerPrivate::viewer_mode_changed( size_t viewer_id )
{
	Core::ScopedCounter signal_block_counter( this->signal_block_count_ );

	if ( this->vm_->active_axial_viewer_->get() == static_cast< int >( viewer_id ) )
	{
		this->vm_->active_axial_viewer_->set( -1 );
	}
	else if ( this->vm_->active_coronal_viewer_->get() == static_cast< int >( viewer_id ) )
	{
		this->vm_->active_coronal_viewer_->set( -1 );
	}
	else if ( this->vm_->active_sagittal_viewer_->get() == static_cast< int >( viewer_id ) )
	{
		this->vm_->active_sagittal_viewer_->set( -1 );
	}

	this->viewers_[ viewer_id ]->is_picking_target_state_->set( false );
	this->update_picking_targets();
}

void ViewerManagerPrivate::viewer_visibility_changed( size_t viewer_id )
{
	Core::ScopedCounter signal_block_counter( this->signal_block_count_ );

	if ( !this->viewers_[ viewer_id ]->viewer_visible_state_->get() )
	{
		if ( this->vm_->active_axial_viewer_->get() == static_cast< int >( viewer_id ) )
		{
			this->vm_->active_axial_viewer_->set( -1 );
		}
		else if ( this->vm_->active_coronal_viewer_->get() == static_cast< int >( viewer_id ) )
		{
			this->vm_->active_coronal_viewer_->set( -1 );
		}
		else if ( this->vm_->active_sagittal_viewer_->get() == static_cast< int >( viewer_id ) )
		{
			this->vm_->active_sagittal_viewer_->set( -1 );
		}

		this->viewers_[ viewer_id ]->is_picking_target_state_->set( false );
	}

	this->update_picking_targets();
}

void ViewerManagerPrivate::viewer_became_picking_target( size_t viewer_id )
{
	if ( this->signal_block_count_ > 0 )
	{
		return;
	}

	ViewerHandle viewer = this->viewers_[ viewer_id ];
	if ( !viewer->is_picking_target_state_->get() )
	{
		return;
	}

	{
		Core::ScopedCounter signal_block_counter( this->signal_block_count_ );

		if ( viewer->view_mode_state_->get() == Viewer::AXIAL_C )
		{
			if ( this->vm_->active_axial_viewer_->get() != static_cast< int >( viewer_id ) )
			{
				if ( this->vm_->active_axial_viewer_->get() >= 0 )
				{
					this->viewers_[ this->vm_->active_axial_viewer_->get() ]->is_picking_target_state_->set( false );
				}
				this->vm_->active_axial_viewer_->set( static_cast< int >( viewer_id ) );
			}
		}
		else if ( viewer->view_mode_state_->get() == Viewer::CORONAL_C )
		{
			if ( this->vm_->active_coronal_viewer_->get() != static_cast< int >( viewer_id ) )
			{
				if ( this->vm_->active_coronal_viewer_->get() >= 0 )
				{
					this->viewers_[ this->vm_->active_coronal_viewer_->get() ]->is_picking_target_state_->set( false );
				}
				this->vm_->active_coronal_viewer_->set( static_cast< int >( viewer_id ) );
			}
		}
		else if ( viewer->view_mode_state_->get() == Viewer::SAGITTAL_C )
		{
			if ( this->vm_->active_sagittal_viewer_->get() != static_cast< int >( viewer_id ) )
			{
				if ( this->vm_->active_sagittal_viewer_->get() >= 0 )
				{
					this->viewers_[ this->vm_->active_sagittal_viewer_->get() ]->is_picking_target_state_->set( false );
				}
				this->vm_->active_sagittal_viewer_->set( static_cast< int >( viewer_id ) );
			}
		}
		else
		{
			//assert( false );
		}
	}

	this->vm_->picking_target_changed_signal_( viewer_id );
}

void ViewerManagerPrivate::viewer_lock_state_changed( size_t viewer_id )
{
	ViewerHandle viewer = this->viewers_[ viewer_id ];
	bool locked = viewer->lock_state_->get();
	if ( locked )
	{
		this->locked_viewers_[ viewer->view_mode_state_->index() ].push_back( viewer_id );
	}
	else
	{
		// We need to go over the locked viewer list for all modes because a viewer can become
		// unlocked due to change of mode.
		bool found = false;
		for ( int i = 0; i < 4; i++ )
		{
			for ( size_t j = 0; j < this->locked_viewers_[ i ].size(); j++ )
			{
				if ( this->locked_viewers_[ i ][ j ] == viewer_id )
				{
					this->locked_viewers_[ i ].erase( this->locked_viewers_[ i ].begin() + j );
					found =  true;
					break;
				}
			}
			if ( found )
			{
				break;
			}
		}
		assert( found );
	}
}

void ViewerManagerPrivate::update_picking_targets()
{
	for ( size_t i = 0; i < 6; i++ )
	{
		ViewerHandle viewer = this->viewers_[ i ];
		if ( !viewer->viewer_visible_state_->get() )
		{
			continue;
		}

		if ( viewer->view_mode_state_->get() == Viewer::AXIAL_C )
		{
			if ( this->vm_->active_axial_viewer_->get() < 0 )
			{
				this->vm_->active_axial_viewer_->set( static_cast< int >( i ) );
				viewer->is_picking_target_state_->set( true );
			}
		}
		else if ( viewer->view_mode_state_->get() == Viewer::CORONAL_C )
		{
			if ( this->vm_->active_coronal_viewer_->get() < 0 )
			{
				this->vm_->active_coronal_viewer_->set( static_cast< int >( i ) );
				viewer->is_picking_target_state_->set( true );
			}
		}
		else if ( viewer->view_mode_state_->get() == Viewer::SAGITTAL_C )
		{
			if ( this->vm_->active_sagittal_viewer_->get() < 0 )
			{
				this->vm_->active_sagittal_viewer_->set( static_cast< int >( i ) );
				viewer->is_picking_target_state_->set( true );
			}
		}
	}
}

void ViewerManagerPrivate::handle_layer_volume_changed( LayerHandle layer )
{
	for ( size_t i = 0; i < this->viewers_.size(); ++i )
	{
		this->viewers_[ i ]->update_slice_volume( layer );
	}

	// NOTE: Redraw should only happen after all the viewers have been updated,
	// otherwise when a volume viewer tries to render slices from some 2D viewer
	// that hasn't received the new volume yet, the rendering would be inconsistent.
	for ( size_t i = 0; i < this->viewers_.size(); ++i )
	{
		this->viewers_[ i ]->redraw_scene();
	}
}

void ViewerManagerPrivate::change_layout( std::string layout )
{
	if( layout == ViewerManager::SINGLE_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( false );
		this->viewers_[ 2 ]->viewer_visible_state_->set( false );
		this->viewers_[ 3 ]->viewer_visible_state_->set( false );
		this->viewers_[ 4 ]->viewer_visible_state_->set( false );
		this->viewers_[ 5 ]->viewer_visible_state_->set( false );
	}
	else if( layout == ViewerManager::_1AND1_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( false );
		this->viewers_[ 2 ]->viewer_visible_state_->set( false );
		this->viewers_[ 3 ]->viewer_visible_state_->set( true );
		this->viewers_[ 4 ]->viewer_visible_state_->set( false );
		this->viewers_[ 5 ]->viewer_visible_state_->set( false );
	}
	else if( layout == ViewerManager::_1AND2_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( false );
		this->viewers_[ 2 ]->viewer_visible_state_->set( false );
		this->viewers_[ 3 ]->viewer_visible_state_->set( true );
		this->viewers_[ 4 ]->viewer_visible_state_->set( true );
		this->viewers_[ 5 ]->viewer_visible_state_->set( false );
	}
	else if( layout == ViewerManager::_1AND3_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( false );
		this->viewers_[ 2 ]->viewer_visible_state_->set( false );
		this->viewers_[ 3 ]->viewer_visible_state_->set( true );
		this->viewers_[ 4 ]->viewer_visible_state_->set( true );
		this->viewers_[ 5 ]->viewer_visible_state_->set( true );
	}
	else if( layout == ViewerManager::_2AND2_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( true );
		this->viewers_[ 2 ]->viewer_visible_state_->set( false );
		this->viewers_[ 3 ]->viewer_visible_state_->set( true );
		this->viewers_[ 4 ]->viewer_visible_state_->set( true );
		this->viewers_[ 5 ]->viewer_visible_state_->set( false );
	}
	else if( layout == ViewerManager::_2AND3_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( true );
		this->viewers_[ 2 ]->viewer_visible_state_->set( false );
		this->viewers_[ 3 ]->viewer_visible_state_->set( true );
		this->viewers_[ 4 ]->viewer_visible_state_->set( true );
		this->viewers_[ 5 ]->viewer_visible_state_->set( true );
	}
	else if( layout == ViewerManager::_3AND3_C )
	{
		this->viewers_[ 0 ]->viewer_visible_state_->set( true );
		this->viewers_[ 1 ]->viewer_visible_state_->set( true );
		this->viewers_[ 2 ]->viewer_visible_state_->set( true );
		this->viewers_[ 3 ]->viewer_visible_state_->set( true );
		this->viewers_[ 4 ]->viewer_visible_state_->set( true );
		this->viewers_[ 5 ]->viewer_visible_state_->set( true );
	}
	else
	{
		assert( false );
	}
}

//////////////////////////////////////////////////////////////////////////
// Class ViewerManager
//////////////////////////////////////////////////////////////////////////

CORE_SINGLETON_IMPLEMENTATION( ViewerManager );

const std::string ViewerManager::SINGLE_C( "single" );
const std::string ViewerManager::_1AND1_C( "1and1" );
const std::string ViewerManager::_1AND2_C( "1and2" );
const std::string ViewerManager::_1AND3_C( "1and3" );
const std::string ViewerManager::_2AND2_C( "2and2" );
const std::string ViewerManager::_2AND3_C( "2and3" );
const std::string ViewerManager::_3AND3_C( "3and3" );

ViewerManager::ViewerManager() :
	StateHandler( "view", false ),
	private_( new ViewerManagerPrivate )
{
	this->private_->signal_block_count_ = 0;
	this->private_->vm_ = this;

	// Step (1)
	// Allow states to be set from outside of the application thread
	this->set_initializing( true );

	// Set the default state of this element
	this->add_state( "layout", this->layout_state_, PreferencesManager::Instance()->
		default_viewer_mode_state_->export_to_string(),	PreferencesManager::Instance()->
		default_viewer_mode_state_->export_list_to_string() );
	this->layout_state_->set_session_priority( Core::StateBase::DEFAULT_LOAD_E + 1 );

	this->add_state( "active_viewer", this->active_viewer_state_, 0 );

	// No viewer will be the active viewer for picking
	// NOTE: The interface will set this up
	this->add_state( "active_axial_viewer", active_axial_viewer_, -1 );
	this->add_state( "active_coronal_viewer", active_coronal_viewer_, -1 );
	this->add_state( "active_sagittal_viewer", active_sagittal_viewer_, -1 );

	this->add_connection( this->layout_state_->value_changed_signal_.connect( boost::bind( 
		&ViewerManagerPrivate::change_layout, this->private_, _1 ) ) );

	// Step (2)
	// Create the viewers that are part of the application
	// Currently a maximum of 6 viewers can be created
	this->private_->viewers_.resize( 6 );
	
	this->private_->viewers_[ 0 ] = ViewerHandle( new Viewer( 0, true,  Viewer::VOLUME_C ) );
	this->private_->viewers_[ 1 ] = ViewerHandle( new Viewer( 1, false, Viewer::AXIAL_C ) );
	this->private_->viewers_[ 2 ] = ViewerHandle( new Viewer( 2, false, Viewer::AXIAL_C ) );
	this->private_->viewers_[ 3 ] = ViewerHandle( new Viewer( 3, true, Viewer::AXIAL_C ) );
	this->private_->viewers_[ 4 ] = ViewerHandle( new Viewer( 4, true, Viewer::SAGITTAL_C ) );
	this->private_->viewers_[ 5 ] = ViewerHandle( new Viewer( 5, true, Viewer::CORONAL_C ) );

	for ( size_t j = 0; j < this->private_->viewers_.size(); j++ )
	{
		this->private_->viewers_[ j ]->set_initializing( true );
	}
	this->private_->change_layout( this->layout_state_->get() );

	for ( size_t j = 0; j < this->private_->viewers_.size(); j++ )
	{
		this->private_->viewers_[ j ]->set_initializing( false );

		// NOTE: ViewerManager needs to process these signals first
		this->add_connection( this->private_->viewers_[ j ]->view_mode_state_->value_changed_signal_.
			connect( boost::bind( &ViewerManagerPrivate::viewer_mode_changed, this->private_, j ), 
			boost::signals2::at_front ) );
		this->add_connection( this->private_->viewers_[ j ]->viewer_visible_state_->value_changed_signal_.
			connect( boost::bind( &ViewerManagerPrivate::viewer_visibility_changed, this->private_, j ), 
			boost::signals2::at_front ) );
		this->add_connection( this->private_->viewers_[ j ]->is_picking_target_state_->value_changed_signal_.
			connect( boost::bind( &ViewerManagerPrivate::viewer_became_picking_target, this->private_, j ), 
			boost::signals2::at_front ) );
		this->add_connection( this->private_->viewers_[ j ]->lock_state_->value_changed_signal_.
			connect( boost::bind( &ViewerManagerPrivate::viewer_lock_state_changed, this->private_, j ), 
			boost::signals2::at_front ) );
			
		// NOTE: For these signals order does not matter	
		this->add_connection( this->private_->viewers_[ j ]->slice_visible_state_->state_changed_signal_.
			connect( boost::bind( &ViewerManager::update_volume_viewers, this ) ) );
	}
	
	// NOTE: ViewerManager needs to process these signals last	
	this->add_connection( LayerManager::Instance()->layer_inserted_signal_.connect(
		boost::bind( &ViewerManager::update_volume_viewers, this ) ) );

	this->add_connection( LayerManager::Instance()->layer_volume_changed_signal_.connect(
		boost::bind( &ViewerManagerPrivate::handle_layer_volume_changed, this->private_, _1 ) ) );

	this->set_initializing( false );
}

ViewerManager::~ViewerManager()
{
	this->disconnect_all();
}

ViewerHandle ViewerManager::get_viewer( size_t idx )
{
	ViewerHandle handle;
	if ( idx < this->private_->viewers_.size() ) handle = this->private_->viewers_[ idx ];
	return handle;
}

ViewerHandle ViewerManager::get_viewer( const std::string viewer_name )
{
	ViewerHandle handle;
	for ( size_t i = 0; i < this->private_->viewers_.size(); i++ )
	{
		if ( this->private_->viewers_[ i ]->get_statehandler_id() == viewer_name )
		{
			handle = this->private_->viewers_[ i ];
			break;
		}
	}
	return handle;
}

void ViewerManager::get_2d_viewers_info( ViewerInfoList viewers[ 3 ] )
{
	Core::StateEngine::lock_type lock( Core::StateEngine::GetMutex() );
	if ( !LayerManager::Instance()->get_active_layer() )
	{
		return;
	}
	for ( size_t i = 0; i < 6; i++ )
	{
		ViewerHandle viewer = this->private_->viewers_[ i ];
		if ( viewer->viewer_visible_state_->get() && !viewer->is_volume_view() )
		{
			Core::StateView2D* view2d = static_cast< Core::StateView2D* >( 
				viewer->get_active_view_state().get() );
			ViewerInfoHandle viewer_info( new ViewerInfo );
			viewer_info->viewer_id_ = i;
			viewer_info->view_mode_ = viewer->view_mode_state_->index();
			viewer_info->depth_ = view2d->get().center().z();
			viewer_info->is_picking_target_ = viewer->is_picking_target_state_->get();
			viewers[ viewer_info->view_mode_ ].push_back( viewer_info );
		}
	}
}

void ViewerManager::update_volume_viewers()
{
	if ( !Core::Application::IsApplicationThread() )
	{
		Core::Application::PostEvent( boost::bind( 
			&ViewerManager::update_volume_viewers, this ) );
		return;
	}

	for ( size_t i = 0; i < 6; i++ )
	{
		ViewerHandle viewer = this->private_->viewers_[ i ];
		if ( !viewer->viewer_visible_state_->get() )
		{
			continue;
		}

		if ( viewer->view_mode_state_->get() == Viewer::VOLUME_C )
		{
			viewer->redraw_all();
		}
	}
}

void ViewerManager::pick_point( size_t source_viewer, const Core::Point& pt )
{
	ViewerHandle src_viewer = this->private_->viewers_[ source_viewer ];
	if ( this->active_axial_viewer_->get() >= 0 && 
		this->active_axial_viewer_->get() != static_cast< int >( source_viewer ) )
	{
		ViewerHandle viewer = this->private_->viewers_[ this->active_axial_viewer_->get() ];
		if ( viewer->view_mode_state_->get() != src_viewer->view_mode_state_->get() )
		{
			viewer->move_slice_to( pt );
		}
	}
	if ( this->active_coronal_viewer_->get() >= 0 && 
		this->active_coronal_viewer_->get() != static_cast< int >( source_viewer ) )
	{
		ViewerHandle viewer = this->private_->viewers_[ this->active_coronal_viewer_->get() ];
		if ( viewer->view_mode_state_->get() != src_viewer->view_mode_state_->get() )
		{
			viewer->move_slice_to( pt );
		}
	}
	if ( this->active_sagittal_viewer_->get() >= 0 && 
		this->active_sagittal_viewer_->get() != static_cast< int >( source_viewer ) )
	{
		ViewerHandle viewer = this->private_->viewers_[ this->active_sagittal_viewer_->get() ];
		if ( viewer->view_mode_state_->get() != src_viewer->view_mode_state_->get() )
		{
			viewer->move_slice_to( pt );
		}
	}
}

std::vector< size_t > ViewerManager::get_locked_viewers( int mode_index )
{
	return this->private_->locked_viewers_[ mode_index ];
}

bool ViewerManager::post_save_states( Core::StateIO& state_io )
{
	for ( size_t i = 0; i < this->number_of_viewers(); i++ )
	{
		this->private_->viewers_[ i ]->save_states( state_io );
	}
	
	return true;
}

bool ViewerManager::post_load_states( const Core::StateIO& state_io )
{
	// Block signals
	Core::ScopedCounter signal_block_counter( this->private_->signal_block_count_ );
	// Clear all picking targets
	for ( size_t i = 0; i < this->number_of_viewers(); i++ )
	{
		this->private_->viewers_[ i ]->is_picking_target_state_->set( false );
	}
	
	// Restore picking targets for each view mode.
	if ( this->active_axial_viewer_->get() >= 0 )
	{
		assert ( this->private_->viewers_[ this->active_axial_viewer_->get() ]->
			view_mode_state_->get() == Viewer::AXIAL_C );
		this->private_->viewers_[ this->active_axial_viewer_->get() ]->
			is_picking_target_state_->set( true ); 
	}
	if ( this->active_coronal_viewer_->get() >= 0 )
	{
		assert ( this->private_->viewers_[ this->active_coronal_viewer_->get() ]->
			view_mode_state_->get() == Viewer::CORONAL_C );
		this->private_->viewers_[ this->active_coronal_viewer_->get() ]->
			is_picking_target_state_->set( true ); 
	}
	if ( this->active_sagittal_viewer_->get() >= 0 )
	{
		assert ( this->private_->viewers_[ this->active_sagittal_viewer_->get() ]->
			view_mode_state_->get() == Viewer::SAGITTAL_C );
		this->private_->viewers_[ this->active_sagittal_viewer_->get() ]->
			is_picking_target_state_->set( true ); 
	}
	
	return true;
}

int ViewerManager::get_session_priority()
{
	return SessionPriority::VIEWER_MANAGER_PRIORITY_E;
}

bool ViewerManager::pre_load_states( const Core::StateIO& state_io )
{
	// Load states of all the viewers before loading ViewerManager states.
	// NOTE: The reason for doing this is that some of the ViewerManager states are affected
	// by certain states of viewers, and we don't want the loaded states to be overwritten.

	for ( size_t i = 0; i < this->number_of_viewers(); i++ )
	{
		this->private_->viewers_[ i ]->load_states( state_io );
	}

	return true;
}

void ViewerManager::update_viewers_overlay( const std::string& view_mode )
{
	if ( !Core::Application::IsApplicationThread() )
	{
		Core::Application::PostEvent( boost::bind( &ViewerManager::update_viewers_overlay,
			this, view_mode ) );
		return;
	}
	
	for ( size_t i = 0; i < this->private_->viewers_.size(); ++i )
	{
		if ( this->private_->viewers_[ i ]->viewer_visible_state_->get() &&
			this->private_->viewers_[ i ]->view_mode_state_->get() == view_mode )
		{
			this->private_->viewers_[ i ]->redraw_overlay();
		}
	}
}

void ViewerManager::update_2d_viewers_overlay()
{
	if ( !Core::Application::IsApplicationThread() )
	{
		Core::Application::PostEvent( boost::bind( 
			&ViewerManager::update_2d_viewers_overlay, this ) );
		return;
	}

	for ( size_t i = 0; i < this->private_->viewers_.size(); ++i )
	{
		if ( this->private_->viewers_[ i ]->viewer_visible_state_->get() &&
			!this->private_->viewers_[ i ]->is_volume_view() )
		{
			this->private_->viewers_[ i ]->redraw_overlay();
		}
	}
}

ViewerHandle ViewerManager::get_active_viewer()
{
	int viewer_id;
	{
		Core::StateEngine::lock_type lock( Core::StateEngine::GetMutex() );
		viewer_id = this->active_viewer_state_->get();
	}
	return this->private_->viewers_[ viewer_id ];
}

void ViewerManager::reset_cursor()
{
	for ( size_t i = 0; i < this->private_->viewers_.size(); ++i )
	{
		this->private_->viewers_[ i ]->set_cursor( Core::CursorShape::CROSS_E );
	}
}

bool ViewerManager::is_busy()
{
	for ( size_t i = 0; i < this->private_->viewers_.size(); ++i )
	{
		if( this->private_->viewers_[ i ]->is_busy() )
		{
			return true;
		}
	}
	return false;
}

} // end namespace Seg3D

