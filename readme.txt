/*
	CADET - Center for Advances in Digital Entertainment Technologies
	Copyright 2012 University of Applied Science Salzburg / MultiMediaTechnology

	http://www.cadet.at
	http://multimediatechnology.at/

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

	CADET - Center for Advances in Digital Entertainment Technologies
	 
	Authors: Robert Praxmarer
	Web: http://www.1n0ut.com
	Email: support@cadet.at
	Created: 16-04-2011

	This wrapper uses FFmpeg, and is licensed and credited as follows:

	 * copyright (c) 2001 Fabrice Bellard
	 *
	 * This  FFmpeg.
	 *
	 * FFmpeg is free software; you can redistribute it and/or
	 * modify it under the terms of the GNU Lesser General Public
	 * License as published by the Free Software Foundation; either
	 * version 2.1 of the License, or (at your option) any later version.
	 *
	 * FFmpeg is distributed in the hope that it will be useful,
	 * but WITHOUT ANY WARRANTY; without even the implied warranty of
	 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 * Lesser General Public License for more details.
	 *
	 * You should have received a copy of the GNU Lesser General Public
	 * License along with FFmpeg; if not, write to the Free Software
	 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA	 
*/

1) build 
  * just set the environment variables CINDER_DIR and BOOST_DIR to the according directories
  
2) Todos
  * Probe for unknown files, right now crash
  * Improve seeking for some files formats (find alternatives to forward encoding)
  * Audio integration and syncing
  * Playing from Url
  * Encoder
  * set primary audio/video stream
  * more video/audio info
  * dealing with dvds chapters...
  * setting ffmpeg options (disable and enable)
  * better encapsulation ,shared_ptrs, ...
  * cross platform
  